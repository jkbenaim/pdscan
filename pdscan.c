#define _GNU_SOURCE
#include <err.h>
#include <inttypes.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hexdump.h"
#include "mapfile.h"

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

extern char *__progname;
static void usage(void);

uint8_t *ptr_src = NULL;
uint8_t *ptr = NULL;

int verbose = 0;

off_t getOffset()
{
	return ptr - ptr_src;
}

void seekMe(off_t offset)
{
	ptr += offset;
}

uint16_t getShort()
{
	uint16_t out = 0;
	out += *ptr++;
	out <<= 8;
	out += *ptr++;
	return out;
}

uint32_t getInt()
{
	uint32_t out = 0;
#if 1
	out = getShort();
	out <<= 16;
	out += getShort();
#else
	out += *ptr++;
	out <<= 8;
	out += *ptr++;
	out <<= 8;
	out += *ptr++;
	out <<= 8;
	out += *ptr++;
#endif
	return out;
}
	
char *getCstring()
{
	char *out = NULL;
	size_t len;
	if (!ptr) errx(1, "called getCstring with ptr=NULL");
	len = strlen((char *)ptr);
	out = malloc(len + 1);
	if (!out) err(1, "in malloc");
	strncpy(out, (char *)ptr, len);
	out[len] = '\0';
	ptr += len;
	ptr++;
	return out;
}

char *getString()
{
	char *out = NULL;
	size_t len = 0;
	len = getShort();
	out = malloc(len + 1);
	if (!out) err(1, "in malloc");
	memcpy(out, ptr, len);
	out[len] = '\0';
	ptr += len;
	for (int i = 0; i < len; i++) {
		if (out[i] == '\x01') {
			out[i] = '^';
		}
	}
	return out;
}

char *getTriplet()
{
	char *a1 = getString();
	char *a2 = getString();
	char *a3 = getString();
	char *triplet = NULL;
	int rc;
	rc = asprintf(&triplet, "%s.%s.%s", a1, a2, a3);
	if (rc == -1) err(1, "in asprintf");
	free(a1);
	free(a2);
	free(a3);
	return triplet;
}

char *getMatcher(const char *prefix)
{
	// a matcher is 3 strings and 2 ints
	char *out = NULL;
	int rc;
	char *triplet = getTriplet();
	int32_t from = getInt();
	int32_t to = getInt();
	const char *my_prefix = "";
	if (!prefix) {
		my_prefix = "";
	} else if (!strcmp(prefix, "replaces ") && (from < 0)) {
		from = -from;
		my_prefix = "follows ";
	} else {
		my_prefix = prefix;
	}
	if (to == 2147483647u) {
		rc = asprintf(&out, "%s'%s' %d maxint", my_prefix, triplet, from);
	} else {
		rc = asprintf(&out, "%s'%s' %d %d", my_prefix, triplet, from, to);
	}
	if (rc == -1) err(1, "in asprintf");
	free(triplet);
	return out;
}

void getRules()
{
	uint16_t rulesCount = getShort();
	printf("\t\trulesCount: %d\n", rulesCount);
	if (rulesCount > 2000) errx(1, "diagnostic abort (too many rules)");
	for (int rule = 0; rule < rulesCount; rule++) {
		char *matcher = getMatcher("replaces ");
		printf("\t\t\t%s\n", matcher);
		free(matcher);
	}
}

void getMachInfo()
{
	// get machine info
	uint32_t machCount = getInt();
	printf("machCount: %d\n", machCount);
	for (int i = 0; i < machCount; i++) {
		char *m = getString();
		printf("\tmach '%s'\n", m);
		free(m);
	}
}

void getPrereqs()
{
	uint16_t prereqSets = getShort();
	printf("\t\tprereq sets: %d\n", prereqSets);
	for (int set = 0; set < prereqSets; set++) {
		uint16_t prereqsCount = getShort();
		printf("\t\tprereqs: %d (\n", prereqsCount);
		for (int a = 0; a < prereqsCount; a++) {
			char *matcher = getMatcher(NULL);
			printf("\t\t\t%s\n", matcher);
			free(matcher);
		}
		printf("\t\t)\n");
	}
}

void getAttrs(const char *prefix, uint16_t flags)
{
	uint32_t attrs = getInt();
	printf("%sattrs: %d\n", prefix, attrs);
	for (int i = 0; i < attrs; i++) {
		char *attr = getString();
		printf("%s\t'%s'\n", prefix, attr);
		free(attr);
	}
}

void getUpdates()
{
	uint16_t updatesCount = getShort();
	printf("\t\tupdatesCount: %d\n", updatesCount);
	for (int i = 0; i < updatesCount; i++) {
		char *matcher = getMatcher("updates ");
		printf("\t\t\t%s\n", matcher);
		free(matcher);
	}
}

int main(int argc, char *argv[])
{
	char *fileName = NULL;
	int rc;
	
	while ((rc = getopt(argc, argv, "f:v")) != -1)
		switch (rc) {
		case 'f':
			if (fileName)
				usage();
			fileName = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (not fileName)
		usage();
	if (*argv != NULL)
		usage();

	struct MappedFile_s m = MappedFile_Open(fileName, false);
	if (!m.data) err(1, "couldn't open file '%s' for reading", fileName);
	ptr = ptr_src = m.data;

	// product
	char *prodId = getCstring();
	printf("prodId: %s\n", prodId);
	uint16_t magic = getShort();
	printf("magic: %04x %s\n", magic, (magic==1988)?"(ok)":"(BAD)");
	uint16_t noOfRoots = getShort();
	printf("noOfRoots: %04x %s\n", noOfRoots, (noOfRoots>=1)?"(ok)":"(BAD)");

	// root
	uint16_t prodMagic = getShort();
	printf("prodMagic: %04x %s\n", prodMagic, (prodMagic==1987)?"(ok)":"(BAD)");
	uint16_t prodFormat = getShort();
	printf("prodFormat: %04x\n", prodFormat);

	char *shortName = getString();
	printf("shortName: '%s'\n", shortName);
	char *longName = getString();
	printf("longName:  '%s'\n", longName);
	uint16_t prodFlags = getShort();
	printf("prodFlags: %04x\n", prodFlags);
	if (prodFormat >= 5) {
		time_t prodDateTime = getInt();
		printf("datetime: %s", ctime(&prodDateTime));
	}

	if (prodFormat >= 5) {
		char *prodIdk = getString();
		printf("prodIdk: '%s'\n", prodIdk);
		free(prodIdk);
	}

	if (prodFormat == 7) {
		getMachInfo();
	}

	if (prodFormat >= 8) {
		getAttrs("", 0);
	}

	uint16_t imageCount = getShort();
	printf("imageCount: %04x\n", imageCount);

	for (int image = 0; image < imageCount; image++) {
		printf("image #%d:\n", image);
		uint16_t imageFlags = getShort();
		printf("\timageFlags: %04x\n", imageFlags);
		char *imageName = getString();
		printf("\timageName: '%s'\n", imageName);
		char *imageId = getString();
		printf("\timageId: '%s'\n", imageId);
		uint16_t imageFormat = getShort();
		printf("\timageFormat: %04x\n", imageFormat);

		uint16_t imageOrder = 0;
		if (prodFormat >= 5) {
			imageOrder = getShort();
		}
		printf("\timageOrder: %04x (%u)\n", imageOrder, imageOrder);

		uint32_t imageVersion = getInt();
		printf("\timageVersion: %u\n", imageVersion);

		if (prodFormat == 5) {
			uint32_t a = getInt();
			uint32_t b = getInt();
			if (a || b) {
				printf("a: %08x\n", a);
				printf("b: %08x\n", b);
				errx(1, "diagnostic abort (has a or b)");
			}
		}

		char *derivedFrom = getString();
		if (strlen(derivedFrom)) {
			printf("\tderivedFrom: '%s'\n", derivedFrom);
		}
		free(derivedFrom);
		if (prodFormat >= 8) {
			getAttrs("\t", imageFlags);
		}

		uint16_t subsysCount = getShort();
		printf("\tsubsysCount: %04x\n", subsysCount);

		for(int subsys = 0; subsys < subsysCount; subsys++) {
			printf("\tsubsys #%d:\n", subsys);
			uint16_t subsysFlags = getShort();
			printf("\t\tsubsysFlags: %04x\n", subsysFlags);
			if (subsysFlags & 0x0002) {
				printf("\t\tdefault\n");
			}
			if (subsysFlags & 0x0400) {
				printf("\t\tpatch\n");
			}
			if (~subsysFlags & 0x0800) {
				printf("\t\tminiroot\n");
			}
			if (subsysFlags & 0x8000) {
				printf("\t\toverlays (see 'b' attribute)\n");
			}
			char *subsysName = getString();
			printf("\t\tsubsysName: '%s'\n", subsysName);
			char *subsysId = getString();
			printf("\t\tsubsysId: '%s'\n", subsysId);
			char *subsysExpr = getString();
			printf("\t\tsubsysExpr: '%s'\n", subsysExpr);
			time_t subsysInstallDate = getInt();
			if (subsysInstallDate != 0) {
				printf("\t\tsubsysInstallDate: %s\n", ctime(&subsysInstallDate));
			}

			getRules();
			getPrereqs();
			free(subsysName);
			free(subsysId);
			free(subsysExpr);
			if (prodFormat >= 5) {
				char *altName = getString();
				printf("\t\taltName: '%s'\n", altName);
				free(altName);
			}
			if (prodFormat >= 6) {
				printf("\t\tincompats\n");
				getRules();
			}
			if (prodFormat >= 8) {
				getAttrs("\t\t", subsysFlags);
			}
			if (prodFormat >= 9) {
				getUpdates();
			}
		}

		free(imageName);
		free(imageId);
	}

	free(shortName);
	free(longName);
	free(prodId);

	MappedFile_Close(m);
	m.data = NULL;
	return EXIT_SUCCESS;
}

static void usage(void)
{
	(void)fprintf(stderr, "usage: %s -f file\n",
		__progname
	);
	exit(EXIT_FAILURE);
}
