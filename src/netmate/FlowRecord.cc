
/*!\file   FlowRecord.cc

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
    implementation for FlowRecord data container class

    $Id: FlowRecord.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "Error.h"
#include "Logger.h"
#include "FlowRecord.h"


/* ------------------------- FlowRecord ------------------------- */

FlowRecord::FlowRecord( int id, string rname, int final )
  : deleteRec(0)
{
    ruleId = id;
    time = ::time(NULL);
    ruleName = rname;
    finalRec = final; 
}


/* ------------------------- ~FlowRecord ------------------------- */

FlowRecord::~FlowRecord()
{
    moduleDataListIter_t iter;

    for ( iter = resultData.begin(); iter != resultData.end(); iter++ ) {
        // delete a MetricData object
        saveDelete(*iter); 
    }
}


/* ------------------------- addData ------------------------- */

void FlowRecord::addData( MetricData *mdata )
{
    resultData.push_back( mdata );
    curr = resultData.begin();
}


/* ------------------------- getData ------------------------- */

MetricData* FlowRecord::getNextData()
{
    if (curr == resultData.end()) {
        return NULL;
    } else { 
        return *(curr)++;
    }
}


/* ------------------------- dump ------------------------- */

void FlowRecord::dump( ostream &os )
{
    moduleDataListIter_t iter;

    for ( iter = resultData.begin(); iter != resultData.end(); iter++ ) {
        os << (*iter)->toString() << " ";
    }

    os << endl;
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, FlowRecord &obj )
{
    obj.dump(os);
    return os;
}
