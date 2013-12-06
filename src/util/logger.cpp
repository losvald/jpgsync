#include "logger.hpp"

#include <cstdlib>

#include <mutex>

namespace {

const std::string kFatalTag = "fatal";
const std::string kErrorTag = "error";

} // namespace

Logger::Logger(const std::string& name, unsigned verbosity, std::ostream* os)
    : name_(name),
      verbosity_(verbosity),
      os_(os),
      exit_status_(0) {}

void Logger::Verbose(const std::string& msg, unsigned min_verbosity) {
  if (verbosity_ >= min_verbosity)
    Write("", msg);
}

void Logger::Warn(const std::string& msg) { Write("warning", msg); }

void Logger::Error(const std::string& msg) {
  Write(kErrorTag, msg);
}

void Logger::Fatal(const std::string& msg) {
  Write(kFatalTag, msg);
  std::terminate();
}

void Logger::Write(const std::string& tag, const std::string& msg) {
  static std::mutex mutex;
  std::lock_guard<decltype(mutex)> locker(mutex);
  if (!tag.empty()) {
    if (tag == kFatalTag)
      exit_status_ |= 1;
    else if (tag == kErrorTag)
      exit_status_ |= 2;

    *os_ << name_ << ": " << tag << ": ";
  }
  *os_ << msg << std::endl;
}

unsigned Logger::verbosity() const { return verbosity_; }
int Logger::exit_status() const { return exit_status_; }
