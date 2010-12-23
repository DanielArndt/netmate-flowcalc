
/*!\file ExportList.cc

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
     This class encapsulates the name and type data for one
     proc module. It stores (possibly multiple) list(s) and
     their width, and the types and names of the columns

     $Id: ExportList.cc 748 2009-09-10 02:54:03Z szander $
*/

#include <ExportList.h>
#include <ProcModule.h>
#include <ProcModuleInterface.h>


/* -------------------- ExportList -------------------- */

ExportList::ExportList( typeInfo_t* rttList )
{
    typeInfo_t *rlist, *rscan;

    if (rttList == NULL) {
        throw Error("illegal parameter for ExportList, rttList == NULL" );
    }
    
    rlist = rttList;
    while (rlist->type != EXPORTEND) {
        if (rlist->type <= INVALID1 || rlist->type >= INVALID2 ) {
            throw Error("illegal type information in export list" );
        }
        rlist++;
    }

    rlist = rttList;
    while (rlist->type != EXPORTEND) {
        
        if (rlist->type != LIST) {
            rscan = rlist+1;
            while (rscan->type != LIST && rscan->type != EXPORTEND) {
                rscan++;
            }
#ifdef DEBUG2
            cerr << "[EXPORTLIST] " << "1-row-list with cols = " << rscan - rlist << endl;
#endif
            newList(rscan - rlist, rlist, 1 );
            rlist = rscan;
        } else { // rlist->type == LIST
            
            rscan = rlist+1;
            while (rscan->type != LISTEND && rscan->type != EXPORTEND) {
                rscan++;
            }
#ifdef DEBUG2
            cerr << "[EXPORTLIST] " << "n-row-list with cols = " << rscan - rlist - 1 << endl;
#endif
            newList(rscan - rlist - 1, rlist + 1, 0 );
            if (rscan->type == LISTEND ) {
                rscan++;
            }
            rlist = rscan;
        }
    }
}


/* -------------------- newList -------------------- */

void ExportList::newList( int ncols, typeInfo_t* rttList, int nrows )
{
    if (ncols <= 0) {
        throw Error("illegal parameter for ExportList, ncols = %d", ncols );
    }

    if (nrows < 0) {  // == 0 is legal!
        throw Error("illegal parameter for ExportList, nrows = %d", nrows );
    }

#ifdef DEBUG2
    cerr << "[EXPORTLIST] " << "got new list with "<<ncols<<" cols, "<<nrows<<" rows"<< endl;
#endif

    for (int i = 0; i < ncols; i++) {
        
        DataType_e type = rttList[i].type;
        
        if (type <= INVALID1 || type >= INVALID2 || 
            type == LIST || type == LISTEND || type == EXPORTEND) {
            
            throw Error("illegal type information '%s' within export list",
                        ProcModule::typeLabel(type).c_str() );
        }
        
#ifdef DEBUG2
        cerr << " " << rttList[i].name << "[" << ProcModule::typeLabel(type) << "]";
#endif
    }
#ifdef DEBUG2
    cerr << endl;
#endif
    
    exportColumns_t cols;
    cols.resize( ncols );
    
    for (int i = 0; i < ncols; i++) {
        cols[i].type = rttList[i].type;
        cols[i].label = rttList[i].name;
    }
    
    exportTable_t table;
    table.cols = cols;
    table.nrows = nrows;

    exportLists.push_back( table );
}


void ExportList::dump( ostream &os )
{
    os << "ExportList with  " << exportLists.size() << "  table"
       << ((exportLists.size() > 1) ? "s":"") << endl;
    
    for (unsigned int i = 0; i < exportLists.size(); i++ ) {
        
        os << " table " << i+1 << " with  " << exportLists[i].cols.size() 
           << "  columns and  " << ((exportLists[i].nrows == 1) ?
                                    "1  row" : "n  rows") << endl;
        
        exportColumns_t cols = exportLists[i].cols;
        
        os << "  columns = ";
        char c = ' ';
        for (unsigned int j = 0; j < cols.size(); j++) {
            os << c << " " << cols[j].label << "(" 
               << ProcModule::typeLabel(cols[j].type) << ")";
        }
        os << endl;
    }
}


ostream& operator<< ( ostream &os, ExportList &list )
{
    list.dump ( os );
    return os;
}

