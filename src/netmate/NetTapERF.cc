
/*!\file   NetTapERF.cc

    Copyright 2004 Sebastian Zander (szander@swin.edu.au)

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
    nettap based on libpcap library

    $Id: NetTapERF.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "NetTapERF.h"
#include "metadata.h"
#include "Timeval.h"


// fixed header lengths
const int ETHER_HLEN = 14;
const int UDP_HLEN = 8;
const int ICMP_HLEN = 4;
const int ICMP6_HLEN = 4;
const int IP6_HLEN = 40;

/* IPv6 extension header types */
const uint8_t IP6HDR_HOP   =  0;
const uint8_t IP6HDR_ROUTE = 43;
const uint8_t IP6HDR_FRAG  = 44;
const uint8_t IP6HDR_DEST  = 60;
const uint8_t IP6HDR_AH    = 51;
const uint8_t IP6HDR_ESP   = 50;

/* dag stuff taken from dagbpf.c */
# define MIN(A,B) (((A)<(B)) ? (A) : (B))

const int edag = 4; /* EtherDAG compatibility       */

typedef struct cell {
        long long       ts;
        unsigned        crc;
        unsigned        header;
        unsigned char   pload[48];
} cell_t;

typedef struct ether {
        long long       ts;
        u_short         len;
        u_char          dst[6];
        u_char          src[6];
        u_short         etype;
        unsigned        pload[40/sizeof(unsigned)];
} ether_t;

typedef struct pos {                    /* PoS comes with the new DAG record format */
        long long       ts;
        unsigned        slen;           /* snap len in this record, must be 64 for now */
        unsigned        wlen;           /* length of the packet on the wire, seems to include FCS */
        unsigned char   chdlc;          /* Cisco HDLC header */
        unsigned char   unused;         /* padding */
        unsigned short  etype;          /* EtherType */
        unsigned        pload[11];      /* one more word than ATM and Ether */
} pos_t;

char usgtxt[256] = "that sux";

// DAG is more accurate than our current internal timestamp format based on pcap
struct timeval NetTapERF::ERF2PcapTime(long long ts)
{
  struct timeval tv;

# if (BYTE_ORDER ==  BIG_ENDIAN)
  ts = ((0xff00000000000000ULL & ts) >> 56) |
       ((0x00ff000000000000ULL & ts) >> 40) |
       ((0x0000ff0000000000ULL & ts) >> 24) |
       ((0x000000ff00000000ULL & ts) >>  8) |
       ((0x00000000ff000000ULL & ts) <<  8) |
       ((0x0000000000ff0000ULL & ts) << 24) |
       ((0x000000000000ff00ULL & ts) << 40) |
       ((0x00000000000000ffULL & ts) << 56) ;
# endif
  tv.tv_sec = (long)(ts >> 32);
  tv.tv_usec = ((ts & 0xffffffffLL) * 1000 * 1000) >> 32;

  return tv;
}

/*!\short   construct and initialize a NetTap object
 */
NetTapERF::NetTapERF(string df, int onl, int pro, unsigned int sl, int noblock, int bsize, 
		     int legacy)
  :  devfile(df), online(onl),  promisc(pro), snap_len(sl), erf_flags(0)
{

    if (devfile.empty()) {
      throw Error("empty file name");
    }

    if (legacy) {
      erf_flags |= ERF_FLAG_LEGACY;
    }

    // no support for online cpaturing yet
    if (online) {
      throw Error("NetTapERF does not support online capturing yet");
      //econtext = erf_create_dag_context(devfile.c_str(), erf_flags);
    } else {

      econtext = erf_create_file_context((char *)devfile.c_str(), erf_flags);
      if (econtext == NULL) {
	throw Error("erf_create_context failed: %s", strerror(errno));
      }
      
      // get the initial time stamp (the timestamp of the first packet)
      char buf[64];
      cell_t *cell = (cell_t *)buf;
      
      int rtype = erf_read_record(econtext, buf, sizeof(buf), 1);
      struct timeval start = ERF2PcapTime(cell->ts);
      Timeval::settimeofday(&start);

      // get the link type
      switch (rtype) {
      case TYPE_ATM:
	s_linkType = DLT_ATM_RFC1483;
	break;
      case TYPE_ETH:
	s_linkType = DLT_EN10MB;
	break;
      case TYPE_HDLC_POS:
# ifdef DLT_CHDLC
	s_linkType = DLT_CHDLC;
# else
	throw Error("for PoS support need a more recent libpcap from www.tcpdump.org");
# endif
	break;
      default:
	 throw Error("unknown erf capture type");
      }

      // reopen the dump file
      erf_destroy_context(econtext);
      econtext = erf_create_file_context((char *)devfile.c_str(), erf_flags);
    }

    // this function does not work properly
    //s_linkType = erf_get_linktype(econtext);

    // 2 lines -> support old g++
    auto_ptr<NetTapStats> _stats(new NetTapERFStats());
    stats = _stats;

    cout << "Listening on: " << devfile << endl;
}


/*!\short   destroy a NetTap object
 */
NetTapERF::~NetTapERF()
{
    erf_destroy_context(econtext);
}



// FIXME make this code extensible to different mac, network and transport layers
void NetTapERF::procPacket(char *buf, unsigned long len, const struct pcap_pkthdr *pkthdr, 
                            const u_char *pktdata)
{
    unsigned short offs = 0;
    int net_type = 0;
    int proto = 0;
    int ret = 0;

    // the current packet
    metaData_t *pkt = (metaData_t *) buf;

    // update the global last packet timestamp
    if (!online) {
      ret = Timeval::settimeofday(&pkthdr->ts);
#ifdef NO_REORDERING
      if (ret < 0) {
        pkt->len = 0;
        return;
      }
#endif
    }
    
    pkt->tv_sec = pkthdr->ts.tv_sec;
    pkt->tv_usec = pkthdr->ts.tv_usec;
    pkt->len = pkthdr->len;
    pkt->cap_len = pkthdr->caplen;
    pkt->reverse = 0;

    if (len < (sizeof(metaData_t) + pkt->cap_len)) {
        throw Error("buffer too small for captured packet");
    }

    memcpy(pkt->payload, pktdata, pkt->cap_len);
    pkt->offs[L_LINK] = 0;
    pkt->offs[L_NET] = -1;
    pkt->offs[L_TRANS] = -1;
    pkt->offs[L_DATA] = -1;
    
    // only support Ethernet and raw IP for now
    switch (s_linkType) {
    case DLT_EN10MB:
        offs += ETHER_HLEN;
        pkt->layers[L_LINK] = L_ETHERNET;
        // get the type of the next layer
        net_type = ntohs(*((unsigned short *) &pkt->payload[pkt->offs[L_LINK] + 12]));

	// use second type/length value in case of VLAN
	if (net_type == 0x8100) {
	    unsigned short tci = ntohs(*((unsigned short *) &pkt->payload[pkt->offs[L_LINK] + 14]));
	    if ((tci & 0x1000) != 0) {
		cerr << "found unsupported ethertype: VLAN with options (CFI=1)" << endl; 
	    }
	    // for VLAN with no options link level header is four bytes longer 
	    net_type = ntohs(*((unsigned short *) &pkt->payload[pkt->offs[L_LINK] + (12+4)]));
	    offs += 4;
	}
        break;
    case DLT_RAW: // in case we have do not have link level header
	pkt->layers[L_LINK] = L_UNKNOWN;
        // get the type of this layer from the IP version
	if ((pkt->payload[0] & 0xf0) == (6<<4)) {
	    net_type = 0x86DD; // IPv6
	} else if ((pkt->payload[0] & 0xf0) == (4<<4)) {
	    net_type = 0x0800; // IPv4
	} else {
	    net_type = 0; // neither v4 nor v6, should not happen for raw IP link type 
	}
        break;
    case DLT_ATM_RFC1483:
        pkt->layers[L_LINK] = L_ATM_RFC1483;
	net_type = ntohs(*((unsigned short *) &pkt->payload[pkt->offs[L_LINK] + 6]));
	offs += 8;
        break;
    default:
        offs = 0;
        pkt->layers[L_LINK] = L_UNKNOWN;
        return; 
    }
    if (offs<pkt->cap_len) {
        pkt->offs[L_NET] = offs;
    } else {
        return;
    }
    
    // only support IP for now
    switch (net_type) {
    case 0x0800:
        // IPv4
        offs += ((pkt->payload[pkt->offs[L_NET]] & 0x0F) << 2);
        proto = pkt->payload[pkt->offs[L_NET] + 9];
        pkt->layers[L_NET] = N_IP;
        break;
    case 0x86DD:
        // IPv6
        offs += IP6_HLEN;
        proto = pkt->payload[pkt->offs[L_NET] + 6];
        // IPv6 skip options
        // FIXME currenty ESP is not supported
        while ((proto == IP6HDR_HOP) || (proto == IP6HDR_ROUTE) || (proto == IP6HDR_FRAG) ||
              (proto == IP6HDR_AH) || (proto == IP6HDR_DEST)) {
            if (proto != IP6HDR_AH) {
                offs += pkt->payload[pkt->offs[L_NET] + offs + 1] * 8 + 8;
            } else {
                offs += pkt->payload[pkt->offs[L_NET] + offs + 1] * 4 + 8;
            }
            proto = pkt->payload[pkt->offs[1] + offs];
        }
        pkt->layers[L_NET] = N_IP6;
        break;
    default:
        offs = 0;
        pkt->layers[L_NET] = N_UNKNOWN;
        return;
    }
          
    if (offs < pkt->cap_len) {
        pkt->offs[L_TRANS] = offs;
    } else {
        return;
    }

    // only support ICMP, UDP and TCP for now
    switch (proto) {
    case IPPROTO_ICMP:
        offs += ICMP_HLEN;
        pkt->layers[L_TRANS] = T_ICMP;
        break;
    case IPPROTO_ICMPV6:
        offs += ICMP6_HLEN;
        pkt->layers[L_TRANS] = T_ICMP6;
        break;
    case IPPROTO_UDP:
        offs += UDP_HLEN;
        pkt->layers[L_TRANS] = T_UDP;
        break;
    case IPPROTO_TCP:
        offs += (pkt->payload[pkt->offs[L_TRANS]+12] & 0xF0) >> 2;
        pkt->layers[L_TRANS] = T_TCP;
        break;
    default:
        offs = 0;
        pkt->layers[L_TRANS] = T_UNKNOWN;
        return;
    }
    if (offs<pkt->cap_len) {
        pkt->offs[L_DATA] = offs;
    }
}


metaData_t *NetTapERF::getPacket(char *buf, unsigned long len)
{
    char    cbuf[64];
    cell_t  *cell = (cell_t *)cbuf;
    ether_t *ep = (ether_t *)cbuf;
    unsigned char   *lp = (unsigned char *)&ep->len;
    pos_t   *pp = (pos_t *)cbuf;
    int     rtype = 0;
    struct pcap_pkthdr pkthdr;
    unsigned char *data;
       
    if ((rtype = erf_read_record(econtext, cbuf, sizeof(cbuf), 1)) < 0) {
      return NULL;
    } else {
      // is a bit ugly but we first convert it to pcap and then
      // feed it into the same functions as in NetTapPcap
      // advantage: can use the same function for the real parsing
      // get timestamp
      pkthdr.ts = ERF2PcapTime(cell->ts);

      switch(rtype) {
      case TYPE_ATM:
	pkthdr.caplen = 48;
	pkthdr.len = ntohs(*(unsigned short *)&cell->pload[8+2])+8;
	//pkthdr.len=48;
	data = cell->pload;
	break;
      case TYPE_ETH:
        int _len;

	/* length field is little endian */
	_len = ((unsigned)lp[1] << 8) + lp[0] - edag;
	/* EtherDAGs count FCS as well */
	pkthdr.caplen = MIN(_len,54);
	pkthdr.len = _len;
	data = ep->dst;
	break;
      case TYPE_HDLC_POS:
# ifdef DLT_CHDLC
	pkthdr.caplen = ntohl(pp->slen) - 16;   /* subtract dag header */
	pkthdr.len = ntohl(pp->wlen) - edag;    /* PoS DAGs have the same problem */
	data = &pp->chdlc;
# else
	throw Error("for PoS support need a more recent libpcap from www.tcpdump.org");
# endif
	break;
      default:
	throw Error("unknown erf capture type");
      }
      
      procPacket(buf, len, &pkthdr, data);
      metaData_t *pkt = (metaData_t *) buf;
      if (pkt->len == 0) {
        return NULL;
      }
      
      stats->packets++;
      stats->bytes += pkt->cap_len;
      
      return pkt;
    }
}


void NetTapERF::dump( ostream &os )
{
    
  os << "NetTapERF dump: " << endl;
  os << *stats;
}


//!overload for <<, so that a network tap object can be thrown into an ostream
ostream& operator<< ( ostream &os, NetTapERF &nt )
{
    nt.dump(os);
    return os;
}       
 
