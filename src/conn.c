#include "config.h"
#include "conn.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

void conn_init(conn_t* cn) {
	memset(cn,0,sizeof(conn_t));

	cn->rbuf_max = 4096;
	cn->rbuf = malloc(cn->rbuf_max);

	assert(cn->rbuf);

	cn->infd = -1;
	cn->outfd = -1;
}

void conn_free(conn_t* cn) {
	if(cn->rbuf) {
		free(cn->rbuf);
		cn->rbuf = NULL;
	}
}

void conn_shift(conn_t* cn, int b) {
	assert(cn && cn->rbuf);
	assert(cn->rbuf_len >= b);
	memmove(cn->rbuf, cn->rbuf+b, cn->rbuf_len-b);
	cn->rbuf_len -= b;
}

ssize_t conn_read(conn_t* cn) {
	assert(cn && cn->rbuf);
	assert(cn->infd != -1);
	if(cn->abort) return 0;
	ssize_t read_len = read(cn->infd, (cn->rbuf+cn->rbuf_len), cn->rbuf_max-cn->rbuf_len);
	cn->rbuf_len += read_len;

	if(read_len == 0)
		cn->abort = 1;

	return read_len;
}

ssize_t conn_readline(conn_t* cn, char* line, size_t maxsize) {
	int i;
	assert(maxsize);
	if(maxsize < 1) return 0;

	while(!cn->abort) {
		if(cn->rbuf_len==0)
			(void)conn_read(cn);

		for(i=0; (i < (int)cn->rbuf_len) && !cn->abort && i < (int)maxsize-1; i++) {
			if(cn->rbuf[i] == (char)NULL || cn->rbuf[i] == '\n') {
				cn->rbuf[i] = (char)NULL;
				strncpy(line, cn->rbuf, i+1);
				conn_shift(cn, i+1);
				return i;
			}
		}

	}
	/*
	while((read_len = conn_read(cn))) {
		// Scan for \n or \0
		for(i=0; (i < (int)cn->rbuf_len) && !cn->abort && i < (int)maxsize-1; i++) {
			if(cn->rbuf[i] == (char)NULL || cn->rbuf[i] == '\n') {
				cn->rbuf[i] = (char)NULL;
				strncpy(line, cn->rbuf, i+1);
				conn_shift(cn, i+1);
				return i;
			}
		}
	}
	*/

	return 0;
}

ssize_t conn_write(conn_t* cn, const char* data, size_t size) {
	assert(cn->outfd != -1);
	if(cn->abort) return 0;
	return write(cn->outfd, data, size);
}

ssize_t conn_printf(conn_t* cn, const char* format, ...) {
	char buffer[512];
	va_list vl;
	va_start(vl,format);
	int s = vsnprintf(buffer, 512, format, vl);
	assert(s<512); // Nothing truncated
	return conn_write(cn,buffer,s);
}

void conn_abort(conn_t* cn) {
	cn->abort = 1;
}

int conn_alive(conn_t* cn) {
	return cn->abort == 0;
}
