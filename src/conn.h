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
#ifndef __conn_h__
#define __conn_h__
#include <unistd.h>

typedef struct {
	int handshaked;		// Said proper hello
	int abort;			// Abort connection
	int infd;
	int outfd;
	int		rbuf_max;
	ssize_t	rbuf_len;
	char*	rbuf;
} conn_t;

void conn_init(conn_t*);
void conn_free(conn_t*);
ssize_t conn_read(conn_t*);
ssize_t conn_readline(conn_t*, char*, size_t);
ssize_t conn_write(conn_t*, const char*, size_t);
ssize_t conn_printf(conn_t*, const char*, ...);
void conn_shift(conn_t*, int);
void conn_abort(conn_t*);
int conn_alive(conn_t*);
#endif
