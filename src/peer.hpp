#ifndef PEER_HPP_
#define PEER_HPP_

#include "util/fd.hpp"

#include <condition_variable>
#include <functional>
#include <string>

#define SYNC_PROTO SOCK_STREAM

#ifndef RELIABLE_UPDATE
#define UPDATE_PROTO SOCK_DCCP
#else
#define UPDATE_PROTO SYNC_PROTO
#endif

#define UpdateProtocol ConnectionConstants<UPDATE_PROTO>
#define SyncProtocol ConnectionConstants<SYNC_PROTO>

class ExifHash;
class Logger;

class Peer {
 public:
  typedef std::function<const char*(void)> PathGenerator;

  Peer(Logger* logger);
  void Sync(PathGenerator path_gen);

 protected:
  virtual void CreateConnections(FD* download_fd, FD* upload_fd) = 0;
  virtual void InitUpdateConnectionSocket(FD* fd);
  virtual void InitUpdateConnection(int sync_fd, FD* fd) = 0;
  virtual void InitSyncConnectionSocket(FD* fd);
  virtual void InitDownloadConnection(FD* fd) = 0;
  virtual void InitUploadConnection(FD* fd) = 0;
  virtual void Download(const ExifHash& hash, FD* fd) = 0;
  // void Upload();

  Logger* logger_;
};

#endif // PEER_HPP_
