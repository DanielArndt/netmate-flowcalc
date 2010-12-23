
/*  \file   RuleFileParser.h

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
    parse rule files

    $Id: RuleFileParser.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _RULEFILEPARSER_H_
#define _RULEFILEPARSER_H_


#include "stdincpp.h"
#include "libxml/parser.h"
#include "Logger.h"
#include "XMLParser.h"
#include "Rule.h"
#include "ConfigParser.h"
#include "FilterDefParser.h"
#include "FilterValParser.h"


//! rule list
typedef vector<Rule*>            ruleDB_t;
typedef vector<Rule*>::iterator  ruleDBIter_t;

class RuleFileParser : public XMLParser
{
  private:

    Logger *log;
    int ch;

    //! parse a config item
    configItem_t parsePref(xmlNodePtr cur);

    //! lookup filter value
    string lookup(filterValList_t *filterVals, string fvalue, filter_t *f);
    //! parse a filter value
    void parseFilterValue(filterValList_t *filterVals, string value, filter_t *f);

  public:

    RuleFileParser(string fname);

    RuleFileParser(char *buf, int len);

    virtual ~RuleFileParser() {}

    //! parse rules and add parsed rules to the list of rules
    virtual void parse(filterDefList_t *filters, filterValList_t *filterVals, ruleDB_t *rules);
};

#endif // _RULEFILEPARSER_H_
