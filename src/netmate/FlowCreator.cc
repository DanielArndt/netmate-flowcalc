
/*! \file   FlowCreator.cc

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
    create flows based on flow key

    $Id: FlowCreator.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "FlowCreator.h"


FlowCreator::FlowCreator()
{
    flows.resize(START_BUCKETS);
    idSource = RuleIdSource(1);
}

FlowCreator::~FlowCreator()
{
    flowListIter_t tmp;

    flowListIter_t i = flows.begin();    
    while (i != flows.end()) {
        tmp = i;
        i++;
        deleteFlow(tmp);
    }
}

void FlowCreator::deleteFlow(flowListIter_t f)
{
    unsigned char *tmp;
    
    idSource.freeId(f->second.flowId);
    tmp = f->first.keyData;
    flows.erase(f);
    saveDeleteArr(tmp);
}

void FlowCreator::deleteFlow(flowInfo_t *fi)
{
  hkey_t k;
  unsigned char *tmp;

  idSource.freeId(fi->flowId);
  tmp = fi->keyData;
  k.len = fi->len;
  k.keyData = fi->keyData;
  flows.erase(k);
  saveDeleteArr(tmp);
}

flowInfo_t *FlowCreator::addFlow(const unsigned char *keyData, unsigned short len)
{
    hkey_t k;
    flowInfo_t entry;

    k.keyData = new unsigned char[len];
    memcpy(k.keyData, keyData, len);
    entry.keyData = k.keyData;
    entry.len = len;
    entry.flowId = idSource.newId();
    entry.newFlow = 1;
    k.len = len;

    pair<flowListIter_t, bool> res = flows.insert(make_pair(k, entry));

    return &res.first->second;
}

flowInfo_t *FlowCreator::getFlow(const unsigned char *keyData, unsigned short len)
{
    hkey_t k;
    flowListIter_t f;

    k.len = len;
    k.keyData = (unsigned char *) keyData;

    f = flows.find(k);

    if (f == flows.end()) {
        return NULL;
    }
    
    return &f->second;
}
