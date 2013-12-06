#ifndef EXIF_HASH_HPP_
#define EXIF_HASH_HPP_

#include <cstdint>

#include <iomanip>
#include <iosfwd>
#include <utility>

struct ExifHash {
 public:
  ExifHash();
  ExifHash(const unsigned char* sha1_digest);

  inline friend bool operator==(const ExifHash& lhs, const ExifHash& rhs) {
    return lhs.word0 == rhs.word0 && lhs.word1 == rhs.word1 &&
        lhs.word2 == rhs.word2 && lhs.word3 == rhs.word3 &&
        lhs.word4 == rhs.word4;
  }

  inline friend bool operator!=(const ExifHash& lhs, const ExifHash& rhs) {
    return !(lhs == rhs);
  }

  friend std::ostream& operator<<(std::ostream& os, const ExifHash& eh);

  void ToDigest(void* bytes) const;

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
  size_t operator()(const ExifHash& ef) const;
};

} // namespace std

#endif /* EXIF_HASH_HPP_ */
