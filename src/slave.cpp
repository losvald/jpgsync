#include "slave.hpp"

#include "debug.hpp"
#include "protocol.hpp"
#include "util/fd.hpp"
#include "util/logger.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <exception>
#include <string>

class AddrInfo {
 public:
  AddrInfo(const std::string& host, int protocol)
      : host_(host),
        port_(0),
        resolved_asi_begin_(NULL) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_protocol = protocol;
  }
  ~AddrInfo() {
    if (resolved_asi_begin_ != NULL)
      freeaddrinfo(resolved_asi_begin_);
    resolved_asi_begin_ = NULL;
  }

  AddrInfo& Resolve(uint16_t port) {
    int rv;
    std::string port_str = ToString(port);
    sys_call2n_rv(0, rv, getaddrinfo, host_.c_str(), port_str.c_str(),
                  &hints, &resolved_asi_begin_);
    port_ = port;
    return *this;
  }

  bool Connect(FD* fd) const {
    SysCallException* exception = NULL;
    for(auto ai = resolved_asi_begin_; ai != NULL; ai = ai->ai_next) {
      sys_call_rv(*fd, socket, ai->ai_family, ai->ai_socktype,
                  ai->ai_protocol);
      DEBUG_OUT_LN(CONNECT, "fd = %d | TRYING", (int)*fd);

      try {
        sys_call(connect, *fd, ai->ai_addr, ai->ai_addrlen);
        DEBUG_OUT_LN(CONNECT, "fd = %d | SUCCEEDED", (int)*fd);
        return true;
      } catch (const SysCallException& e) {
        DEBUG_OUT_LN(CONNECT, "fd = %d | FAILED", (int)*fd);
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

  uint16_t port() const { return port_; }

 private:
  struct addrinfo hints, *resolved_asi_begin_;
  std::string host_;
  uint16_t port_;
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
  update_addr_info_ = new AddrInfo(master_host, UpdateProtocol::protocol);
  DEBUG_OUT_LN(ATTACH, "RESOLVING TCP");
  (sync_addr_info_ = new AddrInfo(master_host, SyncProtocol::protocol))->
      Resolve(master_port);
}

void Slave::InitUpdateConnection(uint16_t update_port, FD* update_fd) {
  logger_->Verbose("Connecting to master at update port: " +
                   ToString(update_addr_info_->port()));
  DEBUG_OUT_LN(INITUPD, "CONNECTING");
  if (!update_addr_info_->Connect(update_fd))
    logger_->Fatal("Failed to establish update connection to master");
  DEBUG_OUT_LN(INITUPD, "CONNECTED");
}

void Slave::InitSyncConnection(FD* sync_fd, uint16_t* update_port) {
  DEBUG_OUT_LN(INITSYNC, "CONNECTING");
  if (!sync_addr_info_->Connect(sync_fd))
    logger_->Fatal("Failed to establish sync connection to master");
  DEBUG_OUT_LN(INITSYNC, "CONNECTED");

  if (!SyncProtocol::ReadExactly(*sync_fd, update_port, sizeof(*update_port)))
    logger_->Fatal("Failed to receive update port from master");
  *update_port = ntohs(*update_port);

  DEBUG_OUT_LN(INITSYNC, "RESOLVING");
  update_addr_info_->Resolve(*update_port);
  DEBUG_OUT_LN(INITSYNC, "RESOLVED");
}
