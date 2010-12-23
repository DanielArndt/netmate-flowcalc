
/*! \file   Bitmap.h

    bitmap implementation used by ClassifierRFC
    this is faster than the bitset stuff from STL (uses 64bit ints internally)
    FIXME not a class (yet)

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
    Bitmap implementation for Classifier RFC

    $Id: Bitmap.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _BITMAP_H_
#define _BITMAP_H_


#include "stdincpp.h"
#include "ClassifierRFCConf.h"


//! size of one element
const int BITMAP_ELEM_SIZE = sizeof(unsigned long long);

//! bits per element
const int BITS_PER_ELEM = BITMAP_ELEM_SIZE * 8;

//! bitmap size in bytes
const int BITMAP_SIZE = MAX_RULES / BITS_PER_ELEM;


//! msb is used to speed up the precomputation for small rulesets
typedef struct {
    //! index to first non-zero element  
    int msb;
    unsigned long long b[BITMAP_SIZE];  
} bitmap_t;


//! test if bit bpos is set in bm
int bmTest(bitmap_t *bm, unsigned long long bpos);

//! set bit bpos in bm
void bmSet(bitmap_t *bm, unsigned long long bpos);

//! reset bit bpos in bm
void bmReset(bitmap_t *bm, unsigned long long bpos);

//! set all bits in bm
void bmSet(bitmap_t *bm);

//! reset all bits in bm
void bmReset(bitmap_t *bm);

//! test whether bitmap is zero
int bmIsZero(const bitmap_t *bm);

//! and bm1 and bm2 and return the result in bmres
void bmAnd(const bitmap_t *bm1, const bitmap_t *bm2, bitmap_t *bmres);

//! print a bitmap without leading zeros
void bmPrint(const bitmap_t *bm1);

int bmCompare(const bitmap_t *b1, const bitmap_t *b2);

#endif // _BITMAP_H_
