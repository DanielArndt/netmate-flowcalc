
/*  \file   FilterDefParser.h

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
    parse filter definition files

    $Id: FilterDefParser.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _FILTERDEFPARSER_H_
#define _FILTERDEFPARSER_H_


#include "stdincpp.h"
#include "libxml/parser.h"
#include "Logger.h"
#include "XMLParser.h"
#include "FilterValue.h"


const string DEF_MASK_IP  = "255.255.255.255";
const string DEF_MASK_IP6 = "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF";


//! conditional item
typedef struct
{
    string name;

    FilterValue mask;
    FilterValue val;
 
} condItem_t;


//! reference points
typedef enum
{
    MAC = 0,
    IP,
    TRANS,
    DATA
} refer_t;


//! filter definition
typedef struct
{
    string name;
    string rname;
    string type;
    refer_t refer;
    condItem_t cond;
    unsigned short offs;
    unsigned short len;
    FilterValue mask;
    //! position of lowest set bit in mask (only computed for len == 1)
    unsigned char shift;
} filterDefItem_t;


//! filter list
typedef map<string,filterDefItem_t>            filterDefList_t;
typedef map<string,filterDefItem_t>::iterator  filterDefListIter_t;


class FilterDefParser : public XMLParser
{

  private:

    Logger *log;
    int ch;

    //! parse filter definition
    filterDefItem_t parseDef(xmlNodePtr cur);

    //! parse reference point
    refer_t parseRefer(string refer);

    //! get reference name
    string getRefer(refer_t r);

  public:

    FilterDefParser(string fname = "");

    virtual ~FilterDefParser() {}

    //! add parsed filter definitions to list
    virtual void parse(filterDefList_t *list);
};


#endif // _FILTERDEFPARSER_H_
