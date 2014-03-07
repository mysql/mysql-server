/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ATTRIBUTE_LIST_HPP
#define ATTRIBUTE_LIST_HPP

#include "ndb_limits.h"

#define JAM_FILE_ID 4


/**
 * Masks and lists used by index and trigger.  Must be plain old Uint32 data.
 * XXX depends on other headers XXX move to some common file
 */

typedef Bitmask<MAXNROFATTRIBUTESINWORDS> AttributeMask;
typedef BitmaskPOD<MAXNROFATTRIBUTESINWORDS> AttributeMaskPOD;

template <Uint32 SZ>
struct Id_array
{
  Uint32 sz;
  Uint32 id[SZ];
};

typedef Id_array<MAX_ATTRIBUTES_IN_INDEX> IndexAttributeList;


#undef JAM_FILE_ID

#endif
