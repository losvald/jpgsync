#ifndef FSTREAM_UTILS_HPP_
#define FSTREAM_UTILS_HPP_

#include <fstream>

template<class Stream>
struct FstreamCloseGuard {
  FstreamCloseGuard(Stream* s) : s_(s) {}
  ~FstreamCloseGuard() { s_->close(); }
 private:
  Stream* s_;
};

template<class Stream>
bool Reopen(const char* filename, std::ios_base::openmode mode,
            Stream* stream) {
  stream->close();
  stream->open(filename, mode);
  return stream->is_open();
}

inline bool ReopenEnd(const char* filename, std::ofstream* ofs) {
  return Reopen(filename, std::ios::binary | std::ios::app, ofs) &&
      !ofs->tellp();
}

inline bool ReopenEnd(const char* filename, std::ifstream* ifs) {
  return Reopen(filename, std::ios::binary | std::ios::ate, ifs) &&
      ifs->tellg();
}

#endif  // FSTREAM_UTILS_HPP_
