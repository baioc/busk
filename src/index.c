#include "index.h"

#include <stb/stb_ds.h> // arrr* and hm* macros

#include <assert.h>
#include <stddef.h> // size_t
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // qsort
#include <string.h> // strlen, memcpy, strncmp


#ifndef INDEX_NGRAM_SIZE
#define INDEX_NGRAM_SIZE 3
#elif INDEX_NGRAM_SIZE < 2
#error "INDEX_NGRAM_SIZE must be at least 2"
#endif

typedef struct {
	char bytes[INDEX_NGRAM_SIZE];
} NGram;

typedef struct {
	uint64_t key; // offset into paths array
} Posting;

typedef struct IndexPostingMapping {
	NGram key;
	Posting *value;
} PostingMapping;


void index_cleanup(struct Index *index)
{
	if (!index) return;
	for (size_t i = 0; i < stbds_hmlenu(index->posting_hm); ++i) {
		Posting *postings = index->posting_hm[i].value;
		stbds_hmfree(postings);
	}
	stbds_hmfree(index->posting_hm);
	stbds_arrfree(index->path_arr);
}


// binary file format:
//
// - header:
//   - 8-byte byte sequence: file magic
//   - 8-byte LE u64: size of ngram index, in number of entries
//   - 8-byte little endian u64: size of path list, in bytes
//
// - paths:
//   - variable-length C strings, concatenated, each terminated by a zero byte
//
// - index:
//   - sequence of variable-length entries, each with the following format:
//     - 4-byte LE u32: size of posting list, in number of items
//     - N-byte ngram: first byte is ngram[0], second is ngram[1], etc
//     - sequence of LE u64: posting list, each item an offset into paths

static int postingmap_cmp(const void *a, const void *b)
{
	const PostingMapping *lhs = a;
	const PostingMapping *rhs = b;
	int cmpresult = 0;
	for (size_t i = 0; i < sizeof(NGram); ++i) {
		cmpresult = (int)lhs->key.bytes[i] - (int)rhs->key.bytes[i];
		if (cmpresult != 0) break;
	}
	return cmpresult;
}

static int postinglist_cmp(const void *a, const void *b)
{
	const Posting *lhs = a;
	const Posting *rhs = b;
	if (lhs->key < rhs->key) return -1;
	else if (lhs->key > rhs->key) return 1;
	else return 0;
}

int64_t index_save(struct Index index, FILE *outfile)
{
	int64_t expected_bytes = 0;
	int64_t written_bytes = 0;

	const char magic[] = {
		'\xFF', // non-ascii byte to avoid confusion with a text file
		'B', 'U', 'S', 'K', // make it read nicely in a hex dump
		'0', '1', // placeholder, may become version number in the future
		'\x1A', // ascii "Ctrl-Z", treated as end of file in DOS
	};
	static_assert(sizeof(magic) == 8, "File magic should be 8 bytes");
	const uint64_t ngrams = stbds_hmlenu(index.posting_hm);
	const uint64_t pathslen = stbds_arrlenu(index.path_arr);

	written_bytes += fwrite(magic, sizeof(magic), 1, outfile) * sizeof(magic);
	for (int i = 0; i < 8; ++i) {
		fputc((ngrams >> (i*8)) & 0xFF, outfile);
		++written_bytes;
	}
	for (int i = 0; i < 8; ++i) {
		fputc((pathslen >> (i*8)) & 0xFF, outfile);
		++written_bytes;
	}
	expected_bytes = 8 * 3;

	written_bytes += fwrite(index.path_arr, 1, pathslen, outfile);
	expected_bytes += pathslen;

	// sort ngrams to get consistent serialization output
	PostingMapping *postingmap_sorted = NULL;
	stbds_arrsetlen(postingmap_sorted, ngrams);
	memcpy(postingmap_sorted, index.posting_hm, sizeof(PostingMapping) * ngrams);
	qsort(postingmap_sorted, ngrams, sizeof(PostingMapping), postingmap_cmp);
	Posting *postinglist_sorted = NULL;

	for (uint64_t i = 0; i < ngrams; ++i) {
		const NGram ngram = postingmap_sorted[i].key;
		const Posting *postings = postingmap_sorted[i].value;

		const uint32_t postinglen = stbds_hmlen(postings);
		for (int i = 0; i < 4; ++i) {
			fputc((postinglen >> (i*8)) & 0xFF, outfile);
			++written_bytes;
		}

		written_bytes += fwrite(ngram.bytes, sizeof(ngram), 1, outfile) * sizeof(ngram);

		// also need to sort posting lists for each individual ngram
		stbds_arrsetlen(postinglist_sorted, postinglen);
		memcpy(postinglist_sorted, postings, sizeof(Posting) * postinglen);
		qsort(postinglist_sorted, postinglen, sizeof(Posting), postinglist_cmp);

		for (uint32_t i = 0; i < postinglen; ++i) {
			const uint64_t offset = postings[i].key;
			for (int i = 0; i < 8; ++i) {
				fputc((offset >> (i*8)) & 0xFF, outfile);
				++written_bytes;
			}
		}

		expected_bytes += 4 + sizeof(ngram) + postinglen*8;
	}

	stbds_arrfree(postinglist_sorted);
	stbds_arrfree(postingmap_sorted);

	const int64_t error = written_bytes - expected_bytes;
	return error ? error : written_bytes;
}

int index_load(struct Index *index, FILE *file)
{
	// return zero: OK
	// return negative: not enough data aka unexpected EOF
	// return positive: something wrong with read data

	unsigned char file_header[8 * 3] = {0};
	if (!fread(file_header, sizeof(file_header), 1, file)) return -3;

	if (strncmp((char *) &file_header[0], "\xFF""BUSK01\x1A", 8) != 0) return 1;

	uint64_t ngrams = 0;
	for (int i = 0; i < 8; ++i) ngrams |= (file_header[8 + i] & 0x00FFull) << (i*8);

	uint64_t pathslen = 0;
	for (int i = 0; i < 8; ++i) pathslen |= (file_header[16 + i] & 0x00FFull) << (i*8);

	char *paths = NULL;
	stbds_arrsetlen(paths, pathslen);
	if (stbds_arrlenu(paths) != pathslen) {
		stbds_arrfree(paths);
		return 3;
	}

	const size_t bytes_read = fread(paths, 1, pathslen, file);
	if (bytes_read < pathslen) {
		stbds_arrfree(paths);
		return -4;
	}

	PostingMapping *postingsmap = NULL;
	int error = 0;
	for (uint64_t i = 0; i < ngrams; ++i) {
		unsigned char ngram_header[4 + sizeof(NGram)] = {0};
		if (!fread(ngram_header, sizeof(ngram_header), 1, file)) {
			error = -5;
			break;
		}

		uint32_t postinglen = 0;
		for (int i = 0; i < 4; ++i) postinglen |= (ngram_header[i] & 0x00FFul) << (i*8);

		NGram ngram = {0};
		memcpy(ngram.bytes, &ngram_header[4], sizeof(ngram));

		Posting *postings = NULL;
		for (uint32_t i = 0; i < postinglen; ++i) {
			unsigned char leu64[8] = {0};
			if (!fread(leu64, sizeof(leu64), 1, file)) {
				error = -5;
				break;
			}

			uint64_t offset = 0;
			for (int i = 0; i < 8; ++i) {
				offset |= (leu64[i] & 0x00FFull) << (i*8);
			}
			if (offset >= pathslen) {
				error = 5;
				break;
			}

			Posting posting = { offset };
			stbds_hmputs(postings, posting);
		}
		if (error) {
			stbds_hmfree(postings);
			break;
		}

		stbds_hmput(postingsmap, ngram, postings);
	}
	if (error) {
		for (size_t i = 0; i < stbds_hmlenu(postingsmap); ++i) {
			Posting *postings = postingsmap[i].value;
			stbds_hmfree(postings);
		}
		stbds_hmfree(postingsmap);
		stbds_arrfree(paths);
		return error;
	}

	*index = (struct Index){
		.path_arr = paths,
		.posting_hm = postingsmap,
	};
	return 0;
}


static void index_ngram(struct Index *index, NGram ngram, uint64_t path_offset)
{
	Posting *postings = stbds_hmget(index->posting_hm, ngram);
	Posting posting = { path_offset };
	stbds_hmputs(postings, posting);
	stbds_hmput(index->posting_hm, ngram, postings);
}

int64_t index_file(struct Index *index, FILE *file, const char *filepath)
{
	int64_t ngram_count = 0;

	// append filepath + null terminator to index
	const size_t path_length = strlen(filepath);
	const size_t path_offset = stbds_arraddnindex(index->path_arr, path_length + 1);
	memcpy(&index->path_arr[path_offset], filepath, path_length);
	index->path_arr[path_offset + path_length] = '\0';
	// TODO: compress common filepath prefixes

	NGram ngram = {0};
	char buffer[4096];

	// read first ngram
	if (!fread(buffer, sizeof(ngram), 1, file)) {
		return ngram_count;
	} else {
		for (size_t i = 0; i < sizeof(ngram); ++i)
			ngram.bytes[i] = buffer[i];
	}
	index_ngram(index, ngram, path_offset);
	++ngram_count;

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
			++ngram_count;
		}
	}

	return ngram_count;
}


size_t index_ngram_size(void)
{
	static_assert(sizeof(NGram) == INDEX_NGRAM_SIZE, "NGram size sanity check");
	return sizeof(NGram);
}

struct IndexResult index_query(struct Index index, struct IndexQuery query)
{
	const struct IndexResult result_empty = {0};
	if (query.text == NULL || query.strlen < sizeof(NGram)) return result_empty;

	NGram ngram = {0};
	memcpy(ngram.bytes, query.text, sizeof(ngram));

	Posting *postings = stbds_hmget(index.posting_hm, ngram);
	if (!postings) return result_empty;

	struct IndexResult result = {
		.offsets = (uint64_t *) postings,
		.length = stbds_hmlenu(postings),
	};
	static_assert(sizeof(Posting) == sizeof(uint64_t), "Posting[] <=> uint64_t[] cast check");

	return result;
}


size_t index_pathlen(struct Index index, uint64_t offset)
{
	if (offset >= stbds_arrlenu(index.path_arr)) return 0;
	const char *path = &index.path_arr[offset];
	const size_t pathlen = strlen(path);
	return pathlen;
}

size_t index_path(struct Index index, uint64_t offset, char *pathbuf, size_t buflen)
{
	if (offset >= stbds_arrlenu(index.path_arr)) return 0;

	const char *path = &index.path_arr[offset];
	const size_t pathlen = strlen(path);

	const size_t writtenlen = buflen < pathlen ? buflen : pathlen;
	memcpy(pathbuf, path, writtenlen);
	if (writtenlen < buflen) pathbuf[writtenlen] = '\0';

	assert(writtenlen <= buflen);
	return writtenlen;
}
