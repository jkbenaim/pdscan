#ifndef _ESCAPE_H_
#define _ESCAPE_H_
char *escape_url(const char *orig) __attribute__((nonnull(1)));
char *escape_xml(const char *orig) __attribute__((nonnull(1)));
char *escape_json(const char *orig) __attribute__((nonnull(1)));
#endif

