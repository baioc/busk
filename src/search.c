#include "index.h"
#define LOG_NAME "busk.search"
#include "log.h"

#include <argp.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdio.h>


typedef struct {
	const char *query;
	bool verbose;
	const char *index_input_path;
} Config;

const char *argp_program_version = "0.1.0";

static const char cli_doc[] = "Query a text search index.";

static const char cli_args_doc[] = "'<QUERY STRING>'";

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
			if (state->arg_num > 1) argp_usage(state); // too many args
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
	argp_parse(&cli, argc, argv, 0, NULL, &cfg);

	if (cfg.verbose) logger.level = LOG_LEVEL_DEBUG;

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

	// TODO: text search
	// - break up query text into ngrams
	// - fetch postings for each ngram using index
	// - intersect postings
	// - open files and confirm query present in each

	index_cleanup(&index);
	return 0;
}
