#include "../src/util/dir.hpp"

#include <iostream>

using namespace std;

int main(int argc, char** argv) {
  Dir dir(argv[1]);
  while (true) {
    const auto& path = dir.Next();
    if (path.empty())
      break;
    cout << path << endl;
  }
  return 0;
}
