/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef LOCAL_ROUTE_ORD_HPP
#define LOCAL_ROUTE_ORD_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 50

struct LocalRouteOrd {
  static constexpr Uint32 StaticLen = 3;
  /**
   * Paths (2 words each) and destinations (1 word each) must
   * fit in the signal body.  Assuming min of 1 path, can
   * have absolute max of 20 destinations.
   * Assuming min of 1 dst, can have absolute max path len
   * of 10.
   * Actual maxima depend on mix.
   */
  static constexpr Uint32 MaxDstCount = (25 - (StaticLen + 2));
  static constexpr Uint32 MaxPathLen = ((25 - (StaticLen + 1)) / 2);

  Uint32 cnt;   // 16-bit path, 16-bit destination
  Uint32 gsn;   // Final gsn
  Uint32 prio;  // Final prio
  Uint32 path[1];
};

#undef JAM_FILE_ID

#endif
