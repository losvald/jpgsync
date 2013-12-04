#ifndef CONNECTION_CONSTANTS_HPP_
#define CONNECTION_CONSTANTS_HPP_

#include "exif_hash.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace {

const size_t kIPMss = 576 - 20;

} // namespace

template<int protocol>
struct ConnectionConstants {
  static inline int InitSocket(int fd) {
    // sys_call_rv(fd, socket, AF_INET, type, protocol);
    return protocol;
  }

  static int type;
  static size_t hashes_per_packet;

 private:
  ConnectionConstants() {}
};

template<> int ConnectionConstants<IPPROTO_DCCP>::type = SOCK_DGRAM;
template<> size_t ConnectionConstants<IPPROTO_DCCP>::hashes_per_packet =
    (kIPMss - 16) / sizeof(ExifHash);

template<> int ConnectionConstants<IPPROTO_TCP>::type = SOCK_STREAM;
template<> size_t ConnectionConstants<IPPROTO_TCP>::hashes_per_packet =
    (kIPMss - 20) / sizeof(ExifHash);

#endif  // CONNECTION_CONSTANTS_HPP_
