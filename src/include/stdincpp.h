
/*!\file stdincpp.h

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
    Include this file (stdincpp) in every .h file.\n
    This .h file includes all the C++ standard include
    files such as  map, string and others

    $Id: stdincpp.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef __STDINCPP_H
#define __STDINCPP_H

extern "C" {
#include "stdinc.h"
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 0))
// dont need include
#else
#include <multimap.h>
#endif
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 0))
#include <ext/hash_map>
using namespace __gnu_cxx;
#else
#include <hash_map.h>
#endif
#include <list>
#include <algorithm>
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 0))
#include <locale>
#endif
#include <cctype>
#include <memory>
#include <set>
#include <bitset>
#include <iomanip>
#include <streambuf>

using namespace std;

// definition of 'safeDelete' and 'safeDeleteArr' as 
// a safer replacement for delete and delete []

// Put an assert to check if x is NULL, this is to catch
// program "logic" errors early. Even though delete works 
// fine with NULL by using assert you are actually catching 
// "bad code" very early
// Defining saveDelete using templates
template <class T>
inline void saveDelete(T& x) {
     assert(x != NULL);
     delete x;
     x = NULL;
}
template <class T*>
inline void saveDelete(T*& x) {
     assert(x != NULL);
     delete x;
     x = NULL;
}
// For delete array 
template <class T>
inline void saveDeleteArr(T& x) {
     assert(x != NULL);
     delete [] x;
     x = NULL;
}

// tolower (string)
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 0))
struct ToLower {
   
  private:
    std::locale loc;

  public:
    ToLower () 
        : loc(std::locale("C")) {;}
    
    char operator() (char c)  { return std::tolower(c,loc); }
 
};
#else 
struct ToLower {
    ToLower () {;}
    char operator() (char c)  { return std::tolower(c); }
  private:
  
};
#endif

#endif // __STDINCPP_H
