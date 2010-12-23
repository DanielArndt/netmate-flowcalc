
/*! \file  pktid_md5.c

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
Action module for generation of an MD5 128 bit packet identifier 
(or MD5 wrapped into 32 bit)

$Id: pktid_md5.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include "stdinc.h"
#include "ProcModule.h"
#include "global.h"
#include "md5.h"
#include "ip.h"


/* export info for lightweight meter */
typeInfo_t exportInfo[] = { { LIST, "ts_id_list" },
                            { UINT32, "timestamp"      },
                            { UINT32, "timestamp_us"   },
                            { BINARY, "128_bit_md5"    },
                            /*{ UINT32, "32_bit_md5"    },*/
                            LIST_END,
                            EXPORT_END };


#define DIG_LEN 16 /* lenght of digest (bytes) */ 
#define MAX_PAYLOAD 27
/* initial space for 10k packets */
#define INIT_ELEM 10000

                              
typedef struct PktEvent{
	unsigned char id[DIG_LEN];
    /* 	unsigned long id; */
	struct timeval ts;
} PktEvent_t;

struct moduleData {
    unsigned long cnt;
    unsigned long free;
    PktEvent_t *pkt;
};


static char *expData = NULL;
static unsigned long expDataSize = 0;


/* md5 hash function */

static void md5(char *message, unsigned char digest[DIG_LEN], unsigned int len)
{
	MD5_CTX context;
	
	MD5Init (&context);
	MD5Update (&context, message, len);
	MD5Final (digest, &context);
	
	return;
}

/* 32 bit md5 hash function (from RTP) */
static uint32_t md5_32(char *string, int length)
{
    MD5_CTX context;
    union {
        char   c[16];
        uint32_t x[4];
    } digest;
    uint32_t r;
    int i;
    
    MD5Init (&context);
    MD5Update (&context, (unsigned char *)string, length);
    MD5Final ((unsigned char *)&digest, &context);
    /* XOR the four parts into one word */
    for (i = 0, r = 0; i < 3; i++){
        r ^= digest.x[i];
    }
    return r;
}    


#ifdef MD5_DEBUG

static void print_id (unsigned char id[DIG_LEN])
{
    unsigned int i;

    for (i = 0; i < DIG_LEN; i++)
        printf ("%02x", id[i]);

    return;
}

#endif

int initModule()
{
    expData = (char *) malloc(8);
    expDataSize = 0;
    
    return 0;
}

int destroyModule()
{
    free(expData);
   
    return 0;
}

int processPacket(char *packet, metaData_t *meta, void *flowdata )
{
  	struct moduleData *data = (struct moduleData*) flowdata;
	struct iphdr *iph;
    struct ip6_hdr *ip6h;
	int payld_size = 0, offs = 0;
	char id[DIG_LEN];
    /* uint32_t id; */
    char fields[37+MAX_PAYLOAD];

#ifdef MD5_DEBUG
	struct timeval start_time, end_time;
#endif
#ifdef MD5_PROFILING
	struct timeval diff_time;
#endif

	if ((meta->layers[1] != N_IP) && (meta->layers[1] != N_IP6)) {  // only check IP packets
	    return 0;
	}

    
	/* select packet fields that are immutable during transmission 
	   and highly variant between packets */

	iph = (struct iphdr*) &packet[meta->offs[1]];
    payld_size = meta->cap_len - meta->offs[2];

    if (iph->version == 4) {
        memcpy(fields, &iph->tot_len, 2 );
        memcpy(&fields[2], &iph->id, 2);
        memcpy(&fields[4], &iph->saddr, 4);
        memcpy(&fields[8], &iph->daddr, 4);
        memcpy(&fields[12], &iph->protocol, 1);
          
        offs = 13;
    } else {
        /* IPv6 */
        ip6h = (struct ip6_hdr *) iph;
        memcpy(fields, &ip6h->ip6_plen, 2);
        /* use 16 bits of the flow label */
        memcpy(&fields[2], &((unsigned char *) &ip6h->ip6_flow)[2], 2); 
        memcpy(&fields[4], &ip6h->ip6_src, 16 ); 
        memcpy(&fields[20], &ip6h->ip6_dst, 16 ); 
        memcpy(&fields[36], &ip6h->ip6_nxt, 1 ); 

        offs = 37;
    }             
  
    /* fill rest of the data with bytes after ip header */
    memset(&fields[offs], 0, MAX_PAYLOAD);

    if (payld_size > 0) {
        if (payld_size > MAX_PAYLOAD) {
            payld_size = MAX_PAYLOAD;
        }

        memcpy(&fields[offs], &packet[meta->offs[2]], payld_size);
    }            

#ifdef MD5_PROFILING
	gettimeofday(&start_time,NULL);
#endif

	md5(fields, id, offs + MAX_PAYLOAD);
    /* id = md_32(fields, offs + MAX_PAYLOAD); */

#ifdef MD5_PROFILING
	gettimeofday(&end_time,NULL);
#endif

#ifdef MD5_DEBUG
	printf("%ld.%06ld ", meta->tv_sec, meta->tv_usec); 
	print_id(id);
#ifdef MD5_PROFILING
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

	memcpy(data->pkt[data->cnt].id, id, DIG_LEN);
    /* data->pkt[data->cnt].id = id; */
	data->pkt[data->cnt].ts.tv_sec = meta->tv_sec;
	data->pkt[data->cnt].ts.tv_usec = meta->tv_usec;

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
        /* 4 bytes for encoding the binary length per list entry */
        expData = realloc(expData, data->cnt*(sizeof(PktEvent_t)+4) + 8);
	}

    STARTEXPORT( expData );
	ADD_LIST(data->cnt);

	for (i = 0; i<data->cnt; i++) {
        ADD_UINT32( data->pkt[i].ts.tv_sec );
        ADD_UINT32( data->pkt[i].ts.tv_usec );
        ADD_BINARY( DIG_LEN, data->pkt[i].id );
        /* ADD_UINT32( data->pkt[i].id ); */
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
    case I_MODNAME: return "pktid_md5_128";
    case I_BRIEF:   return "MD5-128 packet-id generation";
    case I_AUTHOR:  return "Tanja Zseby";
    case I_CREATED: return "2001/02/20";
    case I_VERBOSE: return "MD5-128 packet-id generation";
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

