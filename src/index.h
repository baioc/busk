#ifndef INCLUDE_INDEX_H
#define INCLUDE_INDEX_H

#include <stddef.h> // size_t
#include <stdint.h>
#include <stdio.h> // FILE

struct PostingMapping; // forward decl

// Text search (aka inverted) index. Initialize with `{0}`.
struct Index {
	char *paths; // big array with all paths, concatenated, separated by '\0'.
	struct PostingMapping *postings; // map of NGram -> Set(Posting).
};

// Deallocate all internal data structures used in the index.
void index_cleanup(struct Index *index);

// Serialize the index to a file, returning number of bytes written.
int64_t index_save(struct Index index, FILE *file);

// TODO: int index_load(struct Index *index, FILE *file)

// Index file contents, returning the number of ngrams added.
// Calling this multiple times with the same file WILL process it multiple times.
int64_t index_file(struct Index *index, FILE *file, const char *filepath);

#endif // INCLUDE_INDEX_H
