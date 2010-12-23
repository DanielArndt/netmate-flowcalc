
/*  \file   FilterValue.cc

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
    Class used to store filter/match values with
    flexible length in network byteorder

    $Id: FilterValue.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "FilterValue.h"
#include "Error.h"
#include "ParserFcts.h"


FilterValue::FilterValue(string type, string value)
    : ftype(type), len(0)
{
   
    if (type == "UInt8") {
        setValue((unsigned char) ParserFcts::parseULong(value));
    } else if (type == "SInt8") {
        setValue((char) ParserFcts::parseULong(value));
    } else if (type == "UInt16") {
        setValue((unsigned short) ParserFcts::parseULong(value));
    } else if (type == "SInt16") {
        setValue((short) ParserFcts::parseLong(value));
    } else if (type == "UInt32") {
        setValue((unsigned long) ParserFcts::parseULong(value));
    } else if (type == "SInt32") {
        setValue((long) ParserFcts::parseULong(value));
    } else if (type == "IPAddr") {
        setValue(ParserFcts::parseIPAddr(value));
    } else if (type == "IP6Addr") {
        setValue(ParserFcts::parseIP6Addr(value));
    } else if (type == "Binary") {
        setValue(value.length(), value.c_str());
    } else if (type == "String") {
        setValue(value.c_str());
    } else {
        throw Error("Unsupported type for filter value: %s", type.c_str());
    }
}
   
int FilterValue::getTypeLength(string type)
{
    if ((type == "UInt8") || (type == "SInt8")) {
        return 1;
    } else if ((type == "UInt16") || (type == "SInt16")) {
        return 2;
    } else if ((type == "UInt32") || (type == "SInt32")) {
        return 4;
    } else if (type == "IPAddr") {
        return 4;
    } else if (type == "IP6Addr") {
        return 16;
    } else if (type == "Binary") {
        return 0;
    } else if (type == "String") {
        return 0;
    } else {
        throw Error("Unsupported type for filter value: %s", type.c_str());
    }
}

string FilterValue::getString()
{
    ostringstream s;
    char buf[64];
    
    if (len > 0) {
 
        if ((ftype == "UInt8") || (ftype =="Int8")) {
            s << (int) val[0];
        } else if (ftype == "UInt16") {
            s << ntohs(*((unsigned short *) val));
        } else if (ftype == "Int16") {
            s << ntohs(*((short *) val));
        } else if (ftype == "Uint32") {
            s << ntohl(*((unsigned long *) val));
        } else if (ftype == "Int32") {
            s << ntohl(*((long *) val));
        } else if (ftype == "IPAddr") {
            inet_ntop(AF_INET, val, buf, 64);
            s << buf;
        } else if (ftype == "IP6Addr") {
            inet_ntop(AF_INET6, val, buf, 64);
            s << buf;
        } else if (ftype == "Binary") {
            s << "0x" << hex << setfill('0');
            for (int i=0; i < len; i++) {
                s << setw(2) << (int) val[i];
            }
        }
      
    }
    
    return s.str();
}
