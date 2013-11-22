#ifndef EXIF_HASHER_HPP_
#define EXIF_HASHER_HPP_

#include "exif_hash.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

class ExifHasher {
 public:
  struct Entry {
    Entry* next;
    const ExifHash* hash;
    std::string path;

    Entry();
    Entry(const ExifHash* hash, const std::string& path);
  };

  ExifHasher();
  virtual ~ExifHasher();

  void Run(size_t progress_threshold, std::istream* input);
  const Entry* Get(size_t* count);

 protected:
  virtual bool HashExif(const std::string& path,
                        unsigned char* sha1_hash) const;

 private:
  Entry dummy_entry_;
  Entry* tail_;
  const Entry* new_entry_;

  std::mutex mutex_;
  std::condition_variable new_entries_;
  size_t new_entry_count_;
  bool done_;
};

#endif // EXIF_HASHER_HPP_
