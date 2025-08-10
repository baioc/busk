#include "index.h"
#define LOG_NAME "busk.mk-index"
#include "log.h"
#include "version.h"

#include <argp.h>
#include <dirent.h>
#include <stb/stb_ds.h> // arr* macros
#include <sys/stat.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdint.h>
#include <stdio.h> // fopen
#include <string.h> // strcmp, strlen, memcpy


typedef struct {
	const char **corpus_paths;
	bool verbose;
	const char *index_output_path;
} Config;

static void config_cleanup(Config *cfg)
{
	stbds_arrfree(cfg->corpus_paths);
}

static const char cli_doc[] = "Generate a text search index from the given files and/or directories.";

static const char cli_args_doc[] = "<FILE/DIR>...";

static const struct argp_option cli_options[] = {
	{
		.name="verbose", .key='v',
		.doc="Print more verbose output to stderr",
	},
	{
		.name="output", .key='o', .arg="OUTPUT",
		.doc="Output index to OUTPUT instead of stdout",
	},
	{0},
};

static error_t cli_parser(int key, char *arg, struct argp_state *state)
{
	Config *cfg = state->input;
	switch (key) {
		case 'v':
			cfg->verbose = true;
			break;

		case 'o':
			cfg->index_output_path = arg;
			break;

		case ARGP_KEY_ARG:
			stbds_arrput(cfg->corpus_paths, arg);
			break;

		case ARGP_KEY_END:
			if (state->arg_num < 1) argp_usage(state);
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static const struct argp cli = {
	.doc = cli_doc,
	.args_doc = cli_args_doc,
	.options = cli_options,
	.parser = cli_parser,
};


static int64_t index_dir_rec(struct Index *index, char **pathbufp)
{
	char *pathbuf = *pathbufp;

	DIR *dir = opendir(pathbuf);
	if (!dir) {
		const int error = errno;
		LOG_ERRORF("Failed to open directory at '%s' (errno = %d)", pathbuf, error);
		return -error;
	}

	int64_t file_count = 0;

	const enum LogLevel level = logger.level;
	if (level <= LOG_LEVEL_DEBUG) {
		LOG_DEBUGF("Indexing directory '%s' ...", pathbuf);
		++logger.indent;
	}
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

		struct stat fstat = {0};
		if (stat(pathbuf, &fstat) != 0) {
			LOG_ERRORF("Failed to stat file/dir at '%s' (errno = %d)", pathbuf, errno);
		} else if (S_ISDIR(fstat.st_mode)) {
			const int64_t result = index_dir_rec(index, &pathbuf);
			if (result >= 0) file_count += result;
		} else {
			FILE *file = fopen(pathbuf, "r");
			if (!file) {
				LOG_ERRORF("Failed to open file at '%s' (errno = %d)", pathbuf, errno);
			} else {
				const int64_t ngrams = index_file(index, file, pathbuf);
				++file_count;
				LOG_DEBUGF("Indexed file '%s' (%ld ngrams processed)", pathbuf, ngrams);
			}
		}

		// restore old dir path
		stbds_arrsetlen(pathbuf, oldlen);
		pathbuf[oldlen - 1] = '\0';
	}
	if (errno) LOG_ERRORF("Error while reading directory '%s' (errno = %d)", pathbuf, errno);
	if (level <= LOG_LEVEL_DEBUG) {
		--logger.indent;
		LOG_DEBUGF("Indexed directory '%s' (%ld files processed)", pathbuf, file_count);
	}

	closedir(dir);
	*pathbufp = pathbuf;
	return file_count;
}

static int64_t index_dir(struct Index *index, const char *dirpath)
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


int main(int argc, char *argv[])
{
	int retcode = 0;

	Config cfg = {0};
	argp_program_version = VERSION_STRING;
	argp_parse(&cli, argc, argv, 0, NULL, &cfg);

	if (cfg.verbose) logger.level = LOG_LEVEL_DEBUG;
	const char *outpath = cfg.index_output_path;

	FILE *outfile = NULL;
	if (!outpath) {
		outfile = stdout;
		outpath = "*stdout*";
	} else {
		outfile = fopen(outpath, "w+");
		if (!outfile)
			LOG_FATALF("Failed to open output file at '%s' (errno = %d)", outpath, errno);
	}

	struct Index index = {0};
	uint64_t files_indexed = 0;
	for (int i = 0; i < stbds_arrlen(cfg.corpus_paths); ++i) {
		const char *path = cfg.corpus_paths[i];
		struct stat fstat = {0};
		if (stat(path, &fstat) != 0) {
			LOG_ERRORF("Failed to stat file/dir at '%s' (errno = %d)", path, errno);
		} else if (S_ISDIR(fstat.st_mode)) {
			const int64_t result = index_dir(&index, path);
			if (result >= 0) files_indexed += result;
		} else {
			FILE *file = fopen(path, "r");
			if (!file) {
				LOG_ERRORF("Failed to open file at '%s' (errno = %d)", path, errno);
			} else {
				const int64_t ngrams = index_file(&index, file, path);
				++files_indexed;
				LOG_DEBUGF("Indexed file '%s' (%ld ngrams processed)", path, ngrams);
			}
		}
	}
	LOG_INFOF("Successfully indexed the contents of %ld files", files_indexed);

	const int64_t written = index_save(index, outfile);
	if (written < 0) LOG_FATALF("Failed to write index to output (errno = %ld)", written);
	LOG_INFOF("Search index saved to %s", outpath);

	index_cleanup(&index);
	fclose(outfile);
	config_cleanup(&cfg);
	return retcode;
}
