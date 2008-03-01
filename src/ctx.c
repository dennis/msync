#include <assert.h>
#include <stdio.h>

#include "ctx.h"
void ctx_init(context_t* ctx) {
	ctx->src   = NULL;
	ctx->dst   = NULL;
	ctx->quiet = 0;
	ctx->dry_run = 0;
	ctx->time   = 0;
	ctx->adjust = 0;
}

void ctx_free(context_t* ctx) {
	(void)ctx;
}

void ctx_dump(context_t* ctx) {
	char datetime[32];

	assert(ctx);

	ctime_r(&ctx->time, datetime);

	printf("CTX dump\n"
		"  source            : %s\n"
		"  destination       : %s\n"
		"  quiet             : %d\n"
		"  dry_run           : %d\n"
		"  time              : %d: %s"
		"  adjust            : %d\n",

		ctx->src,
		ctx->dst,
		ctx->quiet,
		ctx->dry_run,
		(int)ctx->time,
		datetime,
		ctx->adjust );
}
