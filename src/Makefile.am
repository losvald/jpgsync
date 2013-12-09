AM_CXXFLAGS = -std=c++0x -Werror
bin_PROGRAMS = jpghash jpgln jpgsync

jpghash_SOURCES = jpghash.cpp exif_hash.cpp exif_hasher.cpp \
	util/fd.cpp util/syscall.cpp
jpghash_LDADD = -lcrypto -lexiv2
jpghash_LDFLAGS = -pthread

jpgln_SOURCES = jpgln.cpp exif_hash.cpp exif_hasher.cpp \
	util/fd.cpp util/syscall.cpp
jpgln_LDADD = -lcrypto -lexiv2
jpgln_LDFLAGS = -pthread

jpgsync_SOURCES = jpgsync.cpp peer.cpp master.cpp slave.cpp \
	exif_hash.cpp exif_hasher.cpp protocol.cpp \
	util/dir.cpp util/fd.cpp util/logger.cpp util/syscall.cpp
jpgsync_LDADD = -lcrypto -lexiv2
jpgsync_LDFLAGS = -pthread
