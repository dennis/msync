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

#include "slave.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <ctype.h>
#include <utime.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "md5.h"
#include "conn.h"
#include "list.h"

#define MAX(x,y) (x>y ? x : y)
#define MIN(x,y) (x<y ? x : y)

typedef struct {
	node_t	node;
	ino_t	inode;
	time_t	mtime;
	char	filename[PATH_MAX];
} hlink_t;

struct newerthan_data {
	conn_t*	cn;
	time_t	ts;
};

struct scan_data {
	conn_t*	cn;
	time_t	ts;
	hlink_t** hardlinks;
};

typedef void (*handle_entry_t)(void*, struct stat*, const char* file);
typedef void (*handle_perror_t)(void*, const char* str);

static void proto_handle_error(conn_t* cn, const char* line) {
	fprintf(stderr, "ERROR %s\n", line);
	conn_abort(cn);
}

static void proto_handle_warning(conn_t* cn, const char* line) {
	(void)cn;
	fprintf(stderr, "WARNING %s\n", line);
}

static void traverse_directory(const char* dir, void* data, 
	handle_entry_t handle_entry, handle_perror_t handle_perror) {

	const int buffer_l = PATH_MAX; char buffer[PATH_MAX];
	struct stat st;
	DIR *dirp;
	char* p;

	// We're chdir() into the directory of "dir", so we need "basename" of dir, to stats on
	p = basename(dir);

	if(lstat(p,&st) == 0) {
		handle_entry(data, &st, dir);
		if(S_ISDIR(st.st_mode)) {
			struct dirent* dp;
			getcwd(buffer, buffer_l);

			dirp = opendir(p);
			if(dirp) {
				while ((dp = readdir(dirp)) != NULL) {
					if(strcmp(dp->d_name,".") != 0 && strcmp(dp->d_name,"..") != 0) {
						const int oldcwd_l = 1024; char oldcwd[oldcwd_l];
						getcwd(oldcwd, oldcwd_l);

						strcpy(buffer,dir);
						strcat(buffer, "/");
						strcat(buffer, dp->d_name);
						chdir(p);
						traverse_directory(buffer, data, handle_entry, handle_perror);
						chdir(oldcwd);
					}
				}
				closedir(dirp);
			}
			else {
				if(handle_perror) handle_perror(data, "WARNING opendir()");
			}
		}
	}
	else {
		if(handle_perror) handle_perror(data, "WARNING stat()");
	}
}

static void find_newerthan_ts_perror(void* data, const char* str) {
	conn_t* cn = ((struct newerthan_data*)data)->cn;
	conn_printf(cn, "WARNING %s\n",str);
}

static void find_newerthan_ts_worker(void* data, struct stat* st, const char* file) {
	conn_t* cn = ((struct newerthan_data*)data)->cn;
	time_t  ts = ((struct newerthan_data*)data)->ts;

	if(ts >= st->st_mtime)
		return;
	
	if(S_ISREG(st->st_mode)) {	
		if(st->st_nlink>1)
			conn_printf(cn, "HLNK %s\n", file);
		else
			conn_printf(cn, "FILE %s\n", file);
	} 
	else if(S_ISDIR(st->st_mode)) {
		char* p = basename(file);
		if(strcmp(".", p) != 0 && strcmp("..", p) != 0)
			conn_printf(cn, "DIR  %s\n", file);
	}
	else if(S_ISLNK(st->st_mode)) {
		conn_printf(cn, "SLNK %s\n", file);
	}
}

static void find_newerthan_ts(conn_t* s, time_t ts, const char* path) {
	char curdir[PATH_MAX];

	struct newerthan_data	data;

	data.ts = ts;
	data.cn = s;

	getcwd(curdir, PATH_MAX);
	chdir(path);
	traverse_directory(".", &data, find_newerthan_ts_worker, find_newerthan_ts_perror);

	chdir(curdir);
}

static void proto_handle_newerthan(conn_t* cn, const char* line) {
	char buf[255];
	char* p;
	time_t ts;
	char *path;

	if(strlen(line) >= 254) {
		conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
		conn_abort(cn);
		return;
	}

	strcpy(buf,line);
	
	p = buf;
	while(p && !isspace(*p))
		p++;

	*p = (char)NULL;
	ts = atoi(buf);

	if((buf+255) < p) {
		conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
		conn_abort(cn);
		return;
	}

	p++;
	path = p;

	find_newerthan_ts(cn, ts, path);

	conn_printf(cn, "END\n");
}

static void proto_handle_hello(conn_t* cn, const char* line) {
	if(memcmp(line, "msync ", 6) != 0) {
		conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
		conn_abort(cn);
		return;
	}
	int v = atoi(line+6);
	assert(v);

	if(v == PROTOCOL_VERSION) {
		conn_printf(cn, "%s", "HELLO\n");
		cn->handshaked = 1;
	}
	else {
		conn_printf(cn, "ERROR Protocol not supported\n");
		conn_abort(cn);
	}
}

static void proto_handle_gettime(conn_t* cn) {
	time_t now;

	now = time(NULL);
	assert(now != (time_t)-1);
	conn_printf(cn, "GETTIME %ld\n", (long int)now);
}

static void find_newest_ts_worker(void* dataraw, struct stat* st, const char* file) {
	struct scan_data* data = (struct scan_data*)dataraw;

	if(S_ISREG(st->st_mode)) {	
		data->ts = MAX(data->ts,st->st_mtime);
		if(st->st_nlink>1) {

			// Find st->st_ino in hardlinks if possible
			hlink_t* n = NULL;
			for(n = *data->hardlinks; n; n = (hlink_t*)((node_t*)n)->next) {
				if(n->inode == st->st_ino)
					break;
			}
			if(n) {
				conn_printf(data->cn, "WARNING Candidate - %lx %s\n", n->inode, n->filename);
				if( n->mtime > st->st_mtime ) {
					// Replace current entry with older one
					n->mtime = st->st_mtime;
					strncpy(n->filename, file, PATH_MAX);
					conn_printf(data->cn, "WARNING ..replacing\n");
				}
			}
			else {
				conn_printf(data->cn, "WARNING Found no candidate, adding '%s'\n", file);
				n = (hlink_t*)malloc(sizeof(hlink_t));
				n->inode = st->st_ino;
				n->mtime = st->st_mtime;
				strncpy(n->filename, file, PATH_MAX);

				list_add((node_t**)data->hardlinks, (node_t*)n);
			}
		}
	}
	else if(S_ISDIR(st->st_mode)) {
		char* p = basename(file);
		if(strcmp(p,".") != 0 && strcmp(p,"..") != 0)
			data->ts = MAX(data->ts,st->st_mtime);
	}
}

static void proto_handle_scan(conn_t* cn, const char* dir, hlink_t** hardlinks) {
	char curdir[PATH_MAX];
	struct scan_data data;

	data.ts = 0;
	data.cn = cn;
	data.hardlinks = hardlinks;

	getcwd(curdir, PATH_MAX);
	traverse_directory(dir, &data, find_newest_ts_worker, find_newerthan_ts_perror);
	chdir(curdir);

	conn_printf(cn, "SCAN %ld\n", (long int)data.ts);
}

static mode_t str2mode(const char* line) {
	mode_t m = 0;

	m |= (line[1] == 'r' ? S_IRUSR : 0);
	m |= (line[2] == 'w' ? S_IWUSR : 0);
	m |= (line[3] == 'x' ? S_IXUSR : 0);

	m |= (line[4] == 'r' ? S_IRGRP : 0);
	m |= (line[5] == 'w' ? S_IWGRP : 0);
	m |= (line[6] == 'x' ? S_IXGRP : 0);

	m |= (line[7] == 'r' ? S_IROTH : 0);
	m |= (line[8] == 'w' ? S_IWOTH : 0);
	m |= (line[9] == 'x' ? S_IXOTH : 0);

	return m;
}

static void mode2str(mode_t mode, char* buf) {
	buf[0] = '-';

	buf[1] = (mode & S_IRUSR ? 'r' : '-');
	buf[2] = (mode & S_IWUSR ? 'w' : '-');
	buf[3] = (mode & S_IXUSR ? 'x' : '-');

	buf[4] = (mode & S_IRGRP ? 'r' : '-');
	buf[5] = (mode & S_IWGRP ? 'w' : '-');
	buf[6] = (mode & S_IXGRP ? 'x' : '-');

	buf[7] = (mode & S_IROTH ? 'r' : '-');
	buf[8] = (mode & S_IWOTH ? 'w' : '-');
	buf[9] = (mode & S_IXOTH ? 'x' : '-');
	buf[10] = (char)NULL;
}

static void proto_handle_mkdir(conn_t* cn, const char* line) {
	// line is: permission mtime ctime atime directoryname
	const char* delim = " ";
	char* saveptr = NULL;
	char* token;
	char* ptr = (char*)line;
	int c = 0;

	mode_t mode = 0;
	time_t mtime = 0;
	time_t ctime = 0;
	time_t atime = 0;
	char* name = NULL;

	while((token = strtok_r(ptr, delim, &saveptr))) {
		switch(c) {
			case 0: mode = str2mode(token); break;
			case 1: atime = atol(token); break; 
			case 2: ctime = atol(token); break;
			case 3: mtime = atol(token); break;
			case 4: name = token; break;
			default:
				conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
				conn_abort(cn);
				return;
				break;
		}
		c++;
		ptr = NULL;
	}

	if(c != 5) {
		conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
		conn_abort(cn);
		return ;
	}

	if(mkdir(name,mode) == -1 && errno != EEXIST) {
		conn_perror(cn, "WARNING mkdir()");
		conn_printf(cn, "ERROR Can't create directory: %s\n", name);
		conn_abort(cn);
		return;
	}

	if(chmod(name,mode)==-1) {
		conn_perror(cn,"WARNING chmod()");
		conn_printf(cn, "WARNING Can't change permissions on directory: %s\n", name);
	}

	struct utimbuf t;
	t.actime = atime;
	t.modtime = mtime;
	if(utime(name,&t)==-1) {
		conn_perror(cn, "WARNING utime");
		conn_printf(cn, "WARNING Can't timestamp on directory: %s\n", name);
	}

	conn_printf(cn, "MKDIR %s\n", name);
}

static void md5bin2str(const unsigned char* md5bin, char* md5str) {
	char hex[] = { 
		'0', '1', '2', '3', 
		'4', '5', '6', '7', 
		'8', '9', 'a', 'b',
		'c', 'd', 'e', 'f' };
				
	int i;
	for(i=0; i < 16; i++) {
		md5str[i*2] = hex[(md5bin[i] >> 4) & 0xf];
		md5str[i*2+1] = hex[(md5bin[i]) & 0xf];
	}
	md5str[32] = (char)NULL;
}

static void proto_handle_get(conn_t* cn, const char* line, hlink_t* hardlinks) {
	struct stat st;

	// Reject filenames with just "." and ".."
	if(memcmp(line, ".\0", 2) == 0 || memcmp(line, "..\0", 3) == 0) {
		conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
		conn_abort(cn);
		return;
	}

	if(lstat(line,&st) == 0) {
		if(S_ISREG(st.st_mode)) {
			if(st.st_nlink>1) {
				hlink_t* n = NULL;
				for(n = hardlinks; n; n = (hlink_t*)((node_t*)n)->next) {
					conn_printf(cn, "WARNING might be - %lx %s\n", n->inode, n->filename);
					if(st.st_ino == n->inode) 
						conn_printf(cn, "WARNING use the above\n");
				}
				conn_printf(cn, "HLNK %s\n", line);
				conn_printf(cn, "foobar\n");
#warning implement me!
			}
			else {
				int fd;
				md5_state_t md5_state;
				md5_init(&md5_state);
				const int buffer_l = 1024; char buffer[buffer_l];
				ssize_t size;
				char md5str[33];
				char modestr[11];

				if((fd = open(line,O_NOATIME))==-1) {
					conn_perror(cn, "WARNING open()");
					conn_printf(cn, "WARNING Can't open file: %s\n", line);
					return;
				}

				// Calcuate MD5
				{
					unsigned char md5bin[16];
					while((size = read(fd, buffer, buffer_l)))
						md5_append(&md5_state, (unsigned char*)buffer, size);
					md5_finish(&md5_state, (unsigned char*)md5bin);

					if(lseek(fd, SEEK_SET, 0)==-1) {
						conn_perror(cn, "ERROR lseek()");
						conn_abort(cn);
						return;
					}

					md5bin2str(md5bin, md5str);
				}

				mode2str(st.st_mode, modestr);
				conn_printf(cn, "PUT %ld %s %s %ld %ld %ld %s\n", 
					st.st_size,
					md5str,
					modestr,
					st.st_atime,
					st.st_ctime,
					st.st_mtime,
					line);
				while((size = read(fd, buffer, buffer_l))) 
					conn_write(cn, buffer, size);
				close(fd);
			}
		}
		else if(S_ISDIR(st.st_mode)) {
			char modestr[11];
			mode2str(st.st_mode, modestr);
			conn_printf(cn, "MKDIR %s %ld %ld %ld %s\n",
				modestr,
				st.st_atime,
				st.st_ctime,
				st.st_mtime,
				line);
		}
		else if(S_ISLNK(st.st_mode)) {
			const int buffer_l = 1024; char buffer[buffer_l];
			conn_printf(cn, "SLNK %s\n", line);

			ssize_t l;
			if((l=readlink(line, buffer, buffer_l))==-1) {
				conn_perror(cn, "WARNING readlink()");
				return;
			}
			buffer[l] = (char)NULL;
			conn_printf(cn, "%s\n", buffer);
		}
		else {
			conn_printf(cn, "WARNING Ignored %s\n", line);
		}
	}
}

static void proto_handle_put(conn_t* cn, const char* line) {
	const char* delim = " ";
	char* saveptr = NULL;
	char* token;
	char* ptr = (char*)line;
	int c = 0;

	ssize_t size = 0;
	mode_t mode = 0;
	time_t mtime = 0;
	time_t ctime = 0;
	time_t atime = 0;
	char* name = NULL;
	char* md5  = NULL;

	while((token = strtok_r(ptr, delim, &saveptr))) {
		switch(c) {
			case 0: size = atol(token); break;
			case 1: md5 = token; break;
			case 2: mode = str2mode(token); break;
			case 3: atime = atol(token); break; 
			case 4: ctime = atol(token); break;
			case 5: mtime = atol(token); break;
			case 6: name = token; break;
		}
		c++;
		ptr = NULL;
	}

	if(c != 7) {
		conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
		conn_abort(cn);
		return ;
	}

	int fd = creat(name, O_CREAT|O_TRUNC);
	if(fd == -1) {
		conn_perror(cn, "WARNING creat()");
		return;
	}

	if(chmod(name,mode)==-1) {
		perror("WARNING chmod()");
	}

	struct utimbuf t;
	t.actime = atime;
	t.modtime = mtime;
	if(utime(name,&t)==-1) {
		perror("WARNING utime");
	}

	// CONTENT
	int bytes_left = size;
	int r;
	md5_state_t md5_state;
	unsigned char md5bin[16];
	char md5str[33];
	md5_init(&md5_state);
	while(!cn->abort && bytes_left) {
		if(cn->rbuf_len == 0) 
			(void)conn_read(cn);

		r = MIN(bytes_left, cn->rbuf_len);
		if(r) {
			write(fd, cn->rbuf, r);
			md5_append(&md5_state, (unsigned char*)cn->rbuf, r);
			conn_shift(cn,r);
			bytes_left -= r;
		}

		assert(bytes_left >= 0);
	}

	close(fd);
	md5_finish(&md5_state, (unsigned char*)md5bin);
	md5bin2str(md5bin, md5str);

	// Check md5
	if(strcmp(md5str,md5)!=0) {
		// Mismatch!
		conn_printf(cn, "WARNING %s md5-mismatch (%s <-> %s), removing file\n", 
			name,
			md5str, md5);
		if(unlink(name)==-1) {
			perror("WARNING: unlink()");
		}
	}
	else {
		struct utimbuf t;
		t.actime = atime;
		t.modtime = mtime;
		if(utime(name,&t)==-1) {
			perror("utime");
			conn_printf(cn, "WARNING Can't timestamp on directory: %s\n", name);
		}
		// md5 is fine
		conn_printf(cn, "GET %s\n", name);
	}
}

// My own implementation of dirname(), since _GNU_SOURCE only got basename()
static char* dirname(const char* src) {
	static char result[PATH_MAX];

	if(strcmp(src,".")==0) {
		strcpy(result, ".");
	}
	else if(strcmp(src,"..")==0) {
		strcpy(result, ".");
	}
	else if(strcmp(src,"/")==0) {
		strcpy(result, "/");
	}
	else {
		strcpy(result, src);
		int l = strlen(result);

		// strip tailing / if any
		int i;
		for(i=l-1; i>=0 && result[i] != '/'; i--)  
			result[i] = (char)NULL;
			
		// strip until next / (reverse) - this will remove the basename()
		for(; i>=0 && result[i] != '/'; i--)
			result[i] = (char)NULL;

		if(result[0] == (char)NULL)
			strcpy(result, ".");
	}

	return result;
}

static void proto_handle_slnk(conn_t* cn, const char* line) {
	const int curdir_l = 1024; char curdir[curdir_l];

	// read next line to get target
	const int target_l = 1024; char target[target_l];
	if(!conn_readline(cn, target, target_l))
		return;

	getcwd(curdir, curdir_l);

	// Make sure it dosnt exist
	unlink(line);

	if(chdir(dirname(line))==-1) {
		conn_perror(cn, "WARNING chdir()");
		return;
	}

	if(symlink(target, basename(line))==-1) {
		conn_perror(cn, "ERROR symlink()");
		conn_abort(cn);
		return;
	}

	chdir(curdir);

	conn_printf(cn, "GET %s\n", line);
}

static void proto_delegator(conn_t* cn, const char* line, hlink_t **hardlinks) {
	if(memcmp(line, "ERROR ",6) == 0)
		proto_handle_error(cn, line+6);
	else if(memcmp(line, "WARNING ",8) == 0)
		proto_handle_warning(cn, line+8);
	else if(cn->handshaked == 0) {
		if(memcmp(line, "HELLO ",6) == 0) {
			proto_handle_hello(cn, line+6);
		}
		else {
			conn_printf(cn, "ERROR Protocol violation (%d) '%s'\n", __LINE__, line);
			conn_abort(cn);
		}
	}
	else {
		if(memcmp(line, "GETTIME\0",8) == 0) {
			proto_handle_gettime(cn);
		}
		else if(memcmp(line, "SCAN ",5) == 0) {
			proto_handle_scan(cn, line+5, hardlinks);
		}
		else if(memcmp(line, "NEWERTHAN ",10) == 0) {
			proto_handle_newerthan(cn, line+10);
		}
		else if(memcmp(line, "MKDIR ",6) == 0) {
			proto_handle_mkdir(cn, line+6);
		}
		else if(memcmp(line, "GET ",4) == 0) {
			proto_handle_get(cn, line+4, *hardlinks);
		}
		else if(memcmp(line, "PUT ",4) == 0) {
			proto_handle_put(cn, line+4);
		}
		else if(memcmp(line, "SLNK ", 5) == 0) {
			proto_handle_slnk(cn, line+5);
		}
		else {
			conn_printf(cn, "ERROR Protocol violation (%d)\n", __LINE__);
			conn_abort(cn);
		}
	}
}

int slave(context_t* ctx) {
	hlink_t*	hardlinks = NULL;	// The hardlinks we found while performing SCAN
	conn_t cn;

	// Config
	(void)ctx;
	conn_init(&cn);
	cn.infd = 0;
	cn.outfd = 1;

	// Main
	const int line_len = 1024; char line[1024];

	ssize_t l;
	while((l = conn_readline(&cn, line, line_len))) {
		proto_delegator(&cn,line,&hardlinks);
	}

	// close down
	conn_free(&cn);

	// Free hardlinks
	hlink_t* n = NULL;
	hlink_t* p;
	for(p = hardlinks; p; p = n) {
		n = (hlink_t*)((node_t*)p)->next;
		free(p);
	}
	hardlinks = NULL;

	return 0;
}
