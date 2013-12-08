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
  void InitUpdateConnection(FD* update_fd);
  void InitSyncConnection(FD* sync_fd, uint16_t* update_port);
 private:
  uint16_t update_port_;
  uint16_t sync_port_;
  FD sync_sock_;
  FD update_sock_;
};

#endif // MASTER_HPP_
