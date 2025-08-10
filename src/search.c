#include "index.h"
#define LOG_NAME "busk.search"
#include "log.h"
#include "version.h"

#include <argp.h>
#include <stb/stb_ds.h> // hm* and arr* macros

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdint.h>
#include <stdio.h>
#include <string.h> // strlen


typedef struct {
	const char *query;
	bool verbose;
	const char *index_input_path;
} Config;

static const char cli_doc[] = "Query an index, listing files to grep a search string in.";

static const char cli_args_doc[] = "\"<SEARCH STRING>\"";

static const struct argp_option cli_options[] = {
	{
		.name="verbose", .key='v',
		.doc="Print more verbose output to stderr",
	},
	{
		.name="index", .key='i', .arg="INPUT",
		.doc="Read index from INPUT instead of stdin",
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

		case 'i':
			cfg->index_input_path = arg;
			break;

		case ARGP_KEY_ARG:
			cfg->query = arg;
			break;

		case ARGP_KEY_END:
			if (state->arg_num != 1) argp_usage(state);
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


int main(int argc, char *argv[])
{
	Config cfg = {0};
	argp_program_version = VERSION_STRING;
	argp_parse(&cli, argc, argv, 0, NULL, &cfg);

	if (cfg.verbose) logger.level = LOG_LEVEL_TRACE;
	const char *query = cfg.query;
	const size_t query_len = strlen(query);

	const size_t ngram_size = index_ngram_size();
	if (query_len < ngram_size) {
		LOG_FATALF(
			"Query string '%s' is too short, need at least %lu characters",
			query, ngram_size
		);
	}
	LOG_DEBUGF("Processing query string \"%s\"", query);

	struct Index index = {0};
	{
		const char* inpath = cfg.index_input_path;
		FILE *infile = NULL;
		if (!inpath) {
			infile = stdin;
		} else {
			infile = fopen(inpath, "r");
			if (!infile)
				LOG_FATALF("Failed to open index file at '%s' (errno = %d)", inpath, errno);
		}

		LOG_DEBUGF("Loading index from '%s' ...", inpath);
		int load_error = index_load(&index, infile);
		if (load_error) {
			LOG_FATALF("Failed to load index from '%s' (errno = %d)", inpath, load_error);
		}
		LOG_DEBUGF("Index loaded from '%s'", inpath);

		fclose(infile);
	}

	typedef struct {
		uint64_t key; // offset from query result
		bool value; // whether offset is in intersection
	} IntersectionResult;
	IntersectionResult *intersection_hm = NULL;
	size_t intersection_len = 0;
	bool first = true;

	LOG_DEBUG("Querying index...");
	assert(query_len >= ngram_size);
	for (size_t i = 0; i <= query_len - ngram_size; ++i) {
		const char *ngram_base = &query[i];
		const size_t remaining_len = query_len - i;
		const struct IndexQuery query = { .text = ngram_base, .strlen = remaining_len };
		const struct IndexResult result = index_query(index, query);

		IntersectionResult *result_set = NULL;
		for (size_t j = 0; j < result.length; ++j) {
			uint64_t offset = result.offsets[j];
			stbds_hmput(result_set, offset, true);
		}

		if (first) { // populate initial set of results
			intersection_hm = result_set;
			intersection_len = stbds_hmlenu(result_set);
			first = false;
		} else { // soft-remove elements not in intersection
			for (size_t j = 0; j < stbds_hmlenu(intersection_hm); ++j) {
				const IntersectionResult entry = intersection_hm[j];
				if (!entry.value) continue;
				const IntersectionResult *found = stbds_hmgetp_null(result_set, entry.key);
				if (!found) {
					stbds_hmput(intersection_hm, entry.key, false);
					--intersection_len;
				}
			}
			stbds_hmfree(result_set);
		}

		if (logger.level <= LOG_LEVEL_TRACE) {
			static char tracebuf[4096];
			size_t tracelen = 0;
			tracebuf[sizeof(tracebuf) - 1] = '\0';

			#define PARTIAL_TRACEF(...) do { \
				if (tracelen < sizeof(tracebuf) - 1) { \
					const size_t remaining = sizeof(tracebuf) - 1 - tracelen; \
					const size_t written = snprintf(&tracebuf[tracelen], remaining, __VA_ARGS__); \
					tracelen += written; \
				} \
			} while (0)

			PARTIAL_TRACEF("Processing ngram='");
			for (size_t i = 0; i < ngram_size; ++i) {
				const char c = ngram_base[i];
				if (c == '\\' || c == '\'') PARTIAL_TRACEF("\\%c", c);
				else if (c >= ' ' && c <= '~') PARTIAL_TRACEF("%c", c);
				else PARTIAL_TRACEF("\\x%02X", c);
			}
			PARTIAL_TRACEF("' files=%lu intersection=%lu", result.length, intersection_len);
			LOG_TRACEF("%s", tracebuf);

			#undef PARTIAL_TRACEF
		}

		// TODO: hard remove elements, then check zero length for early stop
		// TODO: optimize n-way intersection by starting with the smallest set
	}

	LOG_DEBUGF("Got %lu candidate files from ngram index", intersection_len);
	{
		FILE *outfile = stdout;
		char *pathbuf = NULL;
		for (size_t j = 0; j < stbds_hmlenu(intersection_hm); ++j) {
			IntersectionResult entry = intersection_hm[j];
			if (!entry.value) continue;
			const uint64_t offset = entry.key;
			const size_t pathlen = index_pathlen(index, offset);
			stbds_arrsetlen(pathbuf, pathlen + 1);
			const size_t buflen = stbds_arrlenu(pathbuf);
			assert(buflen == pathlen + 1);
			const size_t reallen = index_path(index, offset, pathbuf, buflen);
			assert(reallen == pathlen);
			fprintf(outfile, "%.*s\n", (int)reallen, pathbuf);
		}
		stbds_arrfree(pathbuf);
	}

	// TODO: open files and confirm query present in each

	stbds_hmfree(intersection_hm);
	index_cleanup(&index);
	return 0;
}
