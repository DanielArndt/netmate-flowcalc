
/*! \file   ClassifierNetfilter.h

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
    header file for Classifier_netfilter class

    $Id: ClassifierNetfilter.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _CLASSIFIER_NETFILTER_H_
#define _CLASSIFIER_NETFILTER_H_

#include "stdincpp.h"
#include "MeterComponent.h"
#include "Classifier.h"
#include "metadata.h"


//! store some statistical values about a classifier in action
class ClassifierStats_netfilter : public ClassifierStats {
};


/*! \short   accept and classify packets according to filter specification
  
the ClassifierNetfilter class represents a packet classifier that captures packets
and classifies them among a number of configured filter rules. The results
of this process are the packet data plus metadata (e.g. timestamp) and
the id(s) of the matching filter rule(s)
     */

struct netfilter_rule_entry {
    struct ipt_entry *e;
    unsigned int nsaddrs, ndaddrs; //FIXME: currently supports only 1 src and 1 dst address
    struct in_addr saddrs, daddrs;
};

typedef multimap<int, struct netfilter_rule_entry*> rule_map_t;
typedef multimap<int, struct netfilter_rule_entry*>::iterator rule_map_itor_t;

class ClassifierNetfilter : public Classifier
{
  private:

    /* netlink group */
    unsigned int nlgroup;

    /* ipulog handle */
    struct ipulog_handle *h;

    /* iptc handle */
    iptc_handle_t *iptc_h;

    /* rcv buffer */
    unsigned char* buf;

    /* rcv buffer len */
    /*size_t*/ int len;

    /* pointer to message */
    ulog_packet_msg_t *upkt;

    /* list of all rules */
    rule_map_t rule_list;

    /* statistics */
    ClassifierStats_netfilter stats;

    /* table into which are rules inserted */
    char *table;

    /* chain into which rules are inserted */
    char *chain;

    /* copy range used by default */
    int cprange;

    /* qthreshold used by default */
    int qthres;

    /* functions for maintaining internal rule list */

    struct netfilter_rule_entry *alloc_rule_entry(struct ipt_entry *e, unsigned int nsaddrs,
						  struct in_addr *saddrs, unsigned int ndaddrs, 
						  struct in_addr *daddrs);
	
    void delete_rule_entry(struct netfilter_rule_entry *nfre);

    int check_for_invert(char *param_in, char **param_out);

    void _addRule( RuleInfo &rinfo, int test );

  public:

    /*! \short   construct and initialize a ClassifierNetfilter object
     */
    ClassifierNetfilter(ConfigManager *cnf, int threaded=0);

    /*! \short   destroy a ClassifierNetfilter object
     */
    virtual ~ClassifierNetfilter();

    //! this function will be executed as the thread
    virtual void main();

    /*! \short   add a filter rule

    This adds another filter rule to the rule descriptions already
    installed in the classifier. The classifier is responsible for 
    checking the attributes and values of the given rule description
    for validity and has to return an exception is an error is
    encountered. The given ruleID has to be used in calls to the
    PacketProcessor::processPacket for packets classified in future

    \arg \c rinfo     - the information about the configured rule
    \arg \c test  - if test==1 do only a syntax check, do not install rule
    \pre no rule  with the given id is present within the classifier
    \throws MyErr - that contains a msg stating the cause of the error
    */
    virtual void addRule( RuleInfo &rinfo, int test=0 );


    /*! \short   delete a filter rule

    This removes a previously installed filter rule from the classifier.

    \arg \c ruleId - unique rule id
    \pre a rule with the given id is present within the classifier
    */
    virtual void delRule( int ruleID );

    virtual void run();

    virtual void stop();

    /*! \short   read statistical information from the classifier component

    \returns a struct containing numerous statistical values
    */
    virtual ClassifierStats *getStats();

    /*! \short   dump a ClassifierNetfilter object
     */
    virtual void dump( ostream &os );

    
};


//! overload for <<, so that a ClassifierNetfilter object can be thrown into an ostream
ostream& operator<< ( ostream &os, ClassifierNetfilter &cl );



#endif // _CLASSIFIER_NETFILTER_H_
