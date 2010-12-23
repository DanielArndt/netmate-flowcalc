
/*! \file  rtploss.c

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
action module for rtploss counting

$Id: rtploss.c 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include "stdinc.h"
#include "ProcModule.h"


typeInfo_t exportInfo[] = { { UINT32, "packets"   },
                            { UINT32, "loss_abs"  },
                            { UINT32, "loss_int"  },
                            { UINT32, "loss_frac" },
                            EXPORT_END };


#define RTP_SEQ_MOD   (1<<16)
#define RTP_V(x)      (x & 0x8000) >> 14 /* version */
#define RTP_M(x)      (x & 0x0080) >> 7  /* marker */
#define RTP_P(x)      (x & 0x2000) >> 5  /* padding */
#define RTP_X(x)      (x & 0x1000) >> 4  /* extension header */ 
#define RTP_PT(x)     (x & 0x007F)    
#define RTCP_PT(x)    (x & 0x00FF)   




struct rtp_hdr {
    uint16_t flags;
    uint16_t seqNo;
    uint32_t tstamp;
    uint32_t ssrc;
};

struct lossStats {
    unsigned long loss;
    unsigned long loss_int;
    unsigned long loss_frac;
};

struct rtpData {
    unsigned long cycles;
    unsigned long base_seq;
    unsigned short max_seq;
    unsigned long received;
    unsigned long received_prior;
    unsigned long expected_prior;
    unsigned long bad_seq;
    unsigned long probation;
    unsigned long ssrc;
    unsigned long last_update;
    struct lossStats ls;
    // struct rtp_src *next;
};


/* ---------- module internal functions ---------- */

static void init_rtp_seq(unsigned short seq, unsigned long ssrc,
                         struct rtpData *data)
{
    data->base_seq = seq - 1;
    data->max_seq = seq;
    data->bad_seq = RTP_SEQ_MOD + 1;
    data->cycles = 0;
    data->received = 0;
    data->received_prior = 0;
    data->expected_prior = 0;
    data->ssrc = ssrc;
}


static void update_loss( struct rtpData *data )
{
    unsigned long received_int, expected_int, expected;

    if (data->received == 0) {
        return;
    }

    expected = (data->cycles + data->max_seq) - data->base_seq + 1;
    data->ls.loss = expected - data->received;
    expected_int = expected - data->expected_prior;
    data->expected_prior = expected;
    received_int = data->received - data->received_prior;
    data->received_prior = data->received;
    
    data->ls.loss_int = expected_int - received_int;

    if ((expected_int > 0) && (data->ls.loss_int > 0)) {
        data->ls.loss_frac = (((double) data->ls.loss_int / expected_int)*100);
    } else {
        data->ls.loss_frac = 0;
    }
    
    /*
      fprintf( stderr, "rtploss: SSRC 0x%lx loss rate %ld\n", 
      data->ssrc, data->ls.loss_frac);
    */
}


static int update_rtp_seq( unsigned short seq, unsigned long ssrc,
                           struct rtpData *data )
{
    unsigned short udelta = seq - data->max_seq;
    const int MAX_DROPOUT = 3000;
    const int MAX_MISORDER = 100;
    const int MIN_SEQUENTIAL = 2;

    /* if (udelta!= 1 ) fprintf( stderr, "-%d-\n", udelta ); */

    /*
     * Source is not valid until MIN_SEQUENTIAL packets with
     * sequential sequence numbers have been received.
     */
    if (data->probation) {
        /* packet is in sequence */
        if (seq == data->max_seq + 1) {
            data->probation--;
            data->max_seq = seq;
            if (data->probation == 0) {
                init_rtp_seq(seq, ssrc, data);
                data->received++;
                return 1;
            }
        } else {
            data->probation = MIN_SEQUENTIAL - 1;
            data->max_seq = seq;
        }
        return 1;
    } else if (udelta < MAX_DROPOUT) {
        /* in order, with permissible gap */
        if (seq < data->max_seq) {
            /* Sequence number wrapped - count another 64K cycle. */
            data->cycles += RTP_SEQ_MOD;
        }
        data->max_seq = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
        /* the sequence number made a very large jump */
        if (seq == data->bad_seq) {
            /* Two sequential packets -- assume that the other side
             * restarted without telling us so just re-sync
             * (i.e., pretend this was the first packet). */
            init_rtp_seq(seq, ssrc, data);
        }
        else {
            data->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
            return 0;
        }
    } else {
        /* duplicate or reordered packet */
    }
    
    data->received++; 
    return 0;
}


/* ---------- module interface functions ---------- */


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
    struct rtpData *data = flowdata;
    struct rtp_hdr *rtph;
    unsigned short flags;

    if (meta->layers[2] != T_UDP ) {  /* only use UDP packets */
        return 0;
    }

    /* make sure snap size is sufficient */
    if ((meta->cap_len - meta->offs[3]) < sizeof(struct rtp_hdr)) {
        return 0;
    }

    rtph = (struct rtp_hdr *) &packet[meta->offs[3]];
    flags = ntohs(rtph->flags);
	
    if ((RTP_V(flags) == 2) && ((RTCP_PT(flags) < 200) || (RTCP_PT(flags) > 204))) {    
        /* update sequence number */
        update_rtp_seq(ntohs(rtph->seqNo), ntohl(rtph->ssrc), data);
    }
  
    return 0;
}

int initFlowRec( configParam_t *params, void **flowdata )
{
    struct rtpData *data;

    data = malloc( sizeof( struct rtpData ) );

    if (data == NULL ) {
        return -1;
    }

    memset( data, 0, sizeof(*data) );
    data->probation = 1;

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
    struct rtpData *data = flowdata;
    static unsigned int expData[4];

    update_loss(data);

    /*
     *len = sprintf( buf, "0: %s\n1: %ld\n2: %ld\n3: %ld\n4: %ld\n",
     getModuleInfo(0),
     data->received,
     data->ls.loss,
     data->ls.loss_int,
     data->ls.loss_frac
     );
     // write export data to buf here
     */

    STARTEXPORT( expData );
    ADD_UINT32( data->received     );
    ADD_UINT32( data->ls.loss      );
    ADD_UINT32( data->ls.loss_int  );
    ADD_UINT32( data->ls.loss_frac );
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
    case I_MODNAME:   return "rtploss";
    case I_BRIEF:  return "counting of loss for rtp traffic";
    case I_AUTHOR: return "Carsten Schmoll";
    case I_CREATED: return "2001/02/08";
    case I_VERBOSE: return "rtploss percentage";
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

