
/*! \file   MetricData.h

    Modifications Copyright Daniel Arndt, 2010

    This version has been modified to provide additional output and 
    compatibility. For more information, please visit

    http://web.cs.dal.ca/~darndt

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
    container for measurement data of one proc module

    $Id: MetricData.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _METRICDATA_H_
#define _METRICDATA_H_


#include "stdincpp.h"
#include "ProcModuleInterface.h"
#include "Rule.h"
#include "ExportList.h"

extern unsigned short DataTypeSize[];

/*! \short   a MetricData object is a data container. It stores
    the data for one data collection of one packet processor module

    The stored data include : name of the module that generated
    the data, type information for the data and the measurement
    data itself.
*/

typedef struct
{
    unsigned long long flowId;
    int newFlow;
    char *buf;
    unsigned long len;
    // pointer into buffer
    char *ptr;
    // indicate swapped to host byte order (only for keybufs)
    int swap;
} buf_t;

typedef vector<buf_t> bufList_t;
typedef vector<buf_t>::iterator bufListIter_t;

class MetricData
{
  private:

    static Logger *s_log;
    static int s_ch;

    //! module (metric name)
    string mname;

    //! export lists for the packet proc module
    ExportList *exportLists;
    
    //! flow key definition
    flowKeyInfo_t *flowKeyList;

    // working pointer to flow key list
    flowKeyInfo_t *fkPtr;

    // number of flows this object contains data for
    int flowCount;

    // pointer to current flow
    int currFlow;

    //! number of flow keys
    unsigned short keyCount;

    //! pointer to the current export list
    int currList;

    // flow key and flow data buffer list
    bufList_t keyBufs, dataBufs;

    // align a pointer to a specific data type
    inline char *align(char *var, DataType_e type)
    {
	return (DataTypeSize[type] == 0) ? var :
	    (char*) (((uintptr_t)(var) + DataTypeSize[type]-1) /
		     DataTypeSize[type] * DataTypeSize[type]);
    }

    const char* getNextData( DataType_e type );

  public:

    /*! \short   construct and initialize a MetricData object

        \arg \c minfo - info about evaluation module that generated this data set
        \arg \c size - size of data block in bytes
        \arg \c data - the measurement data block
    */
    MetricData(string mn, ExportList *e, flowKeyInfo_t *fkl, int size, const unsigned char *mdata, 
               unsigned short ksize, const unsigned char *kdata, int newFlow=0, unsigned long long flowId=0);

    //! destroy a MetricData object
    ~MetricData();

    void addFlowData(int size, const unsigned char *mdata, 
                     unsigned short ksize, const unsigned char *kdata, int newFlow, unsigned long long flowId=0);
   
    //! initialize export of this metric data; must be called before
    //  all other functions
    void initExport();
    
    //! cycle through all export lists
    //  return: total number of all rows of list or 0
    int getNextList();

    //! cycle thorugh all (sub) flows contained in this object
    // return: number of rows for the current flow or 0
    int getNextFlow(unsigned long long *flowId, int *newFlow);

    //! get the next row of flow data
    // return: pointer to data fields or NULL
    const char *getNextFlowDataRow(DataType_e *type);

    //! cycle through all flow data field names
    // return: field name or NULL
    const char *getNextFlowDataLabel(DataType_e *type);

    //! get number of data field columns
    int getFlowDataColNum()
      {
          return exportLists->getList(currList)->cols.size();
      }

    //! cyclce through flow key data
    // return: pointer to flow key data or NULL
    const char *getNextFlowKey(DataType_e *type);

    //! cycle through all flow key names
    // return: flow key name or NULL
    const char *getNextFlowKeyLabel(DataType_e *type);

    //! get number of flow key columns
    unsigned short getFlowKeyNum()
      {
          return keyCount;
      }

    //! get name of packet proc module
    inline string getModName()
    {
        return mname;
    }

    /*! generate string representation 
        \returns a string representation of this MetricData object
    */
    string toString();

    //! dump a MetricData object
    void dump( ostream &os );

};


//! overload for <<, so that a MetricData object can be thrown into an ostream
ostream& operator<< ( ostream &os, MetricData &obj );


#endif // _METRICDATA_H_
