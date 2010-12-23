
/*! \file   response.c

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
  http response handling

  $Id: response.c 748 2009-09-10 02:54:03Z szander $

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "httpd.h"

/* ---------------------------------------------------------------------- */
/* os-specific sendfile() wrapper                                         */

/*
 * int xsendfile(out,in,offset,bytes)
 *
 *	out    - outgoing filedescriptor (i.e. the socket)
 *	in     - incoming filedescriptor (i.e. the file to send out)
 *	offset - file offset (where to start)
 *      bytes  - number of bytes to send
 *
 * return value
 *	on error:   -1 and errno set.
 *	on success: the number of successfully written bytes (which might
 *		    be smaller than bytes, we are doing nonblocking I/O).
 *	extra hint: much like write(2) works.
 *
 */

#if defined(__linux__) && !defined(NO_SENDFILE)

# include <sys/sendfile.h>
static int xsendfile(int out, int in, off_t offset, size_t bytes)
{
    return sendfile(out, in, &offset, bytes);
}

#elif defined(__FreeBSD__) && !defined(NO_SENDFILE)

static int xsendfile(int out, int in, off_t offset, size_t bytes)
{
    off_t nbytes = 0;

    if (sendfile(in, out, offset, bytes, NULL, &nbytes, 0) == -1) {
        /* Why the heck FreeBSD returns an /error/ if it has done a partial
           write?  With non-blocking I/O this absolutely normal behavoir and
           no error at all.  Stupid. */
        if (errno == EAGAIN && nbytes > 0) {
            return nbytes;
        }
        return -1;
    }
    return nbytes;
}
#else

# warning using slow sendfile() emulation.

/* Poor man's sendfile() implementation. Performance sucks, but it works. */
#define BUFSIZE 16384

static int xsendfile(int out, int in, off_t offset, size_t bytes)
{
    char buf[BUFSIZE];
    ssize_t nread;
    ssize_t nsent, nsent_total;

    if (lseek(in, offset, SEEK_SET) == -1) {
#ifdef DEBUG
	    perror("lseek");
#endif
        return -1;
    }

    nsent = nsent_total = 0;
    for (;bytes > 0;) {
        /* read a block */
        nread = read(in, buf, (bytes < BUFSIZE) ? bytes : BUFSIZE);
        if (nread == -1) {
#ifdef DEBUG
            perror("read");
#endif
            return nsent_total ? nsent_total : -1;
        }
        if (nread == 0) {
            break;
        }

        /* write it out */
        nsent = write(out, buf, nread);
        if (nsent == -1) {
            return nsent_total ? nsent_total : -1;
        }

        nsent_total += nsent;
        if (nsent < nread) {
            /* that was a partial write only.  Queue full.  Bailout here,
               the next write would return EAGAIN anyway... */
            break;
        }
    
        bytes -= nread;
    }
    return nsent_total;
}

#endif

/* ---------------------------------------------------------------------- */

#ifdef USE_SSL

static inline int wrap_xsendfile(struct REQUEST *req, off_t off, size_t bytes)
{
    if (with_ssl) {
        return ssl_blk_write(req,off,bytes);
    } else {
        return xsendfile(req->fd, req->bfd, off, bytes);
    }
}

static inline int wrap_write(struct REQUEST *req, void *buf, size_t bytes)
{
    if (with_ssl) {
        return ssl_write(req,buf,bytes);
    } else {
        return write(req->fd, buf, bytes);
    }
}

#else

static inline int wrap_xsendfile(struct REQUEST *req, off_t off, size_t bytes)
{
    return xsendfile(req->fd,req->bfd,off,bytes);
}

static inline int wrap_write(struct REQUEST *req, void *buf, size_t bytes)
{
    return write(req->fd,buf,bytes);
}
#endif

/* ---------------------------------------------------------------------- */

static struct HTTP_STATUS {
    int   status;
    char *head;
    char *body;
} http[] = {
    { 200, "200 OK",                       NULL },
    { 206, "206 Partial Content",          NULL },
    { 304, "304 Not Modified",             NULL },
    { 400, "400 Bad Request",              "*PLONK*\n" },
    { 401, "401 Authentication required",  "Authentication required\n" },
    { 403, "403 Forbidden",                "Access denied\n" },
    { 404, "404 Not Found",                "File or directory not found\n" },
    { 408, "408 Request Timeout",          "Request Timeout\n" },
    { 412, "412 Precondition failed.",     "Precondition failed\n" },
    { 500, "500 Internal Server Error",    "Sorry folks\n" },
    { 501, "501 Not Implemented",          "Sorry folks\n" },
    {   0, NULL,                        NULL }
};

/* ---------------------------------------------------------------------- */

#define RESPONSE_START			\
	"HTTP/1.1 %s\r\n"		\
	"Server: %s\r\n"		\
	"Connection: %s\r\n"		\
	"Accept-Ranges: bytes\r\n"
#define RFCTIME				\
	"%a, %d %b %Y %H:%M:%S GMT"
#define BOUNDARY			\
	"XXX_CUT_HERE_%ld_XXX"

void mkerror(struct REQUEST *req, int status, int ka)
{
    int i;
    for (i = 0; http[i].status != 0; i++) {
        if (http[i].status == status) {
            break;
        }
    }
    req->status = status;
    req->body   = http[i].body;
    req->lbody  = strlen(req->body);
    if (!ka) {
        req->keep_alive = 0;
    }
    req->lres = sprintf(req->hres,
                        RESPONSE_START
                        "Content-Type: text/plain\r\n"
                        "Content-Length: %d\r\n",
                        http[i].head,server_name,
                        req->keep_alive ? "Keep-Alive" : "Close",
                        req->lbody);
    if (401 == status) {
        req->lres += sprintf(req->hres+req->lres,
                             "WWW-Authenticate: Basic realm=\"NetMate\"\r\n");
    }
    req->lres += strftime(req->hres+req->lres,80,
                          "Date: " RFCTIME "\r\n\r\n",
                          gmtime(&now));
    req->state = STATE_WRITE_HEADER;
#ifdef DEBUG
	fprintf(stderr,"%03d/%d: error: %d, connection=%s\n",
            req->fd, req->state, status, req->keep_alive ? "Keep-Alive" : "Close");
#endif
}

void mkredirect(struct REQUEST *req, int tcp_port)
{
    req->status = 302;
    req->body   = req->path;
    req->lbody  = strlen(req->body);
    req->lres = sprintf(req->hres,
                        RESPONSE_START
                        "Location: http://%s:%d%s\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: %d\r\n",
                        "302 Redirect",server_name,
                        req->keep_alive ? "Keep-Alive" : "Close",
                        req->hostname,tcp_port,quote(req->path,9999),
                        req->lbody);
    req->lres += strftime(req->hres+req->lres,80,
                          "Date: " RFCTIME "\r\n\r\n",
                          gmtime(&now));
    req->state = STATE_WRITE_HEADER;
#ifdef DEBUG
	fprintf(stderr,"%03d/%d: 302 redirect: %s, connection=%s\n",
            req->fd, req->state, req->path, req->keep_alive ? "Keep-Alive" : "Close");
#endif
}

static int mkmulti(struct REQUEST *req, int i)
{
    req->r_hlen[i] = sprintf(req->r_head+i*BR_HEADER,
                             "\r\n--" BOUNDARY "\r\n"
                             "Content-type: %s\r\n"
#if (SIZEOF_OFF_T == 4)
                             "Content-range: bytes %ld-%ld/%ld\r\n"
#else
                             "Content-range: bytes %lld-%lld/%lld\r\n"
#endif
                             "\r\n",
                             (unsigned long)now, req->mime,
                             req->r_start[i],req->r_end[i]-1,req->bst.st_size);
#ifdef DEBUG
#if (SIZEOF_OFF_T == 4)
	fprintf(stderr,"%03d/%d: send range: %ld-%ld/%ld (%ld byte)\n",
#else
    fprintf(stderr,"%03d/%d: send range: %lld-%lld/%lld (%lld byte)\n",       
#endif
            req->fd, req->state,
            req->r_start[i],req->r_end[i],req->bst.st_size,
            req->r_end[i]-req->r_start[i]);
#endif
    return req->r_hlen[i];
}

void mkheader(struct REQUEST *req, int status, time_t mtime)
{
    int    i;
    off_t  len;
    time_t expires;

    for (i = 0; http[i].status != 0; i++) {
        if (http[i].status == status) {
            break;
        }
    }
    req->status = status;
    req->lres = sprintf(req->hres,
                        RESPONSE_START,
                        http[i].head,server_name,
                        req->keep_alive ? "Keep-Alive" : "Close");

    if (req->ranges == 0) {
        req->lres += sprintf(req->hres+req->lres,
                             "Content-Type: %s\r\n"
#if (SIZEOF_OFF_T == 4)
                             "Content-Length: %ld\r\n",
#else
                             "Content-Length: %lld\r\n",
#endif
                             req->mime,
                             req->body ? req->lbody : req->bst.st_size);

    } else if (req->ranges == 1) {
        req->lres += sprintf(req->hres+req->lres,
                             "Content-Type: %s\r\n"
#if (SIZEOF_OFF_T == 4)
                             "Content-Range: bytes %ld-%ld/%ld\r\n"
                             "Content-Length: %ld\r\n",
#else
                             "Content-Range: bytes %lld-%lld/%lld\r\n"
                             "Content-Length: %lld\r\n",                        
#endif
                             req->mime,
                             req->r_start[0],req->r_end[0]-1,req->bst.st_size,
                             req->r_end[0]-req->r_start[0]);
    } else {
        for (i = 0, len = 0; i < req->ranges; i++) {
            len += mkmulti(req,i);
            len += req->r_end[i]-req->r_start[i];
        }
        req->r_hlen[i] = sprintf(req->r_head+i*BR_HEADER,
                                 "\r\n--" BOUNDARY "--\r\n",
                                 (unsigned long)now);
        len += req->r_hlen[i];
        req->lres += sprintf(req->hres+req->lres,
                             "Content-Type: multipart/byteranges;"
                             " boundary=" BOUNDARY "\r\n"
#if (SIZEOF_OFF_T == 4)
                             "Content-Length: %ld\r\n",
#else
                             "Content-Length: %lld\r\n",
#endif
                             (unsigned long)now,len);
    }
    if (mtime != -1) {
        req->lres += strftime(req->hres+req->lres,80,
                              "Last-Modified: " RFCTIME "\r\n",
                              gmtime(&mtime));
        if (req->lifespan != -1) {
            expires = mtime + req->lifespan;
            req->lres += strftime(req->hres+req->lres,80,
                                  "Expires: " RFCTIME "\r\n",
                                  gmtime(&expires));
        }
    }
    req->lres += strftime(req->hres+req->lres,80,
                          "Date: " RFCTIME "\r\n\r\n",
                          gmtime(&now));
    req->state = STATE_WRITE_HEADER;

#ifdef DEBUG
	fprintf(stderr,"%03d/%d: %d, connection=%s\n",
            req->fd, req->state, status, req->keep_alive ? "Keep-Alive" : "Close");
#endif
}


void write_request(struct REQUEST *req)
{
    int rc;

    for (;;) {
        switch (req->state) {
        case STATE_WRITE_HEADER:
#ifdef TCP_CORK
            if (req->tcp_cork == 0 && !req->head_only) {
                req->tcp_cork = 1;
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: tcp_cork=%d\n",req->fd,req->state,req->tcp_cork);
#endif
                setsockopt(req->fd,SOL_TCP,TCP_CORK,&req->tcp_cork,sizeof(int));
            }
#endif
            rc = wrap_write(req,req->hres + req->written,
                            req->lres - req->written);
            switch (rc) {
            case -1:
                if (errno == EAGAIN) {
                    return;
                }
                if (errno == EINTR) {
                    continue;
                }
                log_error_func(1,LOG_INFO,"write",req->peerhost);
                /* fall through */
            case 0:
                req->state = STATE_CLOSE;
                return;
            default:
                req->written += rc;
                req->bc += rc;
                if (req->written != req->lres) {
                    return;
                }
            }
            req->written = 0;
            if (req->head_only) {
                req->state = STATE_FINISHED;
                return;
            } else if (req->body) {
                req->state = STATE_WRITE_BODY;
            } else if (req->ranges == 1) {
                req->state = STATE_WRITE_RANGES;
                req->rh = -1;
                req->rb = 0;
                req->written = req->r_start[0];
            } else if (req->ranges > 1) {
                req->state = STATE_WRITE_RANGES;
                req->rh = 0;
                req->rb = -1;
            } else {
                req->state = STATE_WRITE_FILE;
            }
            break;
        case STATE_WRITE_BODY:
            rc = wrap_write(req,req->body + req->written,
                            req->lbody - req->written);
            switch (rc) {
            case -1:
                if (errno == EAGAIN) {
                    return;
                }
                if (errno == EINTR) {
                    continue;
                }
                log_error_func(1,LOG_INFO,"write",req->peerhost);
                /* fall through */
            case 0:
                req->state = STATE_CLOSE;
                return;
            default:
                req->written += rc;
                req->bc += rc;
                if (req->written != req->lbody) {
                    return;
                }
            }
            req->state = STATE_FINISHED;
            return;
        case STATE_WRITE_FILE:
            rc = wrap_xsendfile(req, req->written, 
                                req->bst.st_size - req->written);
            switch (rc) {
            case -1:
                if (errno == EAGAIN) {
                    return;
                }
                if (errno == EINTR) {
                    continue;
                }
                log_error_func(1,LOG_INFO,"sendfile",req->peerhost);
                /* fall through */
            case 0:
                req->state = STATE_CLOSE;
                return;
            default:
#ifdef DEBUG
#if (SIZEOF_OFF_T == 4)
                fprintf(stderr,"%03d/%d: %ld/%ld (%ld%%)\r",req->fd,req->state,
#else
                fprintf(stderr,"%03d/%d: %lld/%lld (%lld%%)\r",req->fd,req->state,
#endif
                        req->written,req->bst.st_size,
                        req->written*100/req->bst.st_size);
#endif
                req->written += rc;
                req->bc += rc;
                if (req->written != req->bst.st_size) {
                    return;
                }
            }
            req->state = STATE_FINISHED;
            return;
        case STATE_WRITE_RANGES:
            if (-1 != req->rh) {
                /* write header */
                rc = wrap_write(req,
                                req->r_head + req->rh*BR_HEADER + req->written,
                                req->r_hlen[req->rh] - req->written);
                switch (rc) {
                case -1:
                    if (errno == EAGAIN) {
                        return;
                    }
                    if (errno == EINTR) {
                        continue;
                    }
                    log_error_func(1,LOG_INFO,"write",req->peerhost);
                    /* fall through */
                case 0:
                    req->state = STATE_CLOSE;
                    return;
                default:
                    req->written += rc;
                    req->bc += rc;
                    if (req->written != req->r_hlen[req->rh]) {
                        return;
                    }
                }
                if (req->rh == req->ranges) {
                    /* done -- no more ranges */
                    req->state = STATE_FINISHED;
                    return;
                }
                /* prepare for body writeout */
                req->rb      = req->rh;
                req->rh      = -1;
                req->written = req->r_start[req->rb];
            }
            if (req->rb != -1) {
                /* write body */
                rc = wrap_xsendfile(req, req->written, 
                                    req->r_end[req->rb] - req->written);
                switch (rc) {
                case -1:
                    if (errno == EAGAIN) {
                        return;
                    }
                    if (errno == EINTR) {
                        continue;
                    }
                    log_error_func(1,LOG_INFO,"sendfile",req->peerhost);
                    /* fall through */
                case 0:
                    req->state = STATE_CLOSE;
                    return;
                default:
                    req->written += rc;
                    req->bc += rc;
                    if (req->written != req->r_end[req->rb]) {
                        return;
                    }
                }
                /* prepare for next subheader writeout */
                req->rh = req->rb+1;
                req->rb = -1;
                req->written = 0;
                if (req->ranges == 1) {
                    /* single range only */
                    req->state = STATE_FINISHED;
                    return;
                }
            }
            break;
        } /* switch(state) */
    } /* for (;;) */
}
