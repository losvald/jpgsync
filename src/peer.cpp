#include "peer.hpp"

#include "debug.hpp"
#include "exif_hash.hpp"
#include "exif_hasher.hpp"
#include "protocol.hpp"
#include "util/fd.hpp"
#include "util/logger.hpp"
#include "util/fstream_utils.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <atomic>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <climits>
#include <cstring>

namespace {

inline std::string ToImageStr(const ExifHash& hash,
                              const std::string& filename) {
  return "image with hash " + ToString(hash) + ": " + filename;
}

} // namespace


Peer::Peer(Logger* logger) : logger_(logger) {}
Peer::~Peer() {}

void Peer::Sync(PathGenerator path_gen) {
  ExifHasher exif_hasher;
  exif_hasher.Run(UpdateProtocol::hashes_per_packet, path_gen);

  std::mutex hasher_progress_mutex;
  std::condition_variable hasher_progress;
  std::atomic<size_t> hasher_entry_count(0);
  std::atomic_flag hashing; hashing.test_and_set(); // set to true

  uint16_t update_port = 0;
  std::atomic_flag update_init = ATOMIC_FLAG_INIT;

  std::thread downloader([&] {
      FD download_fd;
      InitSyncConnection(&download_fd, &update_port);

      std::unordered_set<ExifHash> found_hashes;
      std::thread update_sender([&] {
          FD update_fd;
          if (!update_init.test_and_set())
            InitUpdateConnection(update_port, &update_fd);

          unsigned char buf[UpdateProtocol::
                            hashes_per_packet * sizeof(ExifHash)];
          while (true) {
            // wait for hasher progress, until next hash_count hashes are found
            size_t hash_count;
            auto e = exif_hasher.Get(&hash_count);
            if (hash_count == 0)
              break;

            // notify the upload thread of the progress / newly found entries
            hasher_entry_count.fetch_add(hash_count);
            hasher_progress.notify_one();

            // advance e to the latest entry, filling buf along the way
            logger_->Verbose("sending " + ToString(hash_count) + " hashes");
            ssize_t write_count = hash_count * sizeof(ExifHash);
            auto bytes = buf + write_count;
            auto e_first = e;
            do {
              found_hashes.insert(*e->hash);
              e->hash->ToDigest(bytes -= sizeof(ExifHash));
              e = e->next;
            } while (bytes != buf);

            // send the new hashes to the receiver
            UpdateProtocol::WriteFully(update_fd, buf, write_count);
            if (logger_->verbosity() > 1) {
              for (auto e = e_first; hash_count--; e = e->next)
                logger_->Verbose("sent hash:" + ToString(*e->hash), 2);
            }
          }

          // notify the upload thread that hashing is done
          hashing.clear();
          hasher_progress.notify_one();
        });

      // wait until all hashes are sent and notify the receiver
      update_sender.join();

      // notify the receiver that all hashes have been sent
      unsigned char byte;
      SyncProtocol::WriteByte(download_fd, byte);

      logger_->Verbose("started downloading");
      std::ofstream ofs;

      std::vector<ExifHash> missing_hashes;
      unsigned char buf[SyncProtocol::hashes_per_packet * sizeof(ExifHash)];
      unsigned char found_bitmask[
          (SyncProtocol::hashes_per_packet + CHAR_BIT - 1) / CHAR_BIT];

      for (size_t hash_count; SyncProtocol::
               ReadByte(download_fd, &hash_count); ) {
        if (hash_count > SyncProtocol::hashes_per_packet) {
          logger_->Fatal("sync received invalid offer hash count: " +
                         ToString(hash_count));
        }

        size_t read_count = hash_count * sizeof(ExifHash);
        if (!SyncProtocol::ReadExactly(download_fd, buf, read_count)) {
          logger_->Fatal("sync received invalid offer packet length: " +
                         ToString(read_count));
        }

        // figure out missing hashes and confirm found ones via found_bitmask
        missing_hashes.clear();
        memset(found_bitmask, 0, sizeof(found_bitmask));
        auto found = found_bitmask;
        int found_bit = 0;
        auto bytes_end = buf + read_count;
        for (auto bytes = buf; bytes != bytes_end; bytes += sizeof(ExifHash)) {
          missing_hashes.emplace_back(bytes);
          const auto& hash = missing_hashes.back();
          if (found_hashes.count(hash)) {
            logger_->Verbose("rejected download: " + ToString(hash));
            *found |= (1 << found_bit);
            missing_hashes.pop_back();
            continue;
          } else {
            logger_->Verbose("accepted download: " + ToString(hash), 2);
          }
          found += (++found_bit % CHAR_BIT == 0);
        }
        if (!SyncProtocol::WriteExactly(
                download_fd, found_bitmask,
                (hash_count + CHAR_BIT - 1) / CHAR_BIT)) {
          logger_->Fatal("cannot send download confirmation");
        }

        // download missing images
        for (const auto& hash : missing_hashes) {
          // receive filename
          char filename[0xFF + 1];
          unsigned char filename_len;
          if (!SyncProtocol::ReadByte(download_fd, &filename_len) ||
              !SyncProtocol::ReadExactly(download_fd, filename, filename_len)){
            logger_->Fatal("failed to receive filename for " + ToString(hash));
          }
          filename[filename_len] = 0;
#define IMG_STR ToImageStr(hash, filename)

          // receive file size
          size_t file_size;
          if (!SyncProtocol::ReadExactly(
                  download_fd, &file_size, sizeof(file_size))) {
            logger_->Fatal("failed to receive size of " + IMG_STR);
          }

          // create file <filename> or <filename>-<sha1> (if former exists)
          FstreamCloseGuard<decltype(ofs)> ofs_closer(&ofs);
          if (!ReopenEnd(filename, &ofs) &&
              !ReopenEnd((filename + ("-" + ToString(hash))).c_str(), &ofs)) {
            logger_->Error("filename conflict resolution failed for " +
                           IMG_STR);
            // proceed with download, not to confuse the uploader
            ReopenEnd("/dev/null", &ofs);
          }
          ofs.seekp(0, ofs.beg);

          // download the file
          try {
            Download(&download_fd, file_size, &ofs);
          } catch (const std::exception& e) {
            logger_->Verbose(e.what());
            logger_->Fatal("failed to download " + IMG_STR);
          }
#undef IMG_STR
        }
      }
      logger_->Verbose("finished downloading");
    });

  std::thread uploader([&] {
      FD upload_fd;
      InitSyncConnection(&upload_fd, &update_port);

      bool updated = false;
      std::mutex updated_mutex;

      std::unordered_set<ExifHash> received_hashes;
      std::thread update_receiver([&] {
          FD update_fd;
          if (!update_init.test_and_set())
            InitUpdateConnection(update_port, &update_fd);

          unsigned char buf[UpdateProtocol::
                            hashes_per_packet * sizeof(ExifHash)];
          while (true) {
            ssize_t read_count = UpdateProtocol::
                ReadFully(update_fd, buf, sizeof(buf));
            if (!read_count) {
              logger_->Verbose("received end of update");
              break;
            }

            if (read_count % sizeof(ExifHash)) {
              logger_->Warn("update received invalid packet length: " +
                            ToString(read_count));
              continue;
            }

            std::unique_lock<std::mutex> locker(updated_mutex);
            if (!updated)
              break;

            auto bytes = buf + read_count;
            do {
              bytes -= sizeof(ExifHash);
              // received_hashes.emplace(bytes);
              received_hashes.insert(bytes);
            } while (bytes != buf);

            if (logger_->verbosity() > 1) {
              auto bytes = buf + read_count;
              do {
                ExifHash eh(bytes -= sizeof(ExifHash));
                logger_->Verbose("received hash: " + ToString(eh), 2);
              } while (bytes != buf);
            }
          }
        });
      update_receiver.detach();

      // wait until the sender notifies that it has sent all hashes
      char byte;
      ssize_t read_count;
      sys_call_rv(read_count, read, upload_fd, &byte, 1);
      if (!read_count) {
        logger_->Fatal("update failed: no response from update sender");
      }

      // mark the end of update
      {
        std::unique_lock<std::mutex> locker(updated_mutex);
        updated = true;
      }

      logger_->Verbose("started uploading");
      std::ifstream ifs;
      unsigned char buf[SyncProtocol::hashes_per_packet * sizeof(ExifHash)];
      unsigned char found_bitmask[
          (SyncProtocol::hashes_per_packet + CHAR_BIT - 1) / CHAR_BIT];

      size_t processed_entry_count = 0;
      auto latest_entry = exif_hasher.before_first_entry();
      std::vector<decltype(latest_entry)> missing_entries;
      while (true) {
        // wait until new hasher entries are found or hashing is done
        size_t total_entry_count = hasher_entry_count.load();
        if (processed_entry_count == total_entry_count) {
          if (!hashing.test_and_set())  // hasher is done
            break;
          std::unique_lock<std::mutex> locker(hasher_progress_mutex);
          hasher_progress.wait(locker);
          continue;
        }

        // figure out hash_count hashes that might be missing on the receiver,
        // picking at most SyncProtocol::hash_per_packet of them
        size_t hash_count = 0;
        auto bytes = buf;
        missing_entries.clear();
        do {
          latest_entry = latest_entry->next;
          const auto& hash = *latest_entry->hash;
          if (received_hashes.count(hash)) {
            logger_->Verbose("skipping upload of " + ToString(hash), 2);
            continue;
          }
          hash.ToDigest(bytes);
          bytes += sizeof(ExifHash);
          missing_entries.push_back(latest_entry);
          ++hash_count;
        } while (++processed_entry_count != total_entry_count &&
                 hash_count < SyncProtocol::hashes_per_packet);

        if (logger_->verbosity() > 1) {
          logger_->Verbose("sending offer of size " + ToString(hash_count), 2);
          for (const auto& hash : missing_entries)
            logger_->Verbose("offering " + ToString(hash), 2);
        }

        // if all of them confirmed by receiver (in update), nothing to offer
        if (!hash_count)
          continue;

        // send an offer to upload hash_count hashes
        if (!SyncProtocol::WriteByte(upload_fd, hash_count) ||
            !SyncProtocol::WriteExactly(upload_fd, buf,
                                        hash_count * sizeof(ExifHash))) {
          logger_->Fatal("failed to send upload offer of size: " +
                         ToString(hash_count));
        }

        // receive negative acks (what receiver already has) in found_bitmask
        if (!SyncProtocol::ReadExactly(
                upload_fd, found_bitmask,
                (hash_count + CHAR_BIT - 1) / CHAR_BIT)) {
          logger_->Fatal("failed to receive offer confirmation");
        }

        auto found = found_bitmask;
        int found_bit = 0;
        for (auto entry : missing_entries) {
#define IMG_STR ToImageStr(*entry->hash, entry->path)
          if (!(*found & (1 << found_bit))) {
            if (!SyncProtocol::WriteExactly(
                    upload_fd, entry->path.c_str(), entry->path.length())) {
              logger_->Fatal("failed to send filename of " + IMG_STR);
            }

            // open the file to download and send its size
            FstreamCloseGuard<decltype(ifs)> ifs_closer(&ifs);
            if (!ReopenEnd(entry->path.c_str(), &ifs)) {
              logger_->Fatal("failed to open " + IMG_STR);
            }
            size_t file_size = static_cast<size_t>(ifs.tellg());
            if (!SyncProtocol::WriteExactly(
                    upload_fd, &file_size, sizeof(file_size))) {
              logger_->Fatal("failed to send size of " + IMG_STR);
            }
            ifs.seekg(0, ifs.beg);

            // upload the file
            try {
              Upload(file_size, &ifs, &upload_fd);
            } catch (const std::exception& e) {
              logger_->Verbose(e.what());
              logger_->Fatal("failed to upload " + IMG_STR);
            }
          }
          found += (++found_bit % CHAR_BIT == 0);
#undef IMG_STR
        }
      } // while (true)

      logger_->Verbose("finished uploading");
    });

  downloader.join();
  uploader.join();
}

void Peer::Download(FD* fd, size_t file_size, std::ofstream* ofs) {
  std::vector<char> file(file_size);
  if (!SyncProtocol::ReadExactly(*fd, file.data(), file_size))
    throw std::runtime_error("failed to receive image");
  ofs->write(file.data(), file_size);
}

void Peer::Upload(size_t file_size, std::ifstream* ifs, FD* fd) {
  std::vector<char> file(file_size);
  ifs->read(file.data(), file_size);
  if (!SyncProtocol::WriteExactly(*fd, file.data(), file_size))
    throw std::runtime_error("failed to send image");
}
