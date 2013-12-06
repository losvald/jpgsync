AM_CXXFLAGS = -std=c++0x -Werror
bin_PROGRAMS = jpgsync

jpgsync_SOURCES = jpgsync.cpp peer.cpp master.cpp slave.cpp \
	exif_hash.cpp exif_hasher.cpp \
	connection_constants.cpp \
	util/dir.cpp util/fd.cpp util/logger.cpp util/syscall.cpp
jpgsync_LDADD = -lcrypto -lexiv2
jpgsync_LDFLAGS = -pthread
