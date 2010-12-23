
/*!\file PacketQueue.cc

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

    $Id: PacketQueue.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "PacketQueue.h"
#include "Error.h"


// init static
Logger* PacketQueue::s_log = NULL;
int     PacketQueue::s_ch = -1;


PacketQueue::PacketQueue( int maxBufs, int thr,
                          int guaranteedBuf, int avgBufSize ) 
    : threaded(thr)
{
    if (s_log == NULL) {
        s_log = Logger::getInstance();
    }
    if (s_ch == -1) {
        s_ch = s_log->createChannel("PacketQueue");
    }
    
#ifdef DEBUG
    s_log->dlog(s_ch, "Starting");
#endif    

    guardBufLen = (guaranteedBuf >= 1) ? guaranteedBuf : 1;
    maxBuffers = maxBufs;
    maxMemory = maxBufs * avgBufSize;

    s_log->log(s_ch, "creating %d packet buffers", maxBuffers);
    s_log->log(s_ch, "reserving %d bytes for packet data", 
               maxMemory);
    s_log->log(s_ch, "each buffer request gives access to %d"
               " bytes of linear memory", guardBufLen);
    
    droppedPackets = 0;

    // try to reserve the ring buffer memory
    rawData = new char[maxMemory];
    memset(rawData, 0, maxMemory);

    if (rawData == NULL) {

        maxMemory = 0;
        maxBuffers = 0;
        s_log->log(L_CRIT, s_ch, "cannot allocate memory for packet data!");
    }

    // reserve the buffer descriptors memory
    bufRecs = new PktBufRec_t[maxBuffers]; 
    // all entries are set to zero by PktBufRec_t constructor (see .h file)

    if (bufRecs == NULL) {

        maxMemory = 0;
        maxBuffers = 0;
        s_log->log(L_CRIT, s_ch,
                   "could not allocate memory for packet buffers!");
    }

    clearQueue();

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexInit(&maccess);
        threadCondInit(&freeBufCond);
    }
#endif

#ifdef DEBUG
    s_log->dlog(s_ch, "Started");
#endif
}


PacketQueue::~PacketQueue()
{

#ifdef ENABLE_THREADS
    if (threaded) {
        //mutexLock(&maccess);
    }
#endif

    clearQueue();

    saveDeleteArr(bufRecs);
    saveDeleteArr(rawData);

#ifdef ENABLE_THREADS
    if (threaded) {
        //mutexUnlock(&maccess);
        mutexDestroy(&maccess);
        threadCondDestroy(&freeBufCond);
    }
#endif

#ifdef DEBUG
    s_log->dlog(s_ch, "Shutdown");
#endif
}


int PacketQueue::clearQueue()
{
    freeBuffers = maxBuffers;
    freeMemory = maxMemory;
    endData = rawData + maxMemory;
    nextInBuf = 0;
    nextOutBuf = 0;
    curData = rawData;

    return 0;
}


int PacketQueue::getBufferSpace( char **buf )
{
    AUTOLOCK(threaded, &maccess);

    if (freeBuffers == 0 || freeMemory < guardBufLen) {
        droppedPackets++;
        *buf = NULL;
        return -1;
    }

    *buf = curData;
    return 0;
}

int PacketQueue::setBufferOccupied( int len )
{
    AUTOLOCK(threaded, &maccess);

    if (len <= 0 || len > guardBufLen || 
        freeBuffers == 0 || freeMemory < len) {
        return -1;
    }

    bufRecs[nextInBuf].pos = curData;
    bufRecs[nextInBuf].len = len;

#ifdef DEBUG2
    fprintf(stderr, "raw data in buffer %d, at position %d, len = %d ",
            nextInBuf, (unsigned int) curData - (unsigned int) rawData, len);
#endif
    
    nextInBuf += 1;
    if (nextInBuf == maxBuffers) { // wrap around
        nextInBuf = 0;
    }

    freeMemory -= len;
    curData += len;

    // check if we _cannot_ guarantee guardBufLen bytes until end of 
    // ringbuffer. If yes, skip this memory rest and start at the beginning

    if (curData + guardBufLen > endData) {

        freeMemory -= endData - curData;
        curData = rawData;
    }

    freeBuffers--;

#ifdef DEBUG2
    fprintf(stderr, "   free mem = %d, free bufs = %d\n",
            freeMemory, freeBuffers);
#endif

#ifdef ENABLE_THREADS
    // if we go from 0 to 1 packet in the queue
    if (threaded && getUsedBuffers() == 1) {
        threadCondSignal(&freeBufCond);
    }
#endif
    return 0;
}


int PacketQueue::readBuffer( char **buf, int *len )
{
    AUTOLOCK(threaded, &maccess);

    if (buf == NULL || len == NULL ||
        freeBuffers == maxBuffers) {
        return -1;
    }

    *len = bufRecs[nextOutBuf].len;
    *buf = bufRecs[nextOutBuf].pos;

    return 0;
}


int PacketQueue::readBuffer( char **buf, int *len, metaData_t **meta )
{
    AUTOLOCK(threaded, &maccess);

    if (buf == NULL || len == NULL || meta == NULL ||
        freeBuffers == maxBuffers) {
        return -1;
    }

    *len = bufRecs[nextOutBuf].len - sizeof(metaData_t);

    if (*len <= 0) {
        return -1;
    }

    *meta = (metaData_t *) bufRecs[nextOutBuf].pos;
    *buf = bufRecs[nextOutBuf].pos + sizeof(metaData_t);

    return 0;
}


metaData_t* PacketQueue::readBuffer(int block)
{
    AUTOLOCK(threaded, &maccess);

#ifdef ENABLE_THREADS
    if (block) {     
        while (getUsedBuffers() == 0) {
            threadCondWait(&freeBufCond, &maccess);
        }
        return (metaData_t *)bufRecs[nextOutBuf].pos;
    } else {
#endif
        if (getUsedBuffers() > 0) {
            return (metaData_t *)bufRecs[nextOutBuf].pos;
        } else {
            return NULL;
        }
#ifdef ENABLE_THREADS
    }
#endif
}


int PacketQueue::releaseBuffer()
{
    char *pos;
    int len;

    AUTOLOCK(threaded, &maccess);

    if (freeBuffers == maxBuffers) {
        return -1;
    }

    pos = bufRecs[nextOutBuf].pos;
    len = bufRecs[nextOutBuf].len;

#ifdef DEBUG2
    fprintf(stderr, "raw data from buffer %d, at position %d, len = %d ",
            nextOutBuf, (unsigned int) pos - (unsigned int) rawData, len);
#endif

    freeMemory += len;
    pos += len;

    nextOutBuf += 1;
    if (nextOutBuf == maxBuffers) { // wrap around
        nextOutBuf = 0;
    }

    // check if we had another guardBufLen bytes until end of 
    // ringbuffer. If not, skip this memory rest

    if (pos + guardBufLen > endData) {

    	freeMemory += endData - pos;
    }
   
    freeBuffers++;
#ifdef DEBUG2
    fprintf(stderr, "   free mem = %d, free bufs = %d\n",
            freeMemory, freeBuffers);
#endif
    return 0;
}



int PacketQueue::getUsedBuffers()
{
    return maxBuffers - freeBuffers;
}


int PacketQueue::getMaxBuffers()
{
    return maxBuffers;
}


int PacketQueue::getUsedMemory()
{
    return maxMemory - freeMemory;
}


int PacketQueue::getMaxMemory()
{
    return maxMemory;
}


int PacketQueue::getStats()
{
    return droppedPackets;
}


int PacketQueue::resetStats()
{
    droppedPackets = 0;
    return 0;
}


int PacketQueue::getMaxBufSize()
{
    return guardBufLen;
}


//!overload << for use on PacketQueue objects
ostream& operator<< ( ostream &os, PacketQueue &pq )
{
    return os;
}

