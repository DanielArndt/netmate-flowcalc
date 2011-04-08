
/*!\file   NetTapPcap.cc

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
    nettap based on libpcap library

    $Id: NetTapPcap.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "NetTapPcap.h"
#include "metadata.h"
#include "Timeval.h"


//! optimize filter code
const int OFLAG = 1;

//! netmask for filter code
//! do not try to get the real one -> ip broadcast wont work
const unsigned long NETMASK = 0x0;

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


// transfer struct
typedef struct
{
    NetTapPcap *nt;
    char *buf;
    unsigned long len;
} pcap_data_t;


/*!\short   construct and initialize a NetTap object
 */

NetTapPcap::NetTapPcap(string df, int onl, int pro, unsigned int sl, int noblock, int bsize)
    :  devfile(df), online(onl),  promisc(pro), snap_len(sl)
{
    char errbuf[256];

    // if no dev was specified and we want live capture then try to get
    // default device
    if (online && devfile.empty()) {
        
        char *device = pcap_lookupdev(errbuf);
        if (device == NULL || device[0] == '\0') {
            throw Error("libpcap cannot access interface: %s "
                        "(no interface or no permissions to access interface)",
                        errbuf );
        }
        devfile = string(device);
    }
	
    if (online) {
        if ((pcap_handle = pcap_open_live((char *)devfile.c_str(), snap_len, promisc, 0, 
                                          errbuf)) == NULL) {
            throw Error("libpcap open_live failed: %s", errbuf );
        }
	
        if (noblock) {
#ifdef HAVE_PCAP_SETNONBLOCK
            if (pcap_setnonblock(pcap_handle, 1, errbuf) < 0) {
                throw Error("Could not set device \"%s\" to non-blocking: %s", devfile.c_str(), 
                            errbuf);
            }
#else
            if (fcntl(pcap_fileno(pcap_handle), F_SETFL, O_NONBLOCK) < 0) {    
                throw Error("Could not set device \"%s\" to non-blocking: %s", devfile.c_str(), 
                            strerror(errno));
            }
#endif
        }

#ifdef LINUX
	// does that work for other OS than Linux?
        if (bsize > 0) {
	    int rbsize = 0;
	    socklen_t len = sizeof(rbsize);

            // tune socket buffer
            if (setsockopt(pcap_fileno(pcap_handle), SOL_SOCKET, SO_RCVBUF, &bsize, sizeof(bsize)) < 0) {
                throw Error("Could not set socket receive buffer size: %s", strerror(errno));
            }
            
            if (getsockopt(pcap_fileno(pcap_handle), SOL_SOCKET, SO_RCVBUF, &rbsize, &len) < 0) {
                throw Error("Could not read socket receive buffer size: %s", strerror(errno)); 
            }
          
            if (rbsize < bsize) {
                throw Error("Could not set socket receive buffer size to %lu; "
                            "try increasing /proc/sys/net/core/rmem_max etc.", bsize);
            }
        }
#endif
    } else {
        pcap_handle = pcap_open_offline(devfile.c_str(), errbuf);
        if (pcap_handle == NULL) {
            throw Error("libpcap open_offline failed: %s", errbuf );
        }

	// get the initial time stamp (the timestamp of the first packet)
	struct pcap_pkthdr pkthdr;
	pcap_next(pcap_handle, &pkthdr);
	Timeval::settimeofday(&pkthdr.ts);
	
	// and reopen the dump file
	pcap_close(pcap_handle);
	pcap_handle = pcap_open_offline(devfile.c_str(), errbuf);
    }

    s_linkType = pcap_datalink(pcap_handle);

    //cout << "s_LinkType: " << s_linkType << endl;
	
    // 2 lines -> support old g++
    auto_ptr<NetTapStats> _stats(new NetTapPcapStats());
    stats = _stats;

    cout << "Listening on: " << devfile << endl;
}


/*!\short   destroy a NetTap object
 */
NetTapPcap::~NetTapPcap()
{
    pcap_close(pcap_handle);
}


// static function called from pcap which calls procPacket of the appropriate net tap
void NetTapPcap::sProcPacket(u_char *data, const struct pcap_pkthdr *pkthdr, const u_char *pktdata)
{
    pcap_data_t *pd = (pcap_data_t *) data;

    pd->nt->procPacket(pd->buf, pd->len, pkthdr, pktdata);
}

// FIXME make this code extensible to different mac, network and transport layers
void NetTapPcap::procPacket(char *buf, unsigned long len, const struct pcap_pkthdr *pkthdr, 
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
    
    // only support Ethernet, raw IP and loopback for now
    switch (s_linkType) {
    case DLT_NULL:
        offs += 4;
        pkt->layers[L_LINK] = L_UNKNOWN;
        switch (*(u_long *)pkt->payload)
        {
            case AF_INET:   net_type = 0x0800;
                            break;
            case AF_INET6:  net_type = 0x86DD;
                            break;
            default:        net_type = 0;
                            break;
         }
        break;
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
    // FIXME there seems to be a problem with certain IPv6 packets. Test with mawi201101021400.dump
    /*case 0x86DD:
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
            proto = pkt->payload[pkt->offs[L_NET] + offs];
        }
        pkt->layers[L_NET] = N_IP6;
        break;*/
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


metaData_t *NetTapPcap::getPacket(char *buf, unsigned long len)
{
    pcap_data_t pd;

    pd.nt = this;
    pd.buf = buf;
    pd.len = len;

    int ret = pcap_dispatch(pcap_handle, 1, sProcPacket, (u_char *)&pd);
    if (ret < 0) {
        throw Error("pcap_dispatch: %s", pcap_geterr(pcap_handle));
    } else if (ret == 0) {
        return NULL;
    } else {
        metaData_t *pkt = (metaData_t *) buf;
        if (pkt->len > 0) {
          stats->packets++;
          stats->bytes += pkt->cap_len;

          return pkt;
        } else {
          return NULL;
        }
    }
}


void NetTapPcap::checkFilter(string filter)
{
    int netmask;
    struct bpf_program bpfprog;
    
    if (!filter.empty()) {
        netmask = htonl(NETMASK);

        if (pcap_compile(pcap_handle, &bpfprog, (char *)filter.c_str(), OFLAG, netmask ) < 0 ) { 
            throw Error("error while compiling BPF filter %s", filter.c_str());
        }    
    }
}


void NetTapPcap::addFilter(string filter)
{
    int netmask;
    struct bpf_program bpfprog;

    if (!filter.empty()) {
        netmask = htonl(NETMASK);

        if (pcap_compile(pcap_handle, &bpfprog, (char *)filter.c_str(), OFLAG, netmask ) < 0 ) { 
            throw Error("error while compiling BPF filter %s", filter.c_str());
        }    

        if (pcap_setfilter(pcap_handle, &bpfprog ) < 0 ) {
            throw Error("cannot download filter");
        }   
    }
}


void NetTapPcap::delFilter()
{
    if (pcap_setfilter(pcap_handle, NULL ) < 0 ) {
        throw Error("cannot delete filter");
    }   
}


void NetTapPcap::dump( ostream &os )
{
    struct pcap_stat pstats;

    // pcap_stats does only work for online capturing
    if (online) {
      if (pcap_stats(pcap_handle, &pstats) < 0) {
        throw Error("cannot get pcap stats");
      }
      
      ((NetTapPcapStats *) stats.get())->dpackets = pstats.ps_drop;
    }

    os << "NetTapPcap dump: " << endl;
    os << *stats;         
}


int NetTapPcap::getFd()
{
    return pcap_fileno(pcap_handle);
}


//!overload for <<, so that a network tap object can be thrown into an ostream
ostream& operator<< ( ostream &os, NetTapPcap &nt )
{
    nt.dump(os);
    return os;
}       
 
