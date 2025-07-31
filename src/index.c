#define LOG_NAME "search.index"
#include "log.h"

#include <dirent.h>
#include <stb/stb_ds.h> // arrr* and hm* macros
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h> // NULL, size_t
#include <stdint.h>
#include <stdio.h>
#include <string.h> // strlen, memcpy, strcmp


#if !defined(INDEX_NGRAM_SIZE)
#	define INDEX_NGRAM_SIZE 3
#elif INDEX_NGRAM_SIZE < 2
#	error "INDEX_NGRAM_SIZE must be at least 2"
#endif

typedef struct {
	char bytes[INDEX_NGRAM_SIZE];
} NGram;


typedef struct {
	uint64_t key; // offset into paths array
} Posting;

typedef struct {
	NGram key;
	Posting *value;
} PostingMapping;

typedef struct {
	char *paths; // Big array with all paths, concatenated, separated by '\0'
	PostingMapping *postings; // map of NGram -> Set(Posting)
} Index;

void index_cleanup(Index *index)
{
	for (size_t i = 0; i < stbds_hmlenu(index->postings); ++i) {
		Posting *postings = index->postings[i].value;
		stbds_hmfree(postings);
	}
	stbds_hmfree(index->postings);
	stbds_arrfree(index->paths);
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
//     - padding bytes (value = 0xFF) up to a multiple of 4
//     - sequence of LE u64: posting list, each item an offset into paths

void index_save(Index index, FILE *file)
{
	const unsigned char magic[] = {
		'\xFF', // non-ascii byte to avoid confusion with a text file
		'B', 'U', 'S', 'K', // make it read nicely in a hex dump
		'0', '1', // placeholder, may become version number in the future
		'\x1A', // ascii "Ctrl-Z", treated as end of file in DOS
	};
	static_assert(sizeof(magic) == 8, "File magic should be 8 bytes");
	const uint64_t ngrams = stbds_hmlenu(index.postings);
	const uint64_t pathslen = stbds_arrlenu(index.paths);

	fwrite(magic, 8, 1, file);
	for (int i = 0; i < 8; ++i) fputc(ngrams & (0x00FFull << (i*8)), file);
	for (int i = 0; i < 8; ++i) fputc(pathslen & (0x00FFull << (i*8)), file);

	fwrite(index.paths, 1, pathslen, file);

	for (uint64_t i = 0; i < ngrams; ++i) {
		const NGram ngram = index.postings[i].key;
		const Posting *postings = index.postings[i].value;

		const uint32_t postinglen = stbds_hmlen(postings);
		for (int i = 0; i < 4; ++i) fputc(postinglen & (0x00FFul << (i*8)), file);

		fwrite(&ngram, sizeof(ngram), 1, file);
		const int padding = 4 - sizeof(ngram) % 4;
		for (int i = 0; i < padding; ++i) fputc('\xFF', file);

		for (uint32_t i = 0; i < postinglen; ++i) {
			const uint64_t offset = postings[i].key;
			for (int i = 0; i < 8; ++i) fputc(offset & (0x00FFull << (i*8)), file);
		}
	}
}

// TODO: void index_load(Index *index, FILE *file)


void index_ngram(Index *index, NGram ngram, uint64_t path_offset)
{
	Posting *postings = stbds_hmget(index->postings, ngram);
	Posting posting = { path_offset };
	stbds_hmputs(postings, posting);
	stbds_hmput(index->postings, ngram, postings);
}

int64_t index_file(Index* index, const char *filepath)
{
	FILE *file = fopen(filepath, "r");
	if (!file) return -errno;

	int64_t ngram_count = 0;

	// append filepath + null terminator to index
	const size_t path_length = strlen(filepath);
	const size_t path_offset = stbds_arraddnindex(index->paths, path_length + 1);
	memcpy(&index->paths[path_offset], filepath, path_length);
	index->paths[path_offset + path_length] = '\0';
	// TODO: compress common filepath prefixes (and dedup)

	NGram ngram = {0};
	char buffer[4096];

	// read first ngram
	if (!fread(buffer, sizeof(ngram), 1, file)) {
		goto file_done;
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

file_done:
	fclose(file);
	return ngram_count;
}

static int64_t index_dir_rec(Index *index, char **pathbufp)
{
	char *pathbuf = *pathbufp;

	DIR *dir = opendir(pathbuf);
	if (!dir) return -errno;

	int64_t file_count = 0;

	LOG_INFO("Indexing directory '%s' ...", pathbuf);
	logger.indent += 1;
	errno = 0;
	for (struct dirent *entry = NULL; (entry = readdir(dir)); errno = 0) {
		const char *basename = entry->d_name;
		if (strcmp(basename, ".") == 0) continue;
		if (strcmp(basename, "..") == 0) continue;

		// assemble null-terminated path
		const size_t oldlen = stbds_arrlen(pathbuf);
		pathbuf[oldlen - 1] = '/'; // <- no longer null-terminated
		const size_t basename_length = strlen(basename);
		const size_t basename_offset = stbds_arraddnindex(pathbuf, basename_length);
		memcpy(&pathbuf[basename_offset], basename, basename_length);
		stbds_arrput(pathbuf, '\0'); // <- OK, back to null-terminated

		// TODO: handle cycles induced by symlinks
		struct stat fstat = {0};
		if (stat(pathbuf, &fstat) != 0) {
			LOG_ERROR("Could not stat file at '%s' (errno = %d)", pathbuf, errno);
		} else if (S_ISDIR(fstat.st_mode)) {
			const int64_t result = index_dir_rec(index, &pathbuf);
			if (result < 0) {
				LOG_ERROR("Failed to index directory at '%s' (errno = %d)", pathbuf, (int)-result);
			} else {
				file_count += result;
			}
		} else {
			const int64_t result = index_file(index, pathbuf);
			if (result < 0) {
				LOG_ERROR("Failed to index file at '%s' (errno = %d)", pathbuf, (int)-result);
			} else {
				++file_count;
				LOG_INFO("Indexed file '%s'", pathbuf);
			}
		}

		// restore old dir path
		stbds_arrsetlen(pathbuf, oldlen);
		pathbuf[oldlen - 1] = '\0';
	}
	if (errno) LOG_ERROR("Error while reading directory '%s' (errno = %d)", pathbuf, errno);
	logger.indent -= 1;

	closedir(dir);
	*pathbufp = pathbuf;
	return file_count;
}

int64_t index_dir(Index *index, const char *dirpath)
{
	// we'll use a single buffer to build full paths, pushing and popping
	// suffixes like in a stack, and starting with the root directory
	// (just note that this buffer is not null-terminated)
	char *pathbuf = NULL;

	const size_t dirpath_length = strlen(dirpath);
	const size_t dirpath_offset = stbds_arraddnindex(pathbuf, dirpath_length);
	memcpy(&pathbuf[dirpath_offset], dirpath, dirpath_length);
	while (pathbuf[stbds_arrlen(pathbuf) - 1] == '/') {
		stbds_arrpop(pathbuf);
	}
	stbds_arrpush(pathbuf, '\0');

	const int64_t fcount = index_dir_rec(index, &pathbuf);

	stbds_arrfree(pathbuf);
	return fcount;
}
