# busk

Plain text search using an inverted trigram index.


## Usage

### Example

Building the index on the fly, and piping it into the search tool:

```shell
$ busk.mk-index src/ 2> /dev/null | busk.search -c "stbds_arrp"
src/mk-index.c:1213+10:                 stbds_arrput(cfg->corpus_paths, arg);
src/mk-index.c:2576+10:         stbds_arrput(pathbuf, '\\0');
src/mk-index.c:4217+10:         stbds_arrpop(pathbuf);
src/mk-index.c:4244+10: stbds_arrpush(pathbuf, '\\0');
```

### busk.mk-index

Generates an index file which has the single purpose of being consumed by `busk.search`

```shell
Usage: busk.mk-index [-v] [-o OUTPUT] <FILE/DIR>...
  -o, --output=OUTPUT        Output index to OUTPUT instead of stdout
  -v, --verbose              Print more verbose output to stderr
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```

### busk.search

Greps indexed files for a given search string, printing results as `<path>:<offset>+<len>: <match>`

```shell
Usage: search [OPTION...] "<SEARCH STRING>"
  -c, --color                Add terminal colors to search results
  -i, --index=INPUT          Read index file from INPUT instead of stdin
  -v, --verbose              Print more verbose output to stderr
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```

Note:
- Only literal search strings are supported (no regex for now).
- Search strings can span multiple lines and contain arbitrary bytes.
- Matches will be printed with some characters escaped.
- The precise match can be read with `dd if=$path bs=1 skip=$offset count=$len`


## Installation

### Dependencies

- [glibc](https://sourceware.org/glibc/)
- [stb](https://github.com/nothings/stb/)
- [pcre](https://www.pcre.org/)

On Ubuntu (tested on 24.04), these can be installed with
```shell
$ sudo apt install libstb-dev libpcre2-dev
```

### Building from source

- Make sure you have installed the dependencies listed above
- You'll also need `make`, `gcc` and `pkg-config` for the build
- Grab a copy of the source code
- `make clean build RELEASE=1`
- `make test`
- `make install` (installs to `~/.local/bin` by default, make sure that's in your `$PATH`)
  - Undo with `make uninstall`


## License

Copyright (C) 2025-present Gabriel B. Sant'Anna

This software is dual-licensed: you can redistribute it and/or modify it under the terms of the [GNU Affero General Public License version 3](./LICENSE.txt) or, in case you wish to use it under different terms, you can get in touch with the copyright holders in order to negotiate a commercial license.
