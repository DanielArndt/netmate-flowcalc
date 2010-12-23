
/*! \file   Sampler.h

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
    abstract base class for all samplers

    $Id: Sampler.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _SAMPLER_H_
#define _SAMPLER_H_


#include "stdincpp.h"
#include "metadata.h"
#include "Error.h"


class Sampler
{

 public:

  Sampler();

  virtual ~Sampler();

  /*! flag wether packet is sampled
      \returns 1 if sampled 0 otherwise */
  virtual int sample(metaData_t* pkt) = 0;

};

#endif // _SAMPLER_H_
