
/*! \file   httpd.c

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
  httpd main functions

  $Id: httpd.c 748 2009-09-10 02:54:03Z szander $

*/

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

#include "httpd.h"

/* public variables - server configuration */

/* mime definitions */
#define MIMEFILE "/etc/mime.types"

/* configuration */
static int     timeout        = 60; /* network timeout */
static int     keepalive_time = 5;  /* keepalive time for connections */
static char    *mimetypes     = MIMEFILE;
static int     max_conn       = 32; /* max simulatneous connections */

/* globals */
static int     tcp_port       = 0;  /* port number */
static char    server_host[256];    /* server name */
static int     slisten = -1;
/* connection list */
static struct REQUEST *conns = NULL;
/* number of connections */
static int curr_conn = 0;

#ifdef HTTPD_USE_THREADS
static int       nthreads = 1;
static pthread_t *threads;
#endif

/* public stuff */
char    *server_name = NULL; /* our name used in responses */
time_t  now = 0;

#ifdef USE_SSL
int	with_ssl = 0;
#endif

/* callback functions */

access_check_func_t access_check_func = NULL;
parse_request_func_t parse_request_func = NULL;
log_request_func_t log_request_func = NULL;
log_error_func_t log_error_func = NULL;


/* queue a OK response for request req
   
*/
int httpd_send_response(struct REQUEST *req, fd_sets_t *fds)
{
    time_t now;

    if (req->body != NULL) {
        now = time(NULL);
        req->lbody = strlen(req->body);
        /* 200 OK */
        mkheader(req,200,now);
    } else {
        mkerror(req,500,1);
    }

    if (req->state == STATE_WRITE_HEADER) {
        // switch to writing
        FD_CLR(req->fd, &fds->rset);
        FD_SET(req->fd, &fds->wset);
    }

    return 0;
}  

/* immediatly send back an OK response to request req
   can be used for short transactions which require no
   further processing of the app
 */
int httpd_send_immediate_response(struct REQUEST *req)
{
    time_t now;

    if (req->body != NULL) {
        now = time(NULL);
        req->lbody = strlen(req->body);
        /* 200 OK */
        mkheader(req,200,now);
    } else {
        mkerror(req,500,1);
    }

    return 0;
}    

/* handle a file descriptor event */
int httpd_handle_event(fd_set *rset, fd_set *wset, fd_sets_t *fds)
{
   
    struct REQUEST      *req, *prev, *tmp;
    int                 length;
    int opt = 0;
   
    now = time(NULL);

    /* new connection ? */
    if ((rset != NULL) && FD_ISSET(slisten, rset)) {
        req = malloc(sizeof(struct REQUEST));
        if (NULL == req) {
            /* oom: let the request sit in the listen queue */
#ifdef DEBUG
            fprintf(stderr,"oom\n");
#endif
        } else {
            memset(req,0,sizeof(struct REQUEST));
            if ((req->fd = accept(slisten,NULL,&opt)) == -1) {
                if (EAGAIN != errno) {
                    log_error_func(1, LOG_WARNING,"accept",NULL);
                }
                free(req);
            } else {
                fcntl(req->fd,F_SETFL,O_NONBLOCK);
                req->bfd = -1;
                req->state = STATE_READ_HEADER;
                req->ping = now;
                req->lifespan = -1;
                req->next = conns;
                conns = req;
                curr_conn++;
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: new request (%d)\n",req->fd,req->state,curr_conn);
#endif
#ifdef USE_SSL
                if (with_ssl) {
                    open_ssl_session(req);
                }
#endif
                length = sizeof(req->peer);
                if (getpeername(req->fd,(struct sockaddr*)&(req->peer),&length) == -1) {
                    log_error_func(1, LOG_WARNING,"getpeername",NULL);
                    req->state = STATE_CLOSE;
                }
                getnameinfo((struct sockaddr*)&req->peer,length,
                            req->peerhost,64,req->peerserv,8,
                            NI_NUMERICHOST | NI_NUMERICSERV);
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: connect from (%s)\n",
                        req->fd,req->state,req->peerhost);
#endif

                /* host auth callback */
                if (access_check_func != NULL) {
                    if (access_check_func(req->peerhost, NULL) < 0) {
                        /* read request */
                        read_header(req,0);
                        req->ping = now;
                        /* reply with access denied and close connection */
                        mkerror(req,403,0);
                        write_request(req);	     
                        req->state = STATE_CLOSE;
                    }
                }
	 
                FD_SET(req->fd, &fds->rset); 
                if (req->fd > fds->max) {
                    fds->max = req->fd;
                }
            }
        }
    }
    
    /* check active connections */
    for (req = conns, prev = NULL; req != NULL;) {

        /* I/O */
        if ((rset != NULL) && FD_ISSET(req->fd, rset)) {
            if (req->state == STATE_KEEPALIVE) {
                req->state = STATE_READ_HEADER;
            }

            if (req->state == STATE_READ_HEADER) {
                while (read_header(req,0) > 0);
            }
          
            if (req->state == STATE_READ_BODY) {
                while (read_body(req, 0) >0);
            }
            
            req->ping = now;
        }
      
        if ((wset != NULL) && FD_ISSET(req->fd, wset)) {
            write_request(req);
            req->ping = now;
        }

        /* check timeouts */
        if (req->state == STATE_KEEPALIVE) {
            if (now > req->ping + keepalive_time ||
                curr_conn > max_conn * 9 / 10) {
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: keepalive timeout\n",req->fd,req->state);
#endif
                req->state = STATE_CLOSE;
            }
        } else {
            if (now > req->ping + timeout) {
                if ((req->state == STATE_READ_HEADER) ||
                    (req->state == STATE_READ_BODY)) {
                    mkerror(req,408,0);
                } else {
                    log_error_func(0,LOG_INFO,"network timeout",req->peerhost);
                    req->state = STATE_CLOSE;
                }
            }
        }

        /* parsing */
      parsing:
      
        if (req->state == STATE_PARSE_HEADER) {
            parse_request(req, server_host);
        }

        /* body parsing */
        if (req->state == STATE_PARSE_BODY) {
            parse_request_body(req);
        }

        if (req->state == STATE_WRITE_HEADER) {
            /* switch to writing */
            FD_CLR(req->fd, &fds->rset);
            FD_SET(req->fd, &fds->wset);
            
            write_request(req);
        }
        

        /* handle finished requests */
        if (req->state == STATE_FINISHED && !req->keep_alive) {
            req->state = STATE_CLOSE;
        }
        if (req->state == STATE_FINISHED) {
            /* access log hook */
            if (log_request_func != NULL) {
                log_request_func(req, now);
            }

            /* switch to reading */
            FD_CLR(req->fd, &fds->wset);
            FD_SET(req->fd, &fds->rset);
            
            /* cleanup */
            req->auth[0]       = 0;
            req->if_modified   = 0;
            req->if_unmodified = 0;
            req->if_range      = 0;
            req->range_hdr     = NULL;
            req->ranges        = 0;
            if (req->r_start) { 
                free(req->r_start); 
                req->r_start = NULL; 
            }
            if (req->r_end) { 
                free(req->r_end);   
                req->r_end   = NULL; 
            }
            if (req->r_head) { 
                free(req->r_head);  
                req->r_head  = NULL; 
            }
            if (req->r_hlen) { 
                free(req->r_hlen);  
                req->r_hlen  = NULL; 
            }
            list_free(&req->header);
	
            if (req->bfd != -1) {
                close(req->bfd);
                req->bfd  = -1;
            }
	
            /* free memory of response body */
            if ((req->status<400) && (req->body != NULL)) {
                free(req->body);
                req->body = NULL;
            }
            req->written   = 0;
            req->head_only = 0;
            req->rh        = 0;
            req->rb        = 0;
            req->hostname[0] = 0;
            req->path[0]     = 0;
            req->query[0]    = 0;
            req->lifespan = -1;

            if (req->hdata == (req->lreq + req->lbreq)) {
                /* ok, wait for the next one ... */
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: keepalive wait\n",req->fd,req->state);
#endif
                req->state = STATE_KEEPALIVE;
                req->hdata = 0;
                req->lreq  = 0;
                req->lbreq = 0;

#ifdef TCP_CORK
                if (req->tcp_cork == 1) {
                    req->tcp_cork = 0;
#ifdef DEBUG
                    fprintf(stderr,"%03d/%d: tcp_cork=%d\n",req->fd,req->state,req->tcp_cork);
#endif
                    setsockopt(req->fd,SOL_TCP,TCP_CORK,&req->tcp_cork,sizeof(int));
                }
#endif
            } else {
                /* there is a pipelined request in the queue ... */
#ifdef DEBUG
                fprintf(stderr,"%03d/%d: keepalive pipeline\n",req->fd,req->state);
#endif
                req->state = STATE_READ_HEADER;
                memmove(req->hreq,req->hreq + req->lreq + req->lbreq,
                        req->hdata - (req->lreq + req->lbreq));
                req->hdata -= req->lreq + req->lbreq;
                req->lreq  =  0;
                read_header(req,1);
                goto parsing;
            }
        }
      
        /* connections to close */
        if (req->state == STATE_CLOSE) {
            /* access log hook */
            /*if (log_request_func != NULL) {
                log_request_func(req, now);
                }*/

            FD_CLR(req->fd, &fds->rset);
            FD_CLR(req->fd, &fds->wset);
            /* leave max as is */

            /* cleanup */
            close(req->fd);
#ifdef USE_SSL
            if (with_ssl) {
                SSL_free(req->ssl_s);
            }
#endif
            if (req->bfd != -1) {
                close(req->bfd);
#ifdef USE_SSL
                if (with_ssl) {
                    BIO_vfree(req->bio_in);
                }
#endif
            }
	
            curr_conn--;
#ifdef DEBUG
            fprintf(stderr,"%03d/%d: done (%d)\n",req->fd,req->state,curr_conn);
#endif
            /* unlink from list */
            tmp = req;
            if (prev == NULL) {
                conns = req->next;
                req = conns;
            } else {
                prev->next = req->next;
                req = req->next;
            }
            /* free memory  */
            if (tmp->r_start) {
                free(tmp->r_start);
            }
            if (tmp->r_end) {  
                free(tmp->r_end);
            }
            if (tmp->r_head) { 
                free(tmp->r_head);
            }
            if (tmp->r_hlen) { 
                free(tmp->r_hlen);
            }
            list_free(&tmp->header);
            free(tmp);
        } else {
            prev = req;
            req = req->next;
        }
    }

    return 0;
}

/* initialize http server */
/* return file descriptor if success, <0 otherwise */
int httpd_init(int sport, char *sname, int use_ssl,  const char *certificate, 
               const char *password, int use_v6)
{
    struct addrinfo ask,*res = NULL;
    struct sockaddr_storage  ss;
    int opt, rc, ss_len, v4 = 1, v6 = 0;
    char host[INET6_ADDRSTRLEN+1];
    char serv[16];
    char listen_port[6];  
    char *listen_ip = NULL;
 
    /* set config */
    sprintf(listen_port, "%d", sport);
    server_name = sname;
    if (use_v6) {
        v6 = 1;
    } 

#ifdef USE_SSL
    with_ssl = use_ssl;
#endif
    /* FIXME nthreads */

    /* get server name */
    gethostname(server_host,255);
    memset(&ask,0,sizeof(ask));
    ask.ai_flags = AI_CANONNAME;
    if ((rc = getaddrinfo(server_host, NULL, &ask, &res)) == 0) {
        if (res->ai_canonname) {
            strcpy(server_host,res->ai_canonname);
        }
	
        if (res != NULL) {
            freeaddrinfo(res);
            res = NULL;
        }
    }
    
    /* bind to socket */
    slisten = -1;
    memset(&ask, 0, sizeof(ask));
    ask.ai_flags = AI_PASSIVE;
    if (listen_ip) {
        ask.ai_flags |= AI_CANONNAME;
    }
    ask.ai_socktype = SOCK_STREAM;

    /* try ipv6 first ... */
    if (slisten == -1 &&  v6) {
        ask.ai_family = PF_INET6;

        g_timeout = 0;
        alarm(2);
        rc = getaddrinfo(listen_ip, listen_port, &ask, &res);
        if (g_timeout) {
            log_error_func(1, LOG_ERR,"getaddrinfo (ipv6): DNS timeout",NULL);  
        }
        alarm(0);

        if (rc == 0) {
            if ((slisten = socket(res->ai_family, res->ai_socktype,
                                  res->ai_protocol)) == -1) {
                log_error_func(1, LOG_ERR,"socket (ipv6)",NULL);
            }
        } else {
            log_error_func(1, LOG_ERR, "getaddrinfo (ipv6)", NULL);
        }
    }
	
    g_timeout = 0;
    alarm(2);
    
    /* ... failing that try ipv4 */
    if (slisten == -1 &&  v4) {
        ask.ai_family = PF_INET;

        g_timeout = 0;
        alarm(1);
        rc = getaddrinfo(listen_ip, listen_port, &ask, &res);
        if (g_timeout) {
            log_error_func(1, LOG_ERR,"getaddrinfo (ipv4): DNS timeout",NULL);  
            return -1;
        }
        alarm(0);

        if (rc == 0) {
            if ((slisten = socket(res->ai_family, res->ai_socktype,
                                  res->ai_protocol)) == -1) {
                log_error_func(1, LOG_ERR,"socket (ipv4)",NULL);
                return -1;
            }
        } else {
            log_error_func(1, LOG_ERR, "getaddrinfo (ipv4)", NULL);
            return -1;
        }
    }

    if (slisten == -1) {
        return -1;
    }

    memcpy(&ss,res->ai_addr,res->ai_addrlen);
    ss_len = res->ai_addrlen;
    if (res->ai_canonname) {
        strcpy(server_host,res->ai_canonname);
    }
    if ((rc = getnameinfo((struct sockaddr*)&ss,ss_len,
                          host,INET6_ADDRSTRLEN,serv,15,
                          NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
        log_error_func(1, LOG_ERR, "getnameinfo", NULL);
        return -1;
    }


    tcp_port = atoi(serv);
    opt = 1;
    setsockopt(slisten,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    fcntl(slisten,F_SETFL,O_NONBLOCK);

    /* Use accept filtering, if available. */
#ifdef SO_ACCEPTFILTER
    {
        struct accept_filter_arg af;
        memset(&af,0,sizeof(af));
        strcpy(af.af_name,"httpready");
        setsockopt(slisten, SOL_SOCKET, SO_ACCEPTFILTER, (char*)&af, sizeof(af));
    }
#endif /* SO_ACCEPTFILTER */

   
    if (bind(slisten, (struct sockaddr*) &ss, ss_len) == -1) {
        log_error_func(1, LOG_ERR,"bind",NULL);
        return -1;
    }
    if (listen(slisten, 2*max_conn) == -1) {
        log_error_func(1, LOG_ERR,"listen",NULL);
        return -1;
    }


    /* init misc stuff */
    init_mime(mimetypes,"text/plain");
    init_quote();
   
#ifdef USE_SSL
    if (with_ssl) {
        init_ssl(certificate, password);
    }
#endif

#ifdef DEBUG  
	fprintf(stderr,
            "http server started\n"
            "  ipv6  : %s\n"
#ifdef USE_SSL
	        "  ssl   : %s\n"
#endif
            "  node  : %s\n"
            "  ipaddr: %s\n"
            "  port  : %d\n"
            ,
            res->ai_family == PF_INET6 ? "yes" : "no",
#ifdef USE_SSL
            with_ssl ? "yes" : "no",
#endif
            server_host,host,tcp_port);
#endif

	    	
	if (res != NULL) {
        freeaddrinfo(res);
	}

    /* go! */
#ifdef HTTPD_USE_THREADS
    if (nthreads > 1) {
        int i;
        threads = malloc(sizeof(pthread_t) * nthreads);
        for (i = 1; i < nthreads; i++) {
            pthread_create(threads+i,NULL,mainloop,threads+i);
            pthread_detach(threads[i]);
        }
    }
#endif
  
    return slisten;
}

void httpd_shutdown()
{
    struct REQUEST *req, *tmp;

    close(slisten);

    for (req = conns; req != NULL;) {
        tmp = req->next;

        close(req->fd);
        if (req->bfd != -1) {
            close(req->bfd);
        }
        if (req->body != NULL) {
            free(req->body);
        }
        if (req->r_start) { 
            free(req->r_start); 
        }
        if (req->r_end) { 
            free(req->r_end);   
        }
        if (req->r_head) { 
            free(req->r_head);  
        }
        if (req->r_hlen) { 
            free(req->r_hlen);  
        }
        list_free(&req->header);

        free(req);

        req = tmp;
    }

    shutdown_mime();
}

int httpd_uses_ssl()
{
#ifdef USE_SSL
    return (with_ssl > 0);
#else 
    return 0;
#endif
}

int httpd_register_access_check(access_check_func_t f)
{
    access_check_func = f;
  
    return 0;
}

int httpd_register_log_request(log_request_func_t f)
{
    log_request_func = f;

    return 0;
}

int httpd_register_log_error(log_error_func_t f)
{
    log_error_func = f;

    return 0;
}

int httpd_register_parse_request(parse_request_func_t f)
{
    parse_request_func = f;

    return 0;
}

int httpd_get_keepalive()
{
    return keepalive_time;
}
