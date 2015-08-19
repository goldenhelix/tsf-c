# Copyright 2015 Golden Helix, Inc.

TSF_SRCS = src

JANSSON_PATH = src/jansson

SQLITE3_OBJS = src/sqlite3/sqlite3.o

JANSSON_OBJS = $(JANSSON_PATH)/dump.o \
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
BLOSC_OBJS = $(BLOSC_PATH)/blosc.o \
  $(BLOSC_PATH)/blosclz.o \
  $(BLOSC_PATH)/shuffle.o

TSF_OBJS = $(TSF_SRCS)/tsf.o $(SQLITE3_OBJS) $(JANSSON_OBJS) $(BLOSC_OBJS)

DYN_TSF_OBJECTS=$(foreach i,$(TSF_OBJS),$(patsubst %.o,%.os,$(i)))

OPTIMIZATION?=-O3
WARNINGS?=-Wall
DEBUG?=-ggdb
STD?=c99
PEDANTIC?=-pedantic
ALL_CFLAGS=-std=$(STD) $(PEDANTIC) $(CFLAGS) $(OPTIMIZATION) $(WARNINGS) $(DEBUG) $(ALL_DEFINES)
ALL_LDFLAGS=$(LDFLAGS) -lz
CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')

all: test_tsf libtsf.so

test_tsf: $(TSF_OBJS) tests/tests.c
	$(CC) -o test_tsf tests/tests.c $(ALL_CFLAGS) -Isrc -Isrc/jansson -Isrc/blosc $(TSF_OBJS) $(ALL_LDFLAGS)

libtsf.so: $(DYN_TSF_OBJECTS)
	$(CC) -shared -o libtsf.so $(ALL_LDFLAGS)  $(DYN_TSF_OBJECTS)

ALL_DEFINES=$(DEFINES)
DYN_FLAGS:=-fPIC

%.o: %.c
	$(CC) -o $@ -c $(ALL_CFLAGS) -Isrc/jansson -Isrc/blosc $<

%.os: %.c
	$(CC) -o $@ -c $(ALL_CFLAGS) $(DYN_FLAGS) -Isrc/jansson -Isrc/blosc $<
