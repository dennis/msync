#include "../config.h"

#define _XOPEN_SOURCE	// for strptime

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
		" Usage: msync [options] <source> <destination> - to start in master mode\n"
		" Usage: msync --slave [directory]              - to start in slave mode\n"
		"\n"
		"  msync is a simplistic tool that copy files based on the timestamp\n"
		"\n"
		"  -a  --adjust <seconds>  : Allow inaccurate timestamps of <seconds>.\n"
		"  -d  --dry-run           : Just display which files needs to be copied.\n"
		"  -h  --help              : This text.\n"
		"  -q  --quiet             : Minimal output.\n"
		"  -s  --slave             : Start in slave mode.\n"
		"  -t  --time <time>       : Use <time> for timestamp.\n"
		"  -v  --version           : Display version and exit.\n"
		"\n");
}

static char strdigit(const char* s) {
	int l = strlen(s);
	while(l) {
		if(!isdigit(s[l-1]))
			return 0;
		l--;
	}

	return 1;
}

static time_t parse_timestr(const char* s) {
	time_t time;
	struct tm t;

	assert(s);

	// Parse ISO 8601 + h:m:s
	strptime(s,"%F %T", &t);
	time = mktime(&t);
	if(time != -1) return time;

	// Parse ISO 8601
	strptime(s,"%F", &t);
	time = mktime(&t);
	if(time != -1) return time;

	// seconds since epoch
	strptime(s, "%s", &t);
	time = mktime(&t);
	if(time != -1) return time;

	return -1;
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
		{"adjust",	1,	NULL,	'a'},
		{"dry-run",	0,	NULL,	'd'},
		{"help",	0,	NULL,	'h'},
		{"quiet",	0,	NULL,	'q'},
		{"time",	1,	NULL,	't'},
		{"version",	0,	NULL,	'v'},
		{0, 0, 0, 0}
	};

	int c;

	optind = 0;

	context_t	ctx;
	ctx_init(&ctx);
	ctx.msync = argv[0];

	while((c = getopt_long(argc, argv, "a:dhqt:v", long_options, NULL)) != -1) {
		switch(c) {
			case 'a': {
				ctx.adjust = atoi(optarg);
				if(ctx.adjust < 0 || !strdigit(optarg)) {
					fprintf(stderr, "%s: adjust value must be a positive integer\n", argv[0]);
					return 1;
				}
				}
				break;
			case 'd':
				ctx.dry_run = 1;
				break;
			case '?':
			case 'h':
				usage();
				return 0;
				break;
			case 'q':
				ctx.quiet = 1;
				break;
			case 't':
				if(strcmp("yesterday", optarg)==0) {
					struct tm yday;
					time_t now = time(NULL);
					localtime_r(&now, &yday);
					yday.tm_sec = 0;
					yday.tm_min = 0;
					yday.tm_hour = 0;
					yday.tm_mday--;
					ctx.time = mktime(&yday);
				}
				else if(strcmp("midnight", optarg)==0) {
					struct tm yday;
					time_t now = time(NULL);
					localtime_r(&now, &yday);
					yday.tm_sec = 0;
					yday.tm_min = 0;
					yday.tm_hour = 0;
					ctx.time = mktime(&yday);
				}
				else if(*optarg == '+') {
					ctx.time = parse_timestr(++optarg);
				}
				else {
					fprintf(stderr, "%s: unknown time option '%s'\n", argv[0], optarg);
					return 1;
				}
				break;
			case 'v':
				printf("msync version %s\n", VERSION);
				return 0;
				break;
		}
	}

	// Source / Target
    if(argc-optind == 2) {
		ctx.src = argv[optind+0];
		ctx.dst = argv[optind+1];
    }
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
