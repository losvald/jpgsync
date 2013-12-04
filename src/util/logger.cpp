#include "logger.hpp"

#include <cstdlib>

#include <mutex>

Logger::Logger(const std::string& name, unsigned verbosity, std::ostream* os)
    : name_(name),
      verbosity_(verbosity),
      os_(os) {}

void Logger::Verbose(const std::string& msg, unsigned min_verbosity) {
  if (verbosity_ >= min_verbosity)
    Write("", msg);
}

void Logger::Warn(const std::string& msg) { Write("warning", msg); }

void Logger::Error(const std::string& msg) {
  Write("error", msg);
}

void Logger::Fatal(const std::string& msg) {
  Write("fatal", msg);
  std::terminate();
}

void Logger::Write(const std::string& tag, const std::string& msg) {
  static std::mutex mutex;
  std::lock_guard<decltype(mutex)> locker(mutex);
  if (!tag.empty())
    *os_ << name_ << ": " << tag << ": ";
  *os_ << msg << std::endl;
}

unsigned Logger::verbosity() const { return verbosity_; }
