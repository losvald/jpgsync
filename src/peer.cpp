#include "peer.hpp"

#include "connection_constants.hpp"
#include "debug.hpp"
#include "exif_hash.hpp"
#include "exif_hasher.hpp"
#include "util/dir.hpp"
#include "util/fd.hpp"
#include "util/logger.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <arpa/inet.h>

#include <atomic>
#include <thread>
#include <unordered_set>

const int Peer::kSyncProto = SOCK_STREAM;
const int Peer::kUpdateProto =
#ifdef RELIABLE_UPDATE
    SOCK_STREAM
#else
    SOCK_DGRAM
#endif
    ;

namespace {

} // namespace

Peer::Peer(Logger* logger) : logger_(logger) {}

int Peer::InitSyncConnectionSocket(FD* fd) {
  return ConnectionConstants<kSyncProto>::InitSocket(*fd);
}

int Peer::InitUpdateConnectionSocket(FD* fd) {
  return ConnectionConstants<kUpdateProto>::InitSocket(*fd);
}

void Peer::Sync(const std::string& root) {
  Dir dir(root);
  ExifHasher exif_hasher;
  exif_hasher.Run(ConnectionConstants<kUpdateProto>::hashes_per_packet,
                  [&] { return dir.Next().c_str(); });

  FD update_fd;
  std::mutex init_update_mutex;
  // condition_variable<decltype(update_mutex)> init_update_done;

  std::thread downloader([&] {
      std::thread update_sender([&] {
          {
            std::unique_lock<std::mutex> locker(init_update_mutex);
            InitUpdateConnection(upload_fd_, &update_fd);
          }

          unsigned char buf[ConnectionConstants<kUpdateProto>::
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
              e->hash->ToDigest(bytes -= sizeof(ExifHash));
              e = e->next;
            } while (bytes != buf);
            sys_call(write, update_fd, buf, write_count);
            if (logger_->verbosity() > 1) {
              for (auto e = e_first; hash_count--; e = e->next)
                logger_->Verbose("sent hash:" + ToString(*e->hash), 2);
            }
          }
        });

      InitUploadConnection();

      // wait until all hashes are sent and notify the receiver
      update_sender.join();

      ssize_t read_count;
      char byte;
      sys_call_rv(read_count, write, download_fd_, &byte, 1);
      if (!read_count) {
      }

      logger_->Verbose("started downloading: " + root);
      Download();
      logger_->Verbose("finished downloading: " + root);
    });

  std::thread uploader([&] {
      std::unordered_set<ExifHash> received_hashes;

      bool updated = false;
      std::mutex updated_mutex;
      std::thread update_receiver([&] {
          {
            std::unique_lock<std::mutex> locker(init_update_mutex);
            InitUpdateConnection(upload_fd_, &update_fd);
          }

          unsigned char buf[ConnectionConstants<kSyncProto>::
                            hashes_per_packet * sizeof(ExifHash)];
          while (true) {
            ssize_t read_count;
            sys_call_rv(read_count, read, update_fd, buf, sizeof(buf));
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

      InitUploadConnection();

      // wait until the sender notifies that it has sent all hashes
      char byte;
      ssize_t read_count;
      sys_call_rv(read_count, read, upload_fd_, &byte, 1);
      if (!read_count) {
        logger_->Error("update failed: no response from update sender");
      }

      // mark the end of update
      {
        std::unique_lock<std::mutex> locker(updated_mutex);
        updated = true;
      }

      logger_->Verbose("started uploading: " + root);
      Upload();
      logger_->Verbose("finished uploading: " + root);
    });

  downloader.join();
  uploader.join();
}

void Peer::Download() {
  // TODO
}

void Peer::Upload() {
  // TODO

}
