# Copyright 2015 Golden Helix, Inc.

#TIP: To build with -O3, run:
#> OPTIMIZATION='' make

TSF_SRCS = src

SQLITE3_OBJS = src/sqlite3/sqlite3.o

JANSSON_PATH = src/jansson
JANSSON_OBJS = \
  $(JANSSON_PATH)/dump.o \
  $(JANSSON_PATH)/error.o \
  $(JANSSON_PATH)/hashtable.o \
  $(JANSSON_PATH)/load.o \
  $(JANSSON_PATH)/memory.o \
  $(JANSSON_PATH)/pack_unpack.o \
  $(JANSSON_PATH)/strbuffer.o \
  $(JANSSON_PATH)/strconv.o \
  $(JANSSON_PATH)/utf.o \
  $(JANSSON_PATH)/value.o

BLOSC_PATH = src/blosc
BLOSC_OBJS = \
  $(BLOSC_PATH)/blosc.o \
  $(BLOSC_PATH)/blosclz.o \
  $(BLOSC_PATH)/shuffle.o

ZSTD_PATH = src/zstd/lib
ZSTD_OBJS = \
  ${ZSTD_PATH}/common/entropy_common.o \
  ${ZSTD_PATH}/common/zstd_common.o \
  ${ZSTD_PATH}/common/xxhash.o \
  ${ZSTD_PATH}/common/fse_decompress.o \
  ${ZSTD_PATH}/compress/fse_compress.o \
  ${ZSTD_PATH}/compress/huf_compress.o \
  ${ZSTD_PATH}/compress/zbuff_compress.o \
  ${ZSTD_PATH}/compress/zstd_compress.o \
  ${ZSTD_PATH}/decompress/huf_decompress.o \
  ${ZSTD_PATH}/decompress/zbuff_decompress.o \
  ${ZSTD_PATH}/decompress/zstd_decompress.o \
  ${ZSTD_PATH}/dictBuilder/divsufsort.o \
  ${ZSTD_PATH}/dictBuilder/zdict.o

LZ4_PATH = src/lz4/lib
LZ4_OBJS = \
  $(LZ4_PATH)/lz4.o \
  $(LZ4_PATH)/lz4hc.o \
  $(LZ4_PATH)/lz4frame.o \
  $(LZ4_PATH)/xxhash.o

TSF_OBJS = $(TSF_SRCS)/tsf.o $(SQLITE3_OBJS) $(JANSSON_OBJS) $(BLOSC_OBJS) $(ZSTD_OBJS) $(LZ4_OBJS)

DYN_TSF_OBJECTS=$(foreach i,$(TSF_OBJS),$(patsubst %.o,%.os,$(i)))

OPTIMIZATION?=-O3
WARNINGS?=-Wall
DEBUG?=-ggdb
STD?=gnu99
PEDANTIC?=-pedantic
ALL_CFLAGS=-std=$(STD) $(PEDANTIC) $(CFLAGS) $(OPTIMIZATION) $(WARNINGS) $(DEBUG) $(ALL_DEFINES)
ALL_LDFLAGS=$(LDFLAGS) -lz
CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')

all: test_tsf libtsf.so

test_tsf: $(TSF_OBJS) tests/tests.c
	$(CC) -o test_tsf tests/tests.c $(ALL_CFLAGS) -Isrc -I$(JANSSON_PATH) -I$(BLOSC_PATH) -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(LZ4_PATH) $(TSF_OBJS) $(ALL_LDFLAGS)

libtsf.so: $(DYN_TSF_OBJECTS)
	$(CC) -shared -o libtsf.so $(ALL_LDFLAGS)  $(DYN_TSF_OBJECTS)

ALL_DEFINES=$(DEFINES)
DYN_FLAGS:=-fPIC

%.o: %.c
	$(CC) -o $@ -c $(ALL_CFLAGS) -I$(JANSSON_PATH) -I$(BLOSC_PATH) -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(LZ4_PATH) $<

%.os: %.c
	$(CC) -o $@ -c $(ALL_CFLAGS) $(DYN_FLAGS) -I$(JANSSON_PATH) -I$(BLOSC_PATH) -I$(ZSTD_PATH) -I$(ZSTD_PATH)/common -I$(LZ4_PATH) $<
