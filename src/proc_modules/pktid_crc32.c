
/*! \file  pktid_crc32.c

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
Action module for generation of an CRC32 bit packet identifier 

$Id: pktid_crc32.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include "stdinc.h"
#include "ProcModule.h"
#include "ip.h"


/* export info for lightweight meter */
typeInfo_t exportInfo[] = { { LIST, "packetno" },
                            { UINT32, "tstamp_sec"    },
                            { UINT32, "tstamp_usec"   },
                            { UINT32, "32_bit_crc"    },
                            { UINT16, "packet_length" },
                            { UINT8,  "tos_byte"      },
                            LIST_END,
                            EXPORT_END };

/* defines */
#define IPHDR_LEN sizeof(struct iphdr)
#define CRC32INITVAL 0xFFFFFFFF
#define POLYNOMIAL   0x04c11db7        
/* max number of bytes used above IP header */
#define MAX_PAYLOAD 27
/* initial space for 10k packets */
#define INIT_ELEM 2048

                                                             
typedef struct PktEvent{
    unsigned long id;
    struct timeval ts;
    unsigned short pktlen;
    unsigned short tos;
} PktEvent_t;

struct moduleData {
    /* packets seen */
    unsigned long cnt;
    /* free slots */
    unsigned long free;
    PktEvent_t *pkt;
};


/* globals */
static uint32_t crc_table[256];   
static char *expData;
static unsigned long expDataSize;

/* CRC 32 functions */

/* generate the table of CRC remainders for all possible bytes */
static void gen_crc_table( void )
{
    register int i, j;
    register unsigned long crc_accum;

    for ( i = 0;  i < 256;  i++ ) {
        crc_accum = ( (uint32_t) i << 24 );
        for ( j = 0;  j < 8;  j++ ) {
            if ( crc_accum & 0x80000000)
                crc_accum = ( crc_accum << 1 ) ^ POLYNOMIAL;
            else
                crc_accum = ( crc_accum << 1 );
        }
        crc_table[i] = crc_accum;
    }

    return;
}                                           

/* update the CRC on the data block one byte at a time */
static inline uint32_t update_crc (uint32_t crc_accum,
								 u_char   *data_blk_ptr,
								 int      data_blk_size )
{
    register int i, j;

    for ( j = 0;  j < data_blk_size;  j++ ) {
        i = ( (int) ( crc_accum >> 24) ^ *data_blk_ptr++ ) & 0xff;
        crc_accum = ( crc_accum << 8 ) ^ crc_table[i];
    }
    return crc_accum;
}                                                                           
        

int initModule()
{
    /* initialize crc table */
    gen_crc_table( );
   
    expData = (char *) malloc(8);
    expDataSize = 0;

    return 0;
}

int destroyModule()
{
    free( expData );

    return 0;
}

int processPacket(char *packet, metaData_t *meta, void *flowdata )
{
    struct moduleData *data = (struct moduleData*) flowdata;
    struct iphdr *iph;
    struct ip6_hdr *ip6h;
    int payld_size = 0;
    uint32_t crc = 0;
    unsigned short tos = 0;

#ifdef CRC32_DEBUG
    struct timeval start_time, end_time;
#endif
#ifdef CRC32_PROFILING
    struct timeval diff_time;
#endif
    
    if ((meta->layers[1] != N_IP) && (meta->layers[1] != N_IP6)) {  /* only use  IP packets */
        return 0;
    }
        
#ifdef CRC32_PROFILING
	gettimeofday(&start_time,NULL);
#endif

    crc = CRC32INITVAL;   

    /* select packet fields that are immutable during transmission 
       and highly variant between packets */

    iph = (struct iphdr*) &packet[meta->offs[1]];
    payld_size = meta->cap_len - meta->offs[2];

    if (iph->version == 4) {
        crc = update_crc(crc, (u_char *) &iph->tot_len, 2); 
        crc = update_crc(crc, (u_char *) &iph->id, 2); 
        crc = update_crc(crc, (u_char *) &iph->saddr, 4 ); 
        crc = update_crc(crc, (u_char *) &iph->daddr, 4 ); 
        crc = update_crc(crc, (u_char *) &iph->protocol, 1 ); 
 
        tos = iph->tos;
    } else {
        /* else IPv6 */
        ip6h = (struct ip6_hdr *) iph;
        crc = update_crc(crc, (u_char *) &ip6h->ip6_plen, 2);
        /* use 16 bits of the flow label */
        crc = update_crc(crc, &((u_char *) &ip6h->ip6_flow)[2], 2); 
        crc = update_crc(crc, (u_char *) &ip6h->ip6_src, 16 ); 
        crc = update_crc(crc, (u_char *) &ip6h->ip6_dst, 16 ); 
        crc = update_crc(crc, (u_char *) &ip6h->ip6_nxt, 1 ); 
        /* use payload? */

        /* get TC bits */
        tos = (ip6h->ip6_flow & 0x0FF00000) >> 20;
    }

    /* fill rest of the data with bytes after ip header */
    if (payld_size > 0){
        if (payld_size >= MAX_PAYLOAD) {
            payld_size = MAX_PAYLOAD;
        }
        
        crc = update_crc(crc, (u_char*) &packet[meta->offs[2]], payld_size ); 
    }
        
#ifdef CRC32_PROFILING
	gettimeofday(&end_time,NULL);
#endif

#ifdef CRC32_DEBUG
	printf("%ld.%06ld %x", meta->tv_sec, meta->tv_usec, crc); 
#ifdef CRC32_PROFILING
	timersub(&end_time, &start_time, &diff_time); 
	printf(" %ld.%06ld ",diff_time.tv_sec, diff_time.tv_usec);
#endif
	printf("\n");
#endif

    // get more memory if necessary
    if (data->free == 0) {
        /* double memory */
        data->pkt = realloc(data->pkt, data->cnt*sizeof(PktEvent_t)*2);
        data->free = data->cnt;
    }

    data->pkt[data->cnt].id = crc;
    data->pkt[data->cnt].ts.tv_sec = meta->tv_sec;
    data->pkt[data->cnt].ts.tv_usec = meta->tv_usec;
    data->pkt[data->cnt].pktlen = meta->len;
    data->pkt[data->cnt].tos = tos;

    data->cnt++;
    data->free--;

    return 0;
}

int initFlowRec( configParam_t *params, void **flowdata )
{
    struct moduleData *data;

    data = (struct moduleData*) malloc(sizeof(struct moduleData));
    
    if (data == NULL ) {
        return -1;
    }
	
    data->pkt = (PktEvent_t *) malloc(INIT_ELEM*sizeof(PktEvent_t));
    if (data->pkt == NULL) {
        free(data);
        return -1;
    }
    data->cnt = 0;
    data->free = INIT_ELEM;

    *flowdata = data;

    return 0;
}


int resetFlowRec( void *flowdata )
{
    struct moduleData *data = (struct moduleData*) flowdata;

    /* reset to default size */
    data->pkt = (PktEvent_t *) realloc(data->pkt, INIT_ELEM*sizeof(PktEvent_t));
    data->cnt = 0;
    data->free = INIT_ELEM;

    return 0;
}

int destroyFlowRec( void *flowdata )
{
    struct moduleData *data = (struct moduleData*) flowdata;
     
    free(data->pkt);
    free(data);

    return 0;
}

int exportData( void **exp, int *len, void *flowdata )
{
  	struct moduleData *data = (struct moduleData*) flowdata;
	unsigned long i;

	if (expDataSize < data->cnt) {
        expDataSize = data->cnt;
        /* 8 bytes for list */
        expData = realloc(expData, data->cnt*sizeof(PktEvent_t) + 8);
	}

    STARTEXPORT( expData );
	ADD_LIST( data->cnt );
    
	for (i = 0; i<data->cnt; i++) {
        ADD_UINT32( data->pkt[i].ts.tv_sec );
        ADD_UINT32( data->pkt[i].ts.tv_usec );
        ADD_UINT32( data->pkt[i].id );
        ADD_UINT16( data->pkt[i].pktlen );
        ADD_UINT8( data->pkt[i].tos );
	}
    END_LIST();

    ENDEXPORT( exp, len );

    /* reset flow data */
    data->free += data->cnt;
    data->cnt = 0;

    return 0;
}

int dumpData( char *string, void *flowdata )
{
   
    return 0;
}

char* getModuleInfo(int i)
{
   
    switch(i) {
    case I_MODNAME: return "pktid_crc32";   
    case I_BRIEF:   return "CRC32 packet-id generation";
    case I_AUTHOR:  return "Tanja Zseby";
    case I_CREATED: return "2001/02/20";
    case I_VERBOSE: return "CRC32 packet-id generation";
    case I_PARAMS:  return "no parameters";
    case I_RESULTS: return "results description = ?";
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

