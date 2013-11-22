#include <cerrno>
#include <cstring>

#include <sstream>

#include "syscall.hpp"

SysCallException::SysCallException(
    const char* source_file,
    int source_line,
    const std::string& call_str)
    : code_(errno),
      source_file_(source_file),
      source_line_(source_line) {
  std::ostringstream oss;
  oss << source_file << ":" << source_line << ": " << call_str << ": ";
  oss << strerror(code_);
  msg_ = oss.str();
}

SysCallException::~SysCallException() throw() {}
const char* SysCallException::what() const throw() { return msg_.c_str(); }
int SysCallException::code() const { return code_; }
