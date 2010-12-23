
/*  \file   FilterValParser.h

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
    parse filter constant files
   
    $Id: FilterValParser.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _FILTERVALPARSER_H_
#define _FILTERVALPARSER_H_


#include "stdincpp.h"
#include "libxml/parser.h"
#include "Logger.h"
#include "XMLParser.h"
#include "FilterValue.h"
#include "FilterDefParser.h"


//! filter value
typedef struct
{
    string name;
    string type;
    string svalue;
} filterValItem_t;

//! filter value list
typedef map<string,filterValItem_t>            filterValList_t;
typedef map<string,filterValItem_t>::iterator  filterValListIter_t;


class FilterValParser : public XMLParser
{
  private:

    Logger *log;
    int ch;

    //! parse filter value definition
    filterValItem_t parseDef(xmlNodePtr cur);

  public:

    FilterValParser(string fname);

    virtual ~FilterValParser() {}

    //! add parsed filter values to list
    virtual void parse(filterValList_t *list);
};


#endif // _FILTERVALPARSER_H_
