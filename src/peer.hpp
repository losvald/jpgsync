#ifndef PEER_HPP_
#define PEER_HPP_

#include <cstdint>

#include <functional>
#include <iosfwd>
#include <string>

class ExifHash;
class Logger;

class Peer {
 public:
  typedef std::function<const char*(void)> PathGenerator;

  Peer(Logger* logger);
  virtual ~Peer();
  void Sync(PathGenerator path_gen, const std::string& download_dir);

 protected:
  virtual void InitUpdateConnection(int* update_fd) = 0;
  virtual bool InitSyncConnection(int* sync_fd, bool download);
  void Download(int sync_fd, size_t file_size, std::ofstream* ofs);
  void Upload(int sync_fd, size_t file_size, std::ifstream* ifs);

  Logger* logger_;
};

#endif // PEER_HPP_
