
/*!\file   Bitmap.cc

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
    This is faster than STL bitset because it uses 64bit integer plus
    some optimizations for bitsets where only the low bits are set

    $Id: Bitmap.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "Bitmap.h"


//! test if bit bpos is set in bm
int bmTest(bitmap_t *bm, unsigned long long bpos)
{
    return ( ((bm->b)[bpos / BITS_PER_ELEM] & (1ULL << (bpos % BITS_PER_ELEM))) > 0ULL);
}


//! set bit bpos in bm
void bmSet(bitmap_t *bm, unsigned long long bpos)
{
    if ((bpos / BITS_PER_ELEM) > (unsigned long long)bm->msb) {
    	bm->msb = bpos / BITS_PER_ELEM;
    }
    (bm->b)[bpos / BITS_PER_ELEM] |= (1ULL << ((bpos % BITS_PER_ELEM)));
}


//! reset bit bpos in bm
void bmReset(bitmap_t *bm, unsigned long long bpos)
{
    (bm->b)[bpos / BITS_PER_ELEM] &= ~(1ULL << (bpos % BITS_PER_ELEM));
    while ((bm->msb > 0) && ((bm->b)[bm->msb] == 0ULL)) {
    	bm->msb -= 1;
    }
}


//! set all bits in bm
void bmSet(bitmap_t *bm)
{
    bm->msb = BITMAP_SIZE-1;
    memset((bm->b), 0xFF, BITMAP_SIZE * BITMAP_ELEM_SIZE);
}


//! reset all bits in bm
void bmReset(bitmap_t *bm)
{
    bm->msb = 0;
    memset((bm->b), 0, BITMAP_SIZE * BITMAP_ELEM_SIZE);
}


//! test whether bitmap is zero
int bmIsZero(const bitmap_t *bm)
{
    if ((bm->msb == 0) && ((bm->b)[0] == 0ULL)) {
    	return 1;
    } else {
    	return 0;
    }
} 


//! and bm1 and bm2 and return the result in bmres
void bmAnd(const bitmap_t *bm1, const bitmap_t *bm2, bitmap_t *bmres)
{
    int min = (bm1->msb < bm2->msb) ? bm1->msb : bm2->msb;

    bmReset(bmres);

    for (int i = min; i >= 0; i--) {
        (bmres->b)[i] = (bm1->b)[i] & (bm2->b)[i];
        if ((bmres->msb == 0) && (bmres->b[i] != 0ULL)) {
            bmres->msb = i;
        } 
    }
}


//! print a bitmap without leading zeros
void bmPrint(const bitmap_t *bm1)
{
    int seen1 = 0;
    int bit   = 0;

    for (int i = bm1->msb; i >= 0; i--) {
    	for (int j = BITS_PER_ELEM-1; j >= 0; j--) {
    	    bit = ( ((bm1->b)[i] & (1ULL << j)) > 0ULL);
    	    seen1 |= bit;
    	    
    	    if (seen1) {
	        printf("%d", bit);
    	    }
    	} 
    }
    if (!seen1) {
	   printf("0");
    }
}

//! return 1 (b1 < b2), 0 (b1 == b2) or -1 (b2 > b1)
int bmCompare(const bitmap_t *b1, const bitmap_t *b2)
{
    if (b1->msb != b2->msb) {
        return (b1->msb < b2->msb);
    }
    
    for (int i = b1->msb; i >= 0; i--) {
        if (b1->b[i] < b2->b[i]) {
            return 1;
        } else if (b1->b[i] > b2->b[i]) {
                return 0;
        }
    }
    return 0;
}
