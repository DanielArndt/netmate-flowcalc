
/*! \file ExportList.h

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

    $Id: ExportList.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _EXPORTLIST_H_
#define _EXPORTLIST_H_


#include "stdincpp.h"
#include "ProcModuleInterface.h"


//! column definition
typedef struct {
    enum DataType_e type;
    string          label;
} column_t;

//! list of columns
typedef vector< column_t >            exportColumns_t;
typedef vector< column_t >::iterator  exportColumnsIter_t;

//! FIXME add documentation
typedef struct {
    exportColumns_t cols;
    int nrows;
} exportTable_t;

//! table definition
typedef vector< exportTable_t >            exportLists_t;
typedef vector< exportTable_t >::iterator  exportListsIter_t;

/*!
    This class encapsulates the name and type data for one
    proc module. It stores (possibly multiple) list(s) and
    their width, and the types and names of the columns
*/

class ExportList
{
  private:

    exportLists_t exportLists; //! list of configured export lists for a proc module

  public:

    /*! \brief construct a new ExportLists object

        \arg crttList - the export structure specification from the proc module
    */
    ExportList( typeInfo_t* rttList );

    //! return the size of the export list
    inline unsigned int size() 
    { 
        return exportLists.size(); 
    }

    //! get pointer to the export list
    inline exportTable_t* getList(unsigned int i) 
    { 
        return &exportLists[i]; 
    }

    /*! \brief construct a new (sub)list

        This function parses the supplied runtime type information (rttList) and
        stores internally the types and names for a tables of the specified size.

        \arg cncols   - number of columns in this table
        \arg cnrows   - number of rows in this table (==0 means that true size is
                         only known at runtime from export data record (MetricData))
        \arg crttList - the export structure specification from the proc module
                         at the position where a list starts (behind LIST entry)
    */
    void newList( int ncols, typeInfo_t* rttList, int nrows );

    //! write this object to an ostream
    void dump( ostream &os );

};


ostream& operator<< ( ostream &os, ExportList &list );


#endif // _EXPORTLIST_H_
