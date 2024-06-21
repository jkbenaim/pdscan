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
#include "pdscan.h"
#include "version.h"

__attribute__((nonnull (1, 2)))
int _vappend(struct pdscan_ctx_s *ctx, const char *fmt, va_list ap)
{
	__label__ out_error;
	int rc;
	size_t len;
	va_list ap_copy;
	va_copy(ap_copy, ap);

	/* Get length of formatted string. */
	/* After this use of 'ap', it cannot be used again. */
	rc = vsnprintf(NULL, 0, fmt, ap);
	if (rc < 0)
		goto out_error;
	len = rc;

	/* Verify available storage. */
	while (ctx->bufsize < (ctx->bufused + len + (size_t)1)) {
		/* Not enough storage, so resize. */
		const size_t increment = 128*1024;
		char *newbuf = NULL;
		newbuf = realloc(ctx->buf, ctx->bufsize + increment);
		if (!newbuf)
			goto out_error;
		ctx->buf = newbuf;
		ctx->bufsize += increment;
		ctx->bufcur = ctx->buf + ctx->bufused;
	}

	/* Write. */
	rc = vsnprintf(ctx->bufcur, (ctx->bufsize - ctx->bufused), fmt, ap_copy);
	if (rc < 0)
		goto out_error;

	/* Adjust bufused. */
	ctx->bufused += len;
	ctx->bufcur += len;

	va_end(ap_copy);
	return 0;

out_error:
	va_end(ap_copy);
	return -1;
}

__attribute__((nonnull (1, 2)))
int _append(struct pdscan_ctx_s *ctx, const char *fmt, ...)
{
	int rc;
	va_list ap;
	va_start(ap, fmt);
	rc = _vappend(ctx, fmt, ap);
	va_end(ap);
	if (rc)
		err(1, "asdf");
	return rc;
}

#define _a(fmt, ...) _append(ctx, fmt, ##__VA_ARGS__)

__attribute__((nonnull (1)))
uint16_t getShort(struct pdscan_ctx_s *ctx)
{
	uint16_t out = 0;
	out += *(ctx->ptr)++;
	out <<= 8;
	out += *(ctx->ptr)++;
	return out;
}

__attribute__((nonnull (1)))
uint32_t getInt(struct pdscan_ctx_s *ctx)
{
	uint32_t out = 0;
	out = getShort(ctx);
	out <<= 16;
	out += getShort(ctx);
	return out;
}

__attribute__((nonnull (1)))
char *getCstring(struct pdscan_ctx_s *ctx)
{
	char *out = NULL;
	char *out2 = NULL;
	size_t len;
	if (!ctx->ptr) errx(1, "called getCstring with ptr=NULL");
	len = strlen((char *)ctx->ptr);
	out = malloc(len + 1);
	if (!out) err(1, "in malloc");
	strncpy(out, (char *)ctx->ptr, len);
	out[len] = '\0';
	ctx->ptr += len;
	ctx->ptr++;
	out2 = escape_json(out);
	free(out);
	return out2;
}

__attribute__((nonnull (1)))
char *getString(struct pdscan_ctx_s *ctx)
{
	char *out = NULL;
	char *out2 = NULL;
	size_t len = 0;
	len = getShort(ctx);
	out = malloc(len + 1);
	if (!out) err(1, "in malloc");
	memcpy(out, ctx->ptr, len);
	out[len] = '\0';
	ctx->ptr += len;
	for (size_t i = 0; i < len; i++) {
		if (!ctx->is_json) {
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

__attribute__((nonnull (1)))
char *getTriplet(struct pdscan_ctx_s *ctx)
{
	char *a1 = getString(ctx);
	char *a2 = getString(ctx);
	char *a3 = getString(ctx);
	char *triplet = NULL;
	int rc;
	rc = asprintf(&triplet, "%s.%s.%s", a1, a2, a3);
	if (rc == -1) err(1, "in asprintf");
	free(a1);
	free(a2);
	free(a3);
	return triplet;
}

__attribute__((nonnull (1)))
char *getMatcher(struct pdscan_ctx_s *ctx, const char *prefix)
{
	// a matcher is 3 strings and 2 ints
	char *out = NULL;
	int rc;
	char *triplet = getTriplet(ctx);
	int32_t from = getInt(ctx);
	int32_t to = getInt(ctx);
	const char *my_prefix;
	if (!prefix) {
		my_prefix = "";
	} else if (!strcmp(prefix, "replaces ") && (from < 0)) {
		from = -from;
		my_prefix = "follows ";
	} else {
		my_prefix = prefix;
	}
	if (!ctx->is_json) {
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

__attribute__((nonnull (1)))
void getRules(struct pdscan_ctx_s *ctx, const char *label)
{
	uint16_t rulesCount = getShort(ctx);
	if (!ctx->is_json) {
		_a("\t\trulesCount: %d\n", rulesCount);
	} else {
		if (rulesCount) {
			_a("\"%s\": [\n", label);
		} else {
			_a("\"%s\": []", label);
			return;
		}
	}
	if (rulesCount > 2000) errx(1, "diagnostic abort (too many rules)");
	for (int rule = 0; rule < rulesCount; rule++) {
		char *matcher;
		if (!ctx->is_json) {
			matcher = getMatcher(ctx, "replaces ");
			_a("\t\t\t%s\n", matcher);
		} else {
			matcher = getMatcher(ctx, "replaces");
			if (rule == (rulesCount - 1)) {
				_a("\t\t\t%s\n", matcher);
			} else {
				_a("\t\t\t%s,\n", matcher);
			}
		}
		free(matcher);
	}
	if (ctx->is_json) {
		_a("\t\t]");
	}
}

__attribute__((nonnull (1)))
void getMachInfo(struct pdscan_ctx_s *ctx)
{
	// get machine info
	uint32_t machCount = getInt(ctx);
	if (!ctx->is_json) {
		_a("machCount: %d\n", machCount);
	} else {
		if (machCount) {
			_a("\"mach\": [\n");
		} else {
			_a("\"mach\": [],\n");
			return;
		}
	}
	for (size_t i = 0; i < machCount; i++) {
		char *m = getString(ctx);
		if (!ctx->is_json) {
			_a("\tmach '%s'\n", m);
		} else {
			if (i == (machCount - 1)) {
				_a("\t\"%s\"\n", m);
			} else {
				_a("\t\"%s\",\n", m);
			}
		}
		free(m);
	}
	if (ctx->is_json) {
		_a("],\n");
	}
}

__attribute__((nonnull (1)))
void getPrereqs(struct pdscan_ctx_s *ctx)
{
	uint16_t prereqSets = getShort(ctx);
	if (!ctx->is_json) {
		_a("\t\tprereq sets: %d\n", prereqSets);
	} else {
		if (prereqSets) {
			_a("\t\t\"prereqs\": [\n");
		} else {
			_a("\t\t\"prereqs\": []");
			return;
		}
	}
	for (int set = 0; set < prereqSets; set++) {
		uint16_t prereqsCount = getShort(ctx);
		if (!ctx->is_json) {
			_a("\t\tprereqs: %d (\n", prereqsCount);
		} else {
			_a("\t\t\t[\n");
		}
		for (int a = 0; a < prereqsCount; a++) {
			char *matcher = getMatcher(ctx, NULL);
			if (!ctx->is_json) {
				_a("\t\t\t%s\n", matcher);
			} else {
				if (a == (prereqsCount - 1)) {
					_a("\t\t\t\t%s%s", matcher, (a == (prereqsCount - 1))?"":",");
				} else {
					_a("\t\t\t\t%s%s\n", matcher, (a == (prereqsCount - 1))?"":",");
				}
			}
			free(matcher);
		}
		if (!ctx->is_json) {
			_a("\t\t)\n");
		} else {
			_a("\n\t\t\t]%s\n", (set == (prereqSets - 1))?"":",");
		}
	}
	if (ctx->is_json) {
		_a("\t\t]");
	}
}

__attribute__((nonnull (1)))
void getAttrs(struct pdscan_ctx_s *ctx, const char *prefix)
{
	uint32_t attrs = getInt(ctx);
	if (!ctx->is_json) {
		_a("%sattrs: %d\n", prefix, attrs);
	} else {
		if (attrs) {
			_a("%s\"attrs\": [\n", prefix);
		} else {
			_a("%s\"attrs\": []", prefix);
			return;
		}
	}
	for (size_t i = 0; i < attrs; i++) {
		char *attr = getString(ctx);
		if (!ctx->is_json) {
			_a("%s\t'%s'\n", prefix, attr);
		} else {
			if (i == (attrs - 1)) {
				_a("%s\t\"%s\"\n", prefix, attr);
			} else {
				_a("%s\t\"%s\",\n", prefix, attr);
			}
		}
		free(attr);
	}
	if (ctx->is_json) {
		_a("%s]", prefix);
	}
}

__attribute__((nonnull (1)))
void getUpdates(struct pdscan_ctx_s *ctx)
{
	uint16_t updatesCount = getShort(ctx);
	if (!ctx->is_json) {
		_a("\t\tupdatesCount: %d\n", updatesCount);
	} else {
		if (updatesCount) {
			_a("\t\t\"updates\": [\n");
		} else {
			_a("\t\t\"updates\": []");
			return;
		}
	}
	for (int i = 0; i < updatesCount; i++) {
		char *matcher;
		if (!ctx->is_json) {
			matcher = getMatcher(ctx, "updates ");
		} else {
			matcher = getMatcher(ctx, NULL);
		}
		if (!ctx->is_json) {
			_a("\t\t\t%s\n", matcher);
		} else {
			if (i == (updatesCount - 1)) {
				_a("\t\t\t%s\n", matcher);
			} else {
				_a("\t\t\t%s,\n", matcher);
			}
		}
		free(matcher);
	}
	if (ctx->is_json) {
		_a("\t\t]");
	}
}

void decodeFlags(struct pdscan_ctx_s *ctx, uint16_t subsysFlags, const char *prefix)
{
	if (subsysFlags & 0x0001) {
		_a("%s", prefix);
		_a("required\n");
	}
	if (subsysFlags & 0x0002) {
		_a("%s", prefix);
		_a("default\n");
	}
	if (subsysFlags & 0x0004) {
		_a("%s", prefix);
		_a("unknown flag 0004\n");
	}
	if (subsysFlags & 0x0008) {
		_a("%s", prefix);
		_a("unknown flag 0008\n");
	}
	if (~subsysFlags & 0x0010) {
		_a("%s", prefix);
		_a("unknown flag 0010 is not set\n");
	}
	if (subsysFlags & 0x0020) {
		_a("%s", prefix);
		_a("was deleted\n");
	}
	if (~subsysFlags & 0x0040) {
		_a("%s", prefix);
		_a("unknown flag 0040 is not set\n");
	}
	if (subsysFlags & 0x0080) {
		_a("%s", prefix);
		_a("is installed\n");
	}
	if (subsysFlags & 0x0100) {
		_a("%s", prefix);
		_a("unknown flag 0100\n");
	}
	if (subsysFlags & 0x0200) {
		_a("%s", prefix);
		_a("unknown flag 0200\n");
	}
	if (subsysFlags & 0x0400) {
		_a("%s", prefix);
		_a("patch\n");
	}
	if (~subsysFlags & 0x0800) {
		_a("%s", prefix);
		_a("miniroot\n");
	}
	if (subsysFlags & 0x1000) {
		_a("%s", prefix);
		_a("unknown flag 1000\n");
	}
	if (subsysFlags & 0x2000) {
		_a("%s", prefix);
		_a("clientonly\n");
	}
	if (subsysFlags & 0x4000) {
		_a("%s", prefix);
		_a("unknown flag 4000\n");
	}
	if (subsysFlags & 0x8000) {
		_a("%s", prefix);
		_a("overlays (see 'b' attribute)\n");
	}
}

__attribute__((nonnull (1)))
struct pdscan_ctx_s *new_pdscan_ctx(void *pd, size_t pd_len, bool is_json)
{
	(void) pd_len;
	struct pdscan_ctx_s *out = NULL;
	out = malloc(sizeof(*out));
	if (!out) err(1, "in malloc");
	out->ptr = out->ptr_src = (uint8_t *)pd;
	out->is_json = is_json;
	out->buf = NULL;
	out->bufsize = 0;
	out->bufused = 0;
	return out;
}

__attribute__((nonnull (1, 3, 4)))
int pd_analyze(void *pd, size_t pd_len, char **analysis, size_t *analysis_len, bool is_json)
{
	struct pdscan_ctx_s *ctx = NULL;
	ctx = new_pdscan_ctx(pd, pd_len, is_json);

	if (is_json) {
		_a("{\n");
	}

	// product
	char *prodId = NULL;
	if ((ctx->ptr[0] == 'p') || (ctx->ptr[1] == 'd')) {
                prodId = getCstring(ctx);
		if (is_json) {
			_a("\"prodId\": \"%s\",\n", prodId);
		} else {
	                _a("prodId: present, '%s'\n", prodId);
		}
	} else {
		if (!is_json) {
	                _a("prodId: not present\n");
		}
        }

#if 1
	uint16_t magic = getShort(ctx);
	if (!is_json) {
		_a("magic: %04x %s\n", magic, (magic==1988)?"(ok)":"(BAD)");
	} else {
		_a("\"magic\": %u,\n", magic);
	}
        if (magic != 1988) return 1;
	uint16_t noOfProds = getShort(ctx);
	if (!is_json) {
		_a("noOfProds: %04x %s\n", noOfProds, (noOfProds>=1)?"(ok)":"(BAD)");
	}

	if (is_json && noOfProds) {
		_a("\"products\": [\n");
	}

#else
	uint16_t magic = 0;
	uint16_t noOfProds = 1;
#endif
	for (unsigned prodNum = 0; prodNum < noOfProds; prodNum++) {
	// root
	uint16_t prodMagic = getShort(ctx);
	if (!is_json) {
		_a("prodMagic: %04x %s\n", prodMagic, (prodMagic==1987)?"(ok)":"(BAD)");
	} else {
		_a("{\n");
	}
        if (prodMagic != 1987) return 1;
	uint16_t prodFormat = getShort(ctx);
	if (is_json) {
		_a("\"prodFormat\": %u,\n", prodFormat);
	} else {
		_a("prodFormat: %04x\n", prodFormat);
	}
	switch (prodFormat) {
	case 5 ... 9:
		break;
	default:
		errx(1, "bad prodFormat: %d not between 5 and 9 inclusive", prodFormat);
	}

	char *shortName = getString(ctx);
	char *longName = getString(ctx);
	uint16_t prodFlags = getShort(ctx);
	if (!is_json) {
		_a("shortName: '%s'\n", shortName);
		_a("longName:  '%s'\n", longName);
		_a("prodFlags: %04x\n", prodFlags);
	} else {
		_a("\"shortName\": \"%s\",\n", shortName);
		_a("\"longName\": \"%s\",\n", longName);
		_a("\"prodFlags\": %u,\n", prodFlags);
	}
	if (prodFormat >= 5) {
		time_t prodDateTime = getInt(ctx);
		if (!is_json) {
			_a("datetime: %s", ctime(&prodDateTime));
		} else {
			_a("\"datetime\": %lu,\n", prodDateTime);
		}
	}

	if (prodFormat >= 5) {
		char *prodIdk = getString(ctx);
		if (!is_json) {
			_a("prodIdk: '%s'\n", prodIdk);
		} else {
			_a("\"prodIdk\": \"%s\",\n", prodIdk);
		}
		free(prodIdk);
	}

	if (prodFormat == 7) {
		getMachInfo(ctx);
	}

	if (prodFormat >= 8) {
		getAttrs(ctx, "");
		if (is_json) {
			_a(",\n");
		}
	}

	uint16_t imageCount = getShort(ctx);
	if (!is_json) {
		_a("imageCount: %04x\n", imageCount);
	} else {
		_a("\"images\": [\n");
	}

	for (int image = 0; image < imageCount; image++) {
		uint16_t imageFlags = getShort(ctx);
		char *imageName = getString(ctx);
		char *imageId = getString(ctx);
		uint16_t imageFormat = getShort(ctx);
		if (!is_json) {
			_a("product #%d image #%d:\n", prodNum, image);
			_a("\timageFlags: %04x\n", imageFlags);
			_a("\timageName: '%s'\n", imageName);
			_a("\timageId: '%s'\n", imageId);
			_a("\timageFormat: %04x\n", imageFormat);
		} else {
			_a("{\n");
			_a("\t\"imageFlags\": %u,\n", imageFlags);
			_a("\t\"imageName\": \"%s\",\n", imageName);
			_a("\t\"imageId\": \"%s\",\n", imageId);
			_a("\t\"imageFormat\": %u,\n", imageFormat);
		}

		uint16_t imageOrder = 0;
		if (prodFormat >= 5) {
			imageOrder = getShort(ctx);
		}
		if (!is_json) {
			_a("\timageOrder: %04x (%u)\n", imageOrder, imageOrder);
		} else {
			_a("\t\"imageOrder\": %u,\n", imageOrder);
		}

		uint32_t imageVersion = getInt(ctx);
		if (!is_json) {
			_a("\timageVersion: %u\n", imageVersion);
		} else {
			_a("\t\"imageVersion\": %u,\n", imageVersion);
		}

		if (prodFormat == 5) {
			uint32_t a = getInt(ctx);
			uint32_t b = getInt(ctx);
			if (!is_json) {
				if (a || b) {
					_a("a: %08x\n", a);
					_a("b: %08x\n", b);
					//errx(1, "diagnostic abort (has a or b)");
				}
			} else {
				_a("\t\"unk_v5_a\": %u,\n", a);
				_a("\t\"unk_v5_b\": %u,\n", b);
			}
		}

		char *derivedFrom = getString(ctx);
		if (!is_json) {
			if (strlen(derivedFrom)) {
				_a("\tderivedFrom: '%s'\n", derivedFrom);
			}
		} else {
			_a("\t\"derivedFrom\": \"%s\",\n", derivedFrom);
		}
		free(derivedFrom);
		if (prodFormat >= 8) {
			getAttrs(ctx, "\t");
			if (is_json) {
				_a(",\n");
			}
		}

		uint16_t subsysCount = getShort(ctx);
		if (!is_json) {
			_a("\tsubsysCount: %04x\n", subsysCount);
		} else {
			_a("\t\"subsystems\": [\n");
		}

		for(int subsys = 0; subsys < subsysCount; subsys++) {
			uint16_t subsysFlags = getShort(ctx);
			char *subsysName = getString(ctx);
			char *subsysId = getString(ctx);
			char *subsysExpr = getString(ctx);
			time_t subsysInstallDate = getInt(ctx);
			if (!is_json) {
				_a("\tsubsys #%d:\n", subsys);
				_a("\t\tsubsysFlags: %04x\n", subsysFlags);
				decodeFlags(ctx, subsysFlags, "\t\t");
				_a("\t\tsubsysName: '%s'\n", subsysName);
				_a("\t\tsubsysId: '%s'\n", subsysId);
				_a("\t\tsubsysExpr: '%s'\n", subsysExpr);
				if (subsysFlags & 0x0080) {
					_a("\t\tsubsysInstallDate: %s", ctime(&subsysInstallDate));
				}
			} else {
				_a("\t\t{\n");
				_a("\t\t\"subsysFlags\": %u,\n", subsysFlags);
				_a("\t\t\"subsysName\": \"%s\",\n", subsysName);
				_a("\t\t\"subsysId\": \"%s\",\n", subsysId);
				_a("\t\t\"subsysExpr\": \"%s\",\n", subsysExpr);
				_a("\t\t\"subsysInstallDate\": %lu,\n", subsysInstallDate);
			}
			
			if (is_json) {
				_a("\t\t");
			}
			getRules(ctx, "rules");
			if (is_json) {
				_a(",\n");
			}
			getPrereqs(ctx);
			if (is_json) {
				_a(",\n");
			}
			free(subsysName);
			free(subsysId);
			free(subsysExpr);
			if (prodFormat >= 5) {
				char *altName = getString(ctx);
				if (!is_json) {
					_a("\t\taltName: '%s'\n", altName);
				} else {
					_a("\t\t\"altName\": \"%s\"", altName);
				}
				free(altName);
			}
			if (prodFormat >= 6) {
				if (!is_json) {
					_a("\t\tincompats:\n");
				} else {
					_a(",\n");
					_a("\t\t");
				}
				getRules(ctx, "incompats");
			}
			if (prodFormat >= 8) {
				if (is_json) {
					_a(",\n");
				}
				getAttrs(ctx, "\t\t");
			}
			if (prodFormat >= 9) {
				if (is_json) {
					_a(",\n");
				}
				getUpdates(ctx);
			}

			/* close subsystem object */
			if (is_json) {
				if (subsys == (subsysCount - 1)) {
					_a("\n\t\t}\n");
				} else {
					_a("\n\t\t},\n");
				}
			}
		}
		/* close subsystems array */
		if (is_json) {
			_a("\t]\n");
		}

		free(imageName);
		free(imageId);
		/* close image object */
		if (is_json) {
			if (image == (imageCount - 1)) {
				_a("}\n");
			} else {
				_a("},\n");
			}
		}
	}
	/* close images array */
	if (is_json) {
		_a("]\n");
	}
	/* close product object */
	if (is_json) {
		if ((prodNum + 1) == (unsigned)noOfProds) {
			_a("}\n");
		} else {
			_a("},\n");
		}
	}


	free(shortName);
	free(longName);
	} // end of foreach(prod)
	free(prodId);
	/* close products array */
	if (is_json) {
		_a("]\n");
	}
	/* close root object */
	if (is_json) {
		_a("}\n");
	}

	*analysis = ctx->buf;
	*analysis_len = ctx->bufused;

	free(ctx);

	return 0;
}

