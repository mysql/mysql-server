/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_column_values.cc
  Literal values for columns used in the performance
  schema tables (implementation).
*/

#include "storage/perfschema/pfs_column_values.h"

LEX_CSTRING PERFORMANCE_SCHEMA_str = {STRING_WITH_LEN("performance_schema")};

LEX_CSTRING mutex_instrument_prefix = {STRING_WITH_LEN("wait/synch/mutex")};

LEX_CSTRING prlock_instrument_prefix = {STRING_WITH_LEN("wait/synch/prlock")};

LEX_CSTRING rwlock_instrument_prefix = {STRING_WITH_LEN("wait/synch/rwlock")};

LEX_CSTRING sxlock_instrument_prefix = {STRING_WITH_LEN("wait/synch/sxlock")};

LEX_CSTRING cond_instrument_prefix = {STRING_WITH_LEN("wait/synch/cond")};

LEX_CSTRING file_instrument_prefix = {STRING_WITH_LEN("wait/io/file")};

LEX_CSTRING table_io_class_name = {
    STRING_WITH_LEN("wait/io/table/sql/handler")};

LEX_CSTRING table_lock_class_name = {
    STRING_WITH_LEN("wait/lock/table/sql/handler")};

LEX_CSTRING socket_instrument_prefix = {STRING_WITH_LEN("wait/io/socket")};

LEX_CSTRING idle_class_name = {STRING_WITH_LEN("idle")};

LEX_CSTRING metadata_lock_class_name = {
    STRING_WITH_LEN("wait/lock/metadata/sql/mdl")};

LEX_CSTRING thread_instrument_prefix = {STRING_WITH_LEN("thread")};

LEX_CSTRING stage_instrument_prefix = {STRING_WITH_LEN("stage")};

LEX_CSTRING statement_instrument_prefix = {STRING_WITH_LEN("statement")};

LEX_CSTRING transaction_instrument_prefix = {STRING_WITH_LEN("transaction")};

LEX_CSTRING builtin_memory_instrument_prefix = {
    STRING_WITH_LEN("memory/performance_schema/")};

LEX_CSTRING memory_instrument_prefix = {STRING_WITH_LEN("memory")};

LEX_CSTRING error_class_name = {STRING_WITH_LEN("error")};
