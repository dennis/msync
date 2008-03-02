/*
The MIT License

Copyright (c) 2008 Dennis Møllegaard Pedersen <dennis@moellegaard.dk>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
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
