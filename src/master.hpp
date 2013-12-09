#ifndef MASTER_HPP_
#define MASTER_HPP_

#include "peer.hpp"

#include "util/fd.hpp"

#include <cstdint>

class Master : public Peer {
 public:
  Master(Logger* logger);
  uint16_t Listen();
  void set_port(uint16_t port);
 protected:
  void InitUpdateConnection(int* update_fd);
  bool InitSyncConnection(int* sync_fd, bool download);
 private:
  uint16_t update_port_;
  uint16_t sync_port_;
  FD sync_sock_;
  FD update_sock_;
};

#endif // MASTER_HPP_
