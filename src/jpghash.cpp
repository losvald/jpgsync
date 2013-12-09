#include "exif_hasher.hpp"

#include <iostream>

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
    cout << *e->hash << ' ' << ' ' << e->path << endl;
  }

  return 0;
}
