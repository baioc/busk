#!/bin/make

.POSIX:

## Options

BUILDDIR = ./build
RELEASE = 0

# https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc
# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html

CC = gcc
CFLAGS = -std=gnu11 -fno-strict-aliasing -fno-strict-overflow -pipe \
	-Wall -Wextra -Wbidi-chars=any -Werror=format-security \
	-Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS = -Wl,-z,defs
LDLIBS =

ifeq ($(RELEASE), 1)
	CFLAGS += -O2 -ftree-loop-vectorize -flto -DNDEBUG \
		-D_FORTIFY_SOURCE=2 -fstack-clash-protection -fPIE -fstack-protector-strong -fcf-protection
	LDFLAGS += -Wl,-O2 -s -Wl,-z,noexecstack -pie -Wl,-z,relro,-z,now
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
