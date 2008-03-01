#ifndef __ctx_h__
#define __ctx_h__

#include <stddef.h>
#include <time.h>

typedef struct {
	char* msync;
	char* src;
	char* dst;
	char  quiet;
	char  dry_run;
	time_t time;
	int   adjust;
} context_t;

void ctx_init(context_t*);
void ctx_free(context_t*);
void ctx_dump(context_t*);

#endif
