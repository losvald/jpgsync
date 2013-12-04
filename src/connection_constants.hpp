#ifndef CONNECTION_CONSTANTS_HPP_
#define CONNECTION_CONSTANTS_HPP_

#include "exif_hash.hpp"
#include "util/syscall.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

const size_t kIPMss = 576 - 20;

} // namespace

template<int type>
struct ConnectionConstants {
  static inline int InitSocket(int fd) {
    sys_call_rv(fd, socket, AF_INET, type, protocol);
    return protocol;
  }

  static size_t ReadFully(int fd, void* buf, size_t count) {
    ssize_t read_count = 0;
    char* bytes = buf;
    do {
      ssize_t ret;
      sys_call_rv(ret, read, fd, bytes + read_count, count - read_count);
      if (ret == 0 || (read_count += ret) == count)
        return read_count;
    } while (true);
  }

  static size_t WriteFully(int fd, const void* buf, size_t count) {
      ssize_t write_count = 0;
      const char* bytes = buf;
      do {
        ssize_t ret;
        sys_call_rv(ret, write, fd, bytes + write_count, count - write_count);
        if (ret == 0 || (write_count += ret) == count)
          return write_count;
      } while (true);
  }

  static int protocol;
  static size_t hashes_per_packet;

 private:
  ConnectionConstants() {}
};

template<> inline size_t ConnectionConstants<IPPROTO_DCCP>::ReadFully(
    int fd, void* bytes, size_t count) {
  ssize_t ret;
  sys_call_rv(ret, read, fd, bytes, count);
  return ret;
}

template<> inline size_t ConnectionConstants<IPPROTO_DCCP>::WriteFully(
    int fd, const void* bytes, size_t count) {
  ssize_t ret;
  sys_call_rv(ret, write, fd, bytes, count);
  return ret;
}

template<> int ConnectionConstants<SOCK_DGRAM>::protocol = IPPROTO_DCCP;
template<> size_t ConnectionConstants<SOCK_DGRAM>::hashes_per_packet =
    (kIPMss - 16) / sizeof(ExifHash);

template<> int ConnectionConstants<SOCK_STREAM>::protocol = IPPROTO_TCP;
template<> size_t ConnectionConstants<SOCK_STREAM>::hashes_per_packet =
    (kIPMss - 20) / sizeof(ExifHash);

#endif // CONNECTION_CONSTANTS_HPP_
