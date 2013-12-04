#include "slave.hpp"

#include "util/logger.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <exception>
#include <thread>

namespace {

bool Connect(const std::string& host, uint16_t port, int protocol, FD* fd) {
  struct AddrInfo {
   private:
    struct addrinfo hints, *resolved_asi_begin_;

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

    void Resolve(const std::string& host, const std::string& port) {
      int rv;
      sys_call2n_rv(0, rv, getaddrinfo, host.c_str(), port.c_str(),
                    &hints, &resolved_asi_begin_);
    }

    bool Connect(FD* fd) const {
      SysCallException* exception = NULL;
      for(auto ai = resolved_asi_begin_; ai != NULL; ai = ai->ai_next) {
        sys_call_rv(*fd, socket, ai->ai_family, ai->ai_socktype,
                    ai->ai_protocol);
        DEBUG_OUT_LN(DL, "connect loop sock = %d", (int)*fd);

        try {
          sys_call(connect, *fd, ai->ai_addr, ai->ai_addrlen);
          return true;
        } catch (const SysCallException& e) {
          exception = new SysCallException(e);
          // std::cerr << e.what() << std::endl;
        }
      }

      if (exception != NULL) {
        SysCallException e(*exception);
        delete exception;
        throw e;
      } else
        delete exception;

      return false;
    }
  } ai(protocol);

  ai.Resolve(host, ToString(port));
  return ai.Connect(fd);
}

} // namespace

Slave::Slave(const std::string& master_host, uint16_t master_port,
             Logger* logger)
    : Peer(logger),
      master_host_(master_host),
      master_port_(master_port) {}

void Slave::InitDownloadConnection() {}
void Slave::InitUploadConnection() {}

void Slave::InitUpdateConnection(int sync_fd, FD* update_fd) {
  uint16_t port;
  sys_call(read, sync_fd, &port, sizeof(port));
  port = ntohl(port);
  if (!Connect(master_host_, port, IPPROTO_DCCP, update_fd)) {
    logger_->Error("Failed to establish update connection to master");
  }
}

void Slave::Sync(const std::string& root) {
  if (!Connect(master_host_, master_port_, IPPROTO_TCP,
               &download_fd_)) {
    logger_->Error("Failed to establish download connection to master");
  }

  if (!Connect(master_host_, master_port_, IPPROTO_TCP, &upload_fd_)) {
    logger_->Error("Failed to establish upload connection to master");
  }

  Peer::Sync(root);
}
