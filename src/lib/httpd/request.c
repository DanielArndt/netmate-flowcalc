
/*! \file   request.c

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
  http request handling

  $Id: request.c 748 2009-09-10 02:54:03Z szander $

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
#include <ctype.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "httpd.h"

/* ---------------------------------------------------------------------- */

int read_header(struct REQUEST *req, int pipelined)
{
    int             rc;
    char            *h;

  restart:

   
#ifdef USE_SSL
    if (with_ssl) {
        rc = ssl_read(req, req->hreq + req->hdata, MAX_HEADER - req->hdata);
    } else 
#endif
      {
          rc = read(req->fd, req->hreq + req->hdata, MAX_HEADER - req->hdata);
      }
    switch (rc) {
    case -1:
        if (errno == EAGAIN) {
            if (pipelined) {
                break; /* check if there is already a full request */
            } else {
                return rc;
            }
        }
        if (errno == EINTR) {
            goto restart;
        }
        log_error_func(1,LOG_INFO,"read",req->peerhost);
        /* fall through */
    case 0:
        req->state = STATE_CLOSE;
        return rc;
    default:
        req->hdata += rc;
        req->hreq[req->hdata] = 0;
    }
  

    /* check if this looks like a http request after
       the first few bytes... */
    if (strncmp(req->hreq,"GET ",4)  != 0  &&
        strncmp(req->hreq,"PUT ",4)  != 0  &&
        strncmp(req->hreq,"HEAD ",5) != 0  &&
        strncmp(req->hreq,"POST ",5) != 0) {
        mkerror(req,400,0);
        return -1;
    }
    
    /* header complete ?? */
    if (NULL != (h = strstr(req->hreq,"\r\n\r\n")) ||
        NULL != (h = strstr(req->hreq,"\n\n"))) {
        if (*h == '\r') {
            h += 4;
            *(h-2) = 0;
        } else {
            h += 2;
            *(h-1) = 0;
        }

        /* header length */
        req->lreq  = h - req->hreq;
        req->state = STATE_PARSE_HEADER;

        /* pointer to body */
        req->breq = h;
        /* first part of body */
        req->lbreq = req->hdata - req->lreq;

        return 0;
    }
    
    if (req->hdata == MAX_REQ) {
        /* oops: buffer full, but found no complete request ... */
        mkerror(req,400,0);
        return -1;
    }
    return rc;
}

int read_body(struct REQUEST *req, int pipelined)
{
    int rc;

  restart:
#ifdef USE_SSL
    if (with_ssl) {
        rc = ssl_read(req, req->hreq + req->hdata, MAX_REQ - req->hdata);
    } else 
#endif
      {
          rc = read(req->fd, req->hreq + req->hdata, MAX_REQ - req->hdata);
      }
    switch (rc) {
    case -1:
        if (errno == EAGAIN) {
            if (pipelined) {
                break; /* check if there is already a full request */
            } else {
                return rc;
            }
        }
        if (errno == EINTR) {
            goto restart;
        }
        log_error_func(1,LOG_INFO,"read",req->peerhost);
        /* fall through */
    case 0:
        req->state = STATE_CLOSE;
        return rc;
    default:
        req->hdata += rc;
        req->hreq[req->hdata] = 0;
    }
    
    /* body complete */ 
    if ((req->hdata - req->lreq) == req->clen) {
        req->state = STATE_PARSE_BODY;
        req->breq[req->clen] = 0;
        req->lbreq = req->clen;

        return 0;
    }
    
    if (req->hdata == MAX_REQ) {
        /* oops: buffer full, but found no complete request ... */
        mkerror(req,400,0);
        return -1;
    }
    return rc;
}


/* ---------------------------------------------------------------------- */

static time_t parse_date(char *line)
{
    static char *m[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    char month[3];
    struct tm tm;
    int i;

    memset(&tm, 0, sizeof(tm));

    line = strchr(line,' '); /* skip weekday */
    if (NULL == line) {
        return -1;
    }
    line++;

    /* first: RFC 1123 date ... */
    if (6 != sscanf(line,"%2d %3s %4d %2d:%2d:%2d GMT",
                    &tm.tm_mday,month,&tm.tm_year,
                    &tm.tm_hour,&tm.tm_min,&tm.tm_sec)) {
        /* second: RFC 1036 date ... */
        if (6 != sscanf(line,"%2d-%3s-%2d %2d:%2d:%2d GMT",
                        &tm.tm_mday,month,&tm.tm_year,
                        &tm.tm_hour,&tm.tm_min,&tm.tm_sec)) {
            /* third: asctime() format */
            if (6 != sscanf(line,"%3s %2d %2d:%2d:%2d %4d",
                            month,&tm.tm_mday,
                            &tm.tm_hour,&tm.tm_min,&tm.tm_sec,
                            &tm.tm_year)) {
                /* none worked :-( */
                return -1;
            }
        }
    }
    for (i = 0; i <= 11; i++) {
        if (0 == strcmp(month,m[i])) {
            break;
        }
    }
    tm.tm_mon = i;
    if (tm.tm_year > 1900) {
        tm.tm_year -= 1900;
    }

    return mktime(&tm);
}

static off_t parse_off_t(char *str, int *pos)
{
    off_t value = 0;

    while (isdigit((int)str[*pos])) {
        value *= 10;
        value += str[*pos] - '0';
        (*pos)++;
    }
    return value;
}

static int parse_ranges(struct REQUEST *req)
{
    char *h,*line = req->range_hdr;
    int  i,off;

    if (req->if_range > 0 && req->if_range != req->bst.st_mtime) {
        return 0;
    }

    for (h = line, req->ranges=1; *h != '\n' && *h != '\0'; h++) {
        if (*h == ',') {
            req->ranges++;
        }
    }
#ifdef DEBUG
	fprintf(stderr,"%03d/%d: %d ranges:",req->fd,req->state,req->ranges);
#endif
    req->r_start = malloc(req->ranges*sizeof(off_t));
    req->r_end   = malloc(req->ranges*sizeof(off_t));
    req->r_head  = malloc((req->ranges+1)*BR_HEADER);
    req->r_hlen  = malloc((req->ranges+1)*sizeof(int));
    if (NULL == req->r_start || NULL == req->r_end ||
        NULL == req->r_head  || NULL == req->r_hlen) {
        if (req->r_start) free(req->r_start);
        if (req->r_end)   free(req->r_end);
        if (req->r_head)  free(req->r_head);
        if (req->r_hlen)  free(req->r_hlen);
#ifdef DEBUG
	    fprintf(stderr,"oom\n");
#endif
        return 500;
    }
    for (i = 0, off=0; i < req->ranges; i++) {
        if (line[off] == '-') {
            off++;
            if (!isdigit((int)line[off])) {
                goto parse_error;
            }
            req->r_start[i] = req->bst.st_size - parse_off_t(line,&off);
            req->r_end[i]   = req->bst.st_size;
        } else {
            if (!isdigit((int)line[off])) {
                goto parse_error;
            }
            req->r_start[i] = parse_off_t(line,&off);
            if (line[off] != '-') {
                goto parse_error;
            }
            off++;
            if (isdigit((int)line[off])) {
                req->r_end[i] = parse_off_t(line,&off) +1;
            } else {
                req->r_end[i] = req->bst.st_size;
            }
        }
        off++; /* skip "," */
        /* ranges ok? */
#ifdef DEBUG
	    fprintf(stderr," %d-%d",
                (int)(req->r_start[i]),
                (int)(req->r_end[i]));
#endif
        if (req->r_start[i] > req->r_end[i] ||
            req->r_end[i]   > req->bst.st_size) {
            goto parse_error;
        }
    }
#ifdef DEBUG
	fprintf(stderr," ok\n");
#endif
    return 0;

  parse_error:
    req->ranges = 0;
#ifdef DEBUG
	fprintf(stderr," range error\n");
#endif
    return 400;
}

/* delete unneeded path elements */
static void fixpath(char *path)
{
    char *dst = path;
    char *src = path;

    for (;*src;) {
        if (0 == strncmp(src,"//",2)) {
            src++;
            continue;
        }
        if (0 == strncmp(src,"/./",3)) {
            src+=2;
            continue;
        }
        *(dst++) = *(src++);
    }
    *dst = 0;
}

static int base64_table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, 
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, 
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, 
};

static void decode_base64(unsigned char *dest, unsigned char *src, int maxlen)
{
    int a,b,d;

    for (a=0, b=0, d=0; *src != 0 && d < maxlen; src++) {
        if (*src >= 128 || -1 == base64_table[*src]) {
            break;
        }
        a = (a<<6) | base64_table[*src];
        b += 6;
        if (b >= 8) {
            b -= 8;
            dest[d++] = (a >> b) & 0xff;
        }
    }
    dest[d] = 0;
}

void parse_request(struct REQUEST *req, char *server_host)
{
    char filename[4097], proto[5], *h;
    int  port,rc;
    time_t t;
    /*struct passwd *pw=NULL;*/
    
#ifdef DEBUG
	fprintf(stderr,"%s\n",req->hreq);
#endif
    /* parse request. Hehe, scanf is powerfull :-) */
    if (4 != sscanf(req->hreq, "%4[A-Z] %2048[^ \t\r\n] HTTP/%d.%d",
                    req->type,filename,&(req->major),&(req->minor))) {
        mkerror(req,400,0);
        return;
    }
    if (filename[0] == '/') {
        strcpy(req->uri,filename);
    } else {
        port = 0;
        *proto = 0;
        if (4 != sscanf(filename,
                        "%4[a-zA-Z]://%64[a-zA-Z0-9.-]:%d%2048[^ \t\r\n]",
                        proto,req->hostname,&port,req->uri) &&
            3 != sscanf(filename,
                        "%4[a-zA-Z]://%64[a-zA-Z0-9.-]%2048[^ \t\r\n]",
                        proto,req->hostname,req->uri)) {
            mkerror(req,400,0);
            return;
        }
        if (*proto != 0 && 0 != strcasecmp(proto,"http")) {
            mkerror(req,400,0);
            return;
        }
    }

    unquote(req->path,req->query,req->uri);
    fixpath(req->path);
#ifdef DEBUG
	fprintf(stderr,"%03d/%d: %s \"%s\" HTTP/%d.%d\n",
            req->fd, req->state, req->type, req->path, req->major, req->minor);
#endif

    if (0 != strcmp(req->type,"GET") &&
        0 != strcmp(req->type,"HEAD") &&
        0 != strcmp(req->type,"POST")) {
        mkerror(req,501,0);
        return;
    }

    if (0 == strcmp(req->type,"HEAD")) {
        req->head_only = 1;
    }

    /* checks */
    if (req->path[0] != '/') {
        mkerror(req,400,0);
        return;
    }
    if (strstr(req->path,"/../")) {
        mkerror(req,403,1);
        return;
    }

    /* parse header lines */
    req->keep_alive = req->minor;
    for (h = req->hreq; h - req->hreq < req->lreq;) {
        h = strchr(h,'\n');
        if (NULL == h) {
            break;
        }
        h++;

        h[-2] = 0;
        h[-1] = 0;
        list_add(&req->header,h,0);

        if (strncasecmp(h,"Connection: ",12) == 0) {
            req->keep_alive = (strncasecmp(h+12,"Keep-Alive",10)== 0);

        } else if (strncasecmp(h,"Host: ",6) == 0) {
            sscanf(h+6,"%64[a-zA-Z0-9.-]",req->hostname);

        } else if (strncasecmp(h,"If-Modified-Since: ",19) == 0) {
            if ((t = parse_date(h+19)) != -1) {
                req->if_modified = t;
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: if-modified-since: %s",
                        req->fd,req->state,ctime(&t));
#endif
            }

        } else if (strncasecmp(h,"If-Unmodified-Since: ",21) == 0) {
            if ((t = parse_date(h+21)) != -1) {
                req->if_unmodified = t;
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: if-unmodified-since: %s",
                        req->fd,req->state,ctime(&t));
#endif
            }

        } else if (strncasecmp(h,"If-Range: ",10) == 0) {
            if ((t = parse_date(h+10)) != -1) {
                req->if_range = t;
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: if-range: %s\n",req->fd,req->state,ctime(&t));
#endif
            }

        } else if (strncasecmp(h,"Authorization: Basic ",21) == 0) {
            decode_base64(req->auth,h+21,63);
#ifdef DEBUG
            fprintf(stderr,"%03d/%d: auth: %s\n",req->fd,req->state,req->auth);
#endif
	    
        } else if (strncasecmp(h,"Range: bytes=",13) == 0) {
            /* parsing must be done after fstat, we need the file size
               for the boundary checks */
            req->range_hdr = h+13;
        } else if (strncasecmp(h,"Content-Type: ",14) == 0) {
            req->ctype = h+14;
        } else if (strncasecmp(h,"Content-Length: ",16) == 0) {
            sscanf(h+16,"%d", &req->clen);
        }
    }

    /* take care about the hostname */
    if (req->hostname[0] == '\0') {
        strncpy(req->hostname,server_host,64);
    }

    /* lowercase hostname */
    for (h = req->hostname; *h != 0; h++) {
        if (*h < 'A' || *h > 'Z')
            continue;
        *h += 32;
    }

    /* check basic user auth */
    if (access_check_func != NULL) {
        if (access_check_func(NULL, req->auth) < 0) {
            mkerror(req,401,1);
            return;
        }
    }
   
    /* generate the resource name */
    h = filename -1 +sprintf(filename,"%s", req->path);

    /* immediatly read available body (part) */
    while (read_body(req, 0) > 0);

    if (strcmp(req->type,"POST") == 0) {
        if (strcmp(req->ctype,"application/x-www-form-urlencoded") == 0) {
            if (req->lbreq < req->clen) {
                /* read rest of body */
                req->state = STATE_READ_BODY;
                return;
            }
            /* else unquote body and proceed */
            unquote2(req->post_body,req->breq);
            req->state = STATE_PROCESS;
        } else {
            mkerror(req,500,1);
            return;
        }
    }

    /* FIXME: support validation? */
#if 0
    if (req->if_modified > 0 && req->if_modified == ) {
        /* 304 not modified */
        mkheader(req,304,t,-1);
        req->head_only = 1;
    }
#endif 

    if (req->range_hdr) {
        if (0 != (rc = parse_ranges(req))) {
            mkerror(req,rc,1);
            return;
        }
    }

    /* request callback */
    if (parse_request_func != NULL) {
        if (parse_request_func(req) < 0) {
            mkerror(req,404,1);
            return;
        }
    }

    /* no file handling anymore... */

    return;
}

void parse_request_body(struct REQUEST *req)
{
    /* unquote body */
    unquote2(req->post_body,req->breq);

    /* request callback */
    if (parse_request_func != NULL) {
        if (parse_request_func(req) < 0) {
            mkerror(req,404,1);
            return;
        }
    }
}
