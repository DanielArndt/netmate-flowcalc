
/*! \file   NetTapPcap.h

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

    $Id: NetTapPcap.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _NETTAPPCAP_H_
#define _NETTAPPCAP_H_


#include "stdincpp.h"
#include "Error.h"
#include "NetTap.h"


//! default snap size
const int DEF_SNAP = 68; // bytes

//! store some statistical values for the net tap
class NetTapPcapStats : public NetTapStats {

  public:

    unsigned long long dpackets; //!< dropped packets

    NetTapPcapStats() : NetTapStats()
    {
        dpackets = 0;
    }

    virtual ~NetTapPcapStats() {}

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

class NetTapPcap : public NetTap
{
  private:
   
    //! device or filename
    string devfile;

    //! pcap handle
    pcap_t *pcap_handle;

    //! offline or online capturing
    int online;

    //! promiscuous mode enabled
    int promisc;

    //! snap length
    unsigned int snap_len;

    //! link layer type (e.g. Ethernet)
    int s_linkType;

    //! FIXME missing documentation
    static void sProcPacket(u_char *data, const struct pcap_pkthdr *pkthdr, 
                           const u_char *pktdata);

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
    NetTapPcap(string df, int onl, int pro = 0, unsigned int sl = DEF_SNAP,
	       int noblock=1, int bsize = 0);

    //! destroy a NetTap object
    virtual ~NetTapPcap();

    /*! \short get packet from network tap

        This function return the next packet in the queue to the caller. It
        blocks if there is no packet in the queue. The return value can be
        used for error signalization.
    */
    virtual metaData_t *getPacket(char *buf, unsigned long len);
    
    //! check filter
    virtual void checkFilter(string filter);

    /*! \short   add a filter

        This functions adds a filter to the capturing device. This
        is NOT a classification rule but e.g. a prefilter like BPF in the
        OS kernel. 
    */   
    virtual void addFilter(string filter);

    /*! \short   delete a filter

        This removes a previously installed filter rule from the network tap.
    */
    virtual void delFilter();

    //!   dump a network tap object
    virtual void dump( ostream &os );

    //! get file descriptor
    int getFd();

    int isOnline() 
    {
      return (online==1);
    }

};


//! overload for <<, so that a network tap object can be thrown into an ostream
ostream& operator<< ( ostream &os, NetTapPcap &nt );


#endif // _NETTAP_LIBPCAP_H_
