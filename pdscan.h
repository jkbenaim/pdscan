#pragma once

struct pdscan_ctx_s {
	uint8_t *ptr_src;
	uint8_t *ptr;
	bool is_json;
	char *buf;
	char *bufcur;
	size_t bufsize;
	size_t bufused;
};

__attribute__((nonnull (1)))
extern struct pdscan_ctx_s *new_pdscan_ctx(void *pd, size_t pd_len, bool is_json);
__attribute__((nonnull (1, 3, 4)))
extern int pd_analyze(void *pd, size_t pd_len, char **analysis, size_t *analysis_len, bool is_json);
