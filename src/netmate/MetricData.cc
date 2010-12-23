
/*!\file   MetricData.cc

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

    $Id: MetricData.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "Error.h"
#include "Logger.h"
#include "MetricData.h"




Logger* MetricData::s_log = NULL;
int     MetricData::s_ch = 0;


enum ListTypes_e { VARIABLE = 0, FIXED1 = 1 };


/* ------------------------- MetricData ------------------------- */

MetricData::MetricData(string mn, ExportList *e, flowKeyInfo_t *fkl, int size, 
                       const unsigned char *mdata, unsigned short ksize,
                       const unsigned char *kdata, int newFlow, unsigned long long flowId)
    : mname(mn), exportLists(e), flowKeyList(fkl), flowCount(0), keyCount(0)
{
    
    s_log = Logger::getInstance();
    s_ch = s_log->createChannel("MetricData");
#ifdef DEBUG
    s_log->dlog(s_ch, "Creating with size = %d", size);
#endif

   
    // get number of flow keys
    if (flowKeyList != NULL) {
        flowKeyInfo_t *tpos = flowKeyList;
        while (tpos->type != EXPORTEND) {
            tpos++;
            keyCount++;
        }
    }

    if (size > 0) {
        addFlowData(size, mdata, ksize, kdata, newFlow, flowId);
    }

    initExport();
}


/* ------------------------- ~MetricData ------------------------- */

MetricData::~MetricData()
{
#ifdef DEBUG
    s_log->dlog(s_ch, "Destroyed");
#endif

    for (bufListIter_t i = dataBufs.begin(); i != dataBufs.end(); ++i) {
        saveDeleteArr(i->buf);
    }

     for (bufListIter_t i = keyBufs.begin(); i != keyBufs.end(); ++i) {
         if (i->buf != NULL) {
             saveDeleteArr(i->buf);
         }
    }
}


void MetricData::addFlowData(int size, const unsigned char *mdata, 
                             unsigned short ksize, const unsigned char *kdata, 
                             int newFlow, unsigned long long flowId)
{
    buf_t new_buf;

    if (size > 0) {
        new_buf.buf  = new char[size];

        if (new_buf.buf != NULL) {
            // copy metric data (necessary if the data is not exported immediately!
            memcpy(new_buf.buf, mdata, size);
            new_buf.flowId = flowId;
            new_buf.newFlow = newFlow;
            new_buf.len = size;
            new_buf.ptr = new_buf.buf;
	    new_buf.swap = 0;
            dataBufs.push_back(new_buf);
        } else {
            throw Error("could not allocate metric data of size %d", size);
        }

        flowCount++;

        if (ksize > 0) {
            new_buf.buf = new char[ksize];

            if (new_buf.buf != NULL) {
                // copy metric data (necessary if the data is not exported immediately!
                memcpy(new_buf.buf, kdata, ksize);
                //new_buf.flowId = flowId;
                //new_buf.newFlow = newFlow;
                new_buf.len = ksize;
                new_buf.ptr = new_buf.buf;
		new_buf.swap = 0;
            } else {
                throw Error("could not allocate flow key data of size %d", ksize);
            }
        } else {
            new_buf.buf = NULL;
            //new_buf.flowId = 0;
            //new_buf.newFlow = 1;
            new_buf.len = 0;
            new_buf.ptr = NULL;
	    new_buf.swap = 0;
        }

        keyBufs.push_back(new_buf);
    }
}

/* -------------------- initExport -------------------- */

void MetricData::initExport()
{
    currList = -1;
    currFlow = -1;
 
    // reset pointers
    for (bufListIter_t i = dataBufs.begin(); i != dataBufs.end(); ++i) {
        i->ptr = i->buf;
    }

     for (bufListIter_t i = keyBufs.begin(); i != keyBufs.end(); ++i) {
        i->ptr = i->buf;
    }
 
    fkPtr = flowKeyList;
}


/* -------------------- getNextList  -------------------- */

unsigned short DataTypeSize[] =
{
    0, // EXPORTEND
    0, // LIST
    0, // LISTEND
    sizeof(char),
    sizeof(int8_t),
    sizeof(int16_t),
    sizeof(int32_t),
    sizeof(int64_t),
    sizeof(uint8_t),
    sizeof(uint16_t), 
    sizeof(uint32_t),
    sizeof(uint64_t),
    0, // STRING (no alignment)
    sizeof(uint32_t), // BINARY (aligned on 4 byte (int for storing array length value))
    4, // IPV4ADDR
    16, // IPV6ADDR
    sizeof(float),
    sizeof(double),
    0            // List Terminator
};


int MetricData::getNextList()
{
  
    currList++;
    currFlow = -1;

    if (currList == (int) exportLists->size()) {
        return -1;
    } else { 
        switch (exportLists->getList(currList)->nrows) {
        case FIXED1:
            // one line per flow
            return 1 * flowCount; 
        case VARIABLE:
          {
              char *tmp;
              int r = 0;
      
              for (int i=0; i < flowCount; i++) {
                  tmp = align(dataBufs[i].ptr, BINARY);
                  r += *((int *)tmp);
              }

              return r;
          }
        default:
            return -1;
        }
    }
}


int MetricData::getNextFlow(unsigned long long *flowId, int *newFlow)
{

    currFlow++;

    if (currFlow == flowCount) {
        return -1;
    }
   
    *flowId = dataBufs[currFlow].flowId;
    *newFlow = dataBufs[currFlow].newFlow;

    switch (exportLists->getList(currList)->nrows) {
    case FIXED1:
        return 1;
        break;
    case VARIABLE:
      {
          char *tmp;
          int r = 0;

          tmp = align(dataBufs[currFlow].ptr, BINARY);
          r = *((int *)tmp);
          
          // varlists start with header of 2ints
          dataBufs[currFlow].ptr += 2 * sizeof(int); 

          return r;
      }
      break;
    default:
        return -1;
    }
}


// FIXME flow key data is currently not aligned
const char *MetricData::getNextFlowKey(DataType_e *type)
{
    const char *tmp;

    if (keyCount == 0) {
        return NULL;
    }

    if (fkPtr->type == EXPORTEND) {
        fkPtr = flowKeyList;
        keyBufs[currFlow].ptr = keyBufs[currFlow].buf;
	keyBufs[currFlow].swap = 1;
        return NULL;
    }

    *type = fkPtr->type;

    tmp = keyBufs[currFlow].ptr;

    if (*type == STRING) {
      keyBufs[currFlow].ptr += strlen(tmp) + 1;
    } else if (*type == BINARY) {
      unsigned int len = *((unsigned int *)tmp);
      keyBufs[currFlow].ptr = (char *) tmp + DataTypeSize[UINT32] + len;
    } else {
      if (!keyBufs[currFlow].swap) {
	if ((*type == UINT16) || (*type == INT16)) {
	  // still in network byteorder so swap
	  *((uint16_t *) tmp) = ntohs(*((uint16_t *) tmp));
	}
	if ((*type == UINT32) || (*type == INT32)) {
	  // still in network byteorder swap
	  *((uint32_t *) tmp) = ntohs(*((uint32_t *) tmp));
	}
      }
      
      keyBufs[currFlow].ptr += DataTypeSize[fkPtr->type];
    }
    
    fkPtr++;

    return tmp;
}

const char *MetricData::getNextFlowKeyLabel(DataType_e *type)
{
    flowKeyInfo_t *tmp;

    if (keyCount == 0) {
        return NULL;
    }

    if (fkPtr->type == EXPORTEND) {
        fkPtr = flowKeyList;
        return NULL;
    }
    
    tmp = fkPtr;

    *type = fkPtr->type;

    fkPtr++;

    return tmp->name;
}

const char *MetricData::getNextFlowDataLabel(DataType_e *type)
{
    static int i = 0;
    const char *tmp; 

    if (i == getFlowDataColNum()) {
        i = 0;
        return NULL;
    }
    
    
    tmp = exportLists->getList(currList)->cols[i].label.c_str();

    *type = exportLists->getList(currList)->cols[i].type;

    i++;

    return tmp;
}

/* -------------------- getNextData -------------------- */


const char *MetricData::getNextFlowDataRow(DataType_e *type)
{
    static int i = 0;
    DataType_e t;

    if (i == getFlowDataColNum()) {
        i = 0;
        return NULL;
    }

    t = exportLists->getList(currList)->cols[i].type;

    *type = t;

    i++;
    
    return getNextData(t);
}

const char *MetricData::getNextData( DataType_e type )
{
    const char *tmp = NULL;

    switch (type) {
	
    case CHAR: 
    case INT8:
    case UINT8:	
        // not necessary to align on a byte ;)
        tmp = dataBufs[currFlow].ptr;
        dataBufs[currFlow].ptr += DataTypeSize[type];
        break;
    case INT16:
    case UINT16:
    case INT32:
    case UINT32:
    case IPV4ADDR:
    case FLOAT:
    case DOUBLE:
        tmp = align(dataBufs[currFlow].ptr, type);
        dataBufs[currFlow].ptr = (char *) tmp + DataTypeSize[type];
        break;
    case IPV6ADDR:
        // align on 32 bit
        tmp = align(dataBufs[currFlow].ptr, INT32);
        dataBufs[currFlow].ptr = (char *) tmp + DataTypeSize[type];
        break;
    case INT64:
    case UINT64:
        // 64bit values only aligned to 32bit
        tmp = align(dataBufs[currFlow].ptr, INT32); 
        dataBufs[currFlow].ptr = (char *) tmp + DataTypeSize[type];
        break;
    case STRING:
        tmp = dataBufs[currFlow].ptr;
        dataBufs[currFlow].ptr += strlen((char *)dataBufs[currFlow].ptr) + 1;
        break;
    case BINARY:
      {
          tmp = align(dataBufs[currFlow].ptr, UINT32);
          unsigned int len = *((unsigned int *)tmp);
          dataBufs[currFlow].ptr = (char *) tmp + DataTypeSize[UINT32] + len;
      }
      break;
    case INVALID1: 
    case INVALID2: 
    default:       
        ;
    }

    return tmp;
}


/* ------------------------- toString ------------------------- */

// FIXME currently not implemented/used (see code in text_file module)
string MetricData::toString()
{
    return "";
}

void MetricData::dump( ostream &os )
{
    os << toString() << endl;
}


ostream& operator<< ( ostream &os, MetricData &obj )
{
    obj.dump(os);
    return os;
}

