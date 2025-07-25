#!/bin/make

.POSIX:

## Options

BUILDDIR = ./build
RELEASE = 0

CC = gcc
CFLAGS = -std=gnu11 -fno-strict-aliasing \
	-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
	-Wformat-security -Werror=format-security
LDFLAGS =
LDLIBS =

ifeq ($(RELEASE), 1)
	CFLAGS += -O2 -flto -DNDEBUG \
		-D_FORTIFY_SOURCE=2 -fPIE -fstack-protector-strong -fcf-protection
	LDFLAGS += -s -flto -pie -Wl,-z,relro,-z,now
else
	CFLAGS += -O0 -g \
		-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
endif


## Dependencies

# glibc - https://sourceware.org/glibc/manual/latest/html_node/index.html
LDLIBS += -lc

# libstb-dev - https://github.com/nothings/stb
CFLAGS += $(shell pkg-config --cflags stb) -DSTBDS_NO_SHORT_NAMES
LDFLAGS += $(shell pkg-config --libs-only-L stb)
LDLIBS += $(shell pkg-config --libs-only-l stb)


## Targets

.PHONY: build clean test

build: $(BUILDDIR)/mk-index

clean:
	- rm -f $(BUILDDIR)/*

test: $(BUILDDIR)/mk-index
	$(BUILDDIR)/mk-index --help


## Rules

.SUFFIXES:

$(BUILDDIR)/mk-index: src/mk-index.c src/index.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@
