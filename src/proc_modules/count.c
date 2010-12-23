
/*! \file  count.c

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
action module for basic accounting

$Id: count.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include <stdio.h>
#include "ProcModule.h"
#include <sys/types.h>
#include <arpa/inet.h>


typeInfo_t exportInfo[] = { { UINT32, "packets"    },
                            { UINT32, "volume"     },
                            { UINT32, "first_time" },
                            { UINT32, "first_time_us" },
                            { UINT32, "last_time"  },
                            { UINT32, "last_time_us"  },
                            EXPORT_END };



struct accData_t {
    long packets;
    long bytes;
    struct timeval first;
    struct timeval last;
};

struct timeval zerotime = {0,0};


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
    struct accData_t *data = flowdata;

    if (data->packets == 0) {
	    data->first.tv_sec = meta->tv_sec;
	    data->first.tv_usec = meta->tv_usec;	
	}

    data->last.tv_sec = meta->tv_sec;
    data->last.tv_usec = meta->tv_usec;

    data->packets += 1;
    data->bytes   += meta->len;

    return 0;
}


int initFlowRec( configParam_t *params, void **flowdata )
{
    struct accData_t *data;

    data = malloc( sizeof(struct accData_t) );

    if (data == NULL ) {
        return -1;
    }

    resetFlowRec( data );

    *flowdata = data;
    return 0;
}

int resetFlowRec( void *flowdata )
{
    struct accData_t *data = (struct accData_t*) flowdata;

    if (!flowdata ) {
        return -1;
    }

    data->packets = 0;
    data->bytes = 0;
    data->first = zerotime;
    data->last = zerotime;

    return 0;
}


int destroyFlowRec( void *flowdata )
{
    /*
      struct accData_t *data = flowdata;

      fprintf( stderr, "count : destroyFlowRec\n" );
      fprintf(stderr, "packets: %lld, bytes : %lld\n", 
      data->packets,
      data->bytes );
    */

    free(flowdata);
    
    return 0;
}

int exportData( void **exp, int *len, void *flowdata )
{
    static unsigned int expData[8];

    struct accData_t *data = flowdata;

    /*
      fprintf( stderr, "count : exportData\n" );
      fprintf( stderr, "0: %s\n1: %ld\n2: %ld\n3: %ld.%06ld\n4: %ld.%06ld\n",
      getModuleInfo(0),
      data->packets,
      data->bytes,
      data->first.tv_sec, data->first.tv_usec,
      data->last.tv_sec, data->last.tv_usec );
    */

    STARTEXPORT( expData );
    ADD_UINT32( data->packets       );
    ADD_UINT32( data->bytes         );
    ADD_UINT32( data->first.tv_sec  );
    ADD_UINT32( data->first.tv_usec );
    ADD_UINT32( data->last.tv_sec   );
    ADD_UINT32( data->last.tv_usec  );
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
    case I_MODNAME:   return "count";
    case I_VERSION:   return "1.1";
    case I_CREATED:   return "2000/12/06";
    case I_MODIFIED:  return "2003/09/08";
    case I_BRIEF:     return "basic accounting";
    case I_VERBOSE:   return "count packets, volume, save first and last time";
    case I_HTMLDOCS:  return "http://www.fokus.fhg.de/modules/count.html";
    case I_PARAMS:    return "no parameters";
    case I_RESULTS:   return "6 x uint32 - packets,volume,firsttime(sec/usec),lasttime(sec/usec)";
    case I_AUTHOR:    return "Carsten Schmoll";
    case I_AFFILI:    return "Fraunhofer Institute FOKUS, Germany";
    case I_EMAIL:     return "schmoll@fokus.fraunhofer.de";
    case I_HOMEPAGE:  return "http://homepage";
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

