/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file storage/innmem/include/innmem/test.h
InnMEM C++ unit tests hooked inside CREATE TABLE. */

#ifndef INNMEM_TEST_H
#define INNMEM_TEST_H

#include "handler.h"
#include "my_dbug.h" /* DBUG_OFF */
#include "table.h"

#if defined(HAVE_SYS_RESOURCE_H) && defined(HAVE_SYS_TIME_H) && \
    defined(HAVE_SYS_TYPES_H)
#define INNMEM_CPP_HOOKED_TESTS
#endif /* HAVE_SYS_RESOURCE_H && HAVE_SYS_TIME_H && HAVE_SYS_TYPES_H */

#ifdef INNMEM_CPP_HOOKED_TESTS

namespace innmem {

class Test {
 public:
  Test(handlerton* hton, TABLE_SHARE* mysql_table_share, TABLE* mysql_table);
  void correctness();
  void performance();

 private:
  void create_and_drop();

  void scan_empty();

  void scan_hash_index();

  template <class H>
  void sysbench_distinct_ranges_write_only(size_t number_of_rows_to_write);

  template <class H>
  void sysbench_distinct_ranges();

  handlerton* m_hton;
  TABLE_SHARE* m_mysql_table_share;
  TABLE* m_mysql_table;
};

} /* namespace innmem */

#endif /* INNMEM_CPP_HOOKED_TESTS */

#endif /* INNMEM_TEST_H */
