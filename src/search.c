#include "index.h"
#define LOG_NAME "busk.search"
#include "log.h"
#include "version.h"

#include <argp.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stb/stb_ds.h> // hm* and arr* macros

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdint.h>
#include <stdio.h>
#include <string.h> // strlen
#include <limits.h> // LINE_MAX


// TODO: read from cmdline option instead
#ifndef SEARCH_LINE_MAX
#define SEARCH_LINE_MAX LINE_MAX
#endif


typedef struct {
	const char *query;
	bool verbose;
	const char *index_input_path;
	bool color;
} Config;

static const char cli_doc[] = "Query an index and search its backing files for a given string.";

static const char cli_args_doc[] = "\"<SEARCH STRING>\"";

static const struct argp_option cli_options[] = {
	{
		.name="verbose", .key='v',
		.doc="Print more verbose output to stderr",
	},
	{
		.name="index", .key='i', .arg="INPUT",
		.doc="Read index file from INPUT instead of stdin",
	},
	{
		.name="color", .key='c',
		.doc="Add terminal colors to search results",
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

		case 'c':
			cfg->color = true;
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


static void print_char_escaped(char c)
{
	if (c == '\\') fprintf(stdout, "\\\\");                                 // \ is escape char
	else if ((c >= ' ' && c <= '~') || c == '\t') fprintf(stdout, "%c", c); // printed as-is
	else if (c == '\n') fprintf(stdout, "\\n");                             // newline = \n
	else fprintf(stdout, "\\x%02X", c);                                     // otherwise, hexcode
}

static void print_match(
	const char *buffer, size_t buflen,
	size_t begin, size_t end,
	const char *filepath, size_t pathlen, size_t fileoffset,
	bool color
) {
	assert(end <= buflen);

	// <path>:<byteoffset>+<matchlen>:
	const char *color_default = color ? "\033[0m" : "";
	const char *color_match = color ? "\33[01;31m" : "";
	const char *color_path = color ? "\33[35m" : "";
	const char *color_byte = color ? "\33[32m" : "";
	const char *color_sep = color ? "\33[36m" : "";
	const size_t matchlen = end - begin;
	fprintf(
		stdout,
		"%s%.*s%s:%s%zu%s+%s%zu%s: %s",
		color_path, (int)pathlen, filepath, color_sep,
		color_byte, fileoffset + begin, color_default,
		color_byte, matchlen, color_sep,
		color_default
	);

	// walk backwards until we find a newline, null or the buffer limit
	size_t bol = 0;
	if (begin > 0) {
		for (size_t i = begin - 1; i > 0; --i) {
			const char c = buffer[i];
			if (c == '\n' || c == '\0') {
				bol = i + 1;
				break;
			}
		}
	}

	// and similarly to find the end of line
	size_t eol = end;
	for (size_t i = end; i < buflen; ++i) {
		const char c = buffer[i];
		if (c == '\n' || c == '\0') {
			eol = i;
			break;
		}
	}

	// now print the line, making sure to escape non-ASCII characters
	for (size_t i = bol; i < begin; ++i) print_char_escaped(buffer[i]);
	fprintf(stdout, "%s", color_match);
	for (size_t i = begin; i < end; ++i) print_char_escaped(buffer[i]);
	fprintf(stdout, "%s", color_default);
	for (size_t i = end; i < eol; ++i) print_char_escaped(buffer[i]);
	fprintf(stdout, "\n");
}

static int grep(pcre2_code *re, FILE *file, const char *filepath, size_t pathlen, bool color)
{
	int hitcount = 0;

	char buffer[SEARCH_LINE_MAX];
	const size_t buflen = sizeof(buffer);

	pcre2_match_data *match = pcre2_match_data_create_from_pattern(re, NULL);
	if (!match) LOG_FATAL("Failed to allocate match data for this query");

	size_t file_offset = 0;
	for (size_t read_bytes = 0; (read_bytes = fread(buffer, 1, buflen, file)) > 0; file_offset += read_bytes) {
		PCRE2_SIZE match_offset = 0;

	next_match:
		LOG_TRACEF("Grepping %.*s at offset %zu", (int)pathlen, filepath, file_offset + match_offset);
		int rc = pcre2_match(
			re,
			(unsigned char *)buffer, read_bytes,
			match_offset,
			PCRE2_NOTEMPTY,
			match,
			NULL
		);

		// TODO: what about a partial match at the end of the buffer?
		if (rc < 0) { // no match
			continue;
		} else if (rc == 0) {
			LOG_FATAL("Failed to allocate sufficient offsets in match data");
		} else {
			assert(rc > 0);
			++hitcount;
			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match);
			const PCRE2_SIZE match_begin = ovector[0];
			const PCRE2_SIZE match_end = ovector[1];
			print_match(
				buffer, read_bytes,
				match_begin, match_end,
				filepath, pathlen, file_offset,
				color
			);
			assert(match_end > match_offset);
			match_offset = match_end;
			if (match_offset < read_bytes) goto next_match;
		}
	}

	pcre2_match_data_free(match);

	return hitcount;
}


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
			"Query string '%s' is too short, need at least %zu characters",
			query, ngram_size
		);
	}

	// TODO: set context parameters for security
	int errorcode = 0;
	PCRE2_SIZE error_offset = 0;
	pcre2_code *re = pcre2_compile(
		(unsigned char *)query, query_len,
		PCRE2_LITERAL,
		&errorcode, &error_offset,
		NULL
	);
	if (!re) {
		PCRE2_UCHAR buffer[256];
		pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
		LOG_FATALF("Invalid query string '%s': %s", query, buffer);
	}

	struct Index index = {0};
	{
		const char* inpath = cfg.index_input_path;
		FILE *infile = NULL;
		if (!inpath) {
			infile = stdin;
			inpath = "*stdin*";
		} else {
			infile = fopen(inpath, "r");
			if (!infile) LOG_FATALF("Failed to open index file at '%s' (errno = %d)", inpath, errno);
		}

		const int load_error = index_load(&index, infile);
		if (load_error) LOG_FATALF("Failed to parse index from input (errno = %d)", load_error);
		LOG_DEBUGF("Index loaded from %s", inpath);

		fclose(infile);
	}

	typedef struct {
		struct IndexPathHandle key; // handle from query result
		bool value; // whether handle is actually in the intersection
	} IntersectionResult;
	IntersectionResult *intersection_hm = NULL;
	size_t intersection_len = 0;
	bool first = true;

	LOG_DEBUGF("Querying index for string \"%s\"", query);
	assert(query_len >= ngram_size);
	for (size_t i = 0; i <= query_len - ngram_size; ++i) {
		const char *ngram_base = &query[i];
		const size_t remaining_len = query_len - i;
		const struct IndexQuery query = { .text = ngram_base, .strlen = remaining_len };
		const struct IndexResult result = index_query(index, query);

		IntersectionResult *result_set = NULL;
		for (size_t j = 0; j < result.length; ++j) {
			struct IndexPathHandle handle = result.handles[j];
			stbds_hmput(result_set, handle, true);
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
			char tracebuf[4096];
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
			PARTIAL_TRACEF("' files=%zu intersection=%zu", result.length, intersection_len);
			LOG_TRACEF("%s", tracebuf);

			#undef PARTIAL_TRACEF
		}

		// TODO: hard remove elements, then check zero length for early stop
		// TODO: optimize n-way intersection by starting with the smallest set
	}

	bool has_hits = false;

	LOG_DEBUGF("Got %zu candidate files from ngram index", intersection_len);
	{
		char *pathbuf = NULL;
		for (size_t j = 0; j < stbds_hmlenu(intersection_hm); ++j) {
			// extract path from index
			IntersectionResult entry = intersection_hm[j];
			if (!entry.value) continue;
			const struct IndexPathHandle handle = entry.key;
			const size_t pathlen = index_pathlen(index, handle);
			stbds_arrsetlen(pathbuf, pathlen + 1);
			const size_t pathlen2 = index_path(index, handle, pathbuf, pathlen + 1);
			assert(pathlen2 == pathlen);

			// open & grep each file
			FILE *grepfile = fopen(pathbuf, "r");
			if (!grepfile) {
				LOG_ERRORF("Failed to open indexed file at '%s' (errno = %d)", pathbuf, errno);
				continue;
			}
			LOG_DEBUGF("Searching '%s' ...", pathbuf);
			int hits = grep(re, grepfile, pathbuf, pathlen2, cfg.color);
			if (hits > 0) has_hits = true;
			fclose(grepfile);
		}
		stbds_arrfree(pathbuf);
	}

	stbds_hmfree(intersection_hm);
	index_cleanup(&index);
	pcre2_code_free(re);

	return has_hits ? 0 : 1;
}
