#include "exif_hasher.hpp"
#include "util/fd.hpp"
#include "util/syscall.hpp"

#include <exiv2/exiv2.hpp>
#include <openssl/sha.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <thread>
#include <utility>

ExifHasher::Entry::Entry() : next(NULL) {}
ExifHasher::Entry::Entry(const ExifHash* hash, const std::string& path)
    : next(NULL),
      hash(hash),
      path(path) {}

ExifHasher::ExifHasher()
    : tail_(&dummy_entry_),
      new_entry_(tail_),
      new_entry_count_(0),
      done_(false) {}

ExifHasher::~ExifHasher() {
  auto cur = dummy_entry_.next;
  for (decltype(cur) next; cur != NULL; cur = next) {
    next = cur->next;
    delete cur;
  }
}

bool ExifHasher::HashExif(const std::string& path,
                          unsigned char* sha1_hash) const {
  FD fd;
  sys_call_rv(fd, open, path.c_str(), O_RDONLY);
  struct stat stat_buf;
  sys_call(fstat, fd, &stat_buf);
  void* memblock;
  sys_call2_rv(NULL, memblock, mmap, NULL, stat_buf.st_size, PROT_READ,
               MAP_PRIVATE, fd, 0);
  auto bytes = static_cast<const unsigned char*>(memblock);

  Exiv2::Image::AutoPtr image;
  try {
    image = Exiv2::ImageFactory::open(bytes, stat_buf.st_size);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return false;
  }

  if (image.get() != 0)
    image->readMetadata();
  else
    std::cerr << "Warning: EXIF not found in: " << path << std::endl;

  sys_call(munmap, memblock, stat_buf.st_size);
  fd.Close();

  if (image.get() == 0)
    return false;

  const auto& exifData = image->exifData();
  if (!exifData.empty()) {
    std::ostringstream oss;
    auto end = exifData.end();
    for (auto i = exifData.begin(); i != end; ++i) {
      // std::cerr << i->key() << " (" << i->count() << ")" << ": " <<
      //     i->value() << std::endl;
      oss << i->key() << i->value();
    }
    const std::string& exif_str = oss.str();
    SHA1(reinterpret_cast<const unsigned char*>(exif_str.c_str()),
         exif_str.size(), sha1_hash);
    return true;
  } else {
    std::cerr << "Warning: EXIF not found in: " << path << std::endl;
  }

  return false;
}

void ExifHasher::Run(size_t progress_threshold,
                     std::function<const char*(void)> path_gen,
                     bool unique) {
  std::thread thr([this, path_gen, progress_threshold, unique] {
      unsigned char hash_buf[SHA_DIGEST_LENGTH];
      std::string path;
      DEBUG_OUT_LN(RUN, "BEGIN");
      while (!path.replace(0, std::string::npos, path_gen()).empty()) {
        DEBUG_OUT_LN(RUN, "path=%s | PROCESSING", path.c_str());
        if (!HashExif(path, hash_buf))
          continue;

        ExifHash h(hash_buf);
        if (unique && hashes_.count(h)) {
          std::cerr << "Exif hash conflict in: " << path << std::endl;
          // TODO
          continue;
        }

        tail_ = (tail_->next = new Entry(&*hashes_.insert(h).first, path));
        if (hashes_.size() % progress_threshold == 0) {
          std::unique_lock<decltype(mutex_)> locker(mutex_);
          new_entry_count_ += progress_threshold;
          DEBUG_OUT_LN(RUN, "NOTIFY(%s)", DEBUG_STR(new_entry_count_));
          new_entries_.notify_one();
        }
        DEBUG_OUT_LN(RUN, "hash=%s; path=%s", DEBUG_STR(*tail_->hash),
                     path.c_str());
      }

      {
        std::unique_lock<decltype(mutex_)> locker(mutex_);
        new_entry_count_ += hashes_.size() % progress_threshold;
        done_ = true;
        DEBUG_OUT_LN(RUN, "DONE");
        DEBUG_OUT_LN(RUN, "NOTIFY(%s)", DEBUG_STR(new_entry_count_));
        new_entries_.notify_one();
      }
    });
  thr.detach();
}

const ExifHasher::Entry* ExifHasher::Get(size_t* count) {
  {
    std::unique_lock<decltype(mutex_)> locker(mutex_);
    DEBUG_OUT_LN(GET, "WAIT BEGIN(%s)", DEBUG_STR(*count));
    new_entries_.wait(locker, [this, &count] {
        return new_entry_count_ >= *count || done_;
      });
    *count = std::min(*count, new_entry_count_);
    DEBUG_OUT_LN(GET, "WAIT END(%s / %s)", DEBUG_STR(*count),
                 DEBUG_STR(new_entry_count_));
    new_entry_count_ -= *count;
  }

  auto begin = new_entry_->next;
  for (size_t i = 0; i < *count; ++i)
    new_entry_ = new_entry_->next;
  return begin;
}

bool ExifHasher::Contains(const ExifHash& hash) const {
  return hashes_.count(hash);
}

const ExifHasher::Entry* ExifHasher::before_first_entry() const {
  return &dummy_entry_;
}

size_t ExifHasher::entry_count() const { return hashes_.size(); }
