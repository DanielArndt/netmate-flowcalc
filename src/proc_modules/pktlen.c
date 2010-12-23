
/*! \file  pktlen.c

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
evaluation module for logging packet length statistics

$Id: pktlen.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "ProcModule.h"


typeInfo_t exportInfo[] = { { UINT16, "minlen"     },
                            { UINT16, "maxlen"     },
                            { UINT16, "avglen"     },
                            EXPORT_END };


/* the per-task flow data of this evaluation modules */

struct moduleData {
    unsigned short min, max;
    unsigned long long packets, bytes;
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
    struct moduleData *data = (struct moduleData*) flowdata;
    unsigned short len;

    len = meta->len;

    if (data->packets == 0) {
        data->min = len;
        data->max = len;
    } else {
        if (len < data->min ) {
            data->min = len;
        }
        if (len > data->max ) {
            data->max = len;
        }
    }

    data->packets += 1;
    data->bytes += len;

    return 0;
}

int initFlowRec( configParam_t *params, void **flowdata )
{
    struct moduleData *data;

    data = (struct moduleData*) malloc( sizeof( struct moduleData ) );

    if (data == NULL ) {
        return -1;
    }

    data->packets = 0;
    data->bytes = 0;
    data->min = 0;
    data->max = 0;
    
    *flowdata = data;

    return 0;
}

int resetFlowRec( void *flowdata )
{

    return 0;
}

int destroyFlowRec( void *flowdata )
{
   
    free( flowdata );
   
    return 0;
}

int exportData( void **exp, int *len, void *flowdata )
{
    struct moduleData *data = (struct moduleData*) flowdata;
    static unsigned short buf[3];
    unsigned short avg;

 
    avg = (data->packets == 0) ? 0 : data->bytes / data->packets;

    STARTEXPORT( buf );
    ADD_UINT16( data->min );
    ADD_UINT16( data->max );
    ADD_UINT16( avg );
    ENDEXPORT( exp, len );

    data->packets = 0;
    data->bytes = 0;
    data->min = 0;
    data->max = 0;

    return 0;
}

int dumpData( char *string, void *flowdata )
{

    return 0;
}

char* getModuleInfo(int i)
{
   
    switch(i) {
    case I_MODNAME: return "pktlen";
    case I_VERSION: return "1.3";
    case I_BRIEF:   return "count min/max/avg packet length per interval";
    case I_AUTHOR:  return "Carsten Schmoll";
    case I_CREATED: return "2001/05/16";
    case I_VERBOSE: return "count min/max/avg packet length per interval";
    case I_PARAMS:  return "none";
    case I_RESULTS: return "three int16 numbers for min/max/avg packet length";
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

