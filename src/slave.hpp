#ifndef SLAVE_HPP_
#define SLAVE_HPP_

#include "peer.hpp"

#include <cstdint>

#include <string>

class AddrInfo;

class Slave : public Peer {
 public:
  Slave(Logger* logger);
  ~Slave();
  void Attach(const std::string& master_host, uint16_t master_port);
 protected:
  void InitUpdateConnection(FD* update_fd);
  void InitSyncConnection(FD* sync_fd, uint16_t* update_port);
 private:
  AddrInfo* update_addr_info_;
  AddrInfo* sync_addr_info_;
};

#endif // SLAVE_HPP_
