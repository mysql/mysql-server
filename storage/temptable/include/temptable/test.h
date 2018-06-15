/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/test.h
TempTable C++ unit tests hooked inside CREATE TABLE. */

#ifndef TEMPTABLE_TEST_H
#define TEMPTABLE_TEST_H

#include "my_dbug.h"
#include "sql/handler.h"
#include "sql/table.h"

#if defined(HAVE_SYS_RESOURCE_H) && defined(HAVE_SYS_TIME_H) && \
    defined(HAVE_SYS_TYPES_H)
#define TEMPTABLE_CPP_HOOKED_TESTS
#endif /* HAVE_SYS_RESOURCE_H && HAVE_SYS_TIME_H && HAVE_SYS_TYPES_H */

#ifdef TEMPTABLE_CPP_HOOKED_TESTS

namespace temptable {

class Test {
 public:
  Test(handlerton *hton, TABLE_SHARE *mysql_table_share, TABLE *mysql_table);
  void correctness();
  void performance();

 private:
  void create_and_drop();

  void scan_empty();

  void scan_hash_index();

  template <class Handler_type>
  void sysbench_distinct_ranges_write_only(size_t number_of_rows_to_write);

  template <class Handler_type>
  void sysbench_distinct_ranges();

  handlerton *m_hton;
  TABLE_SHARE *m_mysql_table_share;
  TABLE *m_mysql_table;
};

} /* namespace temptable */

#endif /* TEMPTABLE_CPP_HOOKED_TESTS */

#endif /* TEMPTABLE_TEST_H */
