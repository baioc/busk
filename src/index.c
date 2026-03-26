#include "index.h"

#include <stb/stb_ds.h> // arrr* and hm* macros

#include <assert.h>
#include <stdalign.h>
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
	char _padding[INDEX_NGRAM_SIZE % 2];
} NGram;

typedef struct {
	uint64_t key; // offset into paths array
} Posting;

typedef struct IndexPostingMapping {
	NGram key;
	Posting *value;
} IndexPostingMapping;

typedef struct {
	uint16_t allocation_size; // number of bytes used to allocate this
	uint16_t offset_to_prefix; // relative backwards offset to prefix, or zero
	uint16_t prefix_length; // bytes in shared prefix, or zero if n/a
	uint16_t suffix_length; // length of uncompressed part of string
	char suffix_bytes[];
} IndexPathEntry;


void index_cleanup(struct Index *index)
{
	if (!index) return;
	for (size_t i = 0; i < stbds_hmlenu(index->_posting_hm); ++i) {
		Posting *postings = index->_posting_hm[i].value;
		stbds_hmfree(postings);
	}
	stbds_hmfree(index->_posting_hm);
	stbds_arrfree(index->_path_arr);
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
	const IndexPostingMapping *lhs = a;
	const IndexPostingMapping *rhs = b;
	int cmpresult = 0;
	for (size_t i = 0; i < INDEX_NGRAM_SIZE; ++i) {
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

static inline size_t write_le(FILE *file, uint64_t value, size_t size)
{
	uint8_t buffer[sizeof(uintmax_t)];
	assert(size <= sizeof(buffer));
	for (size_t i = 0; i < size; ++i) {
		buffer[i] = (value >> (i*8)) & 0xff;
	}
	return fwrite(buffer, 1, size, file);
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
	const uint64_t ngrams = stbds_hmlenu(index._posting_hm);
	const uint64_t pathslen = stbds_arrlenu(index._path_arr);

	// header
	written_bytes += fwrite(magic, 1, 8, outfile);
	written_bytes += write_le(outfile, pathslen, sizeof(uint64_t));
	written_bytes += write_le(outfile, ngrams, sizeof(uint64_t));
	expected_bytes += 8 * 3;

	// paths
	for (uint64_t offset = 0; offset < pathslen;) {
		const IndexPathEntry *entry = (IndexPathEntry*)&index._path_arr[offset];
		written_bytes += write_le(outfile, entry->allocation_size, sizeof(uint16_t));
		written_bytes += write_le(outfile, entry->offset_to_prefix, sizeof(uint16_t));
		written_bytes += write_le(outfile, entry->prefix_length, sizeof(uint16_t));
		written_bytes += write_le(outfile, entry->suffix_length, sizeof(uint16_t));
		const size_t fam_size = entry->allocation_size - sizeof(IndexPathEntry);
		written_bytes += fwrite(entry->suffix_bytes, 1, fam_size, outfile);
		offset += entry->allocation_size;
	}
	expected_bytes += pathslen;

	// sort ngrams to get consistent serialization output
	IndexPostingMapping *postingmap_sorted = NULL;
	stbds_arrsetlen(postingmap_sorted, ngrams);
	memcpy(postingmap_sorted, index._posting_hm, sizeof(IndexPostingMapping) * ngrams);
	qsort(postingmap_sorted, ngrams, sizeof(IndexPostingMapping), postingmap_cmp);
	Posting *postinglist_sorted = NULL;

	for (uint64_t i = 0; i < ngrams; ++i) {
		const NGram ngram = postingmap_sorted[i].key;
		const Posting *postings = postingmap_sorted[i].value;

		const uint32_t postinglen = stbds_hmlen(postings);
		written_bytes += write_le(outfile, postinglen, sizeof(uint32_t));
		written_bytes += fwrite((char*)&ngram, 1, sizeof(ngram), outfile);

		// also need to sort posting lists for each individual ngram
		stbds_arrsetlen(postinglist_sorted, postinglen);
		memcpy(postinglist_sorted, postings, sizeof(Posting) * postinglen);
		qsort(postinglist_sorted, postinglen, sizeof(Posting), postinglist_cmp);

		for (uint32_t i = 0; i < postinglen; ++i) {
			const uint64_t offset = postings[i].key;
			written_bytes += write_le(outfile, offset, sizeof(uint64_t));
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

	// TODO: optimize for read-only index

	unsigned char file_header[8 * 3] = {0};
	if (!fread(file_header, sizeof(file_header), 1, file)) return -3;

	if (memcmp(&file_header[0], "\xFF""BUSK01\x1A", 8) != 0) return 1;

	uint64_t pathslen = 0;
	for (int i = 0; i < 8; ++i) pathslen |= ((uint64_t)file_header[8 + i] & 0xff) << (i*8);

	uint64_t ngrams = 0;
	for (int i = 0; i < 8; ++i) ngrams |= ((uint64_t)file_header[16 + i] & 0xff) << (i*8);

	uint8_t *paths = NULL;
	stbds_arrsetlen(paths, pathslen);
	if (stbds_arrlenu(paths) != pathslen) {
		stbds_arrfree(paths);
		return 2;
	}

	// paths
	uint64_t last_path_added = 0;
	for (uint64_t offset = 0; offset < pathslen;) {
		unsigned char entry_header[sizeof(IndexPathEntry)] = {0};
		if (!fread(entry_header, sizeof(entry_header), 1, file)) {
			stbds_arrfree(paths);
			return -4;
		}

		IndexPathEntry *entry = (IndexPathEntry*)&paths[offset];

		entry->allocation_size = 0;
		for (int i = 0; i < 2; ++i) entry->allocation_size |= ((uint16_t)entry_header[i] & 0xff) << (i*8);

		entry->offset_to_prefix = 0;
		for (int i = 0; i < 2; ++i) entry->offset_to_prefix |= ((uint16_t)entry_header[2+i] & 0xff) << (i*8);

		entry->prefix_length = 0;
		for (int i = 0; i < 2; ++i) entry->prefix_length |= ((uint16_t)entry_header[4+i] & 0xff) << (i*8);

		entry->suffix_length = 0;
		for (int i = 0; i < 2; ++i) entry->suffix_length |= ((uint16_t)entry_header[6+i] & 0xff) << (i*8);


		const size_t fam_size = entry->allocation_size - sizeof(IndexPathEntry);
		if (!fread(entry->suffix_bytes, fam_size, 1, file)) {
			stbds_arrfree(paths);
			return -4;
		}

		last_path_added = offset;
		offset += entry->allocation_size;
	}

	// ngrams
	IndexPostingMapping *postingsmap = NULL;
	int error = 0;
	for (uint64_t i = 0; i < ngrams; ++i) {
		unsigned char ngram_header[4 + sizeof(NGram)] = {0};
		if (!fread(ngram_header, sizeof(ngram_header), 1, file)) {
			error = -5;
			break;
		}

		uint32_t postinglen = 0;
		for (int i = 0; i < 4; ++i) postinglen |= ((uint32_t)ngram_header[i] & 0xff) << (i*8);

		NGram ngram = {0};
		memcpy(ngram.bytes, &ngram_header[4], INDEX_NGRAM_SIZE);

		Posting *postings = NULL;
		for (uint32_t i = 0; i < postinglen; ++i) {
			unsigned char leu64[8] = {0};
			if (!fread(leu64, sizeof(leu64), 1, file)) {
				error = -5;
				break;
			}

			uint64_t offset = 0;
			for (int i = 0; i < 8; ++i) {
				offset |= ((uint64_t)leu64[i] & 0xff) << (i*8);
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
		._path_arr = paths,
		._posting_hm = postingsmap,
		._last_path_added = last_path_added,
	};
	return 0;
}


static void index_ngram(struct Index *index, NGram ngram, uint64_t path_offset)
{
	IndexPostingMapping *index_mapping = stbds_hmgetp_null(index->_posting_hm, ngram);
	Posting *posting_set = index_mapping ? index_mapping->value : NULL;
	Posting post = { path_offset };
	stbds_hmputs(posting_set, post);
	stbds_hmput(index->_posting_hm, ngram, posting_set);
}

static size_t round_to_alignment(size_t base_size, size_t alignment)
{
	assert(alignment > 0);
	const size_t modulo = base_size % alignment;
	if (modulo == 0) return base_size; // already aligned
	const size_t padding = alignment - modulo;
	return base_size + padding;
}

static size_t shared_length(const char *a, size_t alen, const char *b, size_t blen)
{
	size_t matched = 0;
	while (matched < alen && matched < blen) {
		if (a[matched] != b[matched]) break;
		matched += 1;
	}
	return matched;
}

static uint16_t traverse_to_prefix(
	const struct Index *index, const IndexPathEntry *entry,
	const char *filepath, uint16_t pathlen
) {
	// base case: fully uncompressed string
	if (entry->offset_to_prefix == 0) {
		const uint16_t match_length = shared_length(
			entry->suffix_bytes, entry->suffix_length, filepath, pathlen
		);
		return match_length;
	}

	// otherwise, look at previous entry
	const size_t entry_offset = (uint8_t*)entry - index->_path_arr;
	const size_t prev_offset = entry_offset - entry->offset_to_prefix;
	const IndexPathEntry *previous = (IndexPathEntry*)&index->_path_arr[prev_offset];

	// we should't need to look at the whole previous entry, only the shared prefix
	const uint16_t max_prefix_length = entry->prefix_length < pathlen ? entry->prefix_length : pathlen;
	const uint16_t shared_prefix_length = traverse_to_prefix(index, previous, filepath, max_prefix_length);

	// if we don't use the whole prefix, there's no point in searching the sufix
	if (shared_prefix_length != entry->prefix_length) {
		assert(shared_prefix_length < entry->prefix_length);
		return shared_prefix_length;
	}

	// otherwise, we can try to share even more bits
	const uint16_t remaining_length = pathlen - shared_prefix_length;
	const uint16_t shared_suffix_length = shared_length(
		entry->suffix_bytes, entry->suffix_length,
		&filepath[shared_prefix_length], remaining_length
	);
	return shared_prefix_length + shared_suffix_length;
}

static uint64_t add_path_compressed(struct Index *index, const char *filepath, uint16_t pathlen)
{
	// this is where we'll store the new path entry
	const size_t current_offset = stbds_arrlenu(index->_path_arr);

	// we use a simple compression method which looks at the previous
	// entry and tries to reuse the longest possible prefix found in it.
	// just note that the previous entry might have its prefix encoded by the
	// entry previous to that, and so on recursively
	uint32_t offset_to_prefix = 0;
	uint16_t prefix_length = 0;

	if (current_offset > 0) { // only possible if not the first entry
		const IndexPathEntry *previous = (IndexPathEntry*)&index->_path_arr[index->_last_path_added];
		prefix_length = traverse_to_prefix(index, previous, filepath, pathlen);
		if (prefix_length > 0) {
			const size_t full_offset = current_offset - index->_last_path_added;
			assert(full_offset < UINT16_MAX);
			offset_to_prefix = full_offset;
		}
	}

	const uint16_t suffix_length = pathlen - prefix_length;

	// allocate and initialize entry
	const size_t allocation_size = round_to_alignment(
		sizeof(IndexPathEntry) + suffix_length + 1, // +1 for null terminator
		alignof(IndexPathEntry)
	);
	stbds_arraddnindex(index->_path_arr, allocation_size);
	memset(&index->_path_arr[current_offset], 0, allocation_size);
	IndexPathEntry *entry = (IndexPathEntry*)&index->_path_arr[current_offset];
	entry->allocation_size = allocation_size;
	entry->offset_to_prefix = offset_to_prefix;
	entry->prefix_length = prefix_length;
	entry->suffix_length = suffix_length;
	memcpy(&entry->suffix_bytes, &filepath[prefix_length], suffix_length);
	entry->suffix_bytes[suffix_length] = '\0';

	// keep a pointer to the last entry added
	index->_last_path_added = current_offset;
	return current_offset;
}

int64_t index_file(struct Index *index, FILE *file, const char *filepath, size_t pathlen)
{
	int64_t ngram_count = 0;

	if (pathlen + 1 > UINT16_MAX) return -UINT16_MAX; // see IndexPathEntry
	const uint64_t path_offset = add_path_compressed(index, filepath, pathlen);

	NGram ngram = {0};
	char buffer[4096];

	// read first ngram
	if (!fread(buffer, INDEX_NGRAM_SIZE, 1, file)) return ngram_count;
	memcpy(ngram.bytes, buffer, INDEX_NGRAM_SIZE);
	index_ngram(index, ngram, path_offset);
	++ngram_count;

	// read the following ngrams by sliding an N-byte window with 1-byte steps
	size_t chunk_length = 0;
	while ((chunk_length = fread(buffer, 1, sizeof(buffer), file)) > 0) {
		for (size_t i = 0; i < chunk_length; ++i) {
			memmove(ngram.bytes, ngram.bytes + 1, INDEX_NGRAM_SIZE - 1);
			ngram.bytes[INDEX_NGRAM_SIZE - 1] = buffer[i];
			index_ngram(index, ngram, path_offset);
			++ngram_count;
		}
	}

	return ngram_count;
}


size_t index_ngram_size(void)
{
	return INDEX_NGRAM_SIZE;
}

struct IndexResult index_query(struct Index index, struct IndexQuery query)
{
	const struct IndexResult empty_result = {0};
	if (query.text == NULL || query.strlen < INDEX_NGRAM_SIZE) return empty_result;

	NGram ngram = {0};
	memcpy(ngram.bytes, query.text, INDEX_NGRAM_SIZE);

	IndexPostingMapping *index_mapping = stbds_hmgetp_null(index._posting_hm, ngram);
	if (!index_mapping) return empty_result;

	Posting *postings = index_mapping->value;
	struct IndexResult result = {
		.handles = (struct IndexPathHandle *)postings,
		.length = stbds_hmlenu(postings),
	};
	static_assert(sizeof(Posting) == sizeof(struct IndexPathHandle), "Posting[] <=> IndexPathHandle[] cast check");

	return result;
}

void index_result_cleanup(struct IndexResult *result)
{
	// nothing to free, since result array is shared within index structure
	*result = (struct IndexResult){0};
}


size_t index_pathlen(struct Index index, struct IndexPathHandle handle)
{
	const uint64_t offset = handle._offset;
	if (offset >= stbds_arrlenu(index._path_arr)) return 0;
	const IndexPathEntry *entry = (IndexPathEntry*)&index._path_arr[offset];
	const size_t pathlen = entry->prefix_length + entry->suffix_length;
	return pathlen;
}

static size_t uncompress_path(
	struct Index index, const IndexPathEntry *entry,
	char *buffer, size_t buflen
) {
	// base case: fully uncompressed string
	if (entry->prefix_length == 0) {
		const size_t n = entry->suffix_length <= buflen ? entry->suffix_length : buflen;
		memcpy(buffer, entry->suffix_bytes, n);
		return n;
	}

	// otherwise, we'll need to look at previous entry
	const size_t entry_offset = (uint8_t*)entry - index._path_arr;
	const size_t prev_offset = entry_offset - entry->offset_to_prefix;
	const IndexPathEntry *previous = (IndexPathEntry*)&index._path_arr[prev_offset];

	// we should't need to look at the whole previous entry, only the shared prefix
	const size_t max_prefix_length = entry->prefix_length <= buflen ? entry->prefix_length : buflen;
	size_t written = uncompress_path(index, previous, buffer, max_prefix_length);

	// if still not done after prefix, read the suffix too
	const size_t remaining = buflen - written;
	if (remaining > 0) {
		const size_t n = entry->suffix_length <= remaining ? entry->suffix_length : remaining;
		memcpy(&buffer[written], entry->suffix_bytes, n);
		written += n;
	}

	return written;
}

size_t index_path(struct Index index, struct IndexPathHandle handle, char *pathbuf, size_t buflen)
{
	const uint64_t offset = handle._offset;
	if (offset >= stbds_arrlenu(index._path_arr)) return 0;

	const IndexPathEntry *entry = (IndexPathEntry*)&index._path_arr[offset];

	const size_t writtenlen = uncompress_path(index, entry, pathbuf, buflen);
	if (writtenlen < buflen) pathbuf[writtenlen] = '\0';

	assert(writtenlen <= buflen);
	return writtenlen;
}
