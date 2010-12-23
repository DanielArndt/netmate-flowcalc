
/*! \file   ClassifierRFC.h

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
    Classifier based on Recursive Flow Classification (RFC)
  
    TODO:
    - test inc add of ranges, sets, wildcard rules (more tests)
    - test rules with masks with zero bits (more tests)
    - implement more clever chunk merging
	

    $Id: ClassifierRFC.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _CLASSIFIERRFC_H_
#define _CLASSIFIERRFC_H_


#include "stdincpp.h"
#include "MeterComponent.h"
#include "Classifier.h"
#include "NetTap.h"
#include "Sampler.h"
#include "PacketQueue.h"
#include "FilterValue.h"
#include "ClassifierRFCConf.h"
#include "Bitmap.h"


//! store some statistical values about a classifier in action
class ClassifierRFCStats : public ClassifierStats {};

//! number line (and chunk 0) sizes
const int NUMBER_LINE_SIZE_8B  = 256;
const int NUMBER_LINE_SIZE_16B = 65536;

//! definition of a point
typedef enum
  {
      RULE_START = 1,
      RULE_END,
  } pointType_t;

//! definition of a rule (at a point)
typedef struct
{
    pointType_t type;
    unsigned short rid;
} rule_t;

//! definition of a point (with n rules)
typedef struct 
{
    unsigned int entryCount;    //!< number of entries in point array
    unsigned short ruleCount;     //!< number of rules in rules array
    rule_t *rules;     //!< the rules
} point_t;

//! number line descriptor
typedef struct 
{
    unsigned short offs;   //!< offset of the filter
    unsigned short len;    //!< length of the filter
    refer_t ref;           //!< reference point of the filter
    unsigned short mask;   //!< mask for this dimension (1 or 2 byte)
} numberLineDescr_t;

//! array of number line descriptors (chunk 0)
typedef numberLineDescr_t numberLineDescrs_t[MAX_CHUNKS];

//! a single number line
typedef struct
{
    unsigned int line_size;    //!< number of points
    point_t *points;  //!< points
} numberLine_t;

// number line indexed by chunk# and value of a point on the line
// random access and linear walkthrough

//! number line array
typedef struct
{
    unsigned short lineCount;    //!< number of lines
    numberLine_t lines[MAX_CHUNKS];  // FIXME could be dynamically allocated as well
} numberLines_t;


//! equivalence ID (equivID) type definition
typedef unsigned short equivID_t;


//! comparison operator for bitmaps
struct ltbm
{
    bool operator()(const bitmap_t *b1, const bitmap_t *b2) const
    {
        return bmCompare(b1, b2);
    }
};

/*! list with rule bitmap and the equivalence ID, indexed by rule bitmap
    used for looking up the eqID for an existing bitmap
    FIXME more efficient way?
*/
typedef map<bitmap_t*, equivID_t, ltbm>            chunkBmList_t;
typedef map<bitmap_t*, equivID_t, ltbm>::iterator  chunkBmListIter_t;


//! list with free equivIDs
typedef list<equivID_t> freeList_t;
typedef list<equivID_t>::iterator freeListIter_t;

//! comparison operator for equiv ids
struct lteqi
{
    bool operator()(const equivID_t e1, const equivID_t e2) const
    {
        return (e1 < e2);
    }
};


typedef struct {
    //! bitmap
    bitmap_t *bm;
    //! reference counter
    unsigned short refc;
} bmInfo_t; 

//! array with bitmaps indexed by eqID
typedef vector<bmInfo_t> chunkEqClArray_t;
typedef vector<bmInfo_t>::iterator  chunkEqClArrayIter_t;

//! bitmap list
typedef struct {
    chunkBmList_t bms;    //!< list of ids indexed by bitmap
    chunkEqClArray_t eids;   //!< list of bitmaps indexed by equiv ids
    unsigned short maxId;  //!< current max id
    freeList_t freeList;   //!< free equiv IDs
} eqList_t;

//! array with all eqIDs and bitmap sorted by ID
typedef eqList_t  eqClTable_t[MAX_PHASES][MAX_CHUNKS];


// chunk
typedef struct
{
    unsigned short parentCount;                 //!< number of parents
    unsigned short parentChunks[MAX_CHUNKS];    //!< chunk numbers of all parents (phase-1)
    unsigned int entryCount;                  //!< number of entries in this chunk
    equivID_t  *entries;                    //!< the chunks entries containing eqIDs
} chunk_t;

//! array of all chunks for this phase
typedef struct
{
    unsigned short chunkCount;   //!< number of chunks
    chunk_t chunks[MAX_CHUNKS];  //!< the chunks
} chunkTable_t;

//! array for all phases
typedef struct {
    unsigned short phaseCount;              //!< number of phases
    chunkTable_t phases[MAX_PHASES];
} phases_t;


//! final rule map to match the last index to an array of rule numbers
typedef struct {
    unsigned short ruleCount;               //!< number of rules
    unsigned short rules[MAX_RULES_MATCH];  //!< array with rule indexes
} matchingRules_t;

typedef matchingRules_t *ruleMap_t;


//! stores the calculated indexes during classification
typedef equivID_t eqNum_t[MAX_PHASES][MAX_CHUNKS];


//! classifie which uses recursive flow classification scheme
class ClassifierRFC : public Classifier
{
 private:
    //! number lines
    numberLines_t nlines;

    //! number line descriptors (offset, length of match)
    numberLineDescrs_t nldscs;

    //! equiv classes (bitmap) indexed by class id
    eqClTable_t eqcl;

    //! chunk data
    phases_t cdata;

    //! final rule bitmap to id array map
    ruleMap_t rmap;
    unsigned long rmap_size;

    //! equiv classes for classify
    eqNum_t eqnums;

    //! bitset containing all rule bits currently used
    bitmap_t allrules;


    //! fast initial add (no rules present)
    void addInitialRules(ruleDB_t *rules);

    //! add a rule
    void addRule(Rule *r);
 
    /*! \short delete a rule
    */
    void delRule( Rule *r);

    //! get index on number line based on the value and mask
    void getIndex(FilterValue *v, FilterValue *m, int size, unsigned short offs,
                  unsigned short *ret);

    //! adds a rule to a single point on the number line
    void addRuleToPoint(point_t *p, pointType_t type, unsigned short rid);

    // project directly on phase 0 chunk and remap existing entries
    void projMatchRemap(unsigned short rid, unsigned short chunk_id, int chunk_size, 
                        unsigned short ch, filter_t *f);

    // project directly on phase 0 chunk
    void projMatchDirectly(unsigned short eq, unsigned short chunk_id, int chunk_size, 
                           unsigned short ch, filter_t *f);
    
    //! projects a match on the the number line (multiple points)
    void projMatch(unsigned short rid, unsigned short chunk_id, int chunk_size, 
                   unsigned short ch, filter_t *f);

    //! do the intersection for phases 1-n
    void doIntersection(unsigned short phase, unsigned short chunk, 
            			unsigned short parent, int *indx, bitmap_t *bm);

    //! remap indexes on number line (incremental add)
    void remapIndex(int phase, int chunk, int from, int to, int rid);

    //! initialize data
    void initData();

    //! cleanup data
    void cleanupData();

    //! check if rule has backward filter attributes
    int hasBackwardSpec(Rule *r);

    //! find a number line
    int findNumberLine(refer_t ref, unsigned short offs, unsigned short chunk,
                       unsigned short chunk_size);

    //! add number line description
    void addNumberLineDescr(refer_t ref, unsigned short offs, unsigned short chunk,
                            unsigned short chunk_size, FilterValue* dmask);

    //! add a new number line
    void addNumberLine(refer_t ref, unsigned short offs, unsigned short chunk,
                       unsigned short chunk_size, FilterValue* dmask);

    void genFinalMap(unsigned short phase);

    unsigned short findEC(unsigned short phase, unsigned short chunk, bitmap_t *b);

    void addPreAllocEC(unsigned short phase, unsigned short chunk, 
                       unsigned short cnt);

    void makeNewChunk(unsigned short phase, unsigned int size);

    //! debug functions
    void printNumberLines();
    void printChunk(unsigned short phase, unsigned short chunk, int show_cdata);
    void printProfiling(struct timeval *t1, struct timeval *t2);

 public:

    /*! \short   construct and initialize a ClassifierRFC object
      \arg \c cnf           ConfigManager
      \arg \c nt            NetTap from which to read packets
      \arg \c sa            Sampling algorithm
      \arg \c packetQueue   Classified packets are put in the queue
      \arg \c threaded      1 if classifier should run in separate thread
    */
    ClassifierRFC(ConfigManager *cnf, Sampler *sa=NULL,
                  PacketQueue *packetQueue=NULL, int threaded=0);

    //! \short   destroy a ClassifierRFC object
    virtual ~ClassifierRFC();

    //! classify a packet
    virtual int classify(metaData_t* pkt);

    //! check a ruleset (the filter part)
    virtual void checkRules(ruleDB_t *rules);

    //! add rules
    virtual void addRules(ruleDB_t *rules);

    //! delete rules
    virtual void delRules(ruleDB_t *rules);
};


//! overload for <<, so that a Classifier object can be thrown into an ostream
ostream& operator<< ( ostream &os, ClassifierRFC &cl );


#endif // _CLASSIFIERRFC_H_
