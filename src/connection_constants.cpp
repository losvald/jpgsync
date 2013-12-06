#include "connection_constants.hpp"

namespace {

const size_t kIPMss = 576 - 20;

} // namespace

template<> int ConnectionConstants<SOCK_DCCP>::protocol = IPPROTO_DCCP;
template<> size_t ConnectionConstants<SOCK_DCCP>::hashes_per_packet =
    (kIPMss - 16) / sizeof(ExifHash);

template<> int ConnectionConstants<SOCK_STREAM>::protocol = IPPROTO_TCP;
template<> size_t ConnectionConstants<SOCK_STREAM>::hashes_per_packet =
    (kIPMss - 20) / sizeof(ExifHash);
