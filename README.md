# busk

Plain text search using an inverted trigram index.


## Usage

Combined example:
```shell
$ busk.mk-index src/ | busk.search "stbds_"
src/mk-index.c
src/index.c
src/search.c
```

busk.mk-index:
```shell
Generate a text search index from the given files and/or directories.

Usage: busk.mk-index [-v] [-o OUTPUT] <FILE/DIR>...
  -o, --output=OUTPUT        Output index to OUTPUT instead of stdout
  -v, --verbose              Print more verbose output to stderr
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```

busk.search:
```shell
Query an index, listing files to grep a search string in.

Usage: busk.search [-v] [-i INPUT] "<SEARCH STRING>"
  -i, --index=INPUT          Read index from INPUT instead of stdin
  -v, --verbose              Print more verbose output to stderr
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```


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
- `make test` (optional)
- `make install` (installs to `~/.local/bin` by default, make sure that's in your `$PATH`)
  - Undo with `make uninstall`


## License

Copyright (C) 2025-present Gabriel B. Sant'Anna

This software is dual-licensed: you can redistribute it and/or modify it under the terms of the [GNU Affero General Public License version 3](./LICENSE.txt) or, in case you wish to use it under different terms, you can get in touch with the copyright holders in order to negotiate a commercial license.
