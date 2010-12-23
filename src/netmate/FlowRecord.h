
/*! \file FlowRecord.h

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
    header file for CollectionResult data container class

    $Id: FlowRecord.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _FLOWRECORDLIST_H_
#define _FLOWRECORDLIST_H_


#include "stdincpp.h"
#include "ModuleLoader.h"
#include "MetricData.h"
#include "Module.h"


/*! \short   brief FlowRecord class description
  
    a FlowRecord is an object which stores the results of one
    collection for one metering rule including the measurement data
    from one or more evaluation modules used by this metering rule.
    Additional information include taskID, timestamp and moduleName + 
    typeInformation for every used evaluation module
*/

//! module data list
typedef list<MetricData*>            moduleDataList_t;
typedef list<MetricData*>::iterator  moduleDataListIter_t;


class FlowRecord
{
  private:

    //! corresponding rule identifier
    int ruleId;

    //! creation time
    time_t time;

    //! data 
    moduleDataList_t resultData;

    //! pointer to current metric data
    moduleDataListIter_t   curr;

    //! name of the rule
    string ruleName;

    //! is interim or final flow record;
    int finalRec;

    //! indicate that this flow record should be deleted after poped from
    //  the exporter queue
    int deleteRec;

  public:

    //! construct and initialize a FlowRecord object
    FlowRecord( int ruleId, string rname, int final = 0);
    
    //! destroy a FlowRecord object
    ~FlowRecord();

    /*! \short   return timestamp for creation of this FlowRecord
        /returns time of object creation in seconds from 1.1.1970
    */
    time_t when() 
    { 
        return time;
    }

    void markForDelete()
    {
      deleteRec = 1;
    }

    int getDelete()
    {
      return deleteRec;
    }

    void setFinal(int final)
    {
      finalRec = final;
    }

    int isFinal()
    {
      return finalRec;
    }

    //! return rule id of this flow record
    int getRuleId()
    {
        return ruleId;
    }

    //! return rule name of this flow record
    string getRuleName()
    {
        return ruleName;
    }

    //! add other data exported from an evaluation module to a FlowRecord
    void addData( MetricData *mdata );

    /* \short   get the i-th measurement result from the collection

        returns a pointer to the next stored MetricData object and
        advances the internal pointer to the next MetricData or
        returns NULL if there is no more results data stored in this
        object (end of list)
    */
    MetricData* getNextData();

    //! reset internal data pointer to start of data list */
    void startExport() 
    { 
        curr = resultData.begin();
    }

    //! dump a FlowRecord object
    void dump( ostream &os );

};


//! overload for <<, so that a FlowRecord object can be thrown into an ostream
ostream& operator<< ( ostream &os, FlowRecord &obj );


#endif // _FLOWRECORDLIST_H_
