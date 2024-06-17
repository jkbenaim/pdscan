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
#include "version.h"

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

extern char *__progname;
static void vtryhelp(const char *fmt, va_list args);
static void tryhelp(const char *fmt, ...);
static void usage(void);

uint8_t *ptr_src = NULL;
uint8_t *ptr = NULL;

int is_json = 0;

off_t getOffset()
{
	return ptr - ptr_src;
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
	out = getShort();
	out <<= 16;
	out += getShort();
	return out;
}
	
char *getCstring()
{
	char *out = NULL;
	char *out2 = NULL;
	size_t len;
	if (!ptr) errx(1, "called getCstring with ptr=NULL");
	len = strlen((char *)ptr);
	out = malloc(len + 1);
	if (!out) err(1, "in malloc");
	strncpy(out, (char *)ptr, len);
	out[len] = '\0';
	ptr += len;
	ptr++;
	out2 = escape_json(out);
	free(out);
	return out2;
}

char *getString()
{
	char *out = NULL;
	char *out2 = NULL;
	size_t len = 0;
	len = getShort();
	out = malloc(len + 1);
	if (!out) err(1, "in malloc");
	memcpy(out, ptr, len);
	out[len] = '\0';
	ptr += len;
	for (size_t i = 0; i < len; i++) {
		if (!is_json) {
			if (out[i] == '\x01') {
				out[i] = '^';
			}
		}
	}
	out2 = escape_json(out);
	if (!out2)
		err(1, "in escape_json");
	//fprintf(stdout, "'%s'\n", out2);
	free(out);
	return out2;
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
	const char *my_prefix;
	if (!prefix) {
		my_prefix = "";
	} else if (!strcmp(prefix, "replaces ") && (from < 0)) {
		from = -from;
		my_prefix = "follows ";
	} else {
		my_prefix = prefix;
	}
	if (!is_json) {
		if (to == 2147483647u) {
			rc = asprintf(&out, "%s'%s' %d maxint", my_prefix, triplet, from);
		} else {
			rc = asprintf(&out, "%s'%s' %d %d", my_prefix, triplet, from, to);
		}
	} else {
		if (prefix) {
			rc = asprintf(&out, "[\"%s\", %d, %d, \"%s\"]", triplet, from, to, my_prefix);
		} else {
			rc = asprintf(&out, "[\"%s\", %d, %d]", triplet, from, to);
		}
	}
	if (rc == -1) err(1, "in asprintf");
	free(triplet);
	return out;
}

void getRules(const char *label)
{
	uint16_t rulesCount = getShort();
	if (!is_json) {
		printf("\t\trulesCount: %d\n", rulesCount);
	} else {
		if (rulesCount) {
			printf("\t\t\"%s\": [\n", label);
		} else {
			printf("\"%s\": []", label);
			return;
		}
	}
	if (rulesCount > 2000) errx(1, "diagnostic abort (too many rules)");
	for (int rule = 0; rule < rulesCount; rule++) {
		char *matcher;
		if (!is_json) {
			matcher = getMatcher("replaces ");
			printf("\t\t\t%s\n", matcher);
		} else {
			matcher = getMatcher("replaces");
			if (rule == (rulesCount - 1)) {
				printf("\t\t\t%s\n", matcher);
			} else {
				printf("\t\t\t%s,\n", matcher);
			}
		}
		free(matcher);
	}
	if (is_json) {
		printf("\t\t]");
	}
}

void getMachInfo()
{
	// get machine info
	uint32_t machCount = getInt();
	if (!is_json) {
		printf("machCount: %d\n", machCount);
	} else {
		if (machCount) {
			printf("\"mach\": [\n");
		} else {
			printf("\"mach\": [],\n");
			return;
		}
	}
	for (size_t i = 0; i < machCount; i++) {
		char *m = getString();
		if (!is_json) {
			printf("\tmach '%s'\n", m);
		} else {
			if (i == (machCount - 1)) {
				printf("\t\"%s\"\n", m);
			} else {
				printf("\t\"%s\",\n", m);
			}
		}
		free(m);
	}
	if (is_json) {
		printf("],\n");
	}
}

void getPrereqs()
{
	uint16_t prereqSets = getShort();
	if (!is_json) {
		printf("\t\tprereq sets: %d\n", prereqSets);
	} else {
		if (prereqSets) {
			printf("\t\t\"prereqs\": [\n");
		} else {
			printf("\t\t\"prereqs\": []");
			return;
		}
	}
	for (int set = 0; set < prereqSets; set++) {
		uint16_t prereqsCount = getShort();
		if (!is_json) {
			printf("\t\tprereqs: %d (\n", prereqsCount);
		} else {
			printf("\t\t\t[\n");
		}
		for (int a = 0; a < prereqsCount; a++) {
			char *matcher = getMatcher(NULL);
			if (!is_json) {
				printf("\t\t\t%s\n", matcher);
			} else {
				if (a == (prereqsCount - 1)) {
					printf("\t\t\t\t%s%s", matcher, (a == (prereqsCount - 1))?"":",");
				} else {
					printf("\t\t\t\t%s%s\n", matcher, (a == (prereqsCount - 1))?"":",");
				}
			}
			free(matcher);
		}
		if (!is_json) {
			printf("\t\t)\n");
		} else {
			printf("\n\t\t\t]%s\n", (set == (prereqSets - 1))?"":",");
		}
	}
	if (is_json) {
		printf("\t\t]");
	}
}

void getAttrs(const char *prefix)
{
	uint32_t attrs = getInt();
	if (!is_json) {
		printf("%sattrs: %d\n", prefix, attrs);
	} else {
		if (attrs) {
			printf("%s\"attrs\": [\n", prefix);
		} else {
			printf("%s\"attrs\": []", prefix);
			return;
		}
	}
	for (size_t i = 0; i < attrs; i++) {
		char *attr = getString();
		if (!is_json) {
			printf("%s\t'%s'\n", prefix, attr);
		} else {
			if (i == (attrs - 1)) {
				printf("%s\t\"%s\"\n", prefix, attr);
			} else {
				printf("%s\t\"%s\",\n", prefix, attr);
			}
		}
		free(attr);
	}
	if (is_json) {
		printf("%s]", prefix);
	}
}

void getUpdates()
{
	uint16_t updatesCount = getShort();
	if (!is_json) {
		printf("\t\tupdatesCount: %d\n", updatesCount);
	} else {
		if (updatesCount) {
			printf("\t\t\"updates\": [\n");
		} else {
			printf("\t\t\"updates\": []");
			return;
		}
	}
	for (int i = 0; i < updatesCount; i++) {
		char *matcher;
		if (!is_json) {
			matcher = getMatcher("updates ");
		} else {
			matcher = getMatcher(NULL);
		}
		if (!is_json) {
			printf("\t\t\t%s\n", matcher);
		} else {
			if (i == (updatesCount - 1)) {
				printf("\t\t\t%s\n", matcher);
			} else {
				printf("\t\t\t%s,\n", matcher);
			}
		}
		free(matcher);
	}
	if (is_json) {
		printf("\t\t]");
	}
}

int main(int argc, char *argv[])
{
	char *filename = NULL;
	int rc;
	
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
			is_json = 1;
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
	ptr = ptr_src = m.data;

	if (is_json) {
		printf("{\n");
	}

	// product
	char *prodId = NULL;
	if ((ptr[0] == 'p') || (ptr[1] == 'd')) {
                prodId = getCstring();
		if (is_json) {
			printf("\"prodId\": \"%s\",\n", prodId);
		} else {
	                printf("prodId: present, '%s'\n", prodId);
		}
	} else {
		if (!is_json) {
	                printf("prodId: not present\n");
		}
        }

	uint16_t magic = getShort();
	if (!is_json) {
		printf("magic: %04x %s\n", magic, (magic==1988)?"(ok)":"(BAD)");
	} else {
		printf("\"magic\": %u,\n", magic);
	}
        if (magic != 1988) return 1;
	uint16_t noOfProds = getShort();
	if (!is_json) {
		printf("noOfProds: %04x %s\n", noOfProds, (noOfProds>=1)?"(ok)":"(BAD)");
	}

	if (is_json && noOfProds) {
		printf("\"products\": [\n");
	}

	for (unsigned prodNum = 0; prodNum < noOfProds; prodNum++) {

	// root
	uint16_t prodMagic = getShort();
	if (!is_json) {
		printf("prodMagic: %04x %s\n", prodMagic, (prodMagic==1987)?"(ok)":"(BAD)");
	} else {
		printf("{\n");
	}
        if (prodMagic != 1987) return 1;
	uint16_t prodFormat = getShort();
	if (is_json) {
		printf("\"prodFormat\": %u,\n", prodFormat);
	} else {
		printf("prodFormat: %04x\n", prodFormat);
	}
	switch (prodFormat) {
	case 5 ... 9:
		break;
	default:
		errx(1, "bad prodFormat: %d not between 5 and 9 inclusive", prodFormat);
	}

	char *shortName = getString();
	char *longName = getString();
	uint16_t prodFlags = getShort();
	if (!is_json) {
		printf("shortName: '%s'\n", shortName);
		printf("longName:  '%s'\n", longName);
		printf("prodFlags: %04x\n", prodFlags);
	} else {
		printf("\"shortName\": \"%s\",\n", shortName);
		printf("\"longName\": \"%s\",\n", longName);
		printf("\"prodFlags\": %u,\n", prodFlags);
	}
	if (prodFormat >= 5) {
		time_t prodDateTime = getInt();
		if (!is_json) {
			printf("datetime: %s", ctime(&prodDateTime));
		} else {
			printf("\"datetime\": %lu,\n", prodDateTime);
		}
	}

	if (prodFormat >= 5) {
		char *prodIdk = getString();
		if (!is_json) {
			printf("prodIdk: '%s'\n", prodIdk);
		} else {
			printf("\"prodIdk\": \"%s\",\n", prodIdk);
		}
		free(prodIdk);
	}

	if (prodFormat == 7) {
		getMachInfo();
	}

	if (prodFormat >= 8) {
		getAttrs("");
		if (is_json) {
			printf(",\n");
		}
	}

	uint16_t imageCount = getShort();
	if (!is_json) {
		printf("imageCount: %04x\n", imageCount);
	} else {
		printf("\"images\": [\n");
	}

	for (int image = 0; image < imageCount; image++) {
		uint16_t imageFlags = getShort();
		char *imageName = getString();
		char *imageId = getString();
		uint16_t imageFormat = getShort();
		if (!is_json) {
			printf("product #%d image #%d:\n", prodNum, image);
			printf("\timageFlags: %04x\n", imageFlags);
			printf("\timageName: '%s'\n", imageName);
			printf("\timageId: '%s'\n", imageId);
			printf("\timageFormat: %04x\n", imageFormat);
		} else {
			printf("{\n");
			printf("\t\"imageFlags\": %u,\n", imageFlags);
			printf("\t\"imageName\": \"%s\",\n", imageName);
			printf("\t\"imageId\": \"%s\",\n", imageId);
			printf("\t\"imageFormat\": %u,\n", imageFormat);
		}

		uint16_t imageOrder = 0;
		if (prodFormat >= 5) {
			imageOrder = getShort();
		}
		if (!is_json) {
			printf("\timageOrder: %04x (%u)\n", imageOrder, imageOrder);
		} else {
			printf("\t\"imageOrder\": %u,\n", imageOrder);
		}

		uint32_t imageVersion = getInt();
		if (!is_json) {
			printf("\timageVersion: %u\n", imageVersion);
		} else {
			printf("\t\"imageVersion\": %u,\n", imageVersion);
		}

		if (prodFormat == 5) {
			uint32_t a = getInt();
			uint32_t b = getInt();
			if (!is_json) {
				if (a || b) {
					printf("a: %08x\n", a);
					printf("b: %08x\n", b);
					errx(1, "diagnostic abort (has a or b)");
				}
			} else {
				printf("\t\"unk_v5_a\": %u,\n", a);
				printf("\t\"unk_v5_b\": %u,\n", b);
			}
		}

		char *derivedFrom = getString();
		if (!is_json) {
			if (strlen(derivedFrom)) {
				printf("\tderivedFrom: '%s'\n", derivedFrom);
			}
		} else {
			printf("\t\"derivedFrom\": \"%s\",\n", derivedFrom);
		}
		free(derivedFrom);
		if (prodFormat >= 8) {
			getAttrs("\t");
			if (is_json) {
				printf(",\n");
			}
		}

		uint16_t subsysCount = getShort();
		if (!is_json) {
			printf("\tsubsysCount: %04x\n", subsysCount);
		} else {
			printf("\t\"subsystems\": [\n");
		}

		for(int subsys = 0; subsys < subsysCount; subsys++) {
			uint16_t subsysFlags = getShort();
			char *subsysName = getString();
			char *subsysId = getString();
			char *subsysExpr = getString();
			time_t subsysInstallDate = getInt();
			if (!is_json) {
				printf("\tsubsys #%d:\n", subsys);
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
				printf("\t\tsubsysName: '%s'\n", subsysName);
				printf("\t\tsubsysId: '%s'\n", subsysId);
				printf("\t\tsubsysExpr: '%s'\n", subsysExpr);
				if (subsysFlags & 0x0080) {
					printf("\t\tsubsysInstallDate: %s", ctime(&subsysInstallDate));
				}
			} else {
				printf("\t\t{\n");
				printf("\t\t\"subsysFlags\": %u,\n", subsysFlags);
				printf("\t\t\"subsysName\": \"%s\",\n", subsysName);
				printf("\t\t\"subsysId\": \"%s\",\n", subsysId);
				printf("\t\t\"subsysExpr\": \"%s\",\n", subsysExpr);
				printf("\t\t\"subsysInstallDate\": %lu,\n", subsysInstallDate);
			}

			getRules("rules");
			if (is_json) {
				printf(",\n");
			}
			getPrereqs();
			if (is_json) {
				printf(",\n");
			}
			free(subsysName);
			free(subsysId);
			free(subsysExpr);
			if (prodFormat >= 5) {
				char *altName = getString();
				if (!is_json) {
					printf("\t\taltName: '%s'\n", altName);
				} else {
					printf("\t\t\"altName\": \"%s\"", altName);
				}
				free(altName);
			}
			if (prodFormat >= 6) {
				if (!is_json) {
					printf("\t\tincompats:\n");
				} else {
					printf(",\n");
					printf("\t\t");
				}
				getRules("incompats");
			}
			if (prodFormat >= 8) {
				if (is_json) {
					printf(",\n");
				}
				getAttrs("\t\t");
			}
			if (prodFormat >= 9) {
				if (is_json) {
					printf(",\n");
				}
				getUpdates();
			}

			/* close subsystem object */
			if (is_json) {
				if (subsys == (subsysCount - 1)) {
					printf("\n\t\t}\n");
				} else {
					printf("\n\t\t},\n");
				}
			}
		}
		/* close subsystems array */
		if (is_json) {
			printf("\t]\n");
		}

		free(imageName);
		free(imageId);
		/* close image object */
		if (is_json) {
			if ((image == (imageCount - 1))) {
				printf("}\n");
			} else {
				printf("},\n");
			}
		}
	}
	/* close images array */
	if (is_json) {
		printf("]\n");
	}
	/* close product object */
	if (is_json) {
		if (prodNum == (noOfProds - 1)) {
			printf("}\n");
		} else {
			printf("},\n");
		}
	}


	free(shortName);
	free(longName);
	} // end of foreach(prod)
	free(prodId);
	/* close products array */
	if (is_json) {
		printf("]\n");
	}
	/* close root object */
	if (is_json) {
		printf("}\n");
	}

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
