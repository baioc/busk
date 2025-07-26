#include "index.c"

#include <argp.h>
#include <stb/stb_ds.h> // arr* macros

#include <errno.h>
#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdio.h>
#include <stdlib.h> // exit


#define FATAL(exitcode, format, ...) do { \
	fprintf(stderr, "FATAL: " format "\n", __VA_ARGS__); \
	exit((exitcode)); \
} while (0)

#define ERROR(format, ...) \
	fprintf(stderr, "ERROR: " format "\n", __VA_ARGS__)

#define INFO(format, ...) \
	fprintf(stderr, "INFO: " format "\n", __VA_ARGS__)


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

const char cli_doc[] = "Generate a text search index from the given DIRS.";

const char cli_args_doc[] = "DIRS...";

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
			if (state->arg_num < 1) argp_usage(state); // not enough args
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
	int retcode = 0;

	Config cfg = { .index_output_path = "-" };
	argp_parse(&cli, argc, argv, 0, NULL, &cfg);

	FILE *outfile = NULL;
	if (strcmp(cfg.index_output_path, "-") == 0) {
		outfile = stdout;
	} else {
	 	outfile = fopen(cfg.index_output_path, "w+");
		if (!outfile) {
			FATAL(
				errno, "Could not open file at '%s' for writing the generated index",
				cfg.index_output_path
			);
		}
	}

	Index index = {0};
	for (int i = 0; i < stbds_arrlen(cfg.corpus_root_dirs); ++i) {
		index_dir(&index, cfg.corpus_root_dirs[i]);
	}
	index_print(outfile, index);

	index_cleanup(&index);
	fclose(outfile);
	config_cleanup(&cfg);
	return retcode;
}
