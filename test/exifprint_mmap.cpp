// ***************************************************************** -*- C++ -*-
// exifprint.cpp, $Rev: 2286 $
// Sample program to print the Exif metadata of an image

#include <exiv2/exiv2.hpp>

#include <iostream>
#include <iomanip>
#include <cassert>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// #include <openssl/sha.h>

int main(int argc, char* const argv[])
try {

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " file\n";
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    assert(fd != -1);
    struct stat stat_buf;
    assert(fstat(fd, &stat_buf) != -1);
    void* memblock = mmap(NULL, stat_buf.st_size, PROT_READ,
                          MAP_PRIVATE, // | MAP_POPULATE,
                          fd, 0);
    assert(memblock != MAP_FAILED);
    auto bytes = static_cast<const unsigned char*>(memblock);

    // unsigned chksum = 0;
    // for (auto i = 0; i < stat_buf.st_size; ++i)
    //   chksum += bytes[i];
    // fprintf(stderr, "%u\n", chksum);

    // SHA1(bytes, stat_buf.st_size, hash);
    // for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
    //   printf("%02x", hash[i]);
    // printf("\n");

    // unsigned char hash[SHA_DIGEST_LENGTH];
    // auto chunk_size = 548;
    // for (auto i = chunk_size; i < stat_buf.st_size; i += chunk_size) {
    //   SHA1(bytes + i, chunk_size, hash);
    // }

    Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(
        bytes, stat_buf.st_size);
    assert(image.get() != 0);
    image->readMetadata();

    munmap(memblock, stat_buf.st_size);
    assert(close(fd) != -1);

    Exiv2::ExifData &exifData = image->exifData();
    if (exifData.empty()) {
        std::string error(argv[1]);
        error += ": No Exif data found in the file";
        throw Exiv2::Error(1, error);
    }
    Exiv2::ExifData::const_iterator end = exifData.end();
    for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i) {
        const char* tn = i->typeName();
        std::cout << std::setw(44) << std::setfill(' ') << std::left
                  << i->key() << " "
                  << "0x" << std::setw(4) << std::setfill('0') << std::right
                  << std::hex << i->tag() << " "
                  << std::setw(9) << std::setfill(' ') << std::left
                  << (tn ? tn : "Unknown") << " "
                  << std::dec << std::setw(3)
                  << std::setfill(' ') << std::right
                  << i->count() << "  "
                  << std::dec << i->value()
                  << "\n";
    }

    return 0;
}
//catch (std::exception& e) {
//catch (Exiv2::AnyError& e) {
catch (Exiv2::Error& e) {
    std::cout << "Caught Exiv2 exception '" << e.what() << "'\n";
    return -1;
}
