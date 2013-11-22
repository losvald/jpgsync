#include "../src/exif_hasher.hpp"

#include <cstring>
#include <fstream>

int main(int argc, char* argv[]) {
  using namespace std;
  istream* is;

  const char* input_file = argv[1];
  is = (strcmp(input_file, "-") == 0 ? &cin : new fstream(input_file));

  ExifHasher exif_hasher;
  exif_hasher.Run(1, is);

  size_t count;
  do {
    count = 2;
    auto e = exif_hasher.Get(&count);
    cerr << "main: got " << count << endl;
    for (auto i = count; i--; e = e->next)
      cerr << "main: " << *e->hash << endl;
  } while (count);

  if (is != &cin)
    delete is;

  return 0;
}
