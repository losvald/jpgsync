#ifndef PEER_HPP_
#define PEER_HPP_

#include "util/fd.hpp"

#include <condition_variable>

class Logger;

class Peer {
 public:
  Peer(Logger* logger);
  void Sync(const std::string& root);

 protected:
  static const int kSyncProto;
  static const int kUpdateProto;

  virtual int InitUpdateConnectionSocket(FD* fd);
  virtual void InitUpdateConnection(int sync_fd, FD* fd) = 0;

  virtual int InitSyncConnectionSocket(FD* fd);
  virtual void InitDownloadConnection() = 0;
  virtual void InitUploadConnection() = 0;
  void Download();
  void Upload();

  Logger* logger_;
  FD download_fd_;
  FD upload_fd_;
 private:
  std::string root_;
};

#endif // PEER_HPP_
