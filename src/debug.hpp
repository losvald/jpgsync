#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <cstdio>
#include <pthread.h>

#define DEBUG_MSG_BEGIN "[ %-8s: " "thr=%-10lu" DEBUG_SEP
#define DEBUG_MSG_END "]"
#define DEBUG_SEP " | "

#define DEBUG_OUT_LN(tag, format, ...)                                  \
  _DEBUG_OUT(#tag, "\n" DEBUG_MSG_BEGIN format DEBUG_MSG_END, ##__VA_ARGS__)
#define DEBUG_OUT_BEGIN(tag) _DEBUG_OUT(#tag, "\n" DEBUG_MSG_BEGIN)


#define DEBUG_OUT_END() DEBUG_OUT_FINISH("")

#ifdef DEBUG

#define _DEBUG_OUT(tag, format, ...)                              \
  fprintf(stderr, format, tag,                                    \
          (unsigned long)pthread_self(),  ##__VA_ARGS__)

#define DEBUG_OUT_APPEND(key, format, ...) \
  fprintf(stderr, #key "=" format DEBUG_SEP, ##__VA_ARGS__)
#define DEBUG_OUT_FINISH(format, ...)                   \
  fprintf(stderr, format DEBUG_MSG_END, ##__VA_ARGS__)

#else

#define _DEBUG_OUT(tag, format, ...)
#define DEBUG_OUT_APPEND(key, format, ...)
#define DEBUG_OUT_FINISH(format, ...)

#endif  // DEBUG

#endif  // DEBUG_HPP_
