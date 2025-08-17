#ifndef INCLUDE_INDEX_H
#define INCLUDE_INDEX_H

#include <stddef.h> // size_t
#include <stdint.h>
#include <stdio.h> // FILE


struct IndexPostingMapping; // forward decl

// Text search (aka inverted) index. Must be initialized with `{0}`.
struct Index {
	char *path_arr; // big array with all paths, concatenated, separated by '\0'.
	struct IndexPostingMapping *posting_hm; // map of NGram -> Set(Posting).
};

// Index query, with a pointer to some text and corresponding strlen.
struct IndexQuery {
	const char *text;
	size_t strlen;
};

// Index query result, with an array of path offsets.
// A missing result is indicated by a NULL array + zero length.
// The offsets array is managed internally, and must NOT be modified.
struct IndexResult {
	const uint64_t *offsets;
	size_t length;
};


// Deallocate all internal data structures used in the index.
void index_cleanup(struct Index *index);

// Serialize index to file, returning number of bytes written, or a negative error code.
int64_t index_save(struct Index index, FILE *file);

// Load index from file, returning zero on success or an error code.
int index_load(struct Index *index, FILE *file);

// Index file contents, returning the number of ngrams processed.
uint64_t index_file(struct Index *index, FILE *file, const char *filepath);

// Return the size of an N-gram in bytes (i.e. the value of N).
size_t index_ngram_size(void);

// Query the index for exactly `index_ngram_size()` bytes read from the query text.
struct IndexResult index_query(struct Index index, struct IndexQuery query);

// Returns the number of non-null bytes in the path corresponding to the given offset.
size_t index_pathlen(struct Index index, uint64_t offset);

// Fills (at most buflen bytes in) pathbuf with path corresponding to the given offset.
// Returns the number of characters written to pathbuf, excluding the null terminator.
size_t index_path(struct Index index, uint64_t offset, char *pathbuf, size_t buflen);

#endif // INCLUDE_INDEX_H
