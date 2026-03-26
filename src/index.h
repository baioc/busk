#ifndef INCLUDE_INDEX_H
#define INCLUDE_INDEX_H

#include <stddef.h> // size_t
#include <stdint.h>
#include <stdio.h> // FILE


struct IndexPostingMapping; // forward decl

// Text search (aka inverted) index. Must be initialized with `{0}`.
struct Index {
	uint8_t *_path_arr; // big array with all paths, encoded with compression
	struct IndexPostingMapping *_posting_hm; // map of NGram -> Set(Posting).
	uint64_t _last_path_added; // used for prefix compression
};

// Index query, with a pointer to some text and corresponding strlen.
struct IndexQuery {
	const char *text;
	size_t strlen;
};

struct IndexPathHandle {
	uint64_t _offset;
};

// Index query result, with an array of path handles.
// A missing result is indicated by a NULL array + zero length.
// If not a missing result, must be cleaned up with `index_result_cleanup()`.
struct IndexResult {
	const struct IndexPathHandle *handles;
	size_t length;
};


// Deallocate all internal data structures used in the index.
void index_cleanup(struct Index *index);

// Serialize index to file, returning number of bytes written, or a negative error code.
int64_t index_save(struct Index index, FILE *file);

// Load index from file, returning zero on success or an error code.
int index_load(struct Index *index, FILE *file);

// Index file contents, returning the number of ngrams processed, or a negative error code.
int64_t index_file(struct Index *index, FILE *file, const char *filepath, size_t pathlen);

// Return the size of an N-gram in bytes (i.e. the value of N).
size_t index_ngram_size(void);

// Query the index for exactly `index_ngram_size()` bytes read from the query text.
struct IndexResult index_query(struct Index index, struct IndexQuery query);

void index_result_cleanup(struct IndexResult *result);

// Returns the number of non-null bytes in the path corresponding to the given offset.
size_t index_pathlen(struct Index index, struct IndexPathHandle handle);

// Fills (at most buflen bytes in) pathbuf with path corresponding to the given offset.
// Returns the number of characters written to pathbuf, excluding the null terminator.
size_t index_path(struct Index index, struct IndexPathHandle handle, char *pathbuf, size_t buflen);

#endif // INCLUDE_INDEX_H
