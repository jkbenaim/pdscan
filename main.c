#define _GNU_SOURCE
#include <err.h>
#include <inttypes.h>
#include <iso646.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "escape.h"
#include "mapfile.h"
#include "pdscan.h"
#include "version.h"

extern char *__progname;
static void vtryhelp(const char *fmt, va_list args);
static void tryhelp(const char *fmt, ...);
static void usage(void);

int main(int argc, char *argv[])
{
	char *filename = NULL;
	int rc;
	bool is_json = false;
	
	while ((rc = getopt(argc, argv, "f:jV")) != -1)
		switch (rc) {
		case 'f':
			if (filename)
				usage();
			filename = optarg;
			break;
		case 'j':
			if (is_json)
				usage();
			is_json = true;
			break;
		case 'V':
			fprintf(stderr, "%s\n", PROG_EMBLEM);
			exit(EXIT_SUCCESS);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv != NULL) {
		filename = *argv;
	} else {
		tryhelp("must specify product file");
	}

	struct MappedFile_s m = MappedFile_Open(filename, false);
	if (!m.data) err(1, "couldn't open file '%s' for reading", filename);

	char *out = NULL;
	size_t out_len = 0;

	rc = pd_analyze(m.data, m.size, &out, &out_len, is_json);

	if (!rc && out) {
		fwrite(out, out_len, 1, stdout);
	} else {
		printf("error parsing pdfile\n");
	}
	free(out);

	MappedFile_Close(m);
	m.data = NULL;
	return EXIT_SUCCESS;
}

static void vtryhelp(const char *fmt, va_list args)
{
	if (fmt) {
		fprintf(stderr, "%s: ", __progname);
		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "Try `%s -h' for more information.\n", __progname);
	exit(EXIT_FAILURE);
}

static void tryhelp(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vtryhelp(fmt, ap);
	va_end(ap);
}

static void usage(void)
{
	(void)fprintf(stderr,
"Usage: %s [OPTION] FILE\n"
"Describe the IRIX software package given its product description file.\n"
"\n"
"  -h       print this help text\n"
"  -j       output in JSON format\n"
"  -V       print program version\n"
"\n"
"Please report any bugs to <jkbenaim@gmail.com>.\n"
,		__progname
	);
	exit(EXIT_FAILURE);
}
