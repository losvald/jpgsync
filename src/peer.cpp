#include "peer.hpp"

#include "debug.hpp"
#include "exif_hash.hpp"
#include "exif_hasher.hpp"
#include "protocol.hpp"
#include "util/fd.hpp"
#include "util/logger.hpp"
#include "util/fd.hpp"
#include "util/fstream_utils.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <atomic>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <climits>
#include <cstring>

namespace {

inline std::string ToImageStr(const ExifHash& hash,
                              const std::string& filename) {
  return "image with hash " + ToString(hash) + ": " + filename;
}

const char* ToFilename(const std::string& path) {
  size_t pos = path.rfind('/');
  return path.c_str() + (pos != std::string::npos) * (pos + 1);
}

} // namespace


Peer::Peer(Logger* logger) : logger_(logger) {}
Peer::~Peer() {}

bool Peer::InitSyncConnection(int* sync_fd, bool download) {
  if (!SyncProtocol::WriteByte(*sync_fd, download))
    logger_->Fatal("Failed to send connection id to peer");
  bool peer_download;
  if (!SyncProtocol::ReadByte(*sync_fd, &peer_download))
    logger_->Fatal("Failed to receive peer connection id");
  return peer_download != download;
}

void Peer::Sync(PathGenerator path_gen) {
  ExifHasher exif_hasher;
  exif_hasher.Run(UpdateProtocol::hashes_per_packet, path_gen);

  std::mutex hasher_progress_mutex;
  std::condition_variable hasher_progress;
  std::atomic<size_t> hasher_entry_count(0);
  bool hashing = true;

  std::mutex init_update_mutex;
  FD update_fd;
  auto update_initializer = [&] {
    std::lock_guard<std::mutex> locker(init_update_mutex);
    if (update_fd.closed()) {
      int fd;
      InitUpdateConnection(&fd);
      update_fd = fd;
    }
  };

  // initialize connections in parallel
  int download_fd, upload_fd;
  std::thread sync_initializer([&] {
      InitSyncConnection(&download_fd, true);
      update_initializer();
    });
  bool matched = InitSyncConnection(&upload_fd, false);
  update_initializer();

  // ensure download_fd is connected to peer's upload_fd and vice-versa
  sync_initializer.join();
  if (!matched)
    std::swap(download_fd, upload_fd);
  DEBUG_OUT_LN(SYNC, "match=%d | INIT'D SYNC CONNECTIONS", (int)matched);

  std::thread downloader([&] {
      FD sync_fd = download_fd;
      DEBUG_OUT_LN(SYNCRECV, "fd=%2d | INIT SYNC DONE", (int)sync_fd);

      // send update
      while (true) {
        unsigned char buf[UpdateProtocol::
                          hashes_per_packet * sizeof(ExifHash)];

        // wait for hasher progress, until next hash_count hashes are found
        size_t hash_count = UpdateProtocol::hashes_per_packet;
        auto e = exif_hasher.Get(&hash_count);
        if (hash_count == 0)
          break;

        // notify the upload thread of the progress / newly found entries
        DEBUG_OUT_LN(UPDSEND, "cnt=%2lu | NOTIFY PROGRESS", hash_count);
        hasher_entry_count.fetch_add(hash_count);
        hasher_progress.notify_one();

        // advance e to the latest entry, filling buf along the way
        logger_->Verbose("sending " + ToString(hash_count) + " hashes");
        ssize_t write_count = hash_count * sizeof(ExifHash);
        auto bytes = buf + write_count;
        auto e_first = e;
        do {
          DEBUG_OUT_LN(UPDSEND, "hash=%s; path=%s | NEW ENTRY",
                       DEBUG_STR(*e->hash), e->path.c_str());
          e->hash->ToDigest(bytes -= sizeof(ExifHash));
          e = e->next;
        } while (bytes != buf);

        // send the new hashes to the receive
        DEBUG_OUT_LN(UPDSEND, "update=%s | SENDING",
                     DEBUG_HEX_STR(buf, write_count));
        UpdateProtocol::WriteFully(update_fd, buf, write_count);
        if (logger_->verbosity() > 1) {
          for (auto e = e_first; hash_count--; e = e->next)
            logger_->Verbose("sent hash:" + ToString(*e->hash), 2);
        }
      }

      logger_->Verbose("sent update of size " +
                       ToString(exif_hasher.entry_count()));

      // notify the upload thread that hashing is done
      DEBUG_OUT_LN(UPDSEND, "NOTIFY DONE");
      {
        std::unique_lock<std::mutex> locker(hasher_progress_mutex);
        hashing = false;
        hasher_progress.notify_one();
      }

      // notify the receiver that all hashes have been sent
      DEBUG_OUT_LN(SYNCRECV, "NOTIFYING UPDATE SENT");
      unsigned char byte;
      SyncProtocol::WriteByte(sync_fd, byte);
      DEBUG_OUT_LN(SYNCRECV, "NOTIFIED UPDATE SENT");

      logger_->Verbose("started downloading");
      std::ofstream ofs;

      std::vector<ExifHash> missing_hashes;
      unsigned char buf[SyncProtocol::hashes_per_packet * sizeof(ExifHash)];
      unsigned char found_bitmask[
          (SyncProtocol::hashes_per_packet + CHAR_BIT - 1) / CHAR_BIT];

      for (size_t hash_count; SyncProtocol::
               ReadByte(sync_fd, &hash_count); ) {
        if (hash_count > SyncProtocol::hashes_per_packet) {
          logger_->Fatal("sync received invalid offer hash count: " +
                         ToString(hash_count));
        }

        size_t read_count = hash_count * sizeof(ExifHash);
        if (!SyncProtocol::ReadExactly(sync_fd, buf, read_count)) {
          logger_->Fatal("sync received invalid offer packet length: " +
                         ToString(read_count));
        }
        DEBUG_OUT_LN(SYNCRECV, "offer=%s | RECEIVED OFFER",
                     DEBUG_HEX_STR(buf, read_count));

        // figure out missing hashes and confirm found ones via found_bitmask
        missing_hashes.clear();
        memset(found_bitmask, 0, sizeof(found_bitmask));
        auto found = found_bitmask;
        int found_bit = 0;
        auto bytes_end = buf + read_count;
        for (auto bytes = buf; bytes != bytes_end; bytes += sizeof(ExifHash)) {
          missing_hashes.emplace_back(bytes);
          const auto& hash = missing_hashes.back();
          if (exif_hasher.Contains(hash)) {
            logger_->Verbose("rejected download: " + ToString(hash));
            *found |= (1 << found_bit);
            missing_hashes.pop_back();
            continue;
          } else {
            logger_->Verbose("accepted download: " + ToString(hash), 2);
          }
          found += (++found_bit % CHAR_BIT == 0);
        }
        size_t found_bitmask_size = (hash_count + CHAR_BIT - 1) / CHAR_BIT;
        if (!SyncProtocol::WriteExactly(
                sync_fd, found_bitmask, found_bitmask_size)) {
          logger_->Fatal("cannot send download confirmation");
        }
        DEBUG_OUT_LN(SYNCRECV, "bitmask=%s | SENDING FOUND BITMASK",
                     DEBUG_HEX_STR(found_bitmask, found_bitmask_size));

        // download missing images
        for (const auto& hash : missing_hashes) {
          // receive filename
          char filename[0xFF + 1];
          unsigned char filename_len;
          if (!SyncProtocol::ReadByte(sync_fd, &filename_len) ||
              !SyncProtocol::ReadExactly(sync_fd, filename, filename_len)){
            logger_->Fatal("failed to receive filename for " + ToString(hash));
          }
          filename[filename_len] = 0;
#define IMG_STR ToImageStr(hash, filename)

          // receive file size
          size_t file_size;
          if (!SyncProtocol::ReadFileSize(sync_fd, &file_size))
            logger_->Fatal("failed to receive size of " + IMG_STR);

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
            DEBUG_OUT_LN(SYNCRECV, "hash=%s; size=%lu; name=%s | DOWNLOADING",
                         DEBUG_STR(hash), (size_t)file_size, filename);
            Download(sync_fd, file_size, &ofs);
            DEBUG_OUT_LN(SYNCRECV, "hash=%s; size=%lu; name=%s | DOWNLOADED",
                         DEBUG_STR(hash), (size_t)file_size, filename);
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
      FD sync_fd = upload_fd;
      DEBUG_OUT_LN(SYNCSEND, "fd=%2d | INIT SYNC DONE", (int)sync_fd);

      bool updated = false;
      std::mutex updated_mutex;

      std::unordered_set<ExifHash> received_hashes;
      std::thread update_receiver([&] {
          while (true) {
            unsigned char buf[UpdateProtocol::
                              hashes_per_packet * sizeof(ExifHash)];
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
            if (updated)
              break;

            DEBUG_OUT_LN(UPDRECV, "update=%s | RECEIVED",
                         DEBUG_HEX_STR(buf, read_count));
            auto bytes = buf + read_count;
            do {
              bytes -= sizeof(ExifHash);
              // received_hashes.emplace(bytes); // not supported on GCC 4.6.3
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
          DEBUG_OUT_LN(UPDRECV, "DONE");
        });
      update_receiver.detach();

      // wait until the sender notifies that it has sent all hashes
      DEBUG_OUT_LN(SYNCSEND, "WAITING UNTIL UPDATE RECEIVED");
      char byte;
      ssize_t read_count;
      if (!SyncProtocol::ReadByte(sync_fd, &byte)) {
        logger_->Fatal("update failed: no response from update sender");
      }

      // mark the end of update
      DEBUG_OUT_LN(SYNCSEND, "NOTIFY UPDATE RECEIVED");
      {
        std::unique_lock<std::mutex> locker(updated_mutex);
        updated = true;
      }

      logger_->Verbose("received update of size " +
                       ToString(received_hashes.size()));

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
          std::unique_lock<std::mutex> locker(hasher_progress_mutex);
          if (!hashing) // hasher is done
            break;
          DEBUG_OUT_LN(SYNCSEND, "WAIT FOR HASHER");
          hasher_progress.wait(locker);
          continue;
        }

        // figure out hash_count hashes that might be missing on the receiver,
        // picking at most SyncProtocol::hash_per_packet of them
        missing_entries.clear();
        auto bytes = buf;
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
        } while (++processed_entry_count != total_entry_count &&
                 missing_entries.size() < SyncProtocol::hashes_per_packet);

        DEBUG_OUT_LN(SYNCSEND, "missing=%lu; total=%lu | DETERMINE",
                     missing_entries.size(), (bytes - buf) / sizeof(ExifHash));

        if (logger_->verbosity() > 1) {
          logger_->Verbose("sending offer of size " +
                           ToString(missing_entries.size()), 2);
          for (const auto& hash : missing_entries)
            logger_->Verbose("offering " + ToString(hash), 2);
        }

        // if all of them confirmed by receiver (in update), nothing to offer
        if (missing_entries.empty())
          continue;

        // send an offer to upload hash_count hashes
        size_t hash_count = missing_entries.size();
        DEBUG_OUT_LN(SYNCSEND, "offer=%s | OFFERING",
                     DEBUG_HEX_STR(buf, hash_count * sizeof(ExifHash)));
        if (!SyncProtocol::WriteByte(sync_fd, hash_count) ||
            !SyncProtocol::WriteExactly(sync_fd, buf,
                                        hash_count * sizeof(ExifHash))) {
          logger_->Fatal("failed to send upload offer of size: " +
                         ToString(hash_count));
        }

        // receive negative acks (what receiver already has) in found_bitmask
        size_t found_bitmask_size = (hash_count + CHAR_BIT - 1) / CHAR_BIT;
        if (!SyncProtocol::ReadExactly(sync_fd, found_bitmask,
                                       found_bitmask_size)) {
          logger_->Fatal("failed to receive offer confirmation");
        }
        DEBUG_OUT_LN(SYNCSEND, "bitmask=%s | RECEIVED FOUND BITMASK",
                     DEBUG_HEX_STR(found_bitmask, found_bitmask_size));

        auto found = found_bitmask;
        int found_bit = 0;
        for (auto entry : missing_entries) {
#define IMG_STR ToImageStr(*entry->hash, entry->path)
          if (!(*found & (1 << found_bit))) {
            const char* filename = ToFilename(entry->path);
            auto filename_len = static_cast<unsigned char>(strlen(filename));
            if (!SyncProtocol::WriteByte(sync_fd, filename_len) ||
                !SyncProtocol::WriteExactly(sync_fd, filename, filename_len)) {
              logger_->Fatal("failed to send filename of " + IMG_STR);
            }

            // open the file to download and send its size
            FstreamCloseGuard<decltype(ifs)> ifs_closer(&ifs);
            if (!ReopenEnd(entry->path.c_str(), &ifs)) {
              logger_->Fatal("failed to open " + IMG_STR);
            }
            size_t file_size = static_cast<size_t>(ifs.tellg());
            if (!SyncProtocol::WriteFileSize(sync_fd, file_size)) {
              logger_->Fatal("failed to send size of " + IMG_STR);
            }
            ifs.seekg(0, ifs.beg);

            // upload the file
            try {
              DEBUG_OUT_LN(SYNCSEND, "hash=%s; size=%lu; path=%s | UPLOADING",
                           DEBUG_STR(*entry->hash), file_size,
                           entry->path.c_str());
              Upload(sync_fd, file_size, &ifs);
              DEBUG_OUT_LN(SYNCSEND, "hash=%s; size=%lu; path=%s | UPLOADED",
                           DEBUG_STR(*entry->hash), file_size,
                           entry->path.c_str());
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

void Peer::Download(int sync_fd, size_t file_size, std::ofstream* ofs) {
  std::vector<char> file(file_size);
  if (!SyncProtocol::ReadExactly(sync_fd, file.data(), file_size))
    throw std::runtime_error("failed to receive image");
  ofs->write(file.data(), file_size);
}

void Peer::Upload(int sync_fd, size_t file_size, std::ifstream* ifs) {
  std::vector<char> file(file_size);
  ifs->read(file.data(), file_size);
  if (!SyncProtocol::WriteExactly(sync_fd, file.data(), file_size)) {
    throw std::runtime_error("failed to send image");
  }
}
