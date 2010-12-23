
/*! \file  show_ascii.c

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
    export packet octets as string which contains
    '.' for non-readable chararcters and the original character for readable chars

    $Id: show_ascii.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include "ip.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include "ProcModule.h"


#define FIN  1
#define SYN  2
#define RST  4
#define PSH  8
#define ACK 16
#define URG 32


static uint8_t *expData;
static uint32_t expSize;


#define MINROWS  20 /* initial number of rows in packet data table */
#define NOPORT 1000000  /* marker id for non-available port */
#define IPV6HDR_SIZE 40 /* IPv6 header is always 40 bytes */


/*
  define here the runtime type information structure for this module
*/
typeInfo_t exportInfo[] = {

    { LIST, "packets" },

    { UINT8,  "ipversion" },
    { UINT8,  "ttl" },
    { UINT16, "pktlen" },
    { UINT16, "iphdrlen" },

    { UINT16, "proto" },
    { UINT16, "tcpudphdrlen" },

    { STRING, "srcip" },
    { UINT16, "srcport" },

    { STRING, "dstip" },
    { UINT16, "dstport" },

    { UINT32, "tcpseqno" },
    { STRING, "tcpflags" },

    LIST_END,
    EXPORT_END
};

static char map[256];

#ifdef DEBUG
static char buffer[65537];
#endif

/*#define INET6_ADDRSTRLEN 46*/

typedef struct {

    uint8_t   ipversion;
    uint8_t   ttl;
    uint16_t  pktlen;
    uint16_t  iphdrlen;
    uint16_t  proto;
    uint16_t  tcpudphdrlen;
    uint8_t   srcip[INET6_ADDRSTRLEN];
    uint8_t   dstip[INET6_ADDRSTRLEN];
    uint16_t  srcport;
    uint16_t  dstport;
    uint32_t  tcpseqno;
    uint8_t   tcpflags;

} packetData_t;

#define ROWSTRSIZE (sizeof(packetData_t)+24) /* 24 extra bytes for IP flags string if all bits are set */


typedef struct {

    uint32_t      nrows;   /* number of rows in list == number of stored packets */
    uint32_t      maxrows; /* maximum number of rows storable without resize of pkt */
    packetData_t *pkt;     /* the dynamic list of packet attributes */

} flowRec_t;


/*
  init static members of this module
  is executed only once after first load of this module
*/

int initModule()
{
    uint16_t c;

    /* initialise export data buffer with zero size */
    expData = NULL;
    expSize = 0;

    /*
      fill character map to map bytes to printable chars 
      (non-printable chars will be shown as '.' in DEBUG output mode)
    */

    for (c = 0; c < 256; c++) { /* map all to '.' in first step */
        map[c] = '.';
    }

    for (c = 32; c < 127; c++) { /* map all printable chars to themselves */
        map[c] = c;
    }

#ifdef DEBUG2
    /* show what the mapping results in */
    fprintf(stderr, "the characters 0 - 255 are mapped to the following chars:\n");
    for (c = 0; c < 256; c++) {
	fprintf(stderr, "%c", map[c]);
    }
    printf("\n");
#endif
    return 0;
}


/*
  cleanup code that needs to be executed before the
  module will be unloaded has to go here 
*/

int destroyModule()
{
    free( expData );

    return 0;
}



static inline packetData_t *fetchRow( flowRec_t *data )
{
    /* precondition: data != NULL && data has been initialised (see initFlowRecord) */
    
    /* check if a resize is necessary to have another empty row available */
    if (data->nrows == data->maxrows) {
	packetData_t *newMemory = (packetData_t *) realloc(data->pkt, data->maxrows * sizeof(packetData_t) * 2);
	if (newMemory == NULL) {
	    return NULL;
	}
	data->pkt = newMemory;
	data->maxrows = data->maxrows * 2;
    }

    return &(data->pkt[data->nrows]);
}


static inline char *printFlags(uint8_t tcpflags) 
{
    static uint8_t buf[25];

    if (tcpflags == 0) {
	buf[0] = '\0';
    } else {
	sprintf(buf, "%s%s%s%s%s%s",
		(tcpflags & FIN) ? "fin " : "",
		(tcpflags & SYN) ? "syn " : "",
		(tcpflags & RST) ? "rst " : "",
		(tcpflags & PSH) ? "psh " : "",
		(tcpflags & ACK) ? "ack " : "",
		(tcpflags & URG) ? "urg " : "");
    }
    return buf;
}

/*
  this function will be called for every packet matching
  a specific rule and has to evaluate the incoming packet
*/

int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
    /*fprintf(stderr, "dummy : processPacket (l = %d)\n", meta->len);*/

    uint8_t  *pos;
    packetData_t *pkt;

#ifdef DEBUG2
    fprintf(stderr, "show_ascii::processPacket (l = %d)\n" 
	    "frame type = 0x%04x / offs[0 1 2 3] = %d %d %d %d\n", 
	    meta->len, ntohs(((uint16_t *)packet)[6]),
	    meta->offs[L_LINK], meta->offs[L_NET], 
	    meta->offs[L_TRANS], meta->offs[L_DATA]);
#endif

    /* get a storage location (data row) for the packet attributes */
    if ((pkt = fetchRow(flowdata)) == NULL) {
	return -1;
    }
    
    switch(meta->layers[L_NET]) {

    case N_IP:  /* IPv4 */
	{
	/* map IPv4 header structure onto packet in memory */
	struct iphdr *iphdr = (struct iphdr*) (packet + meta->offs[L_NET]); /* skip Layer 2 header */
	
	pkt->ipversion = iphdr->version;
	pkt->ttl       = iphdr->ttl;
	pkt->pktlen    = ntohs(iphdr->tot_len);
	pkt->iphdrlen  = iphdr->ihl * 4;

	inet_ntop(AF_INET, &(iphdr->saddr), pkt->srcip, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &(iphdr->daddr), pkt->dstip, INET_ADDRSTRLEN);
	break;
	}
    case N_IP6: /* IPv6 */
	{
	/* map IPv6 header structure onto packet in memory */
	struct ip6_hdr *ip6hdr = (struct ip6_hdr*) (packet + meta->offs[L_NET]); /* skip Layer 2 header */
	
	pkt->ipversion = ntohl(ip6hdr->ip6_flow) >> 28;
	pkt->ttl       = ip6hdr->ip6_hlim; /* hop limit */
	pkt->pktlen    = ntohs(ip6hdr->ip6_plen) + IPV6HDR_SIZE;
	pkt->iphdrlen  = IPV6HDR_SIZE;

	inet_ntop(AF_INET6, &(ip6hdr->ip6_src), pkt->srcip, INET6_ADDRSTRLEN);
	inet_ntop(AF_INET6, &(ip6hdr->ip6_dst), pkt->dstip, INET6_ADDRSTRLEN);
	break;
	}
    default:    /* layer 3 type is neither IPv4 nor IPv6 */
        return 0;
    }

    pkt->proto = packet[meta->offs[L_NET] + 9]; /* store protocol type */
    pos = packet + meta->offs[L_TRANS]; /* set pos to Layer 3 start, i.e. TCP/UDP header */

    if (pkt->proto == IPPROTO_TCP) {

	/* discard packet if snapsize is too small (need >=14 TCP header bytes) */
	if (meta->cap_len < (unsigned) meta->offs[L_TRANS] + 14) {
	    printf( "[show_ascii]: snapsize to small - discarding TCP packet\n");
	    return 0;
	}

	pkt->srcport = ntohs(((uint16_t *)(pos))[0]);
	pkt->dstport = ntohs(((uint16_t *)(pos))[1]);
	
	pkt->tcpudphdrlen = (pos[12] & 0xf0) / 4;
	/* divide by 4 equals: shift down 4 bits and multiply by 4 */
	
	pkt->tcpseqno = ntohl(((uint32_t *)(pos))[1]);
	pkt->tcpflags = (pos[13] & 0x3f);
	
    } else if (pkt->proto == IPPROTO_UDP) {
	
	/* discard packet if snapsize is too small (need >=6 UDP header bytes) */
	if (meta->cap_len < (unsigned) meta->offs[L_TRANS] + 6) {
	    printf( "[show_ascii]: snapsize to small - discarding UDP packet\n");
	    return 0;
	}

	pkt->srcport = ntohs(((uint16_t *)(pos))[0]);
	pkt->dstport = ntohs(((uint16_t *)(pos))[1]);
	
	pkt->tcpudphdrlen = 8; /* fixed header size for UDP packets */
	pkt->tcpseqno = 0;
	pkt->tcpflags = 0;

    } else if (pkt->proto == IPPROTO_ICMP) {
	
	/* discard packet if snapsize is too small (need >=2 ICMP header bytes) */
	if (meta->cap_len < (unsigned) meta->offs[L_TRANS] + 2) {
	    printf( "[show_ascii]: snapsize to small - discarding IMCP packet\n");
	    return 0;
	}

	pkt->srcport = ((uint8_t *)(pos))[0]; // store ICMP type as srcport
	pkt->dstport = ((uint8_t *)(pos))[1]; // store ICMP code as dstport
	
    } else { /* it's neither UDP nor TCP nor ICMP */
	
	pkt->tcpudphdrlen = 0;
	strcpy(pkt->srcip, "not TCP|UDP|ICMP");
	strcpy(pkt->dstip, "not TCP|UDP|ICMP");
	pkt->srcport  = 0;
	pkt->dstport  = 0;
	pkt->tcpseqno = 0;
	pkt->tcpflags = 0;
    }
    
    /* remember that we have stored a line of export data */
    ((flowRec_t *)flowdata)->nrows += 1;
    
#ifdef DEBUG

    /* also print packet attributes to stderr */

    fprintf( stderr,
	     "IPv%u, ttl=%u, pktlen=%u, iphdrlen=%u, proto=%u, tcp/udp-hdrlen=%u,\n"
	     "srcip=%s, srcport=%u, dstip=%s, dstport=%d, tcpseqno=%u, tcpflags=%s\n",
	     pkt->ipversion,
	     pkt->ttl,
	     pkt->pktlen,
	     pkt->iphdrlen,
	     pkt->proto,
	     pkt->tcpudphdrlen,
	     pkt->srcip,
	     pkt->srcport,
	     pkt->dstip,
	     pkt->dstport,
	     pkt->tcpseqno,
	     printFlags(pkt->tcpflags));


    /* write printable characters from packet body to stdout (use . for non-printable chars) */

    if (meta->offs[L_DATA] != -1) { /* is there data inside the TCP/UDP packet? */
	uint16_t i, len;

	len = meta->cap_len - meta->offs[L_DATA];
	pos = packet + meta->offs[L_DATA];
	
	for (i = 0; i < len; i++) {
	    buffer[i] = map[pos[i]];
	}
	buffer[i] = 0;
	fprintf(stderr, "packet body (%d bytes): %s\n\n", len, buffer);
    }
#endif  /* DEBUG */

    return 0;
}


/*
  initialize per-rule flow data for newly added rule
*/
int initFlowRec( configParam_t *params, void **flowdata )
{
    flowRec_t *data = (flowRec_t *) malloc(sizeof(flowRec_t));

    if (data == NULL) {
	return -1;
    }

    data->nrows = 0;
    data->pkt   = (packetData_t *)malloc(MINROWS * sizeof(packetData_t));

    if (data->pkt == NULL) {
	data->maxrows = 0;
	return -1;
    }

    data->maxrows = MINROWS;

    *flowdata = data;
    return 0;
}


/*
  cleanup per-rule flow data when rule is deleted
*/
int resetFlowRec( void *flowdata )
{
    flowRec_t *data = (flowRec_t *) flowdata;

    if (data == NULL) {
	return -1;
    } 
    
    if (data->pkt != NULL) {
	free(data->pkt);
	data->nrows = 0;
	data->maxrows = 0;
    }
    return 0;
}


int destroyFlowRec( void *flowdata )
{
    flowRec_t *data = (flowRec_t *) flowdata;

    if (data == NULL) {
	return -1;
    } 
    
    if (data->pkt != NULL) {
	free(data->pkt);
    }

    free(data);

    return 0;
}


/*
  this function will be called upon a scheduled data collection 
  for a specific rule. it has to write measurement values from 
  'flowdata' into 'buf' and set len accordingly
*/
int exportData( void **exp, int *len, void *flowdata )
{
    uint32_t i;
    packetData_t *pkt;
    flowRec_t *data = (flowRec_t *) flowdata;

    /* check if enough buffer space is available and reserve extra memory if not */
    if (expSize < ROWSTRSIZE * (data->nrows + 1)) {
        uint8_t *newMemory = realloc(expData, ROWSTRSIZE * (data->nrows + 1));
	if (newMemory != NULL) {
	    expData = newMemory;
	    expSize = ROWSTRSIZE * data->nrows;
	} else {
	    /* not enough memory to write data to -> don't write data at all */
	    STARTEXPORT(expData);
	    ADD_LIST(0);
	    END_LIST();
	    ENDEXPORT(exp, len);
	    return -1;
	}
    }


    STARTEXPORT(expData);
    ADD_LIST( data->nrows );

    pkt = data->pkt;

    for (i = 0; i < data->nrows; i++) {

	ADD_UINT8(  pkt->ipversion );
	ADD_UINT8(  pkt->ttl );
	ADD_UINT16( pkt->pktlen );
	ADD_UINT16( pkt->iphdrlen );
	
	ADD_UINT16( pkt->proto );
	ADD_UINT16( pkt->tcpudphdrlen );
	
	ADD_STRING( pkt->srcip );
	ADD_UINT16( pkt->srcport );
	
	ADD_STRING( pkt->dstip );
	ADD_UINT16( pkt->dstport );

	ADD_UINT32( pkt->tcpseqno );
	ADD_STRING( printFlags(pkt->tcpflags) );

	pkt++;
    }

    END_LIST();
    ENDEXPORT(exp, len);

    data->nrows = 0;
    
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
    case I_MODNAME:   return "show_ascii";
    case I_BRIEF:  return "do-nothing module";
    case I_AUTHOR: return "Carsten Schmoll";
    case I_CREATED: return "2001/03/20";
    case I_VERBOSE: return "this module does nothing apart from being a valid evaluation module";
    case I_PARAMS:  return "none";
    case I_RESULTS: return "has no results";
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


timers_t *getTimers( void *flowData )
{
    return NULL;
}

