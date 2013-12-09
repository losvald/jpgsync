#ifndef PROTOCOL_HPP_
#define PROTOCOL_HPP_

#include "exif_hash.hpp"
#include "util/syscall.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SYNC_PROTO SOCK_STREAM

#ifndef RELIABLE_UPDATE
#define UPDATE_PROTO SOCK_DCCP
#else
#define UPDATE_PROTO SYNC_PROTO
#endif

#define UpdateProtocol Protocol<UPDATE_PROTO>
#define SyncProtocol Protocol<SYNC_PROTO>

template<int type>
struct Protocol {
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

  static inline bool ReadFileSize(int fd, size_t* file_size) {
    uint32_t buf;
    bool ret = ReadExactly(fd, &buf, sizeof(buf));
    *file_size = static_cast<size_t>(ntohl(buf));
    return ret;
  }

  static inline bool WriteFileSize(int fd, size_t file_size) {
    uint32_t buf = htonl(static_cast<uint32_t>(file_size));
    return WriteExactly(fd, &buf, sizeof(buf));
  }

  static int protocol;
  static size_t hashes_per_packet;

 private:
  Protocol() {}
};

template<> inline size_t Protocol<IPPROTO_DCCP>::ReadFully(
    int fd, void* bytes, size_t count) {
  ssize_t ret;
  sys_call_rv(ret, read, fd, bytes, count);
  return ret;
}

template<> inline size_t Protocol<IPPROTO_DCCP>::WriteFully(
    int fd, const void* bytes, size_t count) {
  ssize_t ret;
  sys_call_rv(ret, write, fd, bytes, count);
  return ret;
}

template<>
template<>
inline bool Protocol<SOCK_STREAM>::ReadByte<unsigned char>(
    int fd, unsigned char* byte) {
  ssize_t read_count;
  sys_call_rv(read_count, read, fd, byte, 1);
  return read_count;
}


extern template int Protocol<SOCK_DCCP>::protocol;
extern template size_t Protocol<SOCK_DCCP>::hashes_per_packet;

extern template int Protocol<SOCK_STREAM>::protocol;
extern template size_t Protocol<SOCK_STREAM>::hashes_per_packet;

#endif // PROTOCOL_HPP_
