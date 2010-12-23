
/*! \file   httpd.h

  Based on webfs Copyright (C) 1999-2001  Gerd Knorr

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License with
  the Debian GNU/Linux distribution in file /usr/share/common-licenses/GPL;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place,
  Suite 330, Boston, MA  02111-1307  USA

  Description:
  http server public interface

  $Id: httpd.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef __HTTPD_H
#define __HTTPD_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/stat.h>
#ifdef HTTPD_USE_THREADS
# include <pthread.h>
#endif

/* states of a http transaction */
#define STATE_READ_HEADER   1
#define STATE_PARSE_HEADER  2
#define STATE_READ_BODY     3
#define STATE_PARSE_BODY    4
#define STATE_PROCESS       5
#define STATE_WRITE_HEADER  6
#define STATE_WRITE_BODY    7
#define STATE_WRITE_FILE    8
#define STATE_WRITE_RANGES  9
#define STATE_FINISHED      10

#define STATE_KEEPALIVE     11
#define STATE_CLOSE         12


#ifdef USE_SSL
# include <openssl/ssl.h>
#endif

/* max header length */
#define MAX_HEADER 4096
/* max header + body */
#define MAX_REQ 128000  
#define BR_HEADER 512

/* timeout for dns queries */
extern int g_timeout;

struct REQUEST {
    int	        fd;		     /* socket handle */
    int	        state;	             /* what to to ??? */
    time_t      ping;                /* last read/write (for timeouts) */
    int         keep_alive;
    int		tcp_cork;

    struct sockaddr_storage peer;         /* client (log) */
    char        peerhost[65];
    char        peerserv[9];
    
    /* request */
    char	hreq[MAX_REQ+1];  /* request header */
    int 	lreq;		     /* request length */
    int         hdata;               /* data in hreq */
    char        type[16];            /* req type */
    char        hostname[65];        /* hostname */
    char	uri[2049];           /* req uri */
    char	path[2049];          /* file path */
    char	query[2049];         /* query string */
    int         major,minor;         /* http version */
    char        auth[64];
    struct strlist *header;
    time_t      if_modified;
    time_t      if_unmodified;
    time_t      if_range;
    char        *range_hdr;
    int         ranges;
    off_t       *r_start;
    off_t       *r_end;
    char        *r_head;
    int         *r_hlen;
    char        *ctype; /* content type in request */
    int         clen;  /* content length */
    char        *breq; /* pointer to request body (POST) */
    char        post_body[MAX_REQ+1];
    int         lbreq;

    /* response */
    int         status;              /* status code (log) */
    int         bc;                  /* byte counter (log) */
    char	hres[MAX_HEADER+1];  /* response header */
    int	        lres;		     /* header length */
    char        *mime;               /* mime type */
    char	*body;
    int         lbody;
    int         bfd;                 /* file descriptor */
    struct stat bst;                 /* file info */
    off_t       written;
    int         head_only;
    int         rh,rb;
    int         lifespan;            /* life time of response */


#ifdef USE_SSL
    /* SSL */
    SSL		*ssl_s;
    BIO		*io;
    BIO		*bio_in;
#endif

    /* linked list */
    struct REQUEST *next;
};

/* --- string lists --------------------------------------------- */

struct strlist {
    struct strlist *next;
    char *line;
    int free_the_mallocs;
};

/* add element (list head) */
static void inline
list_add(struct strlist **list, char *line, int free_the_mallocs)
{
    struct strlist *elem = (struct strlist *) malloc(sizeof(struct strlist));
    memset(elem,0,sizeof(struct strlist));
    elem->next = *list;
    elem->line = line;
    elem->free_the_mallocs = free_the_mallocs;
    *list = elem;
}

/* free whole list */
static void inline
list_free(struct strlist **list)
{
    struct strlist *elem,*next;

    for (elem = *list; NULL != elem; elem = next) {
        next = elem->next;
        if (elem->free_the_mallocs) {
            free(elem->line);
        }
        free(elem);
    }
    *list = NULL;
}

/* --- main.c --------------------------------------------------- */

/* callback functions */

/* access check */
typedef  int (*access_check_func_t)(char *host, char *user);
/* request parsing */
typedef  int (*parse_request_func_t)(struct REQUEST *req);
/* request logging */
typedef  int (*log_request_func_t)(struct REQUEST *req, time_t now);
/* error logging */
typedef int (*log_error_func_t)(int eno, int loglevel, char *txt, char *peerhost);

extern access_check_func_t access_check_func;
extern parse_request_func_t parse_request_func;
extern log_request_func_t log_request_func;
extern log_error_func_t log_error_func;

/* globals */
extern char *server_name;
extern time_t now;

#ifdef USE_SSL
extern int      with_ssl;
#endif


/* struct for fd change signaling */

typedef struct
{
  int fd;
  int mode;
} fd_t;

enum mode
  {
    FD_RD = 0x01,
    FD_WT = 0x02,
    FD_RW = 0x03
  };

typedef struct
{
    int max;
    fd_set rset;
    fd_set wset;
} fd_sets_t;



/* register access check callback */
int httpd_register_access_check(access_check_func_t f);
/* register log request callback */
int httpd_register_log_request(log_request_func_t f);
/* register log error callback */
int httpd_register_log_error(log_error_func_t f);
/* register parse request callback */
int httpd_register_parse_request(parse_request_func_t f);
/* get keepalive timeout */
int httpd_get_keepalive();
/* return 1 if httpd uses SSL */
int httpd_uses_ssl();
/* initialize http server */
int httpd_init(int sport, char *sname, int use_ssl, const char *certificate, 
	       const char *password, int use_v6);
/* handle file descriptor event */
int httpd_handle_event(fd_set *rset, fd_set *wset, fd_sets_t *fds);
/* send response */
int httpd_send_response(struct REQUEST *req, fd_sets_t *fds);
/* send response immediatly */
int httpd_send_immediate_response(struct REQUEST *req);
/* shutdown http server */
void httpd_shutdown();

/* --- ssl.c ---------------------------------------------------- */

#ifdef USE_SSL
int ssl_read(struct REQUEST *req, char *buf, int len);
int ssl_write(struct REQUEST *req, char *buf, int len);
int ssl_blk_write(struct REQUEST *req, int offset, int len);
void init_ssl(const char *certificate, const char *password);
void open_ssl_session(struct REQUEST *req);
#endif

/* --- request.c ------------------------------------------------ */

int read_header(struct REQUEST *req, int pipelined);
int read_body(struct REQUEST *req, int pipelined);
void parse_request(struct REQUEST *req, char *server_host);
void parse_request_body(struct REQUEST *req);

/* --- response.c ----------------------------------------------- */

void mkerror(struct REQUEST *req, int status, int ka);
void mkredirect(struct REQUEST *req, int tcp_port);
void mkheader(struct REQUEST *req, int status, time_t mtime);
void write_request(struct REQUEST *req);

/* --- quote.c ----------------------------------------------------- */

void init_quote(void);
char*  quote(unsigned char *path, int maxlength);
void unquote(unsigned char *path, unsigned char *qs, unsigned char *src);
void unquote2(unsigned char *dst, unsigned char *src);

/* --- mime.c --------------------------------------------------- */

char* get_mime(char *file);
void  init_mime(char *file, char *def);
void shutdown_mime();

/* -------------------------------------------------------------- */

#ifdef HTTPD_USE_THREADS
# define INIT_LOCK(mutex)	pthread_mutex_init(&mutex,NULL)
# define FREE_LOCK(mutex)	pthread_mutex_destroy(&mutex)
# define DO_LOCK(mutex)		pthread_mutex_lock(&mutex)
# define DO_UNLOCK(mutex)	pthread_mutex_unlock(&mutex)
# define INIT_COND(cond)	pthread_cond_init(&cond,NULL)
# define FREE_COND(cond)	pthread_cond_destroy(&cond)
# define BCAST_COND(cond)	pthread_cond_broadcast(&cond);
# define WAIT_COND(cond,mutex)	pthread_cond_wait(&cond,&mutex);
#else
# define INIT_LOCK(mutex)	/* nothing */
# define FREE_LOCK(mutex)	/* nothing */
# define DO_LOCK(mutex)		/* nothing */
# define DO_UNLOCK(mutex)	/* nothing */
# define INIT_COND(cond)	/* nothing */
# define FREE_COND(cond)	/* nothing */
# define BCAST_COND(cond)	/* nothing */
# define WAIT_COND(cond,mutex)	/* nothing */
#endif

#ifdef __cplusplus
}
#endif

#endif
