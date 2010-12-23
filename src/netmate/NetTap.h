
/*! \file   NetTap.h

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
    abstract base class for possible network taps

    $Id: NetTap.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _NETTAP_H_
#define _NETTAP_H_


#include "stdincpp.h"
#include "ProcModuleInterface.h"
#include "CommandLineArgs.h"


//! store some statistical values for the net tap

class NetTapStats {

  public:

    unsigned long long packets;   //!< number of packets processed so far
    unsigned long long bytes;     //!< size of volume of processed packets

    NetTapStats() {
       	packets = 0;
	    bytes = 0;
    }

    virtual ~NetTapStats() {}

    virtual void dump( ostream &os) {
      os << "packets: " << packets << endl;
      os << "bytes: "   << bytes << endl;
    }
};


//! overload for <<, so that a NetTap stats object can be thrown into an ostream
ostream& operator<< ( ostream &os, NetTapStats &nst );

       
/*! \short   capture packets from lower layer (network, file, ...)
  
   An instance of this abstract class is called by the classifier to
   get packets as input
  
*/

class NetTap
{
  protected:

    auto_ptr<NetTapStats> stats;  //!< net tap usage statistics counters
 
  public:

    virtual ~NetTap() {};

    //! check a filter
    virtual void checkFilter(string filter) = 0;

    /*! \short   add a filter
       This functions adds a filter to the capturing device. This
       is NOT a classification rule but e.g. a prefilter like BPF in the
       OS kernel.
     */
    virtual void addFilter(string filter) = 0;

    /*! \short   delete a filter
        This removes a previously installed filter rule from the network tap.
     */
    virtual void delFilter() = 0;

    /*! \short get packet from network tap
        This function return the next packet in the queue to the caller. It
        blocks if there is no packet in the queue. The return value can be
        used for error signalization.
    */
    virtual metaData_t *getPacket(char *buf, unsigned long len) = 0;

    /*! \short   read statistical information from the network tap
        \returns a struct containing numerous statistical values
     */
    virtual NetTapStats *getStats();

    //! dump a network tap object
    virtual void dump( ostream &os )
    {
        os << "NetTap Dump" << endl;
        os << *stats << endl;
    }

    //! get file descriptor net tap is reading from
    virtual int getFd() = 0;

    //! indicate whether the tap is online or file
    virtual int isOnline() = 0;
};


//! overload for <<, so that a network tap object can be thrown into an ostream
ostream& operator<< ( ostream &os, NetTap &nt );


#endif // _NETTAP_H_
