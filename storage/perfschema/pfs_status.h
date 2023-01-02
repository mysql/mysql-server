/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef PFS_STATUS_H
#define PFS_STATUS_H

/**
  @file storage/perfschema/pfs_status.h
  Status variables statistics (declarations).
*/

#include "my_inttypes.h"
#include "sql/system_variables.h"  // COUNT_GLOBAL_STATUS_VARS

struct PFS_status_stats {
  PFS_status_stats();

  void reset();
  void aggregate(const PFS_status_stats *from);
  void aggregate_from(const System_status_var *from);
  void aggregate_to(System_status_var *to);

  bool m_has_stats;
  ulonglong m_stats[COUNT_GLOBAL_STATUS_VARS];
};

void reset_status_by_thread();
void reset_status_by_account();
void reset_status_by_user();
void reset_status_by_host();
void reset_global_status();

#endif
