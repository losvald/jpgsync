#ifndef UTIL_SYS_CALL_HPP_
#define UTIL_SYS_CALL_HPP_

#include <exception>
#include <string>

#define sys_call(function, ...)                 \
  sys_call2(-1, function, ##__VA_ARGS__)

#define sys_call2(error_rv, function, ...)                              \
  do {                                                                  \
    if (function(__VA_ARGS__) == error_rv)                              \
      _throw_sys_call_exception(error_rv, function, ##__VA_ARGS__);     \
  } while (0)

#define sys_call_rv(rv, function, ...)          \
  sys_call2_rv(-1, rv, function, ##__VA_ARGS__)

#define sys_call2_rv(error_rv, rv, function, ...)                       \
  do {                                                                  \
    if ((rv = function(__VA_ARGS__)) == error_rv)                       \
      _throw_sys_call_exception(error_rv, function, ##__VA_ARGS__);     \
  } while (0)

#define sys_call2n_rv(error_rv, rv, function, ...)                      \
  do {                                                                  \
    if ((rv = function(__VA_ARGS__)) != error_rv)                       \
      _throw_sys_call_exception(rv, function, ##__VA_ARGS__);           \
  } while (0)

#define _throw_sys_call_exception(error_rv, function, ...)              \
  throw SysCallException(                                               \
      __FILE__, __LINE__,                                               \
      #function "(" #__VA_ARGS__ ") == " # error_rv)                    \

class SysCallException : public std::exception {
 public:
  explicit SysCallException(const char* source_file, int source_line,
                            const std::string& call_str);
  virtual ~SysCallException() throw();
  const char* what() const throw();
  int code() const;

 private:
  int code_;
  std::string source_file_;
  int source_line_;
  std::string msg_;
};

#endif // UTIL_SYS_CALL_HPP_
