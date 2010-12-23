
/*!\file   Classifier.cc

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
    implementation for Classifier base class functionality

    $Id: Classifier.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "Classifier.h"
#include "Meter.h"

/* ------------------------- Classifier ------------------------- */

Classifier::Classifier( ConfigManager *cnf, string name, Sampler *sa,
		                PacketQueue *queue, int threaded )
  : MeterComponent(cnf, name, threaded), sampler(sa), pQueue(queue)
{
  
    if (sampler == NULL) {
       throw Error("Classifier has no Sampler");
    }
    
    if (pQueue == NULL) {
      throw Error("Classifier has no packet queue");
    }
     
    maxBufSize = pQueue->getMaxBufSize();

    // 2 lines -> support old g++
    auto_ptr<ClassifierStats> _stats(new ClassifierStats());
    stats = _stats;
}


/* ------------------------- ~Classifier ------------------------- */

Classifier::~Classifier()
{
    for (tapListIter_t i=taps.begin(); i != taps.end(); ++i) {
        saveDelete(*i);
    }
}

void Classifier::registerTap(NetTap *nt)
{
    if (nt == NULL) {
        throw Error("Invalid NetTap");
    }

    taps.push_back(nt);

    if (!threaded) {
        // add the file descriptor of the tap to the set
        int fd = nt->getFd();
        
        if (fd > 0) {
            addFd(fd);
        }
    }
}

void Classifier::clearTaps()
{
  for (tapListIter_t i=taps.begin(); i != taps.end(); ++i) {
    saveDelete(*i);
  }
  taps.clear();

  // this function is only used with capture files so we don't care
  // about the tap file descriptors
}

int Classifier::processPacket()
{
    char *buf;
    static tapListIter_t tapi = taps.begin();

    if (pQueue->getBufferSpace(&buf) != 0) {
#ifdef DEBUG
        cerr << "packet queue full" << endl;
#endif
	return 0;
    } else {
        // else got valid buffer space in packet queue
      
        // read packet from net tap and put into packet queue (metaData first)
        upkt = (*tapi)->getPacket(buf, maxBufSize);
        
        if (upkt != NULL) {
#ifdef DEBUG2
            cout << "got packet, length: " << upkt->len << endl;
#endif	 
            // and try to classify the packet
            if (sampler->sample(upkt) && classify(upkt)) {
#ifdef DEBUG2
                cout << "matches rule(s) ";
                for (int i = 0; i < upkt->match_cnt; i++) {
                    cout << upkt->match[i] << ", ";
                }
                cout << endl;
#endif   
                // put packet plus meta-data into queue 
                pQueue->setBufferOccupied(upkt->len + sizeof(metaData_t));
                
                stats->packets += 1;
                stats->bytes   += upkt->len;
            }

	    return 1;
        }

        tapi++;
        if (tapi == taps.end()) {
            tapi = taps.begin();
        }
    }

    if (!(*tapi)->isOnline()) {
      // EOF
      return -1;
    }

    return 0;
}

//! single-threaded classification
int Classifier::handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds)
{

    if (rset != NULL) {
        for (tapListIter_t i=taps.begin(); i != taps.end(); ++i) {
            if (FD_ISSET((*i)->getFd(), rset)) {
                return processPacket();
            }
        }
    } else {
      // offline cap
      for (tapListIter_t i=taps.begin(); i != taps.end(); ++i) {
	return processPacket();
      }
    }

    return 0;
}


//! called as the thread's main function when running Classifier as separate thread

void Classifier::main()
{
    static int eof = 0;
    const char c = 'S';

    assert(taps.size() > 0);

    // this function will be run as a single thread inside the classifier
    log->log(ch, "thread running" );
    
    while (1) {
      if ((processPacket() < 0) && !eof) {
	 cout << "End of capture file reached" << endl;
	 write(Meter::s_sigpipe[1], &c, 1);
	 eof++;
      }
    }
}          


/* ---------------------------- getStats ------------------------------ */

ClassifierStats *Classifier::getStats()
{
    return stats.get();
}       


/* ---------------------------- dump ------------------------------ */

void Classifier::dump( ostream &os )
{
    AUTOLOCK(threaded, &maccess);
    
    os << "Classifier Dump" << endl;
    os << *stats;
    for (tapListIter_t i=taps.begin(); i != taps.end(); ++i) {
        os << *(*i);
    }
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<<(ostream &os, Classifier &cl)
{
    cl.dump(os);
    return os;
}

/* ----------------------- << ClassifierStats ---------------------- */

ostream& operator<< ( ostream &os, ClassifierStats &cst )
{
  cst.dump(os);
  return os;
}


