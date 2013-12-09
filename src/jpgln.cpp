#include "exif_hasher.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <iostream>

#include <cerrno>
#include <unistd.h>

int main(int argc, char* argv[]) {
  using namespace std;

  ExifHasher exif_hasher;
  int arg_ind = 0;
  exif_hasher.Run(1, [&] { return ++arg_ind < argc ? argv[arg_ind] : ""; });

  while (true) {
    size_t count = 1;
    auto e = exif_hasher.Get(&count);
    if (!count)
      break;
    cout << "ln " << e->path << ' ' << *e->hash << endl;
    try {
      sys_call(link, e->path.c_str(), ToString(*e->hash).c_str());
    } catch (const SysCallException& ex) {
      if (ex.code() == EEXIST) {
        cerr << "Warning: link already exists " << e->path << endl;
      } else
        throw ex;
    }
  }

  return 0;
}
