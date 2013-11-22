#include "exif_hash.hpp"

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

namespace std {

size_t hash<ExifHash>::operator()(const ExifHash& ef) const {
  const size_t prime = 31;
  return ((((ef.word0 * prime) + ef.word1) * prime + ef.word2) * prime +
          ef.word3) * prime + ef.word4;
}

} // namespace std
