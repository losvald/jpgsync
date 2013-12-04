#ifndef UTIL_LOGGER_HPP_
#define UTIL_LOGGER_HPP_

#include <iostream>
#include <ostream>
#include <string>

class Logger {
 public:
  Logger(const std::string& name, unsigned verbosity,
         std::ostream* os = &std::cerr);
  void Verbose(const std::string& msg, unsigned min_verbosity = 1);
  void Warn(const std::string& msg);
  void Warn();
  void Error(const std::string& msg);
  void Fatal(const std::string& msg);

  unsigned verbosity() const;
 protected:
  void Write(const std::string& tag, const std::string& msg);
 private:
  std::string name_;
  unsigned verbosity_;
  std::ostream* os_;
};

#endif  // UTIL_LOGGER_HPP_
