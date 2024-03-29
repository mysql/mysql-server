/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DBLQH_COMMON_H
#define DBLQH_COMMON_H

#include <pc.hpp>
#include <ndb_types.h>
#include <Bitmask.hpp>

#define JAM_FILE_ID 443


/*
 * Log part id is from DBDIH.  Number of log parts is configurable with a
 * maximum setting and minimum of 4 parts. The below description assumes
 * 4 parts.
 *
 * A log part is identified by log part number (0-3)
 *
 *   log part number = log part id % 4
 *
 * This may change, and the code (except this file) must not assume
 * any connection between log part number and instance key.
 *
 * Following structure computes log part info for a specific LQH
 * instance (main instance 0 or worker instances 1-4).
 */
struct NdbLogPartInfo {
  Uint32 LogParts;
  NdbLogPartInfo(Uint32 instanceNo);
  Uint32 lqhWorkers;
  Uint32 partCount;
  Uint16 partNo[NDB_MAX_LOG_PARTS];
  Bitmask<(NDB_MAX_LOG_PARTS+31)/32> partMask;
  Uint32 partNoFromId(Uint32 lpid) const;
  bool partNoOwner(Uint32 lpno) const;
  Uint32 partNoIndex(Uint32 lpno) const;
};


#undef JAM_FILE_ID

#endif
