
/*  \file   XMLParser.cc

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
parse XML files, base class for specific parsers

    $Id: XMLParser.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "XMLParser.h"

string XMLParser::err, XMLParser::warn;

XMLParser::XMLParser(string dtdname, string fname, string root)
    : fileName(fname), dtdName(dtdname), XMLDoc(NULL), ns(NULL)
{

    log = Logger::getInstance();
    ch = log->createChannel("XMLParser" );

#ifdef DEBUG
    log->dlog(ch, "DTD file %s, XML file %s, root %s", 
                dtdname.c_str(), fname.c_str(), root.c_str() );
#endif
    
    try {
        struct stat statbuf;

        if (stat( fileName.c_str(), &statbuf ) == -1 ) {
            throw Error("XML %s file '%s' not accessible: %s",
                        root.c_str(), fileName.c_str(), strerror(errno) );
        }

        if (stat( dtdName.c_str(), &statbuf ) == -1 ) {
            throw Error("DTD %s file '%s' not accessible: %s",
                        root.c_str(), dtdName.c_str(), strerror(errno) );
        }

        xmlInitParser();

        // set line numbering
        xmlLineNumbersDefault(1);
        // set error function
        xmlSetGenericErrorFunc(NULL, XMLParser::XMLErrorCB);

        XMLDoc = xmlParseFile(fileName.c_str()); 	    
        if (XMLDoc == NULL) {
            throw Error("XML document parse error in file %s", fileName.c_str());
        }

        validate(root);

    } catch (Error &e) {
        if (XMLDoc != NULL) {
            xmlFreeDoc(XMLDoc);
            XMLDoc = NULL;
        }

        if (!warn.empty()) {
            log->wlog(ch, "%s", warn.c_str());
        }
        
        if (!err.empty()) {
            log->elog(ch, "%s", err.c_str());
        }

        throw(e);
    }
    
}

XMLParser::XMLParser(string dtdname, char *buf, int len, string root)
    : dtdName(dtdname)
{
    

    log = Logger::getInstance();
    ch = log->createChannel("XMLParser" );

    try {

        xmlInitParser();

        xmlSetGenericErrorFunc(NULL, XMLParser::XMLErrorCB);

        XMLDoc = xmlParseMemory(buf, len); 	    
	
        if (XMLDoc == NULL) {
            throw Error("XML document parse error");
        }

        validate(root);
	
    } catch (Error &e) {
        if (XMLDoc != NULL) {
            xmlFreeDoc(XMLDoc);
            XMLDoc = NULL;
        }

        if (!warn.empty()) {
            log->wlog(ch, "%s", warn.c_str());
        }

        if (!err.empty()) {
            log->elog(ch, "%s", err.c_str());
        }

        throw(e);
    }
}

void XMLParser::validate(string root)
{
    xmlNodePtr cur = NULL;
    xmlDtdPtr dtd = NULL;
    xmlValidCtxt cvp;

    try {

        dtd = xmlParseDTD(NULL, (const xmlChar *) dtdName.c_str());
	
        if (dtd == NULL) {
            throw Error("Could not parse DTD %s", dtdName.c_str());
        } else {
            memset(&cvp, 0, sizeof(cvp));
            cvp.userData = this;
            cvp.error = (xmlValidityErrorFunc) XMLParser::XMLErrorCB;
            cvp.warning = (xmlValidityWarningFunc) XMLParser::XMLWarningCB;
        
            if (!xmlValidateDtd(&cvp, XMLDoc, dtd)) {
                throw Error("Document %s does not validate against %s",
                            fileName.c_str(), dtdName.c_str());
            }
	    
            cur = xmlDocGetRootElement(XMLDoc);
            if (cur == NULL) {
                throw Error("empty XML document");
            }
            if (xmlStrcmp(cur->name, (const xmlChar *) root.c_str())) {
                throw Error("document of the wrong type, root node = %s", cur->name);
            }

            ns = xmlSearchNsByHref(XMLDoc,cur,NULL);
       
            // add as external subset
            XMLDoc->extSubset = dtd;
        }
	
    } catch (Error &e) {
        if (ns != NULL) {
            xmlFreeNs(ns);
            ns = NULL;
        }
        if (dtd != NULL) {
            xmlFreeDtd(dtd);
            dtd = NULL;
            ns = NULL;
        }
        if (XMLDoc != NULL) {
            xmlFreeDoc(XMLDoc);
            XMLDoc = NULL;
        }

        if (!warn.empty()) {
            log->wlog(ch, "%s", warn.c_str());
        }

        if (!err.empty()) {
            log->elog(ch, "%s", err.c_str());
        }
        
        throw(e);
    }
}

XMLParser::~XMLParser()
{

    if (ns != NULL) {
        xmlFreeNs(ns);
    }

    if  (XMLDoc->extSubset != NULL) {
        xmlFreeDtd(XMLDoc->extSubset);
        XMLDoc->extSubset = NULL;
    }

    if (XMLDoc != NULL) {
        xmlFreeDoc(XMLDoc);
    }

    xmlCleanupParser();
}

void XMLParser::XMLErrorCB(void *ctx, const char *msg, ...)
{
    char buf[8096];
    va_list argp;

    va_start( argp, msg );
    vsprintf(buf, msg, argp);
    vfprintf(stderr, msg, argp);
    va_end( argp );

    err += buf;
}

void XMLParser::XMLWarningCB(void *ctx, const char *msg, ...)
{
    char buf[8096];
    va_list argp;

    va_start( argp, msg );
    vsprintf(buf, msg, argp); 
    vfprintf(stderr, msg, argp);
    va_end( argp );

    warn += buf;
}

string XMLParser::xmlCharToString(xmlChar *in)
{
    string out = "";

    if (in != NULL) {
        out = (char *) in;
        xmlFree(in);
    }

    return out;
}
