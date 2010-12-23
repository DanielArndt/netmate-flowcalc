
/*! \file  port_use.c

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

Decsription:
packet evaluation module for generating destination port usage statistics

$Id: port_use.c 748 2009-09-10 02:54:03Z szander $
*/

/*
  FIXME: 
  improved sort algorithhm to find top-n
  differentiate between udp and tcp
*/

#include "config.h"
#include <stdio.h>
#include "ProcModule.h"
#include "stdlib.h"
#include "arpa/inet.h"

/*
  define an array that stores the type information for the
  data structure exported by this module's exportData function
*/
typeInfo_t exportInfo[] = { { LIST, "port_list"  },
                            { STRING, "port"     },
                            { UINT64, "packets"  },
                            { UINT64, "volume"   },
                            EXPORT_END };

#define TOP_N 10


struct accData_t {
    unsigned long long all_packets;   /* all packets seen for this rule */
    unsigned long long all_bytes;
    unsigned long long other_packets; /* neither tcp nor udp */
    unsigned long long other_bytes;
    unsigned long long rest_packets;  /* number of packets that do not belong to top n*/
    unsigned long long rest_bytes;
    unsigned long long packets[65536]; /* counters per per port */
    unsigned long long bytes[65536];
    unsigned short num;
    int count_relative;
    unsigned short *top_ports;
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
    unsigned short len, dport;
    struct accData_t *data = flowdata;

    
    /* length of packet */
    len = meta->len;

    data->all_packets += 1;
    data->all_bytes   += len;

    switch ( meta->layers[2] ) {
    case T_UDP:
    case T_TCP:
        dport = ntohs(*((unsigned short *) &packet[meta->offs[2] + 2]));
        data->packets[ dport ] += 1;
        data->bytes  [ dport ] += len;
        break;
    default:
        data->other_packets += 1;
        data->other_bytes   += len;
    }

    return 0;
}

int initFlowRec( configParam_t *params, void **flowdata )
{
    struct accData_t *data;

    data = malloc(sizeof(struct accData_t));
    if (data == NULL ) {
        return -1;
    }

    data-> all_packets = 0;
    data-> all_bytes = 0;
    data-> other_packets = 0;
    data-> other_bytes = 0;
    data-> rest_packets = 0;
    data-> rest_bytes = 0;
    data-> count_relative = 0;

    memset(data->packets, 0, sizeof(data->packets));
    memset(data->bytes, 0, sizeof(data->bytes));

    data->top_ports = malloc( TOP_N * sizeof(unsigned short));
    if (data->top_ports == NULL) {
        return -1;
    }
    memset(data->top_ports, 0, TOP_N * sizeof(unsigned short));

    *flowdata = data;

    return 0;
}

int resetFlowRec( void *flowdata )
{
    
    return 0;
}

int destroyFlowRec( void *flowdata )
{
    struct accData_t *data = flowdata;
   
    free( data->top_ports );
    free( data );
   
    return 0;
}


struct set {
    unsigned short index;
    unsigned long long pck;
    unsigned long long vol;
};

int setcmp( const void *a, const void *b )
{
    return ((struct set*)b)->vol - ((struct set*)a)->vol;
}

int exportData( void **exp, int *len, void *flowdata )
{
    struct accData_t *data = flowdata;
    int i;
    int entries = 0;
    struct set res[65536];
    static char buf[(TOP_N+3)*32 + 8];
    
    /* sort ports by volume */
    /* FIXME - sorting is very slow in the worst case */
    for (i = 0; i < 65536; i++ ) {
        /* only use non zero entries */
        if (data->packets[i] > 0) {
            res[entries].index = i;
            res[entries].pck = data->packets[i];
            res[entries].vol = data->bytes[i];
           
            entries++;
        }
    }
    qsort( res, entries, sizeof(struct set), setcmp );

    data->rest_packets = data->all_packets - data->other_packets;
    data->rest_bytes = data->all_bytes   - data->other_bytes;

    /* max export the TOP_N entries */
    if (entries > TOP_N) {
        entries = TOP_N;
    }

    STARTEXPORT( buf );
    ADD_LIST(entries + 3);

    /* top ports */
    for ( i = 0; i < entries; i++ ) {     
        char cbuf[6];
        sprintf(cbuf, "%d", res[i].index);
        ADD_STRING(cbuf);
        ADD_UINT64(res[i].pck);
        ADD_UINT64(res[i].vol);
       
        data->rest_packets -= res[i].pck;
        data->rest_bytes   -= res[i].vol;
    }

    /* overall, non IP volume and rest (non top ports) */
    ADD_STRING("All");
    ADD_UINT64(data->all_packets);
    ADD_UINT64(data->all_bytes);
    ADD_STRING("Non-IP");
    ADD_UINT64(data->other_packets);
    ADD_UINT64(data->other_bytes);
    ADD_STRING("Rest");
    ADD_UINT64(data->rest_packets);
    ADD_UINT64(data->rest_bytes);

    END_LIST();
    ENDEXPORT( exp, len );

    return 0;
}


int dumpData( char *string, void *flowdata )
{
  
    return 0;
}

char* getModuleInfo(int i)
{
  
    switch(i) {
    case I_MODNAME:   return "port_use";
    case I_BRIEF:  return "measure uages of dst port numbers";
    case I_AUTHOR: return "Carsten Schmoll";
    case I_CREATED: return "2001/03/09";
    case I_VERBOSE: return "count packets+volume per port-number";
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

