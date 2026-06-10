/*****************************************************************************

Copyright (c) 1996, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/dict0load.h
 Loads to the memory cache database object definitions
 from dictionary tables

 Created 4/24/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0load_h
#define dict0load_h

#include "btr0types.h"
#include "dict0types.h"
#include "fil0fil.h"
#include "mem0mem.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0byte.h"
#include "ut0new.h"

#include <deque>

/** A stack of table names related through foreign key constraints */
typedef std::deque<const char *, ut::allocator<const char *>> dict_names_t;

/** Make sure the tablespace name is saved in dict_table_t if the table
uses a general tablespace.
Try to read it from the fil_system_t first, then from SYS_TABLESPACES.
@param[in]  table           Table object */
void dict_get_and_save_space_name(dict_table_t *table);

/** Using the table->heap, copy the null-terminated filepath into
table->data_dir_path. The data directory path is derived from the
filepath by stripping the the table->name.m_name component suffix.
If the filepath is not of the correct form (".../db/table.ibd"),
then table->data_dir_path will remain nullptr.
@param[in,out]  table           table instance
@param[in]      filepath        filepath of tablespace */
void dict_save_data_dir_path(dict_table_t *table, char *filepath);

#endif
