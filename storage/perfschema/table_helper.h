/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_TABLE_HELPER_H
#define PFS_TABLE_HELPER_H

/**
  @file storage/perfschema/table_helper.h
  Performance schema table helpers (declarations).
*/

/**
  @addtogroup Performance_schema_tables
  @{
*/

struct PFS_instrument_view_constants
{
  static const uint FIRST_VIEW= 1;
  static const uint VIEW_MUTEX= 1;
  static const uint VIEW_RWLOCK= 2;
  static const uint VIEW_COND= 3;
  static const uint VIEW_FILE= 4;
  static const uint VIEW_TABLE= 5;
  static const uint LAST_VIEW= 5;
};

struct PFS_object_view_constants
{
  static const uint FIRST_VIEW= 1;
  static const uint VIEW_TABLE= 1;
  static const uint LAST_VIEW= 1;

  /* Future use */
  static const uint VIEW_EVENT= 2;
  static const uint VIEW_PROCEDURE= 3;
  static const uint VIEW_FUNCTION= 4;
};

/** @} */

#endif

