#include "master.hpp"

#include "debug.hpp"
#include "protocol.hpp"
#include "util/logger.hpp"
#include "util/syscall.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>

namespace {

uint16_t BindAndListen(int sock, uint16_t port) {
  sockaddr_in my_address;
  socklen_t my_address_len = sizeof(my_address);
  memset(&my_address, 0, sizeof(my_address));
  my_address.sin_family = PF_INET;
  my_address.sin_port = port;
  my_address.sin_addr.s_addr = INADDR_ANY;
  sys_call(bind, sock, (sockaddr*)&my_address, my_address_len);
  sys_call(listen, sock, 2);
  if (port == 0) {
    sys_call(getsockname, sock, (sockaddr*)&my_address, &my_address_len);
    port = ntohs(my_address.sin_port);
  }
  return port;
}

inline int Accept(int sock) {
  int fd;
  sys_call_rv(fd, accept, sock, NULL, NULL);
  return fd;
}

} // namespace

Master::Master(Logger* logger) : Peer(logger), sync_port_(0) {}

void Master::set_port(uint16_t port) { sync_port_ = port; }

uint16_t Master::Listen() {
  update_sock_ = UpdateProtocol::InitSocket();
  update_port_ = BindAndListen(update_sock_, 0);
  logger_->Verbose("Listening for update on port " + ToString(update_port_));
  sync_sock_ = SyncProtocol::InitSocket();
  return BindAndListen(sync_sock_, sync_port_);
}

void Master::InitUpdateConnection(uint16_t update_port, FD* update_fd) {
  if (update_sock_.closed())
    logger_->Fatal("Cannot connect to multiple slaves");

  // close update_sock_ on exit (even in case of exception)
  auto on_exit = [=] { update_sock_.Close(); };
  struct ScopeExit {
    ScopeExit(decltype(on_exit) f) : f(f) {}
    ~ScopeExit() { f(); }
    decltype(on_exit) f;
  } scope_exit(on_exit);

  DEBUG_OUT_LN(INITUPD, "ACCEPTING UPDATE CONN");
  *update_fd = Accept(update_sock_);
  DEBUG_OUT_LN(INITUPD, "ACCEPTED UPDATE CONN");
}

void Master::InitSyncConnection(FD* sync_fd, uint16_t* update_port) {
  DEBUG_OUT_LN(INITSYNC, "ACCEPTING SYNC CONN");
  *sync_fd = Accept(sync_sock_);
  DEBUG_OUT_LN(INITSYNC, "ACCEPTED SYNC CONN");

  // send the bound update port
  uint16_t port = htons(update_port_);
  if (!SyncProtocol::WriteExactly(*sync_fd, &port, sizeof(port)))
    logger_->Fatal("Failed to send update port to slave");
}
