#include "../src/util/fstream_utils.hpp"

#include <cassert>
#include <iostream>

using namespace std;

int main() {
  auto filename = "/tmp/fstream_test.out";
  auto on_exit = [=] { remove(filename); };
  struct ScopeExit {
    ScopeExit(decltype(on_exit) f) : f(f) {}
    ~ScopeExit() { f(); }
    decltype(on_exit) f;
  } scope_exit(on_exit);

  ofstream ofs;
  assert(!ofs.is_open());
  ofs << "hello world" << endl;

  assert(ReopenEnd(filename, &ofs));
  assert(ofs.good());
  cout << !ofs.tellp() << endl;
  ofs << "this works" << endl;

  ifstream ifs;
  {
    FstreamCloseGuard<decltype(ifs)> close_guard(&ifs);
    assert(ReopenEnd(filename, &ifs));
    ifs.seekg(0, ifs.end);
    cout << "size: " << ifs.tellg() << "(" << (size_t)ifs.tellg() << endl;
    ifs.seekg(0, ifs.beg);
    char c; ifs >> c;
    cout << c << endl;
    assert(ifs.is_open());
  }
  assert(!ifs.is_open());

  return 0;
}
