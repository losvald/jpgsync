#include <cstdio>

#include "../debug.hpp"
#include "file.hpp"
#include "syscall.hpp"

template<>
void File::Close() {
  DEBUG_OUT_LN(FD, "File@%p::Close(%d)", this, res_);
  sys_call(fclose, res_);
  closed_ = true;
}
