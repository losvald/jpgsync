#ifndef PEER_HPP_
#define PEER_HPP_

#include "util/fd.hpp"

#include <cstdint>

#include <functional>
#include <iosfwd>

class ExifHash;
class Logger;

class Peer {
 public:
  typedef std::function<const char*(void)> PathGenerator;

  Peer(Logger* logger);
  virtual ~Peer();
  void Sync(PathGenerator path_gen);

 protected:
  virtual void InitUpdateConnection(uint16_t update_port, FD* update_fd) = 0;
  virtual void InitSyncConnection(FD* sync_fd, uint16_t* update_port) = 0;
  virtual void Download(FD* fd, std::ofstream* ofs);
  // void Upload();

  Logger* logger_;
};

#endif // PEER_HPP_
