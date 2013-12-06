#ifndef CONNECTION_CONSTANTS_HPP_
#define CONNECTION_CONSTANTS_HPP_

#include "exif_hash.hpp"
#include "util/syscall.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

template<int type>
struct ConnectionConstants {
  static inline int InitSocket() {
    int fd;
    sys_call_rv(fd, socket, AF_INET, type, protocol);
    return fd;
  }

  static size_t ReadFully(int fd, void* buf, size_t count) {
    ssize_t read_count = 0;
    auto bytes = static_cast<char*>(buf);
    do {
      ssize_t ret;
      sys_call_rv(ret, read, fd, bytes + read_count, count - read_count);
      if (ret == 0 || (read_count += ret) == count)
        return read_count;
    } while (true);
  }

  static size_t WriteFully(int fd, const void* buf, size_t count) {
      ssize_t write_count = 0;
      auto bytes = static_cast<const char*>(buf);
      do {
        ssize_t ret;
        sys_call_rv(ret, write, fd, bytes + write_count, count - write_count);
        if (ret == 0 || (write_count += ret) == count)
          return write_count;
      } while (true);
  }

  static inline bool ReadExactly(int fd, void* buf, size_t count) {
    return ReadFully(fd, buf, count) == count;
  }

  static inline bool WriteExactly(int fd, const void* buf, size_t count) {
    return WriteFully(fd, buf, count) == count;
  }

  template<typename T>
  static bool ReadByte(int fd, T* integral) {
    unsigned char byte;
    bool ret = ReadByte<unsigned char>(fd, &byte);
    *integral = byte;
    return ret;
  }

  template<typename T>
  static inline bool WriteByte(int fd, T integral) {
    unsigned char byte = integral;
    ssize_t write_count;
    sys_call_rv(write_count, write, fd, &byte, 1);
    return write_count;
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

template<>
template<>
inline bool ConnectionConstants<SOCK_STREAM>::ReadByte<unsigned char>(
    int fd, unsigned char* byte) {
  ssize_t read_count;
  sys_call_rv(read_count, read, fd, byte, 1);
  return read_count;
}


extern template int ConnectionConstants<SOCK_DCCP>::protocol;
extern template size_t ConnectionConstants<SOCK_DCCP>::hashes_per_packet;

extern template int ConnectionConstants<SOCK_STREAM>::protocol;
extern template size_t ConnectionConstants<SOCK_STREAM>::hashes_per_packet;

#endif // CONNECTION_CONSTANTS_HPP_
