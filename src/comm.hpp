#ifndef COMM_HPP_
#define COMM_HPP_

#include <arpa/inet.h>

#include <iomanip>
#include <iostream>

#include <openssl/sha.h>

namespace {

uint32_t BytesToWord32(const unsigned char* bytes) {
  return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

inline std::ostream& PrintWord32(uint32_t word, std::ostream& os) {
  os << std::hex << std::setw(8) << word;
  return os;
}

} // namespace

struct ExifHash {
 public:
  ExifHash() {}

  ExifHash(const unsigned char* sha1_digest)
      : word0(BytesToWord32(sha1_digest + 0)),
        word1(BytesToWord32(sha1_digest + 4)),
        word2(BytesToWord32(sha1_digest + 8)),
        word3(BytesToWord32(sha1_digest + 12)),
        word4(BytesToWord32(sha1_digest + 16)) {}

  inline friend bool operator==(const ExifHash& lhs, const ExifHash& rhs) {
    return lhs.word0 == rhs.word0 && lhs.word1 == rhs.word1 &&
        lhs.word2 == rhs.word2 && lhs.word3 == rhs.word3 &&
        lhs.word4 == rhs.word4;
  }

  inline friend bool operator!=(const ExifHash& lhs, const ExifHash& rhs) {
    return !(lhs == rhs);
  }

  friend std::ostream& operator<<(std::ostream& os, const ExifHash& eh) {
    os << std::hex << std::setfill('0') <<
        std::setw(8) << eh.word0 <<
        std::setw(8) << eh.word1 <<
        std::setw(8) << eh.word2 <<
        std::setw(8) << eh.word3 <<
        std::setw(8) << eh.word4;
    return os;
  }

 private:
  uint32_t word0;
  uint32_t word1;
  uint32_t word2;
  uint32_t word3;
  uint32_t word4;

  friend class std::hash<ExifHash>;
};

namespace std {

template<>
struct hash<ExifHash> {
  size_t operator()(const ExifHash& ef) const {
    const size_t prime = 31;
    return (((((ef.word0 * prime) + ef.word1) * prime + ef.word2) * prime +
             ef.word3) * prime + ef.word4) * prime;
  }
};

} // namespace std

#endif /* COMM_HPP_ */
