/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#define DBQTUX_C
#include "Dbqtux.hpp"

#include <EventLogger.hpp>

#define JAM_FILE_ID 530

Dbqtux::Dbqtux(Block_context &ctx, Uint32 instanceNumber)
    : Dbtux(ctx, instanceNumber, DBQTUX) {}

Uint64 Dbqtux::getTransactionMemoryNeed() {
  Uint32 query_instance_count =
      globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads;
  Uint64 scan_op_byte_count = 1;
  Uint32 tux_scan_recs = 1;
  Uint32 tux_scan_lock_recs = 1;

  scan_op_byte_count += ScanOp_pool::getMemoryNeed(tux_scan_recs);
  scan_op_byte_count *= query_instance_count;

  Uint64 scan_lock_byte_count = 0;
  scan_lock_byte_count += ScanLock_pool::getMemoryNeed(tux_scan_lock_recs);
  scan_lock_byte_count *= query_instance_count;

  const Uint32 nScanBoundWords = tux_scan_recs * ScanBoundSegmentSize * 4;
  Uint64 scan_bound_byte_count = nScanBoundWords * query_instance_count;

  return (scan_op_byte_count + scan_lock_byte_count + scan_bound_byte_count);
}

Dbqtux::~Dbqtux() {}
