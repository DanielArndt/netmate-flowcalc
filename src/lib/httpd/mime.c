
/*! \file   mime.c

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
  mime type handling

  $Id: mime.c 748 2009-09-10 02:54:03Z szander $

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "httpd.h"


struct MIME {
    char  ext[8];
    char  type[64];
};

static char *mime_default;
static struct MIME  *mime_types;
static int mime_count;



static void add_mime(char *ext, char *type)
{
    if ((mime_count % 64) == 0) {
        mime_types = realloc(mime_types,(mime_count+64)*sizeof(struct MIME));
    }
    strcpy(mime_types[mime_count].ext, ext);
    strcpy(mime_types[mime_count].type,type);
    mime_count++;
}

char *get_mime(char *file)
{
    int i;
    char *ext;

    ext = strrchr(file,'.');
    if (NULL == ext) {
        return mime_default;
    }
    ext++;
    for (i = 0; i < mime_count; i++) {
        if (strcasecmp(ext,mime_types[i].ext) == 0){
            return mime_types[i].type;
        }
    }
    return mime_default;
}

void init_mime(char *file,char *def)
{
    FILE *fp;
    char line[128], type[64], ext[8];
    int  len,off;
    
    mime_default = strdup(def);
    if ((fp = fopen(file,"r")) == NULL) {
        fprintf(stderr,"open %s: %s\n",file,strerror(errno));
        return;
    }
    while (NULL != fgets(line,127,fp)) {
        if (line[0] == '#') {
            continue;
        }
        if (sscanf(line,"%63s%n",type,&len) != 1){
            continue;
        }
        off = len;
        for (;;) {
            if (sscanf(line+off,"%7s%n",ext,&len) != 1) {
                break;
            }
            off += len;
            add_mime(ext,type);
        }
    }
    fclose(fp);
}

void shutdown_mime()
{
    free(mime_default);
    free(mime_types);
}
