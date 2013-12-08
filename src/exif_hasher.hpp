#ifndef EXIF_HASHER_HPP_
#define EXIF_HASHER_HPP_

#include "exif_hash.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_set>

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

  void Run(size_t progress_threshold,
           std::function<const char*(void)> path_gen);
  const Entry* Get(size_t* count);
  bool Contains(const ExifHash& hash) const;

  const Entry* before_first_entry() const;

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

  std::unordered_set<ExifHash> hashes_;
};

#endif // EXIF_HASHER_HPP_
