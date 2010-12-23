
/*  \file   ParserFcts.cc

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
functions for parsing string data into varies data formats

    $Id: ParserFcts.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "ParserFcts.h"
#include "Error.h"


long ParserFcts::parseLong(string s, long min, long max)
{
    char *errp = NULL;
    long n;

    n = strtol(s.c_str(), &errp, 0);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not a long number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %li", n);
    }

    return n;
}


unsigned long ParserFcts::parseULong(string s, unsigned long min, unsigned long max)
{
    char *errp = NULL;
    unsigned long n;

    n = strtoul(s.c_str(), &errp, 0);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not an unsigned long number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %ld", n);
    }

    return n;
}


long long ParserFcts::parseLLong(string s, long long min, 
                                 long long max)
{
    char *errp = NULL;
    long long n;

    n = strtoll(s.c_str(), &errp, 0);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not a long long number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %lli", n);
    }

    return n;
}


unsigned long long ParserFcts::parseULLong(string s, unsigned long long min, 
                                           unsigned long long max)
{
    char *errp = NULL;
    unsigned long long n;

    n = strtoull(s.c_str(), &errp, 0);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not an unsigned long long number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %lld", n);
    }

    return n;
}


int ParserFcts::parseInt(string s, int min, int max)
{
    char *errp = NULL;
    int n;

    n = (int)strtol(s.c_str(), &errp, 0);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not an int number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %i", n);
    }

    return n;
}

int isNumericIPv4(string s)
{
    return (s.find_first_not_of("0123456789.", 0) >= s.length());  
}

struct in_addr ParserFcts::parseIPAddr(string s)
{
    int rc;
    struct in_addr a;
    struct addrinfo ask, *res = NULL;
   
    memset(&ask,0,sizeof(ask));
    ask.ai_socktype = SOCK_STREAM;
    ask.ai_flags = 0;
    if (isNumericIPv4(s)) {
        ask.ai_flags |= AI_NUMERICHOST;
    }
    ask.ai_family = PF_INET;

    // set timeout
    g_timeout = 0;
    alarm(2);

    rc = getaddrinfo(s.c_str(), NULL, &ask, &res);

    alarm(0);

    try {
        if (g_timeout) {
            throw Error("DNS timeout: %s", s.c_str());
        }

        if (rc == 0) {
            // take first address only, in case of multiple addresses fill addresses
            // FIXME set match
            a = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
            freeaddrinfo(res);
        } else {
            throw Error("Invalid or unresolvable ip address: %s", s.c_str());
        }
    } catch (Error &e) {
        freeaddrinfo(res);
        throw e;
    }

    return a;
}

int isNumericIPv6(string s)
{
    return (s.find_first_not_of("0123456789abcdefABCDEF:.", 0) >= s.length());
}

struct in6_addr ParserFcts::parseIP6Addr(string s)
{
    int rc;
    struct in6_addr a;
    struct addrinfo ask, *res = NULL;
   
    memset(&ask,0,sizeof(ask));
    ask.ai_socktype = SOCK_STREAM;
    ask.ai_flags = 0;
    if (isNumericIPv6(s)) {
        ask.ai_flags |= AI_NUMERICHOST;
    }
    ask.ai_family = PF_INET6;

    // set timeout
    g_timeout = 0;
    alarm(2);

    rc = getaddrinfo(s.c_str(), NULL, &ask, &res);

    alarm(0);

    try {
        if (g_timeout) {
            throw Error("DNS timeout: %s", s.c_str());
        }

        if (rc == 0) {  
            // take first address only, in case of multiple addresses fill addresses
            // FIXME set match
            a = ((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
            freeaddrinfo(res);
        } else {
            throw Error("Invalid or unresolvable ip6 address: %s", s.c_str());
        }
    } catch (Error &e) {
        freeaddrinfo(res);
        throw e;
    }
    
    return a;
}

int ParserFcts::parseBool(string s)
{
    if ((s == "yes") || (s == "1") || (s == "true")) {
        return 1;
    } else if ((s == "no") || (s == "0") || (s == "false")) {
        return 0;
    } else {
        throw Error("Invalid bool value: %s", s.c_str());
    }
}


float ParserFcts::parseFloat(string s, float min, float max)
{
    char *errp = NULL;
    float n;

    n = strtof(s.c_str(), &errp);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not a float number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %f", n);
    }

    return n;
}


double ParserFcts::parseDouble(string s, double min, double max)
{
    char *errp = NULL;
    double n;

    n = strtod(s.c_str(), &errp);

    if (s.empty() || (*errp != '\0')) {
        throw Error("Not a double number: %s", errp);
    }
    if ((n<min) && (n>max)) {
        throw Error("Out of range: %f", n);
    }

    return n;
}


// parse config items
void ParserFcts::parseItem(string type, string value)
{

    if ((type == "UInt8") || (type == "SInt8")) {
        ParserFcts::parseULong(value);
    } else if ((type == "UInt16") || (type == "SInt16")) {
        ParserFcts::parseLong(value);
    } else if ((type == "UInt32") || (type == "SInt32")) {
        ParserFcts::parseULong(value);
    } else if (type == "IPAddr") {
        ParserFcts::parseIPAddr(value);
    } else if (type == "IP6Addr") {
        ParserFcts::parseIP6Addr(value);
    } else if (type == "String") {
        // always ok
    } else if (type == "Bool") {
        ParserFcts::parseBool(value);
    } else if (type == "Float32") {
        ParserFcts::parseFloat(value);
    } else if (type == "Float64") {
        ParserFcts::parseDouble(value);
    } else {
        throw Error("Unsupported type: %s", type.c_str());
    }
}
