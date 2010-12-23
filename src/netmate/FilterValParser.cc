
/*  \file   FilterValParser.cc

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

    $Id: FilterValParser.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "FilterValParser.h"
#include "constants.h"


FilterValParser::FilterValParser(string filename)
    : XMLParser(FILTERVAL_DTD, filename, "FILTERVAL")
{
    log = Logger::getInstance();
    ch = log->createChannel("FilterValParser" );
}


filterValItem_t FilterValParser::parseDef(xmlNodePtr cur)
{
    filterValItem_t item;
    string mask;

    // get DEF
    item.name = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"NAME"));
    if (item.name.empty()) {
        throw Error("Filter Val Error: missing name at line %d", XML_GET_LINE(cur));
    }
    // use lower case internally
    transform(item.name.begin(), item.name.end(), item.name.begin(), ToLower());

	item.type = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"TYPE"));
    item.svalue = xmlCharToString(xmlNodeListGetString(XMLDoc, cur->xmlChildrenNode, 1));
    if (item.svalue.empty()) {
        throw Error("Filter Val Error: missing value at line %d", XML_GET_LINE(cur));
    }
    // use lower case internally
    transform(item.svalue.begin(), item.svalue.end(), item.svalue.begin(), ToLower());
    // do not parse value here

    return item;
}


void FilterValParser::parse(filterValList_t *list)
{
    xmlNodePtr cur;

    cur = xmlDocGetRootElement(XMLDoc);

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
	 
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"DEF")) && (cur->ns == ns)) {
            // parse
            filterValItem_t item = parseDef(cur);
            // add 
            list->insert(make_pair(item.name, item));
#ifdef DEBUG	
            log->dlog(ch, "%s = %s", item.name.c_str(), item.svalue.c_str());
#endif	
        } 

        cur = cur->next;
    }
}
