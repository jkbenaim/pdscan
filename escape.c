#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char reservedChars[] = "!*'();:@&=+$,/?#[]%";
const char unreservedChars[] = \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	"abcdefghijklmnopqrstuvwxyz" \
	"0123456789-_.~";


bool isReservedChar(char suspect)
{
	return strchr(reservedChars, suspect) != NULL;
}

bool isNotUnreservedChar(char suspect)
{
	return strchr(unreservedChars, suspect) == NULL;
}

bool isXmlReservedChar(char suspect)
{
	if (suspect == '&') return true;
	if (suspect == '<') return true;
	return false;
}

char *escape_url(const char *orig)
{
	char *out = NULL;
	size_t outLength = 0;
	size_t origLength = strlen(orig);
	const char *inp = orig;
	char *outp;
	char *test;
	size_t i;

	out = malloc(origLength*strlen("%99") + 1);
	if (!out) {
		fprintf(stderr, "error in percent_encode: out of memory\n");
		return NULL;
	}

	outp = out;

	for (i = 0; i < origLength; i++) {
		unsigned char suspect = *inp++;
		if (isNotUnreservedChar(suspect)) {
			*outp++ = '%';
			*outp++ = "0123456789abcdef"[(suspect/16)%16];
			*outp++ = "0123456789abcdef"[suspect%16];
			outLength += 3;
		} else {
			*outp++ = suspect;
			outLength++;
		}
	}
	*outp = '\0';
	outLength++;
	test = realloc(out, outLength);
	if (test) {
		out = test;
	}
	return out;
}

char *escape_xml(const char *orig)
{
	char *out = NULL;
	size_t outLength = 0;
	size_t origLength = strlen(orig);
	const char *inp = orig;
	char *outp;
	char *test;
	size_t i;

	out = malloc(origLength*strlen("&amp;") + 1);
	if (!out) {
		fprintf(stderr, "error in xml_escape: out of memory\n");
		return NULL;
	}

	outp = out;

	for (i = 0; i < origLength; i++) {
		char suspect = *inp++;
		switch (suspect) {
		case '&':
			*outp++ = '&';
			*outp++ = 'a';
			*outp++ = 'm';
			*outp++ = 'p';
			*outp++ = ';';
			outLength += 5;
			break;
		case '<':
			*outp++ = '&';
			*outp++ = 'l';
			*outp++ = 't';
			*outp++ = ';';
			outLength += 4;
			break;
		default:
			*outp++ = suspect;
			outLength += 1;
			break;
		}
	}
	*outp = '\0';
	outLength++;
	test = realloc(out, outLength);
	if (test) {
		out = test;
	}
	return out;
}

char *escape_json(const char *orig)
{
	char *out = NULL;
	size_t outLength = 0;
	size_t origLength = strlen(orig);
	const char *inp = orig;
	char *outp;
	char *test;
	size_t i;

	out = malloc(origLength * strlen("\\u0000") + 1);
	if (!out) {
		fprintf(stderr, "error in escape_json: out of memory\n");
		return NULL;
	}

	outp = out;

	for (i = 0; i < origLength; i++) {
		char suspect = *inp++;
		switch (suspect) {
		case '"':
			*outp++ = '\\';
			*outp++ = '"';
			outLength += 2;
			break;
		case '\\':
			*outp++ = '\\';
			*outp++ = '\\';
			outLength += 2;
			break;
#if 0
		case '/':
			*outp++ = '\\';
			*outp++ = '/';
			outLength += 2;
			break;
#endif
#if 1
		case '\b':
			*outp++ = '\\';
			*outp++ = 'b';
			outLength += 2;
			break;
		case '\t':
			*outp++ = '\\';
			*outp++ = 't';
			outLength += 2;
			break;
#endif
		case '\f':
			*outp++ = '\\';
			*outp++ = 'f';
			outLength += 2;
			break;
		case '\n':
			*outp++ = '\\';
			*outp++ = 'n';
			outLength += 2;
			break;
		case '\r':
			*outp++ = '\\';
			*outp++ = 'r';
			outLength += 2;
			break;
		case 0x00 ... 0x07:
			/* fall-thru */
		case 0x0b:
			/* fall-thru */
		case 0x0e ... 0x1f:
			*outp++ = '\\';
			*outp++ = 'u';
			*outp++ = '0';
			*outp++ = '0';
			*outp++ = "0123456789abcdef"[(suspect&0xf0)>>4];
			*outp++ = "0123456789abcdef"[suspect&0xf];
			outLength += strlen("\\u0000");
			break;
		default:
			*outp++ = suspect;
			outLength += 1;
			break;
		}
	}
	*outp = '\0';
	outLength++;
	test = realloc(out, outLength);
	if (test) {
		out = test;
	}
	return out;
}
