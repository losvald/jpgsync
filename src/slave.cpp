#include "slave.hpp"

#include "debug.hpp"
#include "util/logger.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <exception>

class AddrInfo {
 public:
  AddrInfo(int protocol) : resolved_asi_begin_(NULL) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_protocol = protocol;
  }
  ~AddrInfo() {
    if (resolved_asi_begin_ != NULL)
      freeaddrinfo(resolved_asi_begin_);
    resolved_asi_begin_ = NULL;
  }

  AddrInfo& Resolve(const std::string& host, uint16_t port) {
    int rv;
    std::string port_str = ToString(port);
    sys_call2n_rv(0, rv, getaddrinfo, host.c_str(), port_str.c_str(),
                  &hints, &resolved_asi_begin_);
    return *this;
  }

  bool Connect(int fd) const {
    SysCallException* exception = NULL;
    for(auto ai = resolved_asi_begin_; ai != NULL; ai = ai->ai_next) {
      sys_call_rv(fd, socket, ai->ai_family, ai->ai_socktype,
                  ai->ai_protocol);
      DEBUG_OUT_LN(DL, "connect loop sock = %d", fd);

      try {
        sys_call(connect, fd, ai->ai_addr, ai->ai_addrlen);
        return true;
      } catch (const SysCallException& e) {
        exception = new SysCallException(e);
      }
    }

    if (exception != NULL) {
      SysCallException e(*exception);
      delete exception;
      throw e;
    }

    return false;
  }

 private:
  struct addrinfo hints, *resolved_asi_begin_;
};


Slave::Slave(Logger* logger)
    : Peer(logger),
      update_addr_info_(NULL),
      sync_addr_info_(NULL) {}

Slave::~Slave() {
  delete update_addr_info_;
  delete sync_addr_info_;
}

void Slave::Attach(const std::string& master_host, uint16_t master_port) {
  (update_addr_info_ = new AddrInfo(UpdateProtocol::protocol))->
      Resolve(master_host, master_port);
  (sync_addr_info_ = new AddrInfo(SyncProtocol::protocol))->
      Resolve(master_host, master_port);
}

void Slave::InitUpdateConnection(uint16_t update_port, FD* update_fd) {
  if (!update_addr_info_->Connect(*update_fd))
    logger_->Fatal("Failed to establish update connection to master");
}

void Slave::InitSyncConnection(FD* sync_fd, uint16_t* update_port) {
  if (!sync_addr_info_->Connect(*sync_fd))
    logger_->Fatal("Failed to establish sync connection to master");

  if (!SyncProtocol::ReadExactly(*sync_fd, &update_port, sizeof(*update_port)))
    logger_->Fatal("Failed to receive update port from master");
  *update_port = ntohl(*update_port);
}
