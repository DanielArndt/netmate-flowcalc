
/*  \file   FilterDefParser.cc

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

    $Id: FilterDefParser.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "FilterDefParser.h"
#include "ParserFcts.h"
#include "constants.h"


FilterDefParser::FilterDefParser(string filename)
    : XMLParser(FILTERDEF_DTD, filename, "FILTERDEF")
{
    log = Logger::getInstance();
    ch = log->createChannel("FilterDefParser" );
}


refer_t FilterDefParser::parseRefer(string refer)
{
    if (refer == "MAC") {
        return MAC;
    } else if (refer == "IP") {
        return IP;
    } else if (refer == "TRANS") {
        return TRANS;
    } else if (refer == "DATA") {
        return DATA;
    } else {
        throw Error("Unknown reference point %s", refer.c_str());
    }
}


string FilterDefParser::getRefer(refer_t r)
{
    switch (r) {
    case MAC:
        return "MAC";
        break;
    case IP:
        return "IP";
        break;
    case TRANS:
        return "TRANS";
        break;
    case DATA:
        return "DATA";
        break;
    default:
        throw Error("Unknown refer");
    }
}


filterDefItem_t FilterDefParser::parseDef(xmlNodePtr cur)
{
    filterDefItem_t item;
    string mask;

    // get DEF
    item.name = xmlCharToString(xmlNodeListGetString(XMLDoc, cur->xmlChildrenNode, 1));
    if (item.name.empty()) {
        throw Error("Filter Def Error: missing name at line %d", XML_GET_LINE(cur));
    }
    // use lower case internally
    transform(item.name.begin(), item.name.end(), item.name.begin(), ToLower());

    try {
        item.refer = parseRefer(xmlCharToString(xmlGetProp(cur, (const xmlChar *)"REFER")));
    } catch (Error &e) {
        throw Error("Filter Def Error: refer error at line %d: %s", 
                    XML_GET_LINE(cur), e.getError().c_str());
    }
    
    item.type = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"TYPE"));
    item.len = FilterValue::getTypeLength(item.type);
    try {
        if ((item.type == "Binary") || (item.type == "String")) {
            item.len = ParserFcts::parseULong(xmlCharToString(xmlGetProp(cur, (const xmlChar *)"LENGTH")),0,
                                              MAX_FILTER_LEN);
        }
        item.offs = ParserFcts::parseULong(xmlCharToString(xmlGetProp(cur, (const xmlChar *)"OFFSET")),0,65535);
    } catch (Error &e) {
        throw Error("Filter Def Error: type/offset parse error at line %d: %s",  
                    XML_GET_LINE(cur), e.getError().c_str());
    }
    
    try {
        mask = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"MASK"));
        // replace default mask for IP addresses
        if (mask == "0xFF") {
            if (item.type == "IPAddr") {
                mask = DEF_MASK_IP;
            } else if (item.type == "IP6Addr") {
                mask = DEF_MASK_IP6;
            } else  {
                // make default mask as wide as data
                mask = "0x" + string(2*item.len, 'F');
            }
        }
        item.mask = FilterValue(item.type, mask);
        item.len = item.mask.getLen();

	item.shift= 0;
	// if attribute is of type byte then store the number of 
	// lower zero bits of the mask for downshifting during export
	if (item.len == 1) {
	    unsigned char c = item.mask.getValue()[0];
	    while( (c & 0x1) == 0 ) {
		item.shift++;
		c = c>>1;
	    }
	}
	
    } catch (Error &e) {
        throw Error("Filter Def Error: mask parse error at line %d: %s",
                    XML_GET_LINE(cur), e.getError().c_str());
    }

    xmlChar *rev = xmlGetProp(cur, (const xmlChar *)"REV");
    if (rev != NULL) {
        item.rname = xmlCharToString(rev);

        transform(item.rname.begin(), item.rname.end(), item.rname.begin(), ToLower());
    }
    
    return item;
}


void FilterDefParser::parse(filterDefList_t *list)
{
    xmlNodePtr cur/*, cur2*/;

    cur = xmlDocGetRootElement(XMLDoc);

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
	
        if (((!xmlStrcmp(cur->name, (const xmlChar *)"DEF")) || 
             (!xmlStrcmp(cur->name, (const xmlChar *)"COND"))) && (cur->ns == ns)) {
	 
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"DEF")) && (cur->ns == ns)) {
                // parse
                filterDefItem_t item = parseDef(cur);
                // add 
                list->insert(make_pair(item.name, item));
#ifdef DEBUG		
                log->dlog(ch, "%s = [%s+%d]&%s|%d", item.name.c_str(), getRefer(item.refer).c_str(),
                          item.offs, item.mask.getString().c_str(), item.len);
#endif
            }
            
#if 0
            // FIXME conditional definitions not yet supported
            // get COND
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"COND")) && (cur->ns == ns)) {
                condItem_t citem;
                filterDefListIter_t iter;
                string mask;

                citem.name = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"NAME"));
                // use lower case internally
                transform(citem.name.begin(), citem.name.end(), citem.name.begin(), 
                          ToLower());

                // Lookup 
                iter = list->find(citem.name);
                if (iter != list->end()) {

                    // get type, set value
                    citem.val = FilterValue(iter->second.type, 
                                            xmlCharToString(xmlGetProp(cur, (const xmlChar *)"VAL")));

                    // and mask
                    mask = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"MASK"));
                    // replace default mask for IP addresses
                    if ((iter->second.type == "IPAddr") && (mask == "0xFF")) {
                        mask = DEF_MASK_IP;
                    } else if ((iter->second.type == "IP6Addr") && (mask == "0xFF")) {
                        mask = DEF_MASK_IP6;
                    }
                    citem.mask = FilterValue(iter->second.type, mask);
                } else {
                    // must define before used as condidional
                    throw Error("Conditional item used before definition at line %d: %s", 
                                XML_GET_LINE(cur), citem.name.c_str());
                }

                cur2 = cur->xmlChildrenNode;

                while (cur2 != NULL) {

                    if ((!xmlStrcmp(cur2->name, (const xmlChar *)"DEF")) && (cur2->ns == ns)) {
                        // parse
                        filterDefItem_t item = parseDef(cur2);
                        item.cond = citem;
                        // add 
                        list->insert(make_pair(item.name, item));
#ifdef DEBUG			
                        log->dlog(ch, "if %s&%s = %s then %s = [%s+%d]&%s|%d", item.cond.name.c_str(),
                                  item.cond.mask.getString().c_str(), item.cond.val.getString().c_str(),
                                  item.name.c_str(), getRefer(item.refer).c_str(),
                                  item.offs, item.mask.getString().c_str(), item.len);
#endif			
                    }

                    cur2 = cur2->next;
                }
            }
#endif
        }

        cur = cur->next;
    }

    // check attribute reverse definition
    for (filterDefListIter_t i = list->begin(); i != list->end(); i++) {
        if (!i->second.rname.empty()) {
            // find reverse in list
            filterDefListIter_t ref = list->find(i->second.rname);
            if (ref == list->end()) {
                throw Error("Filter Def Error: reverse attribute not found: %s", 
                            i->second.rname.c_str());
            } else {
                // check type
                if (i->second.type != ref->second.type) {
                    throw Error("Filter Def Error: reverse attribute type mismatch: %s", 
                                i->second.rname.c_str());
                }
            }
        }
    }
}
