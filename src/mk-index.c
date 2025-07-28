#include "index.c"

#undef LOG_NAME
#define LOG_NAME "search.mk-index"
#include "log.h"

#include <argp.h>
#include <stb/stb_ds.h> // arr* macros

#include <stdbool.h>
#include <stddef.h> // NULL
#include <stdio.h> // fopen
#include <string.h> // strcmp


typedef struct {
	const char **corpus_root_dirs;
	const char *index_output_path;
	bool verbose;
} Config;

static void config_cleanup(Config *cfg)
{
	stbds_arrfree(cfg->corpus_root_dirs);
}


const char *argp_program_version = "0.0.0";

static const char cli_doc[] = "Generate a text search index from the given DIRs.";

static const char cli_args_doc[] = "<DIR>...";

static const struct argp_option cli_options[] = {
	{
		.name="verbose", .key='v',
		.doc="Print more verbose output to stderr",
	},
	{
		.name="output", .key='o', .arg="FILE",
		.doc="Output index to FILE instead of stdout",
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
			stbds_arrput(cfg->corpus_root_dirs, arg);
			break;

		case ARGP_KEY_END:
			if (state->arg_num < 1) argp_usage(state); // no DIRs given
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
	int retcode = 0;

	Config cfg = { .index_output_path = "-" };
	argp_parse(&cli, argc, argv, 0, NULL, &cfg);

	if (cfg.verbose) logger.level = LOG_LEVEL_DEBUG;

	FILE *outfile = NULL;
	if (strcmp(cfg.index_output_path, "-") == 0) {
		outfile = stdout;
	} else {
		outfile = fopen(cfg.index_output_path, "w+");
		if (!outfile)
			LOG_FATAL("Could not open output file at '%s'", cfg.index_output_path);
	}

	Index index = {0};
	for (int i = 0; i < stbds_arrlen(cfg.corpus_root_dirs); ++i) {
		const char *dir = cfg.corpus_root_dirs[i];
		index_dir(&index, dir);
	}
	index_save(index, outfile);

	index_cleanup(&index);
	fclose(outfile);
	config_cleanup(&cfg);
	return retcode;
}
