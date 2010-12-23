
/*! \file   ssl.c

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
  SSL specific functions

  $Id: ssl.c 748 2009-09-10 02:54:03Z szander $

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#ifdef USE_SSL
#include <openssl/err.h>
#include <openssl/evp.h>
#endif

#include "httpd.h"

#ifdef HTTPD_USE_THREADS
static pthread_mutex_t lock_ssl = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef USE_SSL

static SSL_CTX *ctx;
static BIO	*sbio;
static const char *password = NULL;

int ssl_read(struct REQUEST *req, char *buf, int len)
{
    int rc;

    rc = BIO_read(req->io, buf, len);
    if ((rc == 0 && BIO_should_retry(req->io)) ||
        (rc < 0  && SSL_get_error(req->ssl_s, rc) == SSL_ERROR_WANT_READ)) {
        errno = EAGAIN;
        return -1;
    }

#ifdef DEBUG
    if (rc < 0 && SSL_get_error(req->ssl_s, rc) == SSL_ERROR_SSL) {
        fprintf(stderr,"%03d: ssl read: %d [%s]\n",
                req->fd, SSL_get_error(req->ssl_s, rc),
                ERR_error_string(ERR_get_error(), NULL));
    }
#endif

    if (rc < 0) {
        errno = EIO;
        return -1;
    }
    return rc;
}

int ssl_write(struct REQUEST *req, char *buf, int len)
{
    int rc;

    rc = BIO_write(req->io, buf, len);
#ifdef DEBUG
    if (!BIO_flush(req->io)) {
        fprintf(stderr, "%03d: BIO_flush() failed",req->fd);
    }
#endif

    if (rc < 0 && SSL_get_error(req->ssl_s, rc) == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
        return -1;
    }

#ifdef DEBUG
    if (rc < 0 && SSL_get_error(req->ssl_s, rc) == SSL_ERROR_SSL) {
        fprintf(stderr,"%03d: ssl write: %d [%s]\n",
                req->fd, SSL_get_error(req->ssl_s, rc),
                ERR_error_string(ERR_get_error(), NULL));
    }
#endif

    if (rc < 0) {
        errno = EIO;
        return -1;
    }
    return rc;
}

int ssl_blk_write(struct REQUEST *req, int offset, int len)
{
    int read;
    char buf[4096];

    BIO_seek(req->bio_in, offset);
    if (len > (int) sizeof(buf)) {
        len = sizeof(buf);
    }
    read = BIO_read(req->bio_in, buf, len);
    if (read <= 0) {
        /* shouldn't happen ... */
        req->state = STATE_CLOSE;
        errno = EIO;
        return -1;
    }
    return ssl_write(req, buf, read);
}

static int password_cb(char *buf, int num, int rwflag, void *userdata)
{
    if (NULL == password) {
        return 0;
    }
    if (num < (int) strlen(password)+1) {
        return 0;
    }
    
    strcpy(buf,password);
    return(strlen(buf));
}

void init_ssl(const char *certificate, const char *pwd)
{
    int rc;
    
    password = pwd;

    SSL_load_error_strings();
    SSL_library_init();
    ctx = SSL_CTX_new(SSLv23_server_method());
    if (NULL == ctx) {
        fprintf(stderr, "SSL init error [%s]",strerror(errno));
        exit (1);
    }

    rc = SSL_CTX_use_certificate_chain_file(ctx, certificate);
    switch (rc) {
    case 1:
#ifdef DEBUG
        fprintf(stderr, "SSL certificate load ok\n");
#endif
        break;
    default:
        fprintf(stderr, "SSL cert load error [%s]\n",
                ERR_error_string(ERR_get_error(), NULL));
        break;
    }

    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
    SSL_CTX_use_PrivateKey_file(ctx, certificate, SSL_FILETYPE_PEM);
    switch (rc) {
    case 1:
#ifdef DEBUG
        fprintf(stderr, "SSL private key load ok\n");
#endif
        break;
    default:
        fprintf(stderr, "SSL privkey load error [%s]\n",
                ERR_error_string(ERR_get_error(), NULL));
        break;
    }

    SSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2);
}

void open_ssl_session(struct REQUEST *req)
{
    DO_LOCK(lock_ssl);
    req->ssl_s = SSL_new(ctx);
    if (req->ssl_s == NULL) {
#ifdef DEBUG
        fprintf(stderr,"%03d: SSL session init error [%s]\n",
                req->fd, strerror(errno));
#endif
        /* FIXME: how to handle that one? */
    }
    sbio = BIO_new_socket(req->fd, BIO_NOCLOSE);
#ifdef DEBUG
    fprintf(stderr,"%03d: SSL started 1\n",req->fd);
#endif
    SSL_set_bio(req->ssl_s, sbio, sbio);
#ifdef DEBUG
    fprintf(stderr,"%03d: SSL started 2\n",req->fd);
#endif
    SSL_set_accept_state(req->ssl_s);
    SSL_set_mode(req->ssl_s, SSL_MODE_AUTO_RETRY);
    SSL_set_read_ahead(req->ssl_s, 1);
    req->io=BIO_new(BIO_f_ssl());
    BIO_set_ssl(req->io,req->ssl_s,BIO_CLOSE); 
    DO_UNLOCK(lock_ssl);
}

#endif
