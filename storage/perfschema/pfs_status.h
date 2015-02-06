/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_STATUS_H
#define PFS_STATUS_H

/**
  @file storage/perfschema/pfs_status.h
  Status variables statistics (declarations).
*/

struct PFS_status_stats
{
  PFS_status_stats();

  void reset();
  void aggregate(const PFS_status_stats *from);
  void aggregate_from(const STATUS_VAR *from);
  void aggregate_to(STATUS_VAR *to);

  bool m_has_stats;
  ulonglong m_stats[COUNT_GLOBAL_STATUS_VARS];
};

void reset_status_by_thread();
void reset_status_by_account();
void reset_status_by_user();
void reset_status_by_host();
void reset_global_status();

#endif

