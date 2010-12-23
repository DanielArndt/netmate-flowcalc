
/*! \file  capture.c

    Copyright 2003-2004 Fraunhofer Institute for Open Communication Systems (FOKUS),
                        Berlin, Germany

    This file is part of Network Measurement and Accounting System (NETMATE).

    NETMATE is free software; you can redistribute it and/or modify 
    it under the terms of the GNU General Public License as published by 
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    NETMATE is distributed in the hope that it will be useful, 
    but WITHOUT ANY WARRANTY; without even the implied warranty of 
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this software; if not, write to the Free Software 
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Description:
    action module for capturing packets in producing packet file
    format (libpcap) dumps

    $Id: capture.c 748 2009-09-10 02:54:03Z szander $
*/


#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pcap.h>
#include <arpa/inet.h>
#include <time.h>
#include "ProcModule.h"


#define TCPDUMP_MAGIC 0xa1b2c3d4 
#define INIT_BLEN 65536

typeInfo_t exportInfo[] = { { BINARY, "buffer" },
                            EXPORT_END };


static unsigned char *expData;
static unsigned int expSize;


/*
  a buffer for the packet dump
*/
struct moduleData {
    /* size of the buffer */
    unsigned long size;
    /* content length */
    unsigned long len;
    char *buffer;
};


/* utility function */


int gmt2local()
{
        register int t;
#if !defined(HAVE_ALTZONE) && !defined(HAVE_TIMEZONE)
        struct timeval tv;
        struct timezone tz;
        struct tm tm;
#endif
 
        t = 0;
#if !defined(HAVE_ALTZONE) && !defined(HAVE_TIMEZONE)
        if (gettimeofday(&tv, &tz) < 0) {
            perror("gettimeofday");
        }
        localtime_r((time_t *)&tv.tv_sec, &tm);
#ifdef HAVE_TM_GMTOFF
        t = tm.tm_gmtoff;
#else
        t = tz.tz_minuteswest * -60;
        /* XXX Some systems need this, some auto offset tz_minuteswest... */
        if (tm.tm_isdst) {
                t += 60 * 60;
        }
#endif
#endif

#ifdef HAVE_TIMEZONE
        tzset();
        t = -timezone;
        if (daylight) {
            t += 60 * 60;
        }
#endif
 
#ifdef HAVE_ALTZONE
		tzset();
        t = -altzone;
#endif
 
        return t;
}       

/*
  initialize module
*/
int initModule()
{
    expData = NULL;
    expSize = 0;

    return 0;
}


/*
  cleanup 
*/
int destroyModule()
{
    free( expData );

    return 0;
}


/*
  this function will be called for every packet matching
  a specific rule and has to evaluate the incoming packet
*/
int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
    struct moduleData *data = flowdata;
    struct pcap_pkthdr *hdr;

    if (meta->layers[0] != L_ETHERNET ) {  // only use Ethernet
        return 0;
    }
        
    /* adjust buffer size */
    if ((data->size - data->len) < (sizeof(struct pcap_pkthdr) + meta->cap_len)) {
        data->buffer = realloc(data->buffer, data->size * 2);
        data->size *= 2;

        if (data->buffer == NULL) {
            return -1;
        }
    }

    hdr = (struct pcap_pkthdr *) &data->buffer[data->len];
    hdr->ts.tv_sec = meta->tv_sec;
    hdr->ts.tv_usec = meta->tv_usec;
    hdr->caplen = meta->cap_len;
    hdr->len = meta->len;
    memcpy(&data->buffer[data->len + sizeof(struct pcap_pkthdr)], 
           packet, meta->cap_len);
    
    data->len += sizeof(struct pcap_pkthdr) + meta->cap_len;

    /* FIXME write directly into export buffer for efficiency? */
    
    return 0;
}

/*
  initialize per-rule flow data for newly added rule
*/
int initFlowRec( configParam_t *params, void **flowdata )
{
    struct moduleData *data;
    struct pcap_file_header *hdr; 
    
    data = (struct moduleData*) malloc( sizeof(struct moduleData ) );
    
    if (data == NULL ) {
        return -1;
    }
        
    /* allocate start buffer memory */
    data->buffer = (char *) malloc(INIT_BLEN);
    if (data->buffer == NULL) {
        return -1;
    } 
    data->size = INIT_BLEN;
    
    /* init packet file header */
    data->len = sizeof(struct pcap_file_header);
    hdr = (struct pcap_file_header *) data->buffer;
    
    hdr->magic = TCPDUMP_MAGIC;
    hdr->version_major = PCAP_VERSION_MAJOR;
    hdr->version_minor = PCAP_VERSION_MINOR;
    
    hdr->thiszone = gmt2local();
    /* FIXME make this an option but max snap len could be 65535 */
    hdr->snaplen = 65535;
    hdr->sigfigs = 0;
    /* FIXME set linktype to Ethernet */
    hdr->linktype = DLT_EN10MB;
    
    *flowdata = data;
 
   return 0;
}


/*
  cleanup per-rule flow data when rule is deleted
*/
int resetFlowRec( void *flowdata )
{
    struct moduleData *data = flowdata;

    /* reset buffer to init size */
    data->buffer = (char *) realloc(data->buffer, INIT_BLEN);
    data->size = INIT_BLEN;
    data->len = 0;

    return 0;
}

int destroyFlowRec( void *flowdata )
{
    struct moduleData *data = flowdata;
     
    free(data->buffer);
    free(data);
    data = NULL;

    return 0;
}


/*
  this function will be called upon a scheduled data collection 
  for a specific rule. it has to write measurement values from 
  'flowdata' into 'buf' and set len accordingly
*/
int exportData( void **exp, int *len, void *flowdata )
{
    struct moduleData *data = flowdata;
    
    /* resize (global) export buffer if necessary */
    if (data->len > expSize ) {
        expData = realloc( expData, data->len + 4 );
        expSize = data->len;
    }

    STARTEXPORT(expData);
    ADD_BINARY(data->len, data->buffer);
    ENDEXPORT(exp, len);

    resetFlowRec(flowdata);
    
    return 0;
}


/*
  for debug only - dump module info
*/
int dumpData( char *string, void *flowdata )
{
    return 0;
}

/*
  return a description for this module
  please fill in the corresponding information
*/
char* getModuleInfo(int i)
{
    switch(i) {
    case I_MODNAME: return "capture";
    case I_BRIEF:   return "capture packets";
    case I_AUTHOR:  return "Sebastian Zander";
    case I_CREATED: return "2001/03/12";
    case I_VERBOSE: return "capture packets and store as packet file (libpcap)";
    case I_PARAMS:  return "none";
    case I_RESULTS: return "buffer that contains libpcap style raw trace";
    default: return NULL;
    }
}


/*
  return module specific error message
  getErrorMsg will be called with the return code from a call 
  to processPacket of exportData if it wasn't equal to zero
*/
char* getErrorMsg( int code )
{
   
    return NULL;
}


int timeout( int timerID, void *flowdata )
{
    
    return 0;
}




timers_t* getTimers( void *flowData )
{
    return NULL;
}

