#include "peer.hpp"

#include "connection_constants.hpp"
#include "debug.hpp"
#include "exif_hash.hpp"
#include "exif_hasher.hpp"
#include "util/fd.hpp"
#include "util/logger.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <atomic>
#include <fstream>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

template<class Stream>
bool Reopen(const char* filename, Stream* stream) {
  stream->close();
  stream->clear();
  stream->open(filename);
  return stream->is_open();
}

} // namespace

Peer::Peer(Logger* logger) : logger_(logger) {}
Peer::~Peer() {}

void Peer::Sync(PathGenerator path_gen) {
  ExifHasher exif_hasher;
  exif_hasher.Run(UpdateProtocol::hashes_per_packet,
                  path_gen);

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
            size_t hash_count;
            auto e = exif_hasher.Get(&hash_count);
            if (hash_count == 0)
              break;

            logger_->Verbose("sending " + ToString(hash_count) + " hashes");
            ssize_t write_count = hash_count * sizeof(ExifHash);
            auto bytes = buf + write_count;
            auto e_first = e;
            do {
              found_hashes.insert(*e->hash);
              e->hash->ToDigest(bytes -= sizeof(ExifHash));
              e = e->next;
            } while (bytes != buf);
            UpdateProtocol::
                WriteFully(update_fd, buf, write_count);
            if (logger_->verbosity() > 1) {
              for (auto e = e_first; hash_count--; e = e->next)
                logger_->Verbose("sent hash:" + ToString(*e->hash), 2);
            }
          }
        });

      // wait until all hashes are sent and notify the receiver
      update_sender.join();

      unsigned char byte;
      SyncProtocol::WriteByte(download_fd, byte);

      logger_->Verbose("started downloading");
      std::ofstream ofs;

      std::vector<ExifHash> missing_hashes;
      unsigned char buf[SyncProtocol::
                        hashes_per_packet * sizeof(ExifHash)];

      for (size_t hash_count; SyncProtocol::
               ReadByte(download_fd, &hash_count); ) {
        if (hash_count > SyncProtocol::hashes_per_packet) {
          logger_->Error("sync received invalid offer hash count: " +
                        ToString(hash_count));
          break;
        }

        size_t read_count = hash_count * sizeof(ExifHash);
        if (!SyncProtocol::ReadExactly(
                download_fd, buf, read_count)) {
          logger_->Error("sync received invalid offer packet length: " +
                         ToString(read_count));
          break;
        }

        missing_hashes.clear();
        auto bytes = buf + read_count;
        do {
          missing_hashes.emplace_back(bytes -= sizeof(ExifHash));
          const auto& hash = missing_hashes.back();
          if (found_hashes.count(hash)) {
            logger_->Verbose("rejected download: " + ToString(hash));
            continue;
          }

          logger_->Verbose("accepted download: " + ToString(hash), 2);
          if (!SyncProtocol::WriteExactly(
                  download_fd, bytes, sizeof(ExifHash))) {
            logger_->Error("cannot send download confirmation: " +
                           ToString(hash));
          }
        } while (bytes != buf);

        for (const auto& hash : missing_hashes) {
          char filename[0xFF + 1];
          unsigned char filename_len;
          if (!SyncProtocol::ReadByte(download_fd, &filename_len) ||
              !SyncProtocol::ReadExactly(download_fd, filename, filename_len)){
            logger_->Error("missing filename for: " + ToString(hash));
            continue;
          }
          filename[filename_len] = 0;

          // create filename <filename> or <filename>-<sha1> (if former exists)
          if (!(Reopen(filename, &ofs) ||
                Reopen((filename + ("-" + ToString(hash))).c_str(), &ofs))) {
              logger_->Error("filename conflict resolution failed for: " +
                             ToString(hash));
          }
          // and download
          Download(&download_fd, &ofs);
        }
      }
      download_fd.Close();
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
        logger_->Error("update failed: no response from update sender");
      }

      // mark the end of update
      {
        std::unique_lock<std::mutex> locker(updated_mutex);
        updated = true;
      }

      logger_->Verbose("started uploading");
      // TODO Upload();
      logger_->Verbose("finished uploading");
      upload_fd.Close();
    });

  downloader.join();
  uploader.join();
}

void Peer::Download(FD* fd, std::ofstream* ofs) {
  // TODO
}

// void Peer::Upload() {
//   // TODO
// }
