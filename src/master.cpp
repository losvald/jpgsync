#include "master.hpp"

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
  InitUpdateConnectionSocket(&update_sock_);
  update_port_ = BindAndListen(update_sock_, 0);
  InitSyncConnectionSocket(&sync_sock_);
  return BindAndListen(sync_sock_, sync_port_);
}

void Master::CreateConnections(FD* download_fd, FD* upload_fd) {}

void Master::InitUpdateConnection(int sync_fd, FD* update_fd) {
  if (update_sock_.closed())
    return ;

  // close update_sock_ on exit (even in case of exception)
  auto on_exit = [=] { update_sock_.Close(); };
  struct ScopeExit {
    ScopeExit(decltype(on_exit) f) : f(f) {}
    ~ScopeExit() { f(); }
    decltype(on_exit) f;
  } scope_exit(on_exit);

  InitUpdateConnectionSocket(update_fd);
  *update_fd = Accept(update_sock_);
}

void Master::InitDownloadConnection(FD* fd) {
  *fd = Accept(sync_sock_);
}

void Master::InitUploadConnection(FD* fd) {
  *fd = Accept(sync_sock_);

  // send the bound port for update
  uint16_t port = htonl(update_port_);
  sys_call(write, *fd, &port, sizeof(port));
}

void Master::Download(const ExifHash& hash, FD* fd) {
  // TODO
}
