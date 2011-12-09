/*
   Copyright (C) 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef ROUTE_ORD_HPP
#define ROUTE_ORD_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * Request to allocate node id
 */
struct RouteOrd {
  STATIC_CONST( SignalLength = 4 );

  Uint32 dstRef;
  Uint32 srcRef;
  Uint32 gsn;
  Uint32 cnt;
};

#endif
