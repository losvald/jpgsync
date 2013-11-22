#include <unistd.h>

#include "../debug.hpp"
#include "fd.hpp"
#include "syscall.hpp"

template<>
void FD::Close() {
  DEBUG_OUT_LN(FD, "FD@%p::Close(%d)", this, res_);
  sys_call(close, res_);
  closed_ = true;
}
