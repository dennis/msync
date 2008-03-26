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
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include "master.h"
#include "slave.h"

static void usage() {
	printf(
		" Usage: msync [options] [src-dir] [dst-dir] - to start in master mode\n"
		" Usage: msync --slave [directory]           - to start in slave mode\n"
		"\n"
		"  msync is a simplistic tool that copy files based on the timestamp\n"
		"\n"
		"  -h  --help                  : This text.\n"
		"  -s  --slave                 : Start in slave mode.\n"
		"  -v  --version               : Display version and exit.\n"
		"  -V  --verbose               : More output.\n"
		"  -S  --source 'command'      :\n"
		"  -D  --destination 'command' :\n"
		"\n");
}

static int is_slave(const char* param) {
	if(strcmp("-s", param) == 0 || strcmp("--slave", param) == 0)
		return 1;
	else
		return 0;
}

static int slave_mode(int argc, char* argv[]) {
	char* dir = (char)NULL;
	int i;

	for(i = 1; i < argc; i++) 
		if(!is_slave(argv[i]))
			dir = argv[i];

	if(dir && chdir(dir)==-1) {
		fprintf(stderr, "ERROR couldn't chdir to %s\n", dir);
		return 1;
	}

	context_t	ctx;
	ctx_init(&ctx);
	ctx.msync = argv[0];

	return slave(&ctx);
}

static int master_mode(int argc, char* argv[]) {
	static struct option long_options[] = {
		{"help",	0,	NULL,	'h'},
		{"version",	0,	NULL,	'v'},
		{"verbose",	0,	NULL,	'V'},
		{"source",  1,  NULL,   'S'},
		{"destination",  1,  NULL,   'D'},
		{0, 0, 0, 0}
	};

	int c;

	optind = 0;

	context_t	ctx;
	ctx_init(&ctx);
	ctx.msync = argv[0];

	while((c = getopt_long(argc, argv, "hvS:D:V", long_options, NULL)) != -1) {
		switch(c) {
			case '?':
			case 'h':
				usage();
				return 0;
				break;
			case 'v':
				printf("msync version %s\n", VERSION);
				return 0;
				break;
			case 'V':
				ctx.verbose = 1;
				break;
			case 'S':
				strncpy(ctx.srccmd, optarg, CTXCMD_LEN);
				break;
			case 'D':
				strncpy(ctx.dstcmd, optarg, CTXCMD_LEN);
				break;
		}
	}

	// Source / Target directories given
    if(argc-optind == 2) {
		// Only two direectories supplied. Lets change these to: "msync --slave directory"
		snprintf(ctx.srccmd, CTXCMD_LEN, "%s --slave %s", argv[0], argv[optind+0]);
		snprintf(ctx.dstcmd, CTXCMD_LEN, "%s --slave %s", argv[0], argv[optind+1]);
    }

	// No arguments
	else if(argc-optind == 0) {
		if(!ctx.srccmd[0] || !ctx.dstcmd[0]) {
			usage();
			return 1;
		}
	}

	// A single or too many arguments
	else {
		usage();
		return 1;
	}

	return master(&ctx);
}

int main(int argc, char* argv[]) {
	int i;
	for(i = 1; i < argc; i++) 
		if(is_slave(argv[i]))
			return slave_mode(argc, argv);
	return master_mode(argc, argv);
}
