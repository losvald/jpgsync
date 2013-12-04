#ifndef SLAVE_HPP_
#define SLAVE_HPP_

#include "peer.hpp"

#include <cstdint>

#include <string>

class Slave : public Peer {
 public:
  Slave(const std::string& master_host, uint16_t master_port, Logger* logger);
  void Sync(const std::string& root);
 protected:
  void InitUpdateConnection(int sync_fd, FD* update_fd);
  void InitDownloadConnection();
  void InitUploadConnection();
 private:
  std::string master_host_;
  uint16_t master_port_;
};

#endif // SLAVE_HPP_
