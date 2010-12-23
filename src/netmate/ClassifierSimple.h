
/*! \file   ClassifierSimple.h

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
    header file for abstract Classifier base class

    $Id: ClassifierSimple.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _CLASSIFIERSIMPLE_H_
#define _CLASSIFIERSIMPLE_H_

#include "stdincpp.h"
#include "MeterComponent.h"
#include "Classifier.h"
#include "NetTap.h"
#include "PacketQueue.h"
#include "FilterValue.h"
#include "Sampler.h"


//! store some statistical values about a classifier in action
class ClassifierSimpleStats : public ClassifierStats {};


/*! \short   accept and classify packets according to filter specification
  
  the Classifier_simple class represents a packet classifier that reads
  captured packets from libpcpap (file or netdevice) and classifies them among a number 
  of configured filter rules. The results of this process are the packet data 
  plus metadata (e.g. timestamp) and the id(s) of the matching filter rule(s). 
*/

//! definition of a match (aka filter)
typedef struct 
{
    filterType_t type;      //!< type of filter
    refer_t refer;          //!< reference point
    unsigned short offs;    //!< offset
    unsigned short len;     //!< length
    unsigned short cnt;     //!< number of values
    
    char value[MAX_FILTER_SET_SIZE][MAX_FILTER_LEN];
    char mask[MAX_FILTER_LEN];     //!< value mask
} match_t;

//! list of matches
typedef vector<match_t>             matchList_t;
typedef vector<match_t>::iterator   matchListIter_t;

//! list of rules
typedef map<int, matchList_t>           rules_t;
typedef map<int, matchList_t>::iterator rulesIter_t;


class ClassifierSimple : public Classifier
{
 private:
       
    //! rule list
    rules_t rules;

    //! add a single rule
    void addRule(Rule *r)
    {
        addRule(r, 1);
    }

    //! delete a single rule
    void delRule(Rule *r)
    {
        delRule(r, 1);
    }

    void addRule( Rule *r, int forward);

    void delRule( Rule *r, int forward);

 public:

    /*! \short   construct and initialize a Classifier object

      \arg \c cnf           config manager
      \arg \c nt            network tap to get packets from
      \arg \c sa            sampling algorithm to use
      \arg \c packetQueue   put classified packets into packet queue
      \arg \c threaded      if 1 the classifier will run as separate thread
    */
    ClassifierSimple(ConfigManager *cnf, Sampler *sa=NULL,
                     PacketQueue *packetQueue=NULL, int threaded=0);

    //! destroy a Classifier object
    virtual ~ClassifierSimple();

    //! classify a packet
    virtual int classify( metaData_t* pkt );

    //! check a ruleset (the filter part)
    virtual void checkRules(ruleDB_t *rules);

    //! add rules
    virtual void addRules(ruleDB_t *rules);

    //! delete rules
    virtual void delRules(ruleDB_t *rules);

    //! meter components can specify command line args
    static void add_args(CommandLineArgs *args)
    {
        //args->add('t', "TestArg", "a test arg", "CLASSIFIER");
    }

};


//! overload for <<, so that a Classifier object can be thrown into an ostream
ostream& operator<< ( ostream &os, ClassifierSimple &cl );


#endif // _CLASSIFIER_SIMPLE_H_
