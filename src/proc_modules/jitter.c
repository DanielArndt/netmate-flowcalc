
/*! \file  jitter.c

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
jitter evaluation module that does absolutely nothing
(not even output) but beeing an evaluation module

still missing: computation of variance of packet interarrival time
-> needed: store all interarrival time in array until export

$Id: jitter.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include "stdinc.h" 
#include "ProcModule.h"

  /*
    define here the runtime type information structure for this module
  */
  typeInfo_t exportInfo[] = { { UINT32, "packets"            },
                              { UINT64, "diff_min"    },
                              { UINT64, "diff_avg"    },
                              { UINT64, "diff_max"    },
                              { UINT64, "diff_var"    },
                              EXPORT_END };

/*
  definition of the flowdata structure that is to be used to
  store intermediate measurement results (e.g. counters, tstamps)
*/

typedef struct {
    unsigned long long first_tstamp;
    unsigned long long last_tstamp;
    unsigned long long min_diff;
    unsigned long long max_diff;
    unsigned long long sum;
    unsigned long long sqr_sum;
    unsigned int packets;
} flowdata_t;


/*
  code that initializes the action module goes here -
  initModule will be called exactly once after the 
  module is loaded, i.e. first used by one rule
*/
int initModule()
{
  
    return 0;
}


/*
  cleanup code that needs to be executed before the
  module will be unloaded has to go here 
*/
int destroyModule()
{
  
    return 0;
}


/*
  this function will be called for every packet matching
  a specific rule and has to evaluate the incoming packet
*/
int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
    flowdata_t *data;
    long long now;
    unsigned long diff;
  
    data = (flowdata_t*)flowdata;

    now = meta->tv_sec * 1000000 + meta->tv_usec;

    if (data->first_tstamp == 0) {
        data->first_tstamp = now;
    } else {
        diff = now - data->last_tstamp;

        if (diff < data->min_diff ) {
            data->min_diff = diff;
        }
        if (diff > data->max_diff ) { 
            data->max_diff = diff;
        }

        data->sum += diff;
        data->sqr_sum += diff*diff;

        data->packets++;
    }

    data->last_tstamp = now;
    return 0;
}


/*
  initialize per-rule flow data for newly added rule
*/
int initFlowRec( configParam_t *params, void **flowdata )
{
    flowdata_t *data;
  
    data = (flowdata_t*) malloc( sizeof(flowdata_t) );

    if (data == NULL ) {
        return -1;
    }

    data->first_tstamp = 0;
    data->last_tstamp = 0;
    data->min_diff = (unsigned long long)(-1);
    data->max_diff = 0;
    data->packets = 0;
    data->sum = 0;
    data->sqr_sum = 0;

    *flowdata = data;
    return 0;
}


/*
  cleanup per-rule flow data when rule is deleted
*/
int destroyFlowRec( void *flowdata )
{
    
    free( flowdata );
    return 0;
}


int resetFlowRec( void *flowdata )
{
 
    return 0;
}


/*
  this function will be called upon a scheduled data collection 
  for a specific rule. it has to write measurement values from 
  'flowdata' into 'buf' and set len accordingly
*/
int exportData( void **exp, int *len, void *flowdata )
{
    flowdata_t *data;
    static unsigned int expData[30000];
    data = (flowdata_t*)flowdata;

    STARTEXPORT( expData );

    switch( data->packets ) {
    case 0:
        ADD_UINT32( 0);
        ADD_UINT64( 0);
        ADD_UINT64( 0);
        ADD_UINT64( 0);
        ADD_UINT64( 0);
        break;
    case 1:
        ADD_UINT32( 1 );
        ADD_UINT64( 0);
        ADD_UINT64( 0);
        ADD_UINT64( 0);
        ADD_UINT64( 0);
        break;
    default:
        ADD_UINT32( data->packets );
        ADD_UINT64( data->min_diff );
        ADD_UINT64( (data->last_tstamp - data->first_tstamp) / data->packets );
        ADD_UINT64( data->max_diff );
        ADD_UINT64( (data->sqr_sum - (data->sum*data->sum/data->packets)) / (data->packets - 1) ); 
    }

    ENDEXPORT( exp, len );

    data->packets = 0;
    data->first_tstamp = data->last_tstamp;
    data->min_diff = (unsigned long long)(-1);
    data->max_diff = 0;
    data->sum = 0;
    data->sqr_sum = 0;

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
    case I_MODNAME:   return "jitter";
    case I_BRIEF:  return "compute jitter statistics";
    case I_AUTHOR: return "Carsten Schmoll";
    case I_CREATED: return "2002/07/12";
    case I_VERBOSE: return "this module computes min/max/avg jitter (i.e.packet interarrival times) based on packet timestamps";
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




timers_t* getTimers( void *flowData )
{
    return NULL;
}

