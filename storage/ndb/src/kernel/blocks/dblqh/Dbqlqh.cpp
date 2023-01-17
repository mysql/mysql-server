/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBLQH_C
#include "Dbqlqh.hpp"

#include <EventLogger.hpp>

#define JAM_FILE_ID 518

Dbqlqh::Dbqlqh(Block_context& ctx,
               Uint32 instanceNumber):
  Dblqh(ctx, instanceNumber, DBQLQH)
{
}

Uint64 Dbqlqh::getTransactionMemoryNeed()
{
  Uint32 query_instance_count =
    globalData.ndbMtQueryThreads +
    globalData.ndbMtRecoverThreads;
  Uint32 lqh_scan_recs = 1;
  Uint32 lqh_op_recs = 1;
  Uint64 scan_byte_count = 0;
  scan_byte_count += ScanRecord_pool::getMemoryNeed(lqh_scan_recs);
  scan_byte_count *= query_instance_count;

  Uint64 op_byte_count = 0;
  op_byte_count += TcConnectionrec_pool::getMemoryNeed(lqh_op_recs);
  op_byte_count *= query_instance_count;

  Uint32 lqh_commit_ack_markers = 1;
  Uint64 commit_ack_marker_byte_count = 0;
  commit_ack_marker_byte_count +=
    CommitAckMarker_pool::getMemoryNeed(lqh_commit_ack_markers);
  commit_ack_marker_byte_count *= query_instance_count;
  return (op_byte_count + scan_byte_count + commit_ack_marker_byte_count);
}


Dbqlqh::~Dbqlqh()
{
}
