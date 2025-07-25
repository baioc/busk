#include <stb/stb_ds.h> // arrr* and hm* macros

#include <assert.h>
#include <stddef.h> // size_t, ptrdiff_t
#include <stdio.h>
#include <string.h> // strlen, memcpy


#ifndef NGRAM_SIZE
#	define NGRAM_SIZE 3
#endif

static_assert(NGRAM_SIZE >= 2, "invalid N-gram size, N must be at least 2");

typedef struct {
	char bytes[NGRAM_SIZE];
} NGram;


typedef struct {
	size_t key; // offset into paths array
} Posting;

typedef struct {
	char *paths; // big array with all paths, concatenated, separated by '\0'
	struct { NGram key; Posting *value; } *postings; // NGrams -> Set(Posting)
} Index;

void index_cleanup(Index *index)
{
	for (int i = 0; i < stbds_hmlen(index->postings); ++i) {
		Posting *postings = index->postings[i].value;
		stbds_hmfree(postings);
	}
	stbds_hmfree(index->postings);
	stbds_arrfree(index->paths);
}

void index_print(FILE *stream, Index index)
{
	for (int i = 0; i < stbds_hmlen(index.postings); ++i) {
		NGram ngram = index.postings[i].key;
		fprintf(stream, "[\"");
		for (size_t j = 0; j < sizeof(ngram); ++j) {
			char byte = ngram.bytes[j];
			if (byte >= ' ' && byte <= '~')
				fprintf(stream, "%c", byte);
			else
				fprintf(stream, "\\x%x", byte);
		}
		fprintf(stream, "\"]");

		const Posting *postings = index.postings[i].value;
		fprintf(stream, " -> #{ ");
		for (int j = 0; j < stbds_hmlen(postings); ++j) {
			size_t path_offset = postings[j].key;
			fprintf(stream, "\"%s\" ", &index.paths[path_offset]);
		}
		fprintf(stream, "}\n");
	}
}


void index_ngram(Index *index, NGram ngram, size_t path_offset)
{
	Posting *postings = stbds_hmget(index->postings, ngram);
	Posting posting = { path_offset };
	stbds_hmputs(postings, posting);
	stbds_hmput(index->postings, ngram, postings);
}

ptrdiff_t index_file(Index* index, const char *path)
{
	FILE *file = fopen(path, "r");
	if (!file) return -1;

	ptrdiff_t ngrams_read = 0;

	// append path + null terminator to index
	const size_t path_length = strlen(path);
	const size_t path_offset = stbds_arraddnindex(index->paths, path_length + 1);
	memcpy(&index->paths[path_offset], path, path_length);
	index->paths[path_offset + path_length] = '\0';

	NGram ngram = {0};
	char buffer[4096];

	// read first ngram
	if (!fread(buffer, sizeof(ngram), 1, file)) {
		goto exit;
	} else {
		for (size_t i = 0; i < sizeof(ngram); ++i) {
			ngram.bytes[i] = buffer[i];
		}
	}
	index_ngram(index, ngram, path_offset);
	ngrams_read++;

	// read the following ngrams by sliding an N-byte window with 1-byte steps
	size_t chunk_length = 0;
	while ((chunk_length = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		for (size_t i = 0; i < chunk_length; ++i) {
			const char byte = buffer[i];
			for (size_t j = 0; j < sizeof(ngram) - 1; ++j) {
				ngram.bytes[j] = ngram.bytes[j + 1];
			}
			ngram.bytes[sizeof(ngram) - 1] = byte;
			index_ngram(index, ngram, path_offset);
			ngrams_read++;
		}
	}

exit:
	fclose(file);
	return ngrams_read;
}
