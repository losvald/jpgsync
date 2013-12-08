#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#define DEBUG_MSG_BEGIN "[ %-8s: " "thr=%-10s" DEBUG_SEP
#define DEBUG_MSG_END "]"
#define DEBUG_SEP " | "

#define DEBUG_OUT_LN(tag, format, ...)                                  \
  _DEBUG_OUT(#tag, "\n" DEBUG_MSG_BEGIN format DEBUG_MSG_END, ##__VA_ARGS__)
#define DEBUG_OUT_BEGIN(tag) _DEBUG_OUT(#tag, "\n" DEBUG_MSG_BEGIN)
#define DEBUG_OUT_END() DEBUG_OUT_FINISH("")

#ifdef DEBUG

#include "util/string_utils.hpp"

#include <cstdio>

#include <iomanip>
#include <sstream>
#include <thread>

#define DEBUG_STR(x) (ToString(x).c_str())
#define DEBUG_HEX_STR(bytes, count) (ToHexString(bytes, count).c_str())

#define _DEBUG_OUT(tag, format, ...)                              \
  fprintf(stderr, format, tag,                                    \
          DEBUG_STR(std::this_thread::get_id()),  ##__VA_ARGS__)

#define DEBUG_OUT_APPEND(key, format, ...)                      \
  fprintf(stderr, #key "=" format DEBUG_SEP, ##__VA_ARGS__)
#define DEBUG_OUT_FINISH(format, ...)                   \
  fprintf(stderr, format DEBUG_MSG_END, ##__VA_ARGS__)

inline std::string ToHexString(const void* bytes, size_t count) {
  std::ostringstream oss;
  for (size_t i = 0; i < count; ++i) {
    oss << "\\x" << std::hex << std::setfill('0') << std::setw(2);
    oss << (((char*)bytes)[i] & 0xFF);
  }
  return oss.str();
}

#else

#define DEBUG_STR(x) ""
#define DEBUG_HEX_STR(bytes, count) ""

#define _DEBUG_OUT(tag, format, ...)
#define DEBUG_OUT_APPEND(key, format, ...)
#define DEBUG_OUT_FINISH(format, ...)

#endif  // DEBUG

#endif  // DEBUG_HPP_
