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
