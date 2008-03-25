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
#include "conn.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

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

	return 0;
}

ssize_t conn_write(conn_t* cn, const char* data, size_t size) {
	assert(cn->outfd != -1);
	if(cn->abort) return 0;
	return write(cn->outfd, data, size);
}

ssize_t conn_printf(conn_t* cn, const char* format, ...) {
	const int buffer_l = 512; char buffer[buffer_l];
	va_list vl;
	va_start(vl,format);
	int s = vsnprintf(buffer, buffer_l, format, vl);
	assert(s<buffer_l); // Nothing truncated
	va_end(vl);
	return conn_write(cn,buffer,s);
}

ssize_t conn_perror(conn_t* cn, const char* str) {
	if(str)
		conn_printf(cn, "%s: ", str);
	return conn_printf(cn, "%s\n", strerror(errno));
}

void conn_abort(conn_t* cn) {
	cn->abort = 1;
}

int conn_alive(conn_t* cn) {
	return cn->abort == 0;
}
