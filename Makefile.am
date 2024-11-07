ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -Isrc/blake2

include_HEADERS = src/librsync.h
lib_LTLIBRARIES = librsync.la

librsync_la_SOURCES = \
    src/base64.c \
    src/buf.c src/buf.h \
    src/checksum.c src/checksum.h \
    src/command.c src/command.h \
    src/delta.c \
    src/emit.c src/emit.h \
    src/fileutil.c \
    src/hashtable.c src/hashtable.h \
    src/hex.c \
    src/job.c src/job.h \
    src/mdfour.c src/mdfour.h \
    src/mksum.c \
    src/msg.c \
    src/netint.c src/netint.h \
    src/patch.c \
    src/prototab.c src/prototab.h \
    src/rabinkarp.c src/rabinkarp.h \
    src/readsums.c \
    src/rollsum.c src/rollsum.h \
    src/scoop.c src/scoop.h \
    src/stats.c \
    src/sumset.c src/sumset.h \
    src/trace.c src/trace.h \
    src/tube.c \
    src/util.c src/util.h \
    src/version.c \
    src/whole.c src/whole.h \
    src/blake2/blake2b-ref.c src/blake2/blake2.h src/blake2/blake2-impl.h
