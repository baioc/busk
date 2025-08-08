#include "index.h"
#define LOG_NAME "busk.search"
#include "log.h"

#include <errno.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
	const char* index_path = argv[1];
	FILE *index_file = fopen(index_path, "r");
	if (!index_file) {
		LOG_FATALF(
			"Failed to open index file at '%s' (errno = %d)",
			index_path, errno
		);
	}

	struct Index index = {0};
	LOG_INFOF("Loading index from '%s' ...", index_path);
	int load_error = index_load(&index, index_file);
	if (load_error) {
		LOG_FATALF(
			"Failed to load index from '%s' (errno = %d)",
			index_path, load_error
		);
	}
	LOG_INFOF("Index loaded from '%s'", index_path);

	const char* check_path = argv[2];
	FILE *check_file = fopen(check_path, "w+");
	if (!check_file) LOG_FATALF("Failed to open output file at '%s'", check_path);
	index_save(index, check_file);
	fclose(check_file);

	index_cleanup(&index);
	fclose(index_file);
	return 0;
}
