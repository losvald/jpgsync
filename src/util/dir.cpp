#include "dir.hpp"

#include "syscall.hpp"

#include <cerrno>

Dir::Dir(const std::string& path)
    : path_(path + '/'),
      prefix_len_(path_.length()) {
  sys_call2_rv(NULL, dir_, opendir, path.c_str());
}

Dir::~Dir() {
  sys_call(closedir, dir_);
}

const std::string& Dir::Next() {
  do {
    int saved_errno = (errno = 0);
    entry_ = readdir(dir_);
    if (errno != saved_errno)
      throw SysCallException(__FILE__, __LINE__, "readdir");
    if (entry_ == NULL)
      break;

    // // skip current and parent directory
    // const char* c = entry_->d_name;
    // if (*c == '.' && (*++c == 0 || (*c == '.' && *++c == 0)))
    //   continue;

    // skip directories
    if (entry_->d_type == DT_DIR)
      continue;

    return path_.replace(prefix_len_, std::string::npos, entry_->d_name);
  } while (true);
  return path_.erase(0);
}
