
/*! \file   Classifier.h

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
    header file for abstract Classifier class

    $Id: Classifier.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _CLASSIFIER_H_
#define _CLASSIFIER_H_


#include "stdincpp.h"
#include "Error.h"
#include "Rule.h"
#include "NetTap.h"
#include "metadata.h"
#include "PacketQueue.h"
#include "MeterComponent.h"
#include "Sampler.h"


//! stores statistical values about a classifier in action
class ClassifierStats {

  public:

    unsigned long long packets;        //!< number of packets processed so far
    unsigned long long bytes;     //!< size of volume of processed packets
    unsigned long long rules;          //!< number of rules that have been added so far

    ClassifierStats() {
       	packets = 0;
	    bytes   = 0;
	    rules   = 0;
    }

    virtual ~ClassifierStats() {}

    /*! write classifier statistics to ostream
       \arg \c ostream  -  the output stream (e.g. cout,cerr) to write to
     */
    virtual void dump(ostream &os) {
        os << "packets: " << packets << endl;
        os << "bytes: "   << bytes << endl;
        os << "rules: "   << rules << endl;
    }
};


//! overload for <<, so that a Classifier stats object can be thrown into an ostream
ostream& operator<< ( ostream &os, ClassifierStats &cst );


/*! \short   accept and classify packets according to filter specification
  
    the Classifier class represents a packet classifier that captures packets
    and classifies them among a number of configured filter rules. The results
    of this process are the packet data plus metadata (e.g. timestamp) and
    the id(s) of the matching filter rule(s). The Classifier base class is an
    abstract class upon which the real classifier must be build by implementing
    the pure virtual member functions of this class.
*/

typedef vector<NetTap*> tapList_t;
typedef vector<NetTap*>::iterator tapListIter_t;

class Classifier : public MeterComponent
{
  protected:

    auto_ptr<ClassifierStats> stats;  //!< classifier usage statistics

    tapList_t taps;       //!< link to associated network tap classes
    Sampler *sampler;     //!< link to sampling class in use
    PacketQueue *pQueue;  //!< link to packet queue used by classifier
    int maxBufSize;       //!< max number of bytes to store in queue at once
    metaData_t *upkt;     //!< pointer to an incoming packet message

    /*! \short   process, i.e. classify an incoming packet
        the method is called whenever new packets are ready for being classified.
        there are no parameters as themethod will read the packet and meta data
        from the Classifier's own packet queue where they are stored
    */
    inline int processPacket();

  public:

    /*! \short   construct and initialize a Classifier object
        \arg \c cnf         config manager to query for settings
        \arg \c name        name of this classifier
        \arg \c nt          net tap class to query for incoming packets
        \arg \c sa          sampler class to use for packet selection (sampling)
        \arg \c queue       packet queue to put classified packets into
        \arg \c threaded    1 = run as separate thread, 0 = classifier called from main event loop
    */
    Classifier( ConfigManager *cnf, string name, Sampler *sa = NULL,
                PacketQueue *queue = NULL, int threaded = 0 );

    //! destroy a Classifier object
    virtual ~Classifier();

    virtual void registerTap(NetTap *nt);

    virtual void clearTaps();

    //! check a ruleset (the filter part)
    virtual void checkRules(ruleDB_t *rules) = 0;

    //! add rules
    virtual void addRules(ruleDB_t *rules) = 0;

    //! add a rule
    virtual void addRule(Rule *r) = 0;

    //! delete rules
    virtual void delRules(ruleDB_t *rules) = 0;

    //! delete a rule
    virtual void delRule(Rule *r) = 0;

    //! classify an incoming packet (determine matching rule(s))
    virtual int classify(metaData_t* pkt) = 0;

    //! get packet from net tap, sample and classify
    virtual int handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds);

    //! this function will be called initially as the thread inside the Classifier
    virtual void main();

    /*! \short   fetch statistical usage information from the classifier component
        \returns a pointer to an object containing numerous statistical values
    */
    virtual ClassifierStats *getStats();

    //! dump a Classifier objects properties
    virtual void dump( ostream &os );

    //! return the name of the group for which a Classifier ready configuration values
    virtual string getConfigGroup() { return "CLASSIFIER"; }

};


//! overload for <<, so that a Classifier object can be thrown into an ostream
ostream& operator<< ( ostream &os, Classifier &cl );


#endif // _CLASSIFIER_H_
