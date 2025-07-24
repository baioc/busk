#include <stdlib.h> // EXIT_*, NULL
#include <stdbool.h>
#include <stdio.h>

#include <argp.h>
#include <stb/stb_ds.h> // arr* macros


struct config {
	const char **corpus_paths;
	const char *index_path;
	bool verbose;
};


const char *argp_program_version = "0.1";

static const char cli_doc[] = "Generate a text search index from the given PATHS.";

static const char cli_args_doc[] = "PATHS...";

static const struct argp_option cli_options[] = {
	{
		.name="output", .key='o', .arg="FILE",
		.doc="Output index to FILE instead of stdout",
	},
	{
		.name="verbose", .key='v',
		.doc="Print more verbose output to stderr",
	},
	{ 0 },
};

static error_t cli_parser(int key, char *arg, struct argp_state *state)
{
	struct config *cfg = state->input;
	switch (key) {
		case 'o':
			cfg->index_path = arg;
			break;

		case 'v':
			cfg->verbose = true;
			break;

		case ARGP_KEY_ARG:
			arrput(cfg->corpus_paths, arg);
			break;

		case ARGP_KEY_END:
			if (state->arg_num < 1) argp_usage(state); // not enough args
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
	struct config cfg = { .index_path = "-" };

	argp_parse(&cli, argc, argv, 0, NULL, &cfg);

	fprintf(stderr, "VERBOSE = %d\n", cfg.verbose);
	fprintf(stderr, "FILE = %s\n", cfg.index_path);
	for (int i = 0; i < arrlen(cfg.corpus_paths); ++i) {
		fprintf(stderr, "PATHS[%d] = %s\n", i, cfg.corpus_paths[i]);
	}

	arrfree(cfg.corpus_paths);

	return EXIT_SUCCESS;
}
