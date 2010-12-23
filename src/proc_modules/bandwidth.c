
/*! \file  bandwidth.c

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
    action module for basic accounting plus bandwidth calculation and 
    intermediate counter logging (writes all collected counters on next export)

    $Id: bandwidth.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include <stdio.h>
#include "ProcModule.h"
#include "arpa/inet.h"
#include <sys/types.h>
#include <time.h>     


const int COUNTCHUNK = 20;    /* new entries per realloc */

/*
  defines timer for making 'counter snapshot'
*/

timers_t timers[] = {
    /* handle, ival_msec, flags */
    {       1,  1000 * 0, TM_RECURRING },
    /*         ival == 0 means no timeout between collections by default */
    TIMER_END
};


typeInfo_t exportInfo[] = { 
    { LIST,   "counters" },
    { UINT32, "packets"       },
    { UINT32, "bytes"         },
    { UINT32, "first_time"    },
    { UINT32, "first_time_us" },
    { UINT32, "last_time"     },
    { UINT32, "last_time_us"  },
    { UINT32, "packet_rate"   },
    { UINT32, "bandwidth"     },
    LIST_END,
    EXPORT_END
};


/* counter snapshot record (one record per snapshot is stored in a list) */

typedef struct {
    long long packets;    /* number of packets received */
    long long bytes;      /* number of bytes received   */
    struct timeval first; /* timestamp of first packet received */
    struct timeval last;  /* timestamp of last packet received  */
    long int packet_bw;   /* bandwidth in packets per second */
    long int bandwidth;   /* bandwidth in bytes per second */
} measData_t;


/* per task flow record information */

typedef struct {
    long int  export_ts;   /* timestamp of measurement start */
    int       numSets;    /* number of records currently stored */
    int       maxSets;    /* maximum number of datasets currently storable (without resize) */
    measData_t curr;      /* current set of (active) counters for last interval*/
    measData_t *dataSets; /* dynamic array for storing intermediate couters */
    timers_t currTimers[ sizeof(timers) / sizeof(timers[0]) ];
} accData_t;


struct timeval zerotime = {0,0};


static char* expData;
static unsigned int   expDataSize;


int initModule()
{
    expDataSize = sizeof(measData_t) + 20;
    expData = (char*) malloc(expDataSize);
    
    return 0;
}


int destroyModule()
{
    free( expData );
    
    return 0;
}


inline void resetCurrent( accData_t *data )
{
    data->curr.packets = 0;
    data->curr.bytes = 0;
    data->curr.first = zerotime;
    data->curr.last = zerotime;
    data->curr.packet_bw = 0;
    data->curr.bandwidth = 0;
}


void appendCounters(accData_t *data )
{
    if (data->currTimers[0].ival_msec == 0) {
	return;
    }
    
    if (data->numSets == data->maxSets ) {
	/* fprintf( stderr, "allocating %d sets\n", (data->maxSets + COUNTCHUNK) ); */
	data->dataSets = (measData_t*) realloc( data->dataSets, (data->maxSets + COUNTCHUNK)*sizeof(measData_t) );
	data->maxSets += COUNTCHUNK; 
    }
    
    /* calculate bandwidth for this collection interval */
    data->curr.bandwidth = data->curr.bytes * 1000 / data->currTimers[0].ival_msec;
    data->curr.packet_bw = data->curr.packets * 1000 / data->currTimers[0].ival_msec;
    
    memcpy( &(data->dataSets[data->numSets++]), &(data->curr), sizeof(measData_t) );
}


int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
    accData_t *data = flowdata;

    /* save timestamp from first incoming packet */
    if (data->curr.packets == 0) {
        data->curr.first.tv_sec = meta->tv_sec;
        data->curr.first.tv_usec = meta->tv_usec;
    }

    /* save timestamp for last incoming packet */
    data->curr.last.tv_sec = meta->tv_sec;
    data->curr.last.tv_usec = meta->tv_usec;

    /* count packets and account packet volume (including link layer) */
    data->curr.packets += 1;
    data->curr.bytes   += meta->len;
    return 0;
}


int initFlowRec( configParam_t *params, void **flowdata )
{
    accData_t *data;

    data = malloc( sizeof(accData_t) );

    if (data == NULL ) {
        return -1;
    }
    
    data->numSets = 0;
    data->maxSets = 0;
    data->dataSets = NULL;
    data->export_ts = time(NULL);
    
    /* init current block of counters */
    resetCurrent( data );

    /* copy default timers to current timers array for a specific task */
    memcpy(data->currTimers, timers, sizeof(timers));

    while (params[0].name != NULL) {
        if (!strcmp(params[0].name, "Interval")) {
            int a = atoi(params[0].value);
            if (a > 0) {
                data->currTimers[0].ival_msec = 1000 * a;
#ifdef DEBUG
		fprintf( stderr, "bandwidth module: setting snapshot timer to %d secs\n", a );
#endif
            }
        }
        params++;
    }
    
    *flowdata = data;

    return 0;
}


int resetFlowRec( void *flowdata )
{

    return 0;
}


int destroyFlowRec( void *flowdata )
{
  accData_t *data = flowdata;

  free( data->dataSets );
  free( data );

  return 0;
}


int exportData( void **exp, int *len, void *flowdata )
{
    accData_t *data = flowdata;
    measData_t *curr;
    int i;

    /* if no intermediate snapshots were taken then only export current pointers */
    if (data->numSets == 0) {

	long int tstamp = time(NULL);

	/* calculate bandwidth for this collection interval */
	if (tstamp == data->export_ts) { // avoid division by zero
	  data->curr.bandwidth = 0;
	  data->curr.packet_bw = 0;
	} else {
	  data->curr.bandwidth = data->curr.bytes / (tstamp - data->export_ts);
	  data->curr.packet_bw = data->curr.packets / (tstamp -data->export_ts);
	}

	STARTEXPORT(expData);
	ADD_LIST(1); /* one row of data */

        ADD_UINT32( data->curr.packets       );
        ADD_UINT32( data->curr.bytes         );
        ADD_UINT32( data->curr.first.tv_sec  );
        ADD_UINT32( data->curr.first.tv_usec );
        ADD_UINT32( data->curr.last.tv_sec   );
        ADD_UINT32( data->curr.last.tv_usec  );
        ADD_UINT32( data->curr.packet_bw     );
        ADD_UINT32( data->curr.bandwidth     );

	END_LIST();
	ENDEXPORT( exp, len );

	resetCurrent(data);       /* reset counters */
	data->export_ts = tstamp; /* remember time of last export */

	return 0;
    }

    /* else export stored snapshots */

    if (expDataSize < data->numSets * sizeof(measData_t) + 20) {
        expDataSize = data->numSets * sizeof(measData_t) + 20;
        expData = (char*) realloc( expData, expDataSize );
    }

    STARTEXPORT( expData );
    ADD_LIST( data->numSets );
    
    curr = data->dataSets;
    for( i = 0; i<data->numSets; i++ ) {
        
        ADD_UINT32( curr->packets       );
        ADD_UINT32( curr->bytes         );
        ADD_UINT32( curr->first.tv_sec  );
        ADD_UINT32( curr->first.tv_usec );
        ADD_UINT32( curr->last.tv_sec   );
        ADD_UINT32( curr->last.tv_usec  );
        ADD_UINT32( curr->packet_bw     );
        ADD_UINT32( curr->bandwidth     );
        
        curr++;
    }

    END_LIST();
    ENDEXPORT( exp, len );

    data->numSets = 0;

    return 0;
}


int dumpData( char *string, void *flowdata )
{

    return 0;
}


char* getModuleInfo(int i)
{
    /* fprintf( stderr, "count : getModuleInfo(%d)\n",i ); */

    switch(i) {
    case I_MODNAME:    return "bandwidth";
    case I_VERSION:    return "2.0";
    case I_CREATED:    return "2002/12/19";
    case I_MODIFIED:   return "2003/09/08";
    case I_BRIEF:      return "intermediate accounting (packets+volume) "
                         "plus bandwidth calculation";
    case I_VERBOSE:    return "count packets, volume, save first and last "
                         "time, packet and volume bandwidth";
    case I_HTMLDOCS:   return "http://www.fokus.fhg.de/modules/bandwidth.html";
    case I_PARAMS:     return "Interval[int] : store counters every [int] "
                         "seconds";
    case I_RESULTS:    return "results description = ...";
    case I_AUTHOR:     return "Carsten Schmoll";
    case I_AFFILI:     return "Fraunhofer Institute FOKUS, Germany";
    case I_EMAIL:      return "schmoll@fokus.fraunhofer.de";
    case I_HOMEPAGE:   return "http://homepage";
    default: return NULL;
    }
}


char* getErrorMsg( int code )
{
    return NULL;
}


int timeout( int timerID, void *flowdata )
{
    accData_t *data = (accData_t *)flowdata;

#ifdef DEBUG
    fprintf( stderr, "bandwidth: timeout(n = %d)\n", timerID );
#endif

    appendCounters(data);
    resetCurrent(data);

    return 0;
}


timers_t* getTimers( void *flowData )
{
    accData_t *data = (accData_t *)flowData;

    /* return timers record if ival > 0, else return "no timers available" (NULL) */
    return (data->currTimers[0].ival_msec > 0) ? data->currTimers : NULL;
}
