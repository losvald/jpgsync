AM_CXXFLAGS = -std=c++0x -Wall
bin_PROGRAMS = exifprint exifprint-mmap sha1print unittest_all

exifprint_SOURCES = exifprint.cpp
exifprint_LDADD = -lexiv2
exifprint_mmap_SOURCES = exifprint_mmap.cpp
exifprint_mmap_LDADD = -lexiv2 #-lcrypto

sha1print_SOURCES = sha1print.cpp
sha1print_LDADD = -lcrypto

unittest_all_SOURCES = test.cpp \
	exif_hash_unittest.cpp ../src/exif_hash.cpp \
	../src/util/fd.cpp ../src/util/syscall.cpp
unittest_all_LDADD = -lgtest -lcrypto -lexiv2
unittest_all_LDFLAGS = -pthread
