
/*! \file  metadata.h

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
    packet meta data format

    $Id: metadata.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef __METADATA_H
#define __METADATA_H

// max number of rules which can match a single packet
#define MAX_RULES_MATCH 128


#include "stdinc.h"
#ifdef ENABLE_NF
#ifdef HAVE_LIBIPULOG_LIBIPULOG_H
#ifdef __cplusplus
extern "C" {
#endif
#include <libipulog/libipulog.h>
#ifdef __cplusplus
}
#endif
#endif
#endif

// protocol constants for layer array

typedef enum {
    L_UNKNOWN = 0,
    L_ETHERNET,
    L_ATM_RFC1483
} linkProt_t;

typedef enum {
    N_UNKNOWN = 0,
    N_IP,
    N_IP6
} netProt_t;

typedef enum {
    T_UNKNOWN = 0,
    T_ICMP    = 1,
    T_IGMP    = 2,
    T_GGP     = 3,
    T_IPIP    = 4,
    T_STREAM  = 5,
    T_TCP     = 6,
    T_EGP     = 8,
    T_IGP     = 9,
    T_UDP     = 17,
    T_MUX     = 18,
    T_IDPR    = 35,
    T_IPV6    = 41,
    T_IDRP    = 45,
    T_RSVP    = 46,
    T_GRE     = 47,
    T_MOBILE  = 55,
    T_ICMP6   = 58
} transProt_t;


typedef enum {
    L_LINK = 0,
    L_NET,
    L_TRANS,
    L_DATA
} pktLayer_t;


/* \short   data that will be supplied to the packet processor by the classifier        
 */

#ifdef ENABLE_NF 
#ifdef HAVE_LIBIPULOG_LIBIPULOG_H
// netfilter specific struct not yet
#endif
// we should never get here
#else
typedef struct {
    unsigned long tv_sec;
    unsigned long tv_usec;
    char indev_name[255];
    char outdev_name[255];
    // == min( snaplen, pktlen )
    size_t cap_len;
    // size of the original packet
    size_t len;
    // offsets to the different headers
    // 0: mac (should always be 0)
    // 1: ip
    // 2: trans
    // 3: data
    int offs[4];
    // protocol for each layer as determined by NetTapPcap 
    int layers[4];
    // indicate reverse direction (for bidir flows)
    // FIXME: not supported by RFC classifier
    int reverse;
    // count and rule ids of matching rules
    unsigned short match_cnt;
    unsigned int match[MAX_RULES_MATCH]; 

    // pointer to the data
    unsigned char payload[1];
} metaData_t;

#endif

#endif /* __METADATA_H */
