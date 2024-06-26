/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_COLUMN_VALUES_H
#define PFS_COLUMN_VALUES_H

#include "lex_string.h"

/**
  @file storage/perfschema/pfs_column_values.h
  Literal values for columns used in the
  performance schema tables (declarations).
*/

/** String, "PERFORMANCE_SCHEMA". */
extern LEX_CSTRING PERFORMANCE_SCHEMA_str;

/** String prefix for all mutex instruments. */
extern LEX_CSTRING mutex_instrument_prefix;
/** String prefix for all prlock instruments. */
extern LEX_CSTRING prlock_instrument_prefix;
/** String prefix for all rwlock instruments. */
extern LEX_CSTRING rwlock_instrument_prefix;
/** String prefix for all sxlock instruments. */
extern LEX_CSTRING sxlock_instrument_prefix;
/** String prefix for all cond instruments. */
extern LEX_CSTRING cond_instrument_prefix;
/** String prefix for all file instruments. */
extern LEX_CSTRING file_instrument_prefix;
/** Name of the global table I/O class. */
extern LEX_CSTRING table_io_class_name;
/** Name of the global table lock class. */
extern LEX_CSTRING table_lock_class_name;
/** String prefix for all socket instruments. */
extern LEX_CSTRING socket_instrument_prefix;
/** Name of the global idle class. */
extern LEX_CSTRING idle_class_name;
/** Name of the global metadata lock class. */
extern LEX_CSTRING metadata_lock_class_name;
/** String prefix for all thread instruments. */
extern LEX_CSTRING thread_instrument_prefix;
/** String prefix for all stage instruments. */
extern LEX_CSTRING stage_instrument_prefix;
/** String prefix for all statement instruments. */
extern LEX_CSTRING statement_instrument_prefix;
/** String prefix for all transaction instruments. */
extern LEX_CSTRING transaction_instrument_prefix;
/** String prefix for built-in memory instruments. */
extern LEX_CSTRING builtin_memory_instrument_prefix;
/** String prefix for all memory instruments. */
extern LEX_CSTRING memory_instrument_prefix;
/** String prefix for all meter instruments. */
extern LEX_CSTRING meter_instrument_prefix;
/** String prefix for all metric instruments. */
extern LEX_CSTRING metric_instrument_prefix;
/** String prefix for all logger instruments. */
extern LEX_CSTRING logger_instrument_prefix;
/** Name of the global error class. */
extern LEX_CSTRING error_class_name;

#endif
