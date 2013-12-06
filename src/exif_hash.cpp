#include "exif_hash.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>

namespace {

inline uint32_t BytesToWord32(const unsigned char* bytes) {
  return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

inline void ToNetworkOrderByte(uint32_t word, void* byte) {
  word = htonl(word);
  memcpy(byte, &word, sizeof(uint32_t));
}

} // namespace

ExifHash::ExifHash() {}

ExifHash::ExifHash(const unsigned char* sha1_digest)
    : word0(BytesToWord32(sha1_digest + 0)),
      word1(BytesToWord32(sha1_digest + 4)),
      word2(BytesToWord32(sha1_digest + 8)),
      word3(BytesToWord32(sha1_digest + 12)),
      word4(BytesToWord32(sha1_digest + 16)) {}

std::ostream& operator<<(std::ostream& os, const ExifHash& eh) {
  os << std::hex << std::setfill('0') <<
      std::setw(8) << eh.word0 <<
      std::setw(8) << eh.word1 <<
      std::setw(8) << eh.word2 <<
      std::setw(8) << eh.word3 <<
      std::setw(8) << eh.word4;
  return os;
}

void ExifHash::ToDigest(void* bytes) const {
  auto sha1_digest = static_cast<unsigned char*>(bytes);
  ToNetworkOrderByte(word0, sha1_digest + 0);
  ToNetworkOrderByte(word1, sha1_digest + 4);
  ToNetworkOrderByte(word2, sha1_digest + 8);
  ToNetworkOrderByte(word3, sha1_digest + 12);
  ToNetworkOrderByte(word4, sha1_digest + 16);
}

namespace std {

size_t hash<ExifHash>::operator()(const ExifHash& ef) const {
  const size_t prime = 31;
  return ((((ef.word0 * prime) + ef.word1) * prime + ef.word2) * prime +
          ef.word3) * prime + ef.word4;
}

} // namespace std
