#include "index.c"

#include <argp.h>
#include <stb/stb_ds.h> // arr* macros

#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdio.h>


typedef struct {
	const char **corpus_root_dirs;
	const char *index_output_path;
	bool verbose;
} Config;

void config_cleanup(Config *cfg)
{
	stbds_arrfree(cfg->corpus_root_dirs);
}


const char *argp_program_version = "0.1";

const char cli_doc[] = "Generate a text search index from the given PATHS.";

const char cli_args_doc[] = "PATHS...";

const struct argp_option cli_options[] = {
	{
		.name="output", .key='o', .arg="FILE",
		.doc="Output index to FILE instead of stdout",
	},
	{
		.name="verbose", .key='v',
		.doc="Print more verbose output to stderr",
	},
	{0},
};

error_t cli_parser(int key, char *arg, struct argp_state *state)
{
	Config *cfg = state->input;
	switch (key) {
		case 'o':
			cfg->index_output_path = arg;
			break;

		case 'v':
			cfg->verbose = true;
			break;

		case ARGP_KEY_ARG:
			stbds_arrput(cfg->corpus_root_dirs, arg);
			break;

		case ARGP_KEY_END:
			if (state->arg_num < 1)
				argp_usage(state); // not enough args
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const struct argp cli = {
	.doc = cli_doc,
	.args_doc = cli_args_doc,
	.options = cli_options,
	.parser = cli_parser,
};


int main(int argc, char *argv[])
{
	Config cfg = { .index_output_path = "-" };
	argp_parse(&cli, argc, argv, 0, NULL, &cfg);
	fprintf(stderr, "VERBOSE = %d\n", cfg.verbose);
	fprintf(stderr, "FILE = %s\n", cfg.index_output_path);

	Index index = {0};
	for (int i = 0; i < stbds_arrlen(cfg.corpus_root_dirs); ++i) {
		fprintf(stderr, "PATHS[%d] = %s\n", i, cfg.corpus_root_dirs[i]);
		int ngrams = index_file(&index, cfg.corpus_root_dirs[i]);
		fprintf(stderr, "\tNGRAMS = %d\n", ngrams);
	}
	index_print(stdout, index);
	index_cleanup(&index);

	config_cleanup(&cfg);

	return 0;
}
