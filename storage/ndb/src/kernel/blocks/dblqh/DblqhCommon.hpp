/* Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef DBLQH_COMMON_H
#define DBLQH_COMMON_H

#include <pc.hpp>
#include <ndb_types.h>
#include <Bitmask.hpp>

/*
 * Log part id is from DBDIH.  Number of log parts is fixed as 4.
 * A log part is identified by log part number (0-3)
 *
 *   log part number = log part id % 4
 *
 * Currently instance key (1-4) is
 *
 *   instance key = 1 + log part number
 *
 * This may change, and the code (except this file) must not assume
 * any connection between log part number and instance key.
 *
 * Following structure computes log part info for a specific LQH
 * instance (main instance 0 or worker instances 1-4).
 */
struct NdbLogPartInfo {
  enum { LogParts = 4 };
  NdbLogPartInfo(Uint32 instanceNo);
  Uint32 lqhWorkers;
  Uint32 partCount;
  Uint16 partNo[LogParts];
  Bitmask<(LogParts+31)/32> partMask;
  Uint32 partNoFromId(Uint32 lpid) const;
  bool partNoOwner(Uint32 lpno) const;
  bool partNoOwner(Uint32 tabId, Uint32 fragId);
  Uint32 partNoIndex(Uint32 lpno) const;
  Uint32 instanceKey(Uint32 lpno) const;
};

#endif
