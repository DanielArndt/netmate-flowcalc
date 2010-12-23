
/*! \file   NetTapERF.h

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
    nettap for the endance record format
    (needs libdagtools)

    $Id: NetTapERF.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _NETTAPERF_H_
#define _NETTAPERF_H_


#include "stdincpp.h"
#include "Error.h"
#include "NetTap.h"
#include "NetTapPcap.h"
extern "C" {
#include <erf.h>
}


//! store some statistical values for the net tap
class NetTapERFStats : public NetTapStats {

  public:

    unsigned long long dpackets; //!< dropped packets

    NetTapERFStats() : NetTapStats()
    {
        dpackets = 0;
    }

    virtual ~NetTapERFStats() {}

    virtual void dump( ostream &os) {
        os << "packets: "         << packets  << endl;
        os << "bytes: "           << bytes    << endl;
        os << "dropped packets: " << dpackets << endl;
    }
};

/*! \short   capture packets from lower layer (network, file, ...)
  
    An instance of this abstract class is called by the classifier to
    get packets as input
*/

class NetTapERF : public NetTap
{
  private:
   
    //! device or filename
    string devfile;

    //! offline or online capturing
    int online;

    //! promiscuous mode enabled
    int promisc;

    //! snap length
    unsigned int snap_len;

    //! link layer type (e.g. Ethernet)
    int s_linkType;

    //! erf handle
    erf_context_t *econtext;
    //! erf flags (not supported at the moment)
    int erf_flags;

    struct timeval ERF2PcapTime(long long t);

    void procPacket(char *buf, unsigned long len, const struct pcap_pkthdr *pkthdr, 
                    const u_char *pktdata);

  public:

    /*! \short   construct and initialize a NetTap object
        \arg \c df       device or file to open
        \arg \c onl      online capturing (net) or offline cpaturing (file)
        \arg \c pro      promiscuous mode (online only)
        \arg \c sl       snap length
        \arg \c noblock  use pcap in non blocking mode (needed if meter is one single thread
        \arg \c bsize    set receive buffer size
     */
    NetTapERF(string df, int onl, int pro = 0, unsigned int sl = DEF_SNAP,
	       int noblock=1, int bsize = 0, int legacy = 0);

    //! destroy a NetTap object
    virtual ~NetTapERF();

    /*! \short get packet from network tap

        This function return the next packet in the queue to the caller. It
        blocks if there is no packet in the queue. The return value can be
        used for error signalization.
    */
    virtual metaData_t *getPacket(char *buf, unsigned long len);
    
    //!   dump a network tap object
    virtual void dump( ostream &os );

    int isOnline() 
    {
      return (online==1);
    }

    //! ERF supports no filter (for now)
    virtual void checkFilter(string filter) {}
    virtual void addFilter(string filter) {}
    virtual void delFilter() {}

    //! not supported (for now)
    virtual int getFd()
    {
      return 0;
    }

};


//! overload for <<, so that a network tap object can be thrown into an ostream
ostream& operator<< ( ostream &os, NetTapERF &nt );


#endif // _NETTAP_LIBPCAP_H_
