
/*! \file  netai_flowstats.c

Modifications Copyright Daniel Arndt, 2010

This version has been modified to provide additional output and 
compatibility. For more information, please visit

http://web.cs.dal.ca/~darndt/

Copyright 2005-2006 Swinburne University of Technology 

This file is part of Network Traffic based Application Identification (netAI).

netAI is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

netAI is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this software; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


Description:
module to compute various flow statistics

$Id: netai_flowstats.c 18 2006-01-23 01:43:50Z szander $
*/

#include "config.h"
#include <stdio.h>
#include "ProcModule.h"
#include <sys/types.h>
#include <arpa/inet.h>

/* definiton of export data */
typeInfo_t exportInfo[] = { 
  { UINT32, "total_fpackets"    }, // totals
  { UINT32, "total_fvolume"     },
  { UINT32, "total_bpackets"    },
  { UINT32, "total_bvolume"     },
  { INT32, "min_fpktl" }, // pkt len distribution
  { INT32, "mean_fpktl" },
  { INT32, "max_fpktl"  },
  { UINT32, "std_fpktl" },
  { INT32, "min_bpktl" },
  { INT32, "mean_bpktl" },
  { INT32, "max_bpktl"  },
  { UINT32, "std_bpktl" },
  { UINT64, "min_fiat"  }, // inter-arrival time dist
  { UINT64, "mean_fiat" },
  { UINT64, "max_fiat" },
  { UINT64, "std_fiat" },
  { UINT64, "min_biat" }, // inter-arrival time dist
  { UINT64, "mean_biat" },
  { UINT64, "max_biat" },
  { UINT64, "std_biat" },
  { UINT64, "duration" },
  { UINT64, "min_active" },
  { UINT64, "mean_active" },
  { UINT64, "max_active" },
  { UINT64, "std_active" },
  { UINT64, "min_idle" },
  { UINT64, "mean_idle" },
  { UINT64, "max_idle" },
  { UINT64, "std_idle" },
  { UINT32, "sflow_fpackets" }, // packets, bytes per subflow
  { UINT32, "sflow_fbytes" },
  { UINT32, "sflow_bpackets" },
  { UINT32, "sflow_bbytes" },
  { UINT32, "fpsh_cnt" }, // push and urg counters
  { UINT32, "bpsh_cnt" },
  { UINT32, "furg_cnt" },
  { UINT32, "burg_cnt" },
  { UINT32, "total_fhlen"    },
  { UINT32, "total_bhlen"    },
  //{ UINT64, "time_s" },
  EXPORT_END 
};


const unsigned long long DEF_MIN_DURATION = 0ULL; // min duration in usecs
const unsigned long long DEF_TIME_CAP = 0xFFFFFFFFFFFFFFFFULL; // ignore further packets after
const unsigned long long DEF_IDLE_THRESHOLD = 62000000ULL; // in usecs (62s)

/* stuff for TCP connection tracking */

/* TCP bits */
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20


/* TCP states */
typedef enum {
  STATE_INITIAL = 0,
  STATE_SYN,
  STATE_SYNACK,
  STATE_ESTABLISHED,
  STATE_FIN,
  STATE_CLOSED
} tcp_state_t;


int isSet(int bit, int test)
{
  return ((bit & test) == bit);
}

/* our main data structure */

struct flowData_t {
  uint32_t fpackets;
  uint32_t fbytes;
  uint32_t bpackets;
  uint32_t bbytes;

  int32_t min_fpktl;
  double pktl_fsum;
  double pktl_fsqsum;
  int32_t max_fpktl;
  int32_t min_bpktl;
  double pktl_bsum;
  double pktl_bsqsum;
  int32_t max_bpktl;
  
  uint32_t min_fiat;
  uint32_t max_fiat;
  double iat_fsum;
  double iat_fsqsum;
  uint32_t min_biat;
  uint32_t max_biat;
  double iat_bsum;
  double iat_bsqsum;

  uint64_t flast;
  uint64_t blast;
  // first packet for the flow
  uint64_t first;
  // last start of active period
  uint64_t lactive_start;

  // no of active periods
  int32_t activep;
  // active time
  double activet;
  uint64_t min_active;
  uint64_t max_active;
  double active_sqsum;

  // no of idle periods
  int32_t idlep;
  double idlet;
  uint64_t min_idle;
  uint64_t max_idle;
  double idle_sqsum;

  // number of packet pairs (number of iat samples)
  uint32_t fivals;
  uint32_t bivals;

  // state in client and server
  tcp_state_t cstate;
  tcp_state_t sstate;
  // set to 1 if we ever reach establish
  int valid;
  int tcp_has_data;

  /* tcp header flags specific counters */
  uint32_t fpsh_cnt, bpsh_cnt; 
  uint32_t furg_cnt, burg_cnt; 

  /* DA: header lengths */
  uint32_t total_fhlen;
  uint32_t total_bhlen;

  uint64_t idle_threshold;
  uint64_t min_duration;

  uint64_t time_cap;
};


/* proc module API functions start from here */

int initModule()
{
  return 0;
}


int destroyModule()
{
  return 0;
}


// get the timestamp of the last packet (which is either a forward pkt or a backward pkt)
unsigned long long getLast(struct flowData_t *data)
{
  if (data->blast == 0ULL) {
    return data->flast;
  } else if (data->flast == 0ULL) {
    return data->blast;
  } else {
    return (data->flast > data->blast) ? data->flast : data->blast;
  }
}


/* dir is the direction of state, pdir is the direction of the packet */
int updateState(tcp_state_t *state, int flags, int dir, int pdir)
{
  if (isSet(TCP_RST, flags)) {
    *state = STATE_CLOSED;
  }
  else if (isSet(TCP_FIN, flags) && (dir == pdir)) {
    *state = STATE_FIN;
  }
  else if (*state == STATE_FIN) {
    if (isSet(TCP_ACK, flags) && (dir != pdir)) {
      *state = STATE_CLOSED;
    }
  } 
  else if (*state == STATE_INITIAL) {
    if (isSet(TCP_SYN, flags) && (dir == pdir)) {
      *state = STATE_SYN;
    }
  }
  else if (*state == STATE_SYN) {
    if (isSet(TCP_SYN, flags) && isSet(TCP_ACK, flags) && (dir != pdir)) {
      *state = STATE_SYNACK;
    }
  }
  else if (*state == STATE_SYNACK) {
    if (isSet(TCP_ACK, flags) && (dir == pdir)) {
      *state = STATE_ESTABLISHED;
    }
  }

  return 0;
}


int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
  struct flowData_t *data = flowdata;
  unsigned long long now = 0;
  unsigned long long diff = 0;
  unsigned short len = 0, proto = 0;
  
  // len = meta->len;
  // get the length of the IP header and data
  len = ntohs(*((unsigned short *) &meta->payload[meta->offs[L_NET] + 2]));
  
  now = meta->tv_sec*1000000ULL + meta->tv_usec;
  assert(now >= data->first);
  
  /* ignore 'reordered packets' */
  if (now < getLast(data)) {
    printf("flowstats: ignoring reordered packet\n");
    return 0;
  }
  /* get length of IP header */
  unsigned short iphlen = (meta->payload[meta->offs[L_NET]] & 0x0F) * 4;
  /* DA: Add the ip header length to the total header length */
  if (meta->reverse == 0) {
    data->total_fhlen += iphlen;
  } else {
    data->total_bhlen += iphlen;
  }

  /* transport proto specific stuff */
  proto = (unsigned short) meta->payload[meta->offs[L_NET] + 9];
  
  /* set valid immediatly for ICMP */
  if (!data->valid && (proto == T_ICMP)) {
    data->valid++;
    /* Add 8 to header total for ICMP */
    if (meta->reverse == 0) {
      data->total_fhlen += 8;
    } else {
      data->total_bhlen += 8;
    }
  }
  
  /* set valid if UDP and at least 1 byte payload */
  if (!data->valid && (proto==T_UDP)) {
    /* must at least have one byte of data */
    unsigned short udplen = ntohs(*((unsigned short *) &meta->payload[meta->offs[L_TRANS] + 4]));
    
    /* Add 8 header total for UDP */
    if (meta->reverse == 0) {
      data->total_fhlen += 8;
    } else {
      data->total_bhlen += 8;
    }
    if (udplen > 8) { 
      data->valid++;
    }
  }
  
  /* FIXME: test for 1 packet in both dir for ICMP/UDP too */
  
  /* heuristic for detecting a valid TCP connection, connection must have been
	 established with a SYN, SYN/ACK, ACK and at least 1 packet must contain data
	 OR at least one packet with ACK flag set contains data AND there must be
	 at least 1 packet in each direction */
  if (proto==6) {
    unsigned short flags = (unsigned short) meta->payload[meta->offs[L_TRANS] + 13];
    
    updateState(&(data->cstate), flags, 0, meta->reverse);
    updateState(&(data->sstate), flags, 1, meta->reverse);
    /* printf("%d %d\n", data->cstate, data->sstate); */
    unsigned long tcphlen = ((meta->payload[meta->offs[L_TRANS] + 12] & 0xF0) >> 4) * 4;
    if (!data->valid) {
      if ( (data->cstate == STATE_ESTABLISHED) || 
        (data->sstate == STATE_ESTABLISHED) /*|| isSet(TCP_ACK, flags) */) {
        /* if IP packet is longer than IP/TCP headers than there must be data */
        if (len > (iphlen + tcphlen)) {
          data->tcp_has_data++;
        }
      }
      
      // hacked for time cap (counters won't be increased after time cap
      if (data->tcp_has_data && ( ((now - data->first) > data->time_cap) || ((data->fpackets > 0) && (data->bpackets > 0)) )) {
        data->valid++;
      }
    }
    
    /* don't compute after time cap */
    if ((data->first == 0) || ((now - data->first) <= data->time_cap)) {
      
      if (isSet(TCP_PSH, flags)) {
        if (meta->reverse == 0) {
          data->fpsh_cnt++;
        } else {
          data->bpsh_cnt++;
        }
      }
      
      if (isSet(TCP_URG, flags)) {
        if (meta->reverse == 0) {
          data->furg_cnt++;
        } else {
          data->burg_cnt++;
        }
      }
    } // time cap check
    
    /* possibly ignore control packetes in the attributes */
    /*if (isSet(TCP_SYN, flags) || isSet(TCP_FIN, flags) || isSet(TCP_RST, flags)) {
    return 0;
  }*/
    if (meta->reverse == 0) {
      data->total_fhlen += tcphlen;
    } else {
      data->total_bhlen += tcphlen;
    }
  } 
  
  /* must do this after valid check! */ 
  if ((data->first > 0) && ((now - data->first) > data->time_cap)) {
    /* ignore rest of the packets after time cap */
    goto exit;
  }
  
  if (data->first == 0) {
    data->first = now;
    data->activep = 1;
    data->lactive_start = now;
  } else {
    /* check isub flow timeout */
    diff = now - getLast(data);
    if ( diff > data->idle_threshold) {
      /* idle time */
      if (diff > data->max_idle) {
        data->max_idle = diff;
      }
      if ((diff < data->min_idle) || (data->min_idle == 0ULL)) {
        data->min_idle = diff;
      }
      data->idlet += diff;
      data->idle_sqsum += diff*diff;
      data->idlep++;
      
      /* active time */
      diff = getLast(data) - data->lactive_start;
      if (diff > data->max_active) {
        data->max_active = diff;
      }
      if ((diff < data->min_active) || (data->min_active == 0ULL)) {
        data->min_active = diff;
      }
      
      data->activet += diff; 
      data->active_sqsum += diff*diff;
      data->activep++;
      data->flast = 0;
      data->blast = 0;
      data->lactive_start = now;
    }
  }
  
  if (meta->reverse == 0) {
    /* packet length */
    if ((len < data->min_fpktl) || (data->min_fpktl == 0)) {
      data->min_fpktl = len;
    }
    if (len > data->max_fpktl ) {
      data->max_fpktl = len;
    }
    
    data->pktl_fsum += len; 
    data->pktl_fsqsum += len*len;
    
    /* interarrival time */
    if (data->flast > 0) {
      diff = now - data->flast;
      
      if ((diff < data->min_fiat) || (data->min_fiat == 0LL)) {
        data->min_fiat = diff;
      }
      if (diff > data->max_fiat ) { 
        data->max_fiat = diff;
      }
      
      data->iat_fsum += diff;
      data->iat_fsqsum += diff*diff;
      data->fivals++;
    }
    
    data->flast = now;
    data->fpackets += 1;
    data->fbytes   += len;
  } else {
    /* packet length */
    if ((len < data->min_bpktl) || (data->min_bpktl == 0)) {
      data->min_bpktl = len;
    }
    if (len > data->max_bpktl ) {
      data->max_bpktl = len;
    }
    
    data->pktl_bsum += len;
    data->pktl_bsqsum += len*len;
    
    /* interarrival time */
    if (data->blast > 0) {
      diff = now - data->blast;
      
      if ((diff < data->min_biat) || (data->min_biat == 0LL)) {
        data->min_biat = diff;
      }
      if (diff > data->max_biat ) { 
        data->max_biat = diff;
      }
      
      data->iat_bsum += diff;
      data->iat_bsqsum += diff*diff;
      data->bivals++;
    }  
    
    data->blast = now;
    data->bpackets += 1;
    data->bbytes += len;
  }
  
  exit:
  
  if ((proto == 6) && (data->cstate == STATE_CLOSED) && (data->sstate == STATE_CLOSED)) {
    /* trigger immediate export */
    return 1;
  } else {
    return 0;
  }
  
  return 0;
}


int initFlowRec( configParam_t *params, void **flowdata )
{
  struct flowData_t *data;
  
  data = malloc( sizeof(struct flowData_t) );
  
  if (data == NULL ) {
    return -1;
  }
  
  resetFlowRec( data );
  
  data->idle_threshold = DEF_IDLE_THRESHOLD;
  data->min_duration = DEF_MIN_DURATION;
  data->time_cap = DEF_TIME_CAP;
  
  while (params->name != NULL) {
    if (!strcmp(params->name, "Idle_Threshold")) {
      data->idle_threshold = atoll(params->value);
      /* fprintf(stdout, "flowstats module: using %lldus idle threshold\n", data->idle_threshold); */
    }
    else if (!strcmp(params->name, "Min_Duration")) {
      data->min_duration = atoll(params->value);
      /* fprintf(stdout, "flowstats module: using %lldus min duration\n", data->min_duration); */
    }
    else if (!strcmp(params->name, "Time_Cap")) {
      data->time_cap = atoll(params->value);
      /* fprintf(stdout, "flowstats module: using %lldus time cap\n", data->time_cap); */
    }
    
    params++;
  }
  
  *flowdata = data;
  return 0;
}


int resetFlowRec( void *flowdata )
{
  struct flowData_t *data = (struct flowData_t*) flowdata;
  
  if (!flowdata ) {
    return -1;
  }
  
  /* reset everything to 0 except the last 3 configuration parameters (bit hacky) */
  memset(data, 0, sizeof(struct flowData_t) - sizeof(unsigned long long)*3);
  
  return 0;
}


int destroyFlowRec( void *flowdata )
{
  struct flowData_t *data = (struct flowData_t*) flowdata;
  
  free(data);
  
  return 0;
}


/* compute standard deviation based on sum and square sum and number of elements */
double stddev(double sqsum, double sum, unsigned long n)
{
  return (unsigned long) sqrt((sqsum - (sum*sum/n))/(n - 1));
}


int exportData( void **exp, int *len, void *flowdata )
{
  static unsigned int expData[sizeof(struct flowData_t)];
  struct flowData_t *data = flowdata;
  unsigned long long diff = 0;
  
  STARTEXPORT( expData );
  
  if (!data->valid) {
    goto exit;
  }
  
  if ( ((unsigned long long) getLast(data) - data->first) < data->min_duration) {
    goto exit;
  }
  //if (data->fpackets > 1.0) printf("%d ", data->fpackets);
  ADD_UINT32( data->fpackets );
  ADD_UINT32( data->fbytes );
  ADD_UINT32( data->bpackets );
  ADD_UINT32( data->bbytes );
  
  if (data->fpackets > 0) {
    assert(data->fbytes > 0);
    assert(data->pktl_fsum > 0);
    assert(data->pktl_fsqsum > 0);
    assert(data->min_fpktl > 0);
    assert((data->max_fpktl > 0) && (data->max_fpktl >= data->min_fpktl));
    assert(data->fivals < data->fpackets);
  }
  assert((data->bivals == 0) || (data->bivals < data->bpackets));
  
  ADD_INT32( (data->fpackets == 0) ? -1 : data->min_fpktl );
  ADD_INT32( (data->fpackets == 0) ? -1 : (unsigned short) (data->pktl_fsum/data->fpackets) );
  ADD_INT32( (data->fpackets == 0) ? -1 : data->max_fpktl );
  ADD_UINT32( (data->fpackets < 2) ? 0 : (long ) stddev(data->pktl_fsqsum, data->pktl_fsum, data->fpackets) );
  
  if (data->bpackets > 0) {
    assert(data->bpackets > 0);
    assert(data->bbytes > 0);
    assert(data->pktl_bsum > 0);
    assert(data->pktl_bsqsum > 0);
    assert(data->min_bpktl > 0);
    assert((data->max_bpktl > 0) && (data->max_bpktl >= data->min_bpktl));
  }
  
  ADD_INT32( (data->bpackets == 0) ? -1 : (long) data->min_bpktl );
  ADD_INT32( (data->bpackets == 0) ? -1 : (long) (data->pktl_bsum/data->bpackets) );
  ADD_INT32( (data->bpackets == 0) ? -1 : (long) data->max_bpktl );
  ADD_UINT32( (data->bpackets < 2) ? 0 : (long ) stddev(data->pktl_bsqsum, data->pktl_bsum, data->bpackets) );
    
  if (data->fivals > 0) {
    assert(data->fivals > 0);
    assert(data->iat_fsum >= 0);
    assert(data->iat_fsqsum >= 0);
    assert(data->min_fiat >= 0);
    assert((data->max_fiat >= 0) && (data->max_fiat >= data->min_fiat));
  }
  
  ADD_UINT64( (data->fivals == 0) ? 0LL : (long long) data->min_fiat );
  ADD_UINT64( (data->fivals == 0) ? 0LL : (long long) (data->iat_fsum/data->fivals) );
  ADD_UINT64( (data->fivals == 0) ? 0LL : (long long) data->max_fiat );
  ADD_UINT64( (data->fivals < 2) ? 0LL : (long long) stddev(data->iat_fsqsum, data->iat_fsum, data->fivals) );
  
  if (data->bivals > 0) {
    assert(data->bivals > 0);
    assert(data->iat_bsum >= 0);
    assert(data->iat_bsqsum >= 0);
    assert(data->min_biat >= 0);
    assert((data->max_biat >= 0) && (data->max_biat >= data->min_biat));
  }
  
  ADD_UINT64( (data->bivals == 0) ? 0LL : (long long) data->min_biat );
  ADD_UINT64( (data->bivals == 0) ? 0LL : (long long) (data->iat_bsum/data->bivals) );
  ADD_UINT64( (data->bivals == 0) ? 0LL : (long long) data->max_biat );
  ADD_UINT64( (data->bivals < 2) ? 0LL : (long long) stddev(data->iat_bsqsum, data->iat_bsum, data->bivals) );
  
  if (data->fpackets > 0) {
    assert(data->first > 0ULL);
    assert((data->blast > 0ULL) || (data->flast > 0ULL));
  }
  
  assert((getLast(data) - data->first) >= 0ULL);
  ADD_UINT64( (unsigned long long) getLast(data) - data->first );
  
  /* add the last active period */
  diff = getLast(data) - data->lactive_start;
  data->activet += diff;
  if (diff > data->max_active) {
    data->max_active = diff;
  }
  if ((diff < data->min_active) || (data->min_active == 0ULL)) {
    data->min_active = diff;
  }
  data->active_sqsum += diff*diff;
  
  if ((data->fpackets > 1) || (data->bpackets > 1)) {
    assert(data->activep > 0);
  }
  
  ADD_UINT64( data->min_active );
  ADD_UINT64( (unsigned long long) (data->activet/data->activep) );
  ADD_UINT64( data->max_active );
  ADD_UINT64( (data->activep < 2) ? 0LL : (unsigned long long) stddev(data->active_sqsum, data->activet, data->activep) );
  
  if (data->idlep > 0) {
    assert((data->idlet/data->idlep) >= data->idle_threshold);
  }
  
  ADD_UINT64( data->min_idle );
  ADD_UINT64( (data->idlep > 0) ? (unsigned long long) (data->idlet/data->idlep) : 0ULL );
  ADD_UINT64( data->max_idle );
  ADD_UINT64( (data->idlep < 2) ? 0LL : (unsigned long long) stddev(data->idle_sqsum, data->idlet, data->idlep) );
  
  assert(data->activep <= (data->fpackets + data->bpackets));
  
  ADD_UINT32( (data->activep > 0) ? (unsigned long) data->fpackets/data->activep : 0);
  ADD_UINT32( (data->activep > 0) ? (unsigned long) data->fbytes/data->activep : 0);
  ADD_UINT32( (data->activep > 0) ? (unsigned long) data->bpackets/data->activep : 0);
  ADD_UINT32( (data->activep > 0) ? (unsigned long) data->bbytes/data->activep : 0);
  
  ADD_UINT32( data->fpsh_cnt );
  ADD_UINT32( data->bpsh_cnt );
  ADD_UINT32( data->furg_cnt );
  ADD_UINT32( data->burg_cnt );

  ADD_UINT32( data->total_fhlen );
  ADD_UINT32( data->total_bhlen );
  
  //ADD_UINT64( data->first );
  
  exit:
  
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
  case I_MODNAME:   return "flowstats";
  case I_VERSION:   return "1.0DArev2";
  case I_CREATED:   return "2004/06/08";
  case I_MODIFIED:  return "2010/10/08";
  case I_BRIEF:     return "compute various flow statistics";
  case I_VERBOSE:   return "this module computes various statistics based on packet lengths\n"
	  "and inter-arrival times that are used for flow characterisation";
  case I_HTMLDOCS:  return "";
  case I_PARAMS:    return "Idle_Threshold: longer periods are considered idle periods\n"
	  "Min_Duration: shorter flows are not exported\n"
	  "Time_Cap: packets after time cap are ignored in the statistics"; 
  case I_RESULTS:   return "TBD";
  case I_AUTHOR:    return "Sebastian Zander";
  case I_AFFILI:    return "Swinburne University, Melbourne, Australia";
  case I_EMAIL:     return "szander@swin.edu.au";
  case I_HOMEPAGE:  return "http://www.caia.swin.edu.au/urp/dstc";
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

