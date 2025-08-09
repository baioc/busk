#ifndef INCLUDE_INDEX_H
#define INCLUDE_INDEX_H

#include <stddef.h> // size_t
#include <stdint.h>
#include <stdio.h> // FILE

struct PostingMapping; // forward decl

// Text search (aka inverted) index. Must be initialized with `{0}`.
struct Index {
	char *path_arr; // big array with all paths, concatenated, separated by '\0'.
	struct PostingMapping *posting_hm; // map of NGram -> Set(Posting).
};

// Deallocate all internal data structures used in the index.
void index_cleanup(struct Index *index);

// Serialize index to file, returning number of bytes written, or a negative error code.
int64_t index_save(struct Index index, FILE *file);

// Load index from file, returning zero on success or an error code.
int index_load(struct Index *index, FILE *file);

// Index file contents, returning the number of ngrams processed.
int64_t index_file(struct Index *index, FILE *file, const char *filepath);

#endif // INCLUDE_INDEX_H
