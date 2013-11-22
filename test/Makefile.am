AM_CXXFLAGS = -std=c++0x -Wall
bin_PROGRAMS = unittest_all \
	exifprint exifprint-mmap sha1print exif_hasher_test

exifprint_SOURCES = exifprint.cpp
exifprint_LDADD = -lexiv2
exifprint_mmap_SOURCES = exifprint_mmap.cpp
exifprint_mmap_LDADD = -lexiv2 #-lcrypto

sha1print_SOURCES = sha1print.cpp
sha1print_LDADD = -lcrypto

exif_hasher_test_SOURCES = exif_hasher_test.cpp ../src/exif_hasher.cpp \
	../src/exif_hash.cpp ../src/util/fd.cpp ../src/util/syscall.cpp
exif_hasher_test_LDADD = -lcrypto -lexiv2
exif_hasher_test_LDFLAGS = -pthread

unittest_all_SOURCES = test.cpp \
	exif_hash_unittest.cpp ../src/exif_hash.cpp \
	../src/util/fd.cpp ../src/util/syscall.cpp
unittest_all_LDADD = -lgtest -lcrypto -lexiv2
unittest_all_LDFLAGS = -pthread
