
/*! \file   FlowCreator.h

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
    create flows based on flow key

    $Id: FlowCreator.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _FLOWCREATOR_H_
#define _FLOWCREATOR_H_


#include "stdincpp.h"
#include "Error.h"
#include "ProcModule.h"
#include "Rule.h"
#include "RuleIdSource.h"

const int START_BUCKETS = 16;

// FIXME missing struct documentation
typedef struct
{
    ProcModule *module;
    ProcModuleInterface_t *mapi; // module API
    void *flowData;
    // config params for module
    configParam_t *params;
} ppaction_t;

//! actions for each rule
typedef vector<ppaction_t>            ppactionList_t;
typedef vector<ppaction_t>::iterator  ppactionListIter_t;

typedef struct {
    time_t lastPkt;

    ppactionList_t actions;
    unsigned char *keyData;
    unsigned short len;
    // unique flow id
    unsigned long long flowId;
    // designate new flows
    int newFlow;

} flowInfo_t;

typedef struct
{
    unsigned short len;
    // flow key
    unsigned char *keyData;
} hkey_t;

struct eqkey
{
    inline bool operator()(const hkey_t &s1, const hkey_t &s2) const
    {
        if (s1.len == s2.len) {
            return (memcmp(s1.keyData, s2.keyData, s1.len) == 0);
        } else {
            return 0;
        }
    }
};

// hash function craeted by Bob Jenkins (bob_jenkins@burtleburtle.net)

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

struct hfnct
{
    inline size_t operator()(const hkey_t &key) const
    {
        const unsigned char *k = key.keyData;        /* the key */
        unsigned long  length = key.len;   /* the length of the key in ub4 */
        unsigned long  initval = 0;  /* the previous hash, or an arbitrary value */
        unsigned long a,b,c,len;

        /* Set up the internal state */
        len = length;
        a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
        c = initval;         /* the previous hash value */

        /*---------------------------------------- handle most of the key */
        while (len >= 3) {
            a += k[0];
            b += k[1];
            c += k[2];
            mix(a,b,c);
            k += 3; len -= 3;
        }

        /*-------------------------------------- handle the last 2 ub4's */
        c += length;
        switch(len) {             /* all the case statements fall through */
            /* c is reserved for the length */
        case 2 : b+=k[1];
        case 1 : a+=k[0];
            /* case 0: nothing left to add */
        }
        mix(a,b,c);
        /*-------------------------------------------- report the result */
        return c;
    }
};


//! action list for each rule
typedef hash_map<const hkey_t, flowInfo_t, hfnct, eqkey> flowList_t;
typedef hash_map<const hkey_t, flowInfo_t, hfnct, eqkey>::iterator flowListIter_t;


/*! \short   manage and apply Action Modules, retrieve flow data

    the PacketProcessor class allows to manage filter rules and their
    associated actions and apply those actions to incoming packets, 
    manage and retrieve flow data
*/

class FlowCreator
{
  private:

    // pool of unique flow ids
    RuleIdSource idSource;

    flowList_t flows;

  public:

    FlowCreator();

    ~FlowCreator();

    flowInfo_t *getFlow(const unsigned char *KeyData, unsigned short len);

    flowInfo_t *addFlow(const unsigned char *keyData, unsigned short len);

    void deleteFlow(flowListIter_t f);

    void deleteFlow(flowInfo_t *fi);

    flowList_t *getFlows()
      {
          return &flows;
      }

    void setLastTime(flowInfo_t *fi, time_t time)
      {
          fi->lastPkt = time;
      }
};

#endif
