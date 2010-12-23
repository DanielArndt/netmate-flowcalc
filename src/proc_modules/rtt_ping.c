
/*! \file  rtt_ping.c

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
action module for round trip time measurement based on ping packets

$Id: rtt_ping.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "ProcModule.h"


/*
  define an array that stores the type information for the
  data structure exported by this module's exportData function
*/
typeInfo_t exportInfo[] = { { UINT32, "matches"     },
                            { UINT32, "min"         },
                            { UINT32, "min_us"      },
                            { UINT32, "max"         },
                            { UINT32, "max_us"      },
                            { UINT32, "avg"         },
                            { UINT32, "avg_us"      },
                            EXPORT_END };


#define ENTRIES 32

#define TMIN( a, b )  (a.tv_sec<b.tv_sec || (a.tv_sec==b.tv_sec && a.tv_usec<b.tv_usec)) ? a : b ;
#define TMAX( a, b )  (a.tv_sec>b.tv_sec || (a.tv_sec==b.tv_sec && a.tv_usec>b.tv_usec)) ? a : b ;


struct icmp_echo_hdr {
	uint8_t  type;
	uint8_t  code;
	uint16_t chksum2;
	uint16_t id;
	uint16_t seqno;
};

struct entry {
	unsigned short id;
	unsigned short seqno;
	struct timeval tstamp;
};

struct moduleData {
	int matches, rel;
	int lsize, free, curr;
	struct timeval min, max, sum;
	struct entry *list;
};

int initModule()
{
   
    return 0;
}

int destroyModule()
{
    
    return 0;
}

int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
	int i;
	struct timeval rtt;
	struct moduleData *data;
	struct icmp_echo_hdr *hdr;

	if ((meta->layers[2] != T_ICMP) && (meta->layers[2] != T_ICMP6)) {  // only use ICMP packets!
	    return 0;
	}

	data = (struct moduleData *) flowdata;
	hdr = (struct icmp_echo_hdr *) (&packet[meta->offs[2]]); /* skip layer 1 and 2 */

    // only want echo request and reply
	if ((hdr->type != 8) && (hdr->type != 0)) {
        return 0;
    }
    // dont want "unreachable" replies
	if (hdr->code != 0) {
        return 0;
    }

	if (hdr->type == 8 ) {  /* ICMP echo */

	    /* find new unused entry - if possible */
	    data->curr = 0;
	    if (data->free > 0) {
            while (data->list[data->curr].id != 0) {
                data->curr += 1;
            }
        }
	    
	    data->list[data->curr].id = hdr->id;
	    data->list[data->curr].seqno = hdr->seqno;
	    data->list[data->curr].tstamp.tv_sec = meta->tv_sec;
	    data->list[data->curr].tstamp.tv_usec = meta->tv_usec;

	    if (data->free > 0) {
            data->free -= 1;
        }

	} else {   /* ICMP reply */

	    for ( i = 0; i < data->lsize; i++ ) {
		
            if (data->list[i].id == 0) {
                continue; /* unused list entry */
            }
		
            if ((data->list[i].id == hdr->id) && (data->list[i].seqno == hdr->seqno)) {

                rtt.tv_sec = meta->tv_sec - data->list[i].tstamp.tv_sec;
                rtt.tv_usec = meta->tv_usec - data->list[i].tstamp.tv_usec;
                if (rtt.tv_usec < 0) {
                    rtt.tv_usec += 1000000;
                    rtt.tv_sec  -= 1;
                }

                /*fprintf( stderr, "%ld %ld \n", rtt.tv_sec, rtt.tv_usec );*/

                data->min = TMIN( data->min, rtt );
                data->max = TMAX( data->max, rtt );
		    
                data->sum.tv_usec += (unsigned long long) rtt.tv_usec;
                data->sum.tv_sec  += (unsigned long long) rtt.tv_sec;
                if (data->sum.tv_usec > 1000000) {
                    data->sum.tv_usec -= 1000000;
                    data->sum.tv_sec  += 1;
                }					

                data->list[i].id = 0;
                data->free += 1;
                data->matches += 1;

                if (data->matches == 1 ) {
                    data->min = rtt;
                }
            }			
	    }
	}

	return 0;
}

int initFlowRec( configParam_t *params, void **flowdata )
{
    struct moduleData *data;

    data = malloc( sizeof( struct moduleData ) );
    if (data == NULL ) {
        return -1;
    }
    
    data->matches = 0;
    data->curr = 0;
    data->min.tv_sec = 0;
    data->min.tv_usec = 0;
    data->max.tv_sec = 0;
    data->max.tv_usec = 0;
    data->sum.tv_sec = 0;
    data->sum.tv_usec = 0;

    if (params[0].name!= NULL && params[0].name[0]!= '\0' && !strcmp(params[0].name,"rel") ) {
        data->rel = 1;
    } else {
        data->rel = 0;
    }

    data->lsize = ENTRIES;
    data->free = data->lsize;
    data->list = malloc( data->lsize * sizeof(struct entry) );
    memset(data->list, 0, data->lsize * sizeof(struct entry));

    *flowdata = data;

    /* force bidirectional rules */
    return 1;
}

int resetFlowRec( void *flowdata )
{
    struct moduleData *data = (struct moduleData*) flowdata;

    data->matches = 0;
    data->min.tv_sec = 0;
    data->min.tv_usec = 0;
    data->max.tv_sec = 0;
    data->max.tv_usec = 0;
    data->sum.tv_sec = 0;
    data->sum.tv_usec = 0;

    return 0;
}

int destroyFlowRec( void *flowdata )
{
    struct moduleData *data = (struct moduleData*) flowdata;

    free(data->list);
    free(data);

    return 0;
}

int exportData( void **exp, int *len, void *flowdata )
{
    struct timeval avg;
    struct moduleData *data = (struct moduleData*) flowdata;
    static unsigned int buf[7];


    if (data->matches == 0) { 
        avg.tv_sec = avg.tv_usec = 0;
    } else {
        avg.tv_sec = data->sum.tv_sec / data->matches;
        avg.tv_usec = (avg.tv_sec * data->matches * 1000000 + data->sum.tv_usec) / data->matches;
    }

    STARTEXPORT( buf );
    ADD_UINT32( data->matches  );
    ADD_UINT32( data->min.tv_sec  );
    ADD_UINT32( data->min.tv_usec );
    ADD_UINT32( data->max.tv_sec  );
    ADD_UINT32( data->max.tv_usec );
    ADD_UINT32( avg.tv_sec );
    ADD_UINT32( avg.tv_usec );
    ENDEXPORT( exp, len );

    /* reset for relative mode */
    if (data->rel ) { 
        data->matches = 0;
        data->min.tv_sec = 0;
        data->min.tv_usec = 0;
        data->max.tv_sec = 0;
        data->max.tv_usec = 0;
        data->sum.tv_sec = 0;
        data->sum.tv_usec = 0;
    }

    return 0;
}

int dumpData( char *string, void *flowdata )
{

    return 0;
}

char* getModuleInfo(int i)
{
    
    switch(i) {
    case I_MODNAME: return "rtt_ping";
    case I_BRIEF:   return "calculate min/max/avg ping based round trip times";
    case I_AUTHOR:  return "Carsten Schmoll";
    case I_CREATED: return "2001/05/20";
    case I_VERBOSE: return "matches ICMP echo/reply (ping) packets and calculates round trip time";
    case I_PARAMS:  return "keyword 'rel' will reset min/max/avg every collection";
    case I_RESULTS: return "description of measurement data";
    default: return NULL;
    }
}


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

