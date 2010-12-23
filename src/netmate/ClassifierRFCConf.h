
/*! \file   ClassifierRFCConf.h

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
    configuration of RFC classifier used by Bitmap and ClassifierRFC 

    $Id: ClassifierRFCConf.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef _CLASSIFIER_RFC_CONF_H_
#define _CLASSIFIER_RFC_CONF_H_


/*! max number of rules supported (must be power of 2!) 
    multiply by 2 because each rule can result in 2 equiv classes (bidir rules)
 */
const unsigned short MAX_RULES = 1024 * 2;

//! max number of chunks (should be power of 2)
const unsigned short MAX_CHUNKS = 32;

//! max number of phases
const unsigned short MAX_PHASES = 6;

//! define this for faster incremental add but higher memory usage
#define PREALLOC_EQUIV_CLASSES
const unsigned PREALLOC_CLASSES = 32;

//! define this for remapping phase 0 chunks entries after rule delete
//#define REMAP_AFTER_DELETE

#endif // _CLASSIFIER_RFC_CONF_H_
