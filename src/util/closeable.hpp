#ifndef UTIL_CLOSEABLE_HPP_
#define UTIL_CLOSEABLE_HPP_

#include "../debug.hpp"

#ifdef DEBUG
#include "string_utils.hpp"
#endif

template<typename T>
class Closeable {
 protected:
  T res_;
  bool closed_;
 public:
  Closeable() : res_(), closed_(true) {}
  Closeable(const T& res) : res_(res), closed_(false) {}
  ~Closeable() {
    if (!closed())
      Close();
  }
  Closeable& operator=(const T& res) {
#ifdef DEBUG
    DEBUG_OUT_LN(FD, "Closeable@%p::operator=(const T& %s)", this,
                 ToString(res).c_str());
#endif
    if (!closed())
      Close();
    res_ = res;
    closed_ = false;
    return *this;
  }

  operator const T&() const { return res_; }

  void Close();
  bool closed() const { return closed_; }
};

#endif // UTIL_CLOSEABLE_HPP_
