
/*  \file   FilterValue.h

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

    $Id: FilterValue.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef _FILTERVALUE_H_
#define _FILTERVALUE_H_


#include "stdincpp.h"
#include "Error.h"


//! maximum length of a filter value
const unsigned short MAX_FILTER_LEN = 32;

/* \short FilterValue

   This class stores filter values in an array. If filter values are
   set from numeric types like integers they are converted into network
   byte order
*/

class FilterValue
{
  private:

    string ftype;
    unsigned short len;
    unsigned char val[MAX_FILTER_LEN];

    void setValue(unsigned char x)   
    {
        len = sizeof(x);
        val[0] = x;
    }

    void setValue(char x)   
    {
        len = sizeof(x);
        val[0] = x;
    }

    void setValue(unsigned short x)     
    {
        len = sizeof(x);
        x = htons(x);
        memcpy(val, &x, len);
    }

    void setValue(short x)     
    {
        len = sizeof(x);
        x = htons(x);
        memcpy(val, &x, len);
    }
    
    void setValue(unsigned long x)
    {
        len = sizeof(x);
        x = htonl(x);
        memcpy(val, &x, len);
    }

    void setValue(struct in_addr x)
    {
        len = sizeof(x);
        memcpy(val, &x, len);
    }

    void setValue(struct in6_addr x)
    {
        len = sizeof(x.s6_addr);
        memcpy(val, &x.s6_addr, len);
    }

    void setValue(long x)
    {
        len = sizeof(x);
        x = htonl(x);
        memcpy(val, &x, len);
    }

    void setValue(unsigned short l, const char *x)
      {
          // x is a string
          char buf[5] = "0x00";

          if (strncmp(x, "0x", 2)) {
              throw Error("char array must be specified in hex notation");
          }

          // convert 2 bytes from string into 1 number byte
          if ((l-2)/2 <= MAX_FILTER_LEN) {
              for (int i=1; i <= (l-2)/2; i++) {
                  memcpy(&buf[2], &x[i*2], 2);
                  val[i-1] = strtol(buf, NULL, 16);
              }

              len = (l-2)/2;
          } else {
              throw Error("max filter length exceeded");
          }
      }

    void setValue(const char *x) 
      {
          if (strlen(x) <= MAX_FILTER_LEN) {
              len = strlen(x);
              strncpy((char *) val, x, len);
          } else {
              throw Error("max filter length exceeded");
          }
      }
    
  public:

    FilterValue() : len(0) {}

    FilterValue(string type, string value);
    
    // get value as string
    string getString();
     
    // get value length
    int getLen()
    {
        return len;
    }

    // get access to the value
    unsigned char *getValue()
    {
        return val;
    }

    //! get length by type (for all fixed types)
    static int getTypeLength(string type);

    // FIXME nice to have operators
    //FilterValue &operator&(const FilterValue v&);
    //bool operator==(const FilterValue &v1, const FilterValue &v2);
    //FilterValue &operator=(const FilterValue &v);
    
};


#endif // _FILTERVALUE_H_
