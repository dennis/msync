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
#include "config.h"

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "master.h"
#include "conn.h"
#include "list.h"

/*#define DMSG(x) { x }while(0)*/
#define DMSG(x)
#define MAX(x,y) (x>y ? x :y)
#define MIN(x,y) (x<y ? x :y)

typedef struct {
	node_t node;
	char *entry;
} fileentry_t;

static pid_t start_slave(const context_t* ctx, const char* dir, int rpfd[2], int wpfd[2]) {
	pid_t pid;

	switch( pid = fork() ) {
		case -1:
			perror("fork()");
			exit(EXIT_FAILURE);
			break;

		case 0:
			close(rpfd[0]); 
			close(wpfd[1]); 
			// Redirect STDOUT and STDERR
			dup2(rpfd[1],1);
			dup2(wpfd[0],0);
			{
			char* msync_cmd[] = {"msync", "--slave", (char*)dir, (char*)NULL };
			if(execve(ctx->msync, msync_cmd, environ)==-1)
				perror("execve");
			}
			exit(EXIT_FAILURE);
			break;
		default:
			close(rpfd[1]);
			close(wpfd[0]);
	}

	return pid;
}

static int readline(conn_t* cn, char* buffer, size_t size) {
	int l = 0;
	if((l=conn_readline(cn, buffer, size))==0) 
		return 0;

	DMSG(printf("%lx < %s\n", (long int)cn, buffer););

	if(memcmp("ERROR ", buffer, 6)==0) {
		puts(buffer);
		conn_abort(cn);
		return 0;
	}
	else if(memcmp("WARNING ", buffer, 8)==0) {
		puts(buffer);
		return readline(cn, buffer, size);
	}

	return l;
}

static int expect_keyword(conn_t* cn, const char* keyword) {
	const int buffer_l = 256; char buffer[buffer_l];

	if(readline(cn, buffer, buffer_l)==0)
		return 0;

	return memcmp(keyword, buffer, strlen(keyword)) == 0;
}

static int proto_handshake(conn_t* cn) {
	const int buffer_l = 256; char buffer[buffer_l];
	conn_printf(cn, "HELLO msync %d\n", PROTOCOL_VERSION);
	DMSG(printf("%lx > HELLO msync %d\n", (long int)cn, PROTOCOL_VERSION););
	if(readline(cn, buffer, buffer_l)==0) 
		return 0;

	return memcmp("HELLO\0", buffer, 6) == 0;
}

static time_t proto_scan(conn_t* cn) {
	const int buffer_l = 256; char buffer[buffer_l];
	conn_printf(cn, "SCAN .\n");
	DMSG(printf("%lx > SCAN .\n", (long int)cn););
	if(readline(cn, buffer, buffer_l)==0) 
		return 0;

	return atol(buffer+7);
}

static void proto_newerthan(conn_t* cn, time_t ts, fileentry_t** xferlist) {
	const int buffer_l = 1024; char buffer[buffer_l];
	conn_printf(cn, "NEWERTHAN %ld ./\n", ts);
	DMSG(printf("%lx > NEWERTHAN %ld .\n", (long int)cn, ts););
	
	fileentry_t* n;
	while(readline(cn, buffer, buffer_l)) {
		if(strcmp("END", buffer)==0)
			break;
		n = (fileentry_t*)malloc(sizeof(fileentry_t));
		n->entry = (char*)malloc(strlen(buffer)+1);
		strcpy(n->entry,buffer);
		list_add((node_t**)xferlist, (node_t*)n);
	}
}

static void proto_sync(conn_t* src, conn_t* dst, const char* entry) {
	const int buffer_l = 1024; char buffer[buffer_l];
	conn_printf(src, "GET %s\n", entry+5);
	DMSG(printf("%lx > GET %s\n", (long int)src, entry+5););
	readline(src, buffer, buffer_l);

	if(memcmp("PUT ", buffer, 4) == 0 ) {
		size_t size = 0;

		{
			char sizestr[10];
			char* p;
			p = buffer+4;
			int i = 0;
			while(*p && *p != ' ' && i < 9)
				sizestr[i++] = *(p++);
			sizestr[i] = (char)NULL;
			size = atol(sizestr);
		}

		int bytes_left = size;
		int r;
		conn_printf(dst, "%s\n", buffer);
		DMSG(printf("%lx > %s\n", (long int)dst, buffer););
		while(!src->abort && bytes_left) {
			if(src->rbuf_len == 0)
				(void)conn_read(src);
			r = MIN(bytes_left, src->rbuf_len);

			if(r) {
				write(dst->outfd, src->rbuf, r);
				conn_shift(src,r);
				bytes_left -= r;
			}

			assert(bytes_left >= 0);
		}
		expect_keyword(dst, "GET");
	}
	else if(memcmp("MKDIR ", buffer, 6) == 0 ) {
		conn_printf(dst, "%s\n", buffer);
		DMSG(printf("%lx > %s\n", (long int)dst, buffer););
		expect_keyword(dst, "MKDIR");
	}
	else if(memcmp("SLNK ", buffer, 5) == 0 ) {
		conn_printf(dst, "%s\n", buffer);
		DMSG(printf("%lx > %s\n", (long int)dst, buffer););
		readline(src, buffer, buffer_l);
		conn_printf(dst, "%s\n", buffer);
		DMSG(printf("%lx > %s\n", (long int)dst, buffer););
	}
	else {
		puts(buffer);
		assert(0);
	}
}

static pid_t start(context_t* ctx, conn_t* cn, const char* dir) {
	int rpfd[2]; 
	int wpfd[2];

	if(pipe(rpfd)==-1) { perror("pipe"); exit(EXIT_FAILURE); }
	if(pipe(wpfd)==-1) { perror("pipe"); exit(EXIT_FAILURE); }

	cn->infd = rpfd[0];
	cn->outfd = wpfd[1];

	pid_t pid = start_slave(ctx, dir, rpfd, wpfd); 
	
	return pid;
}

static void stop(context_t* ctx, pid_t pid, conn_t* cn) {
	(void)ctx;

	dprintf(cn->outfd, "QUIT\n");

	close(cn->infd);
	close(cn->outfd);
	if(kill(pid, SIGTERM)==-1)
		perror("kill()");
}

int master(context_t* ctx) {
	long tdiff = 0;
	time_t dst_newest_ts;

	pid_t  src_pid; 
	conn_t src_conn; 
	conn_init(&src_conn);

	pid_t  dst_pid; 
	conn_t dst_conn; 
	conn_init(&dst_conn);

	src_pid = start(ctx, &src_conn, ctx->src);
	dst_pid = start(ctx, &dst_conn, ctx->dst);

	// Perform handshake
	if(!proto_handshake(&src_conn) || !proto_handshake(&dst_conn)) {
		dprintf(3, "Internal error: %d (handshake failed)\n", __LINE__);
	}
	else {
		// Ask for time on both sides. Calculate difference
		/*
		long src_tdiff = proto_gettime(&src_conn);
		long dst_tdiff = proto_gettime(&dst_conn);
		*/

		if(conn_alive(&src_conn) && conn_alive(&dst_conn)) {
			tdiff = TIME_MARGIN_SEC;

			//printf("tdiff: %ld\n", tdiff);

			// Find the newest file at dst
			dst_newest_ts = proto_scan(&dst_conn) - tdiff;

			if(conn_alive(&src_conn) && conn_alive(&dst_conn)) {
				fileentry_t* transferlist = NULL;

				// Get files from source, newer than dst_newest_ts
				proto_newerthan(&src_conn, dst_newest_ts, &transferlist );

				// cleanup
				fileentry_t* p;
				fileentry_t* n = NULL;
				for(p = transferlist; p; p = n) { 
					n = (fileentry_t*)((node_t*)p)->next;
					proto_sync(&src_conn, &dst_conn, p->entry);
					free(p->entry);
					free(p);
				}
			}
		}
	}

	stop(ctx, src_pid, &src_conn);
	stop(ctx, dst_pid, &dst_conn);

	return 0;
}
