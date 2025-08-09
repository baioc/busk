#!/bin/make

.POSIX:

## Options

BUILDDIR = ./build
RELEASE = 0

INDEX_NGRAM_SIZE = 3
LOG_BUFFER_SIZE = 4000

# https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc
# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html

CC = gcc
CFLAGS = -std=gnu11 -pipe -fvisibility=hidden \
	-Wall -Wextra -Wbidi-chars=any -Werror=format-security \
	-Wno-unused-parameter -Wno-missing-field-initializers \
	-DINDEX_NGRAM_SIZE=$(INDEX_NGRAM_SIZE) -DLOG_BUFFER_SIZE=$(LOG_BUFFER_SIZE)
LDFLAGS = -Wl,-z,defs
LDLIBS =

ifeq ($(RELEASE), 1)
	CFLAGS += -O2 -ftree-loop-vectorize -flto -DNDEBUG \
		-D_FORTIFY_SOURCE=2 -fstack-clash-protection -fPIE -fstack-protector-strong -fcf-protection
	LDFLAGS += -Wl,-O2 -flto -s -Wl,-z,noexecstack -pie -Wl,-z,relro,-z,now
else
	CFLAGS += -O0 -g \
		-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
endif

CFLAGS += -fno-strict-aliasing -fno-strict-overflow


## Dependencies

# glibc - https://sourceware.org/glibc/manual/latest/html_node/index.html
LDLIBS += -lc

# libstb-dev - https://github.com/nothings/stb
CFLAGS += $(shell pkg-config --cflags stb) -DSTBDS_NO_SHORT_NAMES
LDFLAGS += $(shell pkg-config --libs-only-L stb)
LDLIBS += $(shell pkg-config --libs-only-l stb)


## Targets

.PHONY: build clean test

build: $(BUILDDIR)/mk-index $(BUILDDIR)/search

clean:
	- rm -f $(BUILDDIR)/*

test: $(BUILDDIR)/mk-index $(BUILDDIR)/search
	$(BUILDDIR)/mk-index -v -o $(BUILDDIR)/index.bin 'src///' Makefile
	$(BUILDDIR)/search -i $(BUILDDIR)/index.bin "stbds_arrp"


## Rules

.SUFFIXES:

$(BUILDDIR)/%.o: src/%.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%: $(BUILDDIR)/%.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# ^ patterns adapted from defaults (as seen with `make -p`)

$(BUILDDIR)/mk-index: src/mk-index.c $(BUILDDIR)/index.o $(BUILDDIR)/log.o
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(filter %.o, $^) $(LDLIBS) -o $@

$(BUILDDIR)/search: src/search.c $(BUILDDIR)/index.o $(BUILDDIR)/log.o
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(filter %.o, $^) $(LDLIBS) -o $@

$(BUILDDIR)/index.o: src/index.c src/index.h

$(BUILDDIR)/log.o: src/log.c src/log.h
