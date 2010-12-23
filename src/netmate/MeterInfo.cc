
/*! \file MeterInfo.cc

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
    meter info data structs used in getinfo meter command

    $Id: MeterInfo.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "MeterInfo.h"


char *MeterInfo::INFOS[] = { "all", 
                             "version",
                             "uptime",
                             "tasks_stored",
                             "modules_loaded",
                             "configfile",
                             "use_ssl",
                             "hello",
                             "tasklist",
                             "modlist",
                             "task" };

typeMap_t MeterInfo::typeMap; //std::map< string, infoType_t >();


MeterInfo::MeterInfo() : list(NULL)
{
    // init static type map for name->type lookup via getInfoType()

    if (typeMap.empty()) {  // only fill map first time here
        for (int i = 0; i < I_NUMMETERINFOS; i++ ) {
            typeMap[INFOS[i]] = (infoType_t)i;
        } 
    }
}


MeterInfo::~MeterInfo()
{
    if (list != NULL) {
        saveDelete(list);
    }
}


string MeterInfo::getInfoString( infoType_t info )
{
    if (info < 0 || info >= I_NUMMETERINFOS ) {
        return "";
    } else {
        return INFOS[info];
    }
}


infoType_t MeterInfo::getInfoType( string item )
{
    typeMapIter_t i = typeMap.find(item);

    if (i != typeMap.end()) {  // item found
        return i->second; /* type */
    } else {
        return I_UNKNOWN;
    }
}


void MeterInfo::addInfo( infoType_t type, string param )
{
    if (list == NULL) {
        list = new infoList_t();
    }

    list->push_back(info_t(type, param));
}


void MeterInfo::addInfo( string item, string param )
{
    infoType_t type = getInfoType(item);

    if (type == I_UNKNOWN) {  // no such info item
        throw Error("Unknown meter info '%s'", item.c_str());
    }

    switch (type) {
    case I_ALL:
        addInfo(I_HELLO);
        addInfo(I_UPTIME);
        addInfo(I_TASKS_STORED);
        addInfo(I_FUNCTIONS_LOADED);
        addInfo(I_CONFIGFILE);
        addInfo(I_USE_SSL);
        addInfo(I_TASKLIST);
        addInfo(I_MODLIST);
        break;
    case I_TASK:
        addInfo(I_TASK, param );
        break;
    default: 
        addInfo( type );
        break;
    }

}
