
/*! \file PacketQueue.h

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
packet queue between classifier and packet processor

    $Id: PacketQueue.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _PACKETQUEUE_H_
#define _PACKETQUEUE_H_


#include "stdincpp.h"
#include "Logger.h"
#include "Threads.h"
#include "metadata.h"


//! size of buffer guaranteed by (successful returning) call to getBufferSpace
const int MIN_QUEUE_BUF = (sizeof(metaData_t) + 65536);

//! average number of bytes to reserve per buffer (*number of buffers = size of ringbuffer for raw data storage), '300' was choosen as being a round number somewhat larger than the average IP packet size (260 bytes incl. IP header)
const int AVG_BUF_DATA = 300;

//! structure storing position of one packet buffer inside the big buffer queue
struct PktBufRec_t {

    PktBufRec_t() : pos(NULL), len(0) {};

    char *pos; //!< position of start of raw packet data in ring buffer
    int  len;  //!< packet length (as stored) in ring buffer space
};



/*! \short  provides a fast queue for incoming packets 

  the PacketQueue class is used to hold a number of packets and their
  associated meta-data information
  
  the PacketQueue is intended to be used in between the Classifier 
  and the PacketEvaluator (in order to decouple these two components)
*/

class PacketQueue
{
    
  private:

    static Logger *s_log; //!< link to global logger
    static int s_ch;      //!< logging channel used by objects of this class

    int   threaded;       //! flag which tells if threading was configured
    
    int   freeBuffers;    //!< number of packet buffers still unused
    int   maxBuffers;     //!< maximum number of buffers to be usable by queue

    int   freeMemory;     //!< amount of unused buffer space in bytes
    int   maxMemory;      //!< maximum storage in buffer space in bytes

    int   droppedPackets; //!< number of dropped packets so far
    int   guardBufLen;    //!< guaranteed buffer size for buffer returned by getBufferSpace

    char *rawData;    //!< memory space for storage of raw packet data
    char *endData;    //!< pointer to end of storage space + 1

    int   nextInBuf;  //!< position of next free buffer (for incoming packets)
    int   nextOutBuf; //!< position of next outgoing buffer (oldest packet in queue)
    char *curData;    //!< current start of unused memory area

    /*! array storing (a) start locations of raw data in ring buffer, 
                      (b) packet lengths of raw data (as stored in ring buffer),
                      (c) associated packet meta data records
     */
    PktBufRec_t *bufRecs; 

#ifdef ENABLE_THREADS
    mutex_t        maccess;     //!< semaphore for queue access
    thread_cond_t  freeBufCond; //!< condition semaphore for signalling
#endif

  public:

    /*! \short  generate a new PacketQueue

        \arg \c maxBuf - maximum number of bytes to use for packet data
        \arg \c guaranteedBuf - size of guaranteed buffer size returned by call to getBufferSpace
        \arg \c avgBufferSize - average number of memory space to reserve for each buffer. A single buffer might hold more data (up to 'guaranteedBuf' bytes) but the queue will only store maxBuf * avgBufSize bytes overall.
    */
    PacketQueue( int maxBufs, int threaded = 0,
                 int guaranteedBuf = MIN_QUEUE_BUF,
                 int avgBufSize = AVG_BUF_DATA  );

    /*! \short  destroy a packet queue object 

      this also discardes any packet data currently stored in the queue
    */ 
    ~PacketQueue();

    /*! \short  request a block of memory to write packet data into

        If the call succeeds data with up to 2^16-1 bytes can be stored in
        the returned memory space. Then call setBufferOccupied (see below).

        \arg \c buf - location to store the free memory address into
        \returns 0 on success, !=0 else (also sets *buf=NULL if no space left)
    */
    int getBufferSpace( char **buf );

    /*! \short  mark a portion of the memory area (which has been requested by 
                a call to getBufferSpace before) as used

        This function also stores the metaData (associated to the packet in
        the buffer) in the queue object. // aka SetNextFreeBufferOccupied
      
        \arg \c len - length of the packet which has been stored in the buffer
        \returns 0 on success, !=0 else
    */
    int setBufferOccupied( int len );

    /*! \short  get access to the first packet stored in the queue

        This function returns pointers for accessing the raw packet data 
        (still stored in the queue) as well as its meta data. The length is
        also returned via a reference parameter (len). Note that all memory
        still remains property of the queue object.

        \arg \c buf - location where to store the memory buffer address
        \arg \c len - location to store the length of the raw packet data
        \arg \c meta - location where to store the location of the metadata
        \returns 0 on success, !=0 else
    */

    // FIXME do we need all those read methods???
    // Only the last one is used
    int readBuffer( char **buf, int *len, metaData_t **meta );
    int readBuffer( char **buf, int *len );
    metaData_t* readBuffer(int block=1);
    
    /*! \short  tells the queue that the buffer space used by the first packet
                in the queue can be used again for incoming packet data

        This function needs to be called alternating with readBuffer by
        the code that implements the reader of a queue object.

        \returns 0 on success, !=0 else
    */
    int releaseBuffer();

    //!return the number of used buffers, i.e. number of packets in the queue
    int getUsedBuffers();

    //! return the maximum number of packets to be stored in the queue
    int getMaxBuffers();

    //! return the amount of memory currently allocated by raw packet data stored in the queue
    int getUsedMemory();

    //! return the amount of memory used at most for raw packet data
    int getMaxMemory();

    //! discard any data stored in the queue
    int clearQueue();

    //! return statistical information (packet losses)
    int getStats();

    //! reset the statistics information for this queue
    int resetStats();

    //! return max buffer space usable for one buffer
    int getMaxBufSize();

};


//! overload << for use on PacketQueue objects
ostream& operator<< ( ostream &os, PacketQueue &pq );


#endif // _PACKETQUEUE_H_
