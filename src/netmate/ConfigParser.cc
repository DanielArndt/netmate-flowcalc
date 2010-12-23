
/*  \file   ConfigParser.cc

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
    parses configuration file and adds item into config db

    $Id: ConfigParser.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "config.h"
#include "Logger.h"
#include "ConfigParser.h"
#include "FilterValue.h"
#include "ParserFcts.h"
#include "constants.h"


// init static members
string ConfigParser::bindir = "";


ConfigParser::ConfigParser(string filename, string binary)
    : XMLParser(CONFIGFILE_DTD, filename, "CONFIG")
{
    if (bindir.empty()) {
        bindir = dirname((char *)(binary + " ").c_str());
    }

    log = Logger::getInstance();
    ch = log->createChannel("ConfigParser");
}


configItem_t ConfigParser::parsePref(xmlNodePtr cur)
{
    configItem_t item;

    item.name = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"NAME"));
    if (item.name.empty()) {
        throw Error("Config Parser Error: missing name at line %d", XML_GET_LINE(cur));
    }
    item.value = xmlCharToString(xmlNodeListGetString(XMLDoc, cur->xmlChildrenNode, 1));
    if (item.value.empty()) {
        throw Error("Config Parser Error: missing value at line %d", XML_GET_LINE(cur));
    }
    item.type = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"TYPE"));

    // prepend binary directory to config items stating a relative path
    if (item.type == "String" && bindir != "." &&
        (item.value.substr(0, 2) == "../" ||
         item.value.substr(0, 3) == "./" )) {
	    item.value = bindir + "/" + item.value;
    }

    // check if item can be parsed
    try {
        ParserFcts::parseItem(item.type, item.value);
    } catch (Error &e) {    
        throw Error("Config Parser Error: parse value error at line %d: %s", XML_GET_LINE(cur), 
                    e.getError().c_str());
    }
     
    return item;
}


void ConfigParser::parse(configItemList_t *list, configADList_t *ad_list)
{
    xmlNodePtr cur, cur2, cur3, cur4;

    cur = xmlDocGetRootElement(XMLDoc);

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
	
        if (((!xmlStrcmp(cur->name, (const xmlChar *)"MAIN")) || 
             (!xmlStrcmp(cur->name, (const xmlChar *)"CONTROL")) ||
             (!xmlStrcmp(cur->name, (const xmlChar *)"CLASSIFIER")) || 
             (!xmlStrcmp(cur->name, (const xmlChar *)"PKTPROCESSOR")) ||
             (!xmlStrcmp(cur->name, (const xmlChar *)"EXPORTER"))) && (cur->ns == ns)) {
	    
            cur2 = cur->xmlChildrenNode;

            while (cur2 != NULL) {
				
                // get PREF
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"PREF")) && (cur2->ns == ns)) {
                    // parse
                    configItem_t item = parsePref(cur2);
                    item.group = (char *) cur->name; 	
                    // add
                    list->push_back(item);

#ifdef DEBUG
                    log->dlog(ch, "%s.%s = %s", item.group.c_str(), item.name.c_str(), item.value.c_str());
#endif
                }

                // parse MODULES
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"MODULES")) && (cur2->ns == ns)) {
		   
                    cur3 = cur2->xmlChildrenNode;

                    while (cur3 != NULL) {
                        configItem_t item;

                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"MODULE")) && (cur3->ns == ns)) { 

                            cur4 = cur3->xmlChildrenNode;

                            while (cur4 != NULL) {

                                if ((!xmlStrcmp(cur4->name, (const xmlChar *)"PREF")) && (cur4->ns == ns)) {
                                    // parse
                                    configItem_t item = parsePref(cur4);

                                    item.group = (char *) cur->name;
                                    item.module = xmlCharToString(xmlGetProp(cur3, (const xmlChar *)"NAME"));
                                    if (item.module.empty()) {
                                        throw Error("Config Parser Error: missing module name at line %d", 
                                                    XML_GET_LINE(cur3));
                                    }
                    
                                    // add
                                    list->push_back(item);
#ifdef DEBUG
                                    log->dlog(ch, "%s.%s.%s = %s", item.group.c_str(), 
                                              item.module.c_str(), item.name.c_str(), item.value.c_str());
#endif				    
                                }
				
                                cur4 = cur4->next;
                            }
                        }

                        cur3 = cur3->next;
                    }
                }
		
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"ACCESS")) && (cur2->ns == ns)) { 
                    cur3 = cur2->xmlChildrenNode;

                    while (cur3 != NULL) {

                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"ALLOW")) && (cur3->ns == ns)) { 
                            configADItem_t ad_item;

                            ad_item.ad = ALLOW;
                            ad_item.value = xmlCharToString(xmlNodeListGetString(XMLDoc, 
                                                                                 cur3->xmlChildrenNode, 1));
                            if (ad_item.value.empty()) {
                                throw Error("Config Parser Error: missing value at line %d", 
                                            XML_GET_LINE(cur3));
                            }
                            
                            ad_item.type = xmlCharToString(xmlGetProp(cur3, (const xmlChar *)"TYPE"));

                            // add ALLOW
                            ad_list->push_back(ad_item);
#ifdef DEBUG
                            log->dlog(ch, "CONTROL.ACCESS ALLOW %s %s",
                                      ad_item.type.c_str(),
                                      ad_item.value.c_str());
#endif
                        }

                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"DENY")) && (cur3->ns == ns)) { 
                            configADItem_t ad_item;

                            ad_item.ad = DENY;
                            ad_item.value = xmlCharToString(xmlNodeListGetString(XMLDoc, 
                                                                                 cur3->xmlChildrenNode, 1));
                            if (ad_item.value.empty()) {
                                throw Error("Config Parser Error: missing value at line %d", 
                                            XML_GET_LINE(cur3));
                            }

                            ad_item.type = xmlCharToString(xmlGetProp(cur3, (const xmlChar *)"TYPE"));

                            // add DENY
                            ad_list->push_back(ad_item);
#ifdef DEBUG
                            log->dlog(ch, "CONTROL.ACCESS DENY %s %s",
                                      ad_item.type.c_str(),
                                      ad_item.value.c_str());
#endif
                        }

                        cur3 = cur3->next;
                    }
		    
                }
	
                cur2 = cur2->next;
            }
        }

        cur = cur->next;
    }
}


/* -------------------- configItemList_t::getValue -------------------- */

string configItemList_t::getValue( string name )
{
    configItemListIter_t iter;
    
    for( iter = begin(); iter != end(); iter++ ) {
        if( iter->name == name ) {
            return iter->value;
        }
    }
    return string();
};



/* -------------------- << for configItem_t -------------------- */

ostream& operator<< ( ostream &os, configItem_t &item )
{
    os << "group = " << item.group << ", module = " << item.module
       << ", name = " << item.name << ", value = " << item.value
       << ", type = " << item.type << endl;
    return os;
}


/* -------------------- << for configItemList_t -------------------- */

ostream& operator<< ( ostream &os, configItemList_t &list )
{
    configItemListIter_t iter;

    for (iter = list.begin(); iter != list.end(); iter++) {
        os << (*iter);
    }
    return os;
}


/* -------------------- << for configADItem_t -------------------- */

ostream& operator<< ( ostream &os, configADItem_t &item )
{
    
    if (item.type == "Host") {
        os << ((item.ad == ALLOW)?"ALLOW":"DENY") << " type = " << item.type
           << " value = " << item.value << ", addr = "
           << item.resolve_addr << endl;
    } else {
        os << ((item.ad == ALLOW)?"ALLOW":"DENY") << " type = " << item.type
           << " value = " << item.value << endl;
    }
    return os;
}


/* -------------------- << for configItemADList_t -------------------- */

ostream& operator<< ( ostream &os, configADList_t &list )
{
    configADListIter_t iter;
    
    for (iter = list.begin(); iter != list.end(); iter++) {
        os << (*iter);
    }
    return os;
}
