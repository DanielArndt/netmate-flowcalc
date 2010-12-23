
/*!\file   RuleIdSource.cc

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
manage unique numeric rule id space

    $Id: RuleIdSource.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "RuleIdSource.h"



RuleIdSource::RuleIdSource(int _unique)
  : num(0), unique(_unique)
{
   
}


RuleIdSource::~RuleIdSource()
{
    // nothing to do
}


unsigned long long RuleIdSource::newId(void)
{
    unsigned long long id;
   
    if (freeIds.empty()) {
        return num++;
    }

    // else use id from free list
    id = freeIds.front();
    freeIds.pop_front();

    return id;
}


void RuleIdSource::freeId(unsigned long long id)
{
  if (!unique) {
    freeIds.push_back(id);
    num--;
  }
}


void RuleIdSource::dump( ostream &os )
{
    os << "RuleIdSource dump:" << endl
       << "Number of used ids is : " << num - freeIds.size() << endl;
}


ostream& operator<< ( ostream &os, RuleIdSource &rim )
{
    rim.dump(os);
    return os;
}
