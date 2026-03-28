# busk

Plain text search using an inverted trigram index.


## Performance

The whole point of a search index is to make searches fast.

Once the index is ready, we're currently faster than grep:

```shell
$ hyperfine -i \
  "grep -I -e 'TODO' -R ~/code" \
  "busk.search -i code.busk 'TODO'"

Benchmark 1: grep -I -e 'TODO' -R ~/code
  Time (mean ± σ):     12.803 s ±  2.835 s    [User: 9.906 s, System: 1.526 s]
  Range (min … max):   10.697 s … 18.541 s    10 runs

  Warning: Ignoring non-zero exit code.

Benchmark 2: busk.search -i code.busk 'TODO'
  Time (mean ± σ):      3.106 s ±  0.880 s    [User: 2.264 s, System: 0.522 s]
  Range (min … max):    2.670 s …  5.157 s    10 runs

Summary
  busk.search -i code.busk 'TODO' ran
    4.12 ± 1.48 times faster than grep -I -e 'TODO' -R ~/code
```

Building the index, however, might be slow:

```shell
$ du -Lsh -c ~/code/ 2> /dev/null | grep total
# 6.0G	total

$ time busk.mk-index -o code.busk ~/code/
# 137.72s elapsed 120.70s user 5.82s system 91% cpu 636KB maxRSS

$ du -h code.busk
# 412M	code.busk
```


## Usage

### Example

Building the index on the fly, and piping it into the search tool:

```shell
$ busk.mk-index src/ 2> /dev/null | busk.search -c "stbds_arrp"
src/index.c:9090+10:    stbds_arrput(postings, path_offset);
src/mk-index.c:1291+10:                         stbds_arrput(cfg->corpus_paths, arg);
src/mk-index.c:3701+10:                 stbds_arrput(pathbuf, '\\0');
src/mk-index.c:5449+10:                 stbds_arrpop(pathbuf);
src/mk-index.c:5476+10:         stbds_arrpush(pathbuf, '\\0');
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
- The precise match can be read with the equivalent of `dd if=$path bs=1 skip=$offset count=$len`


## Installation

### Dependencies

- [glibc](https://sourceware.org/glibc/)
- [stb](https://github.com/nothings/stb/) (vendored)
- [pcre](https://www.pcre.org/)

On Ubuntu (tested on 24.04), these can be installed with
```shell
$ sudo apt install libpcre2-dev
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
