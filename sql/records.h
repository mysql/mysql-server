#ifndef SQL_RECORDS_H
#define SQL_RECORDS_H
/* Copyright (c) 2008, 2020, Oracle and/or its affiliates.

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

#include <sys/types.h>
#include <memory>
#include <string>

#include "my_alloc.h"
#include "my_base.h"
#include "sql/basic_row_iterators.h"
#include "sql/composite_iterators.h"
#include "sql/ref_row_iterators.h"
#include "sql/row_iterator.h"
#include "sql/sorting_iterator.h"

class QEP_TAB;
class THD;
struct AccessPath;
struct TABLE;

AccessPath *create_table_access_path(THD *thd, TABLE *table, QEP_TAB *qep_tab,
                                     bool count_examined_rows);

/**
  Creates an iterator for the given table, then calls Init() on the resulting
  iterator. Unlike create_table_iterator(), this can create iterators for sort
  buffer results (which are set in the TABLE object during query execution).
  Returns nullptr on failure.
 */
unique_ptr_destroy_only<RowIterator> init_table_iterator(
    THD *thd, TABLE *table, QEP_TAB *qep_tab, bool ignore_not_found_rows,
    bool count_examined_rows);

#endif /* SQL_RECORDS_H */
