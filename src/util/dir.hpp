#ifndef UTIL_DIR_HPP_
#define UTIL_DIR_HPP_

#include <sys/types.h>
#include <dirent.h>

#include <string>

class Dir {
 public:
  Dir(const std::string& path);
  ~Dir();

  const std::string& Next();
 private:
  std::string path_;
  size_t prefix_len_;
  DIR* dir_;
  dirent* entry_;
};

#endif // UTIL_DIR_HPP_
