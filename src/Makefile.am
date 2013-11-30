AM_CXXFLAGS = -std=c++0x -Werror
bin_PROGRAMS = jpgsync

jpgsync_SOURCES = jpgsync.cpp util/fd.cpp util/syscall.cpp
jpgsync_LDADD = -lcrypto -lexiv2
jpgsync_LDFLAGS = -pthread
