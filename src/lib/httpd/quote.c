
/*! \file   quote.c

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
  html quoting/unquoting functions

  $Id: quote.c 748 2009-09-10 02:54:03Z szander $

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "httpd.h"



static char do_quote[256];

void init_quote(void)
{
    int i;

    for (i = 0; i < 256; i++) {
        do_quote[i] = (isalnum(i) || ispunct(i)) ? 0 : 1;
    }
    do_quote['+'] = 1;
    do_quote['#'] = 1;
    do_quote['%'] = 1;
}

char *quote(unsigned char *path, int maxlength)
{
    static unsigned char buf[2048]; /* FIXME: threads break this... */
    int i,j,n=strlen(path);

    if (n > maxlength) {
        n = maxlength;
    }

    for (i=0, j=0; i<n; i++, j++) {
        if (!do_quote[path[i]]) {
            buf[j] = path[i];
            continue;
        }
        sprintf(buf+j,"%%%02x",path[i]);
        j += 2;
    }
    buf[j] = 0;
    return buf;
}

static int unhex(unsigned char c)
{
    if (c < '@') {
        return c - '0';
    }
    return (c & 0x0f) + 9;
}

/* handle %hex quoting, also split path / querystring */
void unquote(unsigned char *path, unsigned char *qs, unsigned char *src)
{
    int q;
    unsigned char *dst;

    q=0;
    dst = path;
    while (src[0] != 0) {
        if (!q && *src == '?') {
            q = 1;
            *dst = 0;
            dst = qs;
            src++;
            continue;
        }
        if (q && *src == '+') {
            *dst = ' ';
        } else if ((*src == '%') && isxdigit(src[1]) && isxdigit(src[2])) {
            *dst = (unhex(src[1]) << 4) | unhex(src[2]);
            src += 2;
        } else {
            *dst = *src;
        }
        dst++;
        src++;
    }
    *dst = 0;
}

/* handle %hex quoting */
void unquote2(unsigned char *dst, unsigned char *src)
{
    while (src[0] != 0) {
        if (*src == '+') {
            *dst = ' ';
        } else if ((*src == '%') && isxdigit(src[1]) && isxdigit(src[2])) {
            *dst = (unhex(src[1]) << 4) | unhex(src[2]);
            src += 2;
        } else {
            *dst = *src;
        }
        dst++;
        src++;
    }
    *dst = 0;

}
