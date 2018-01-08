/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file storage/temptable/src/test.cc
TempTable C++ unit tests hooked inside CREATE TABLE. */

#include <array>
#include <cstring>
#include <memory>
#include <thread>

#include "my_dbug.h"
#include "sql/field.h"
#include "sql/table.h"
#include "storage/heap/ha_heap.h"
#include "storage/temptable/include/temptable/handler.h"
#include "storage/temptable/include/temptable/storage.h"
#include "storage/temptable/include/temptable/test.h"

#ifdef TEMPTABLE_CPP_HOOKED_TESTS

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

namespace temptable {

/** A "chronometer" used to clock snippets of code.
Example usage:
  ut_chrono_t ch("this loop");
  for (;;) { ... }
  ch.show();
would print the timings of the for() loop, prefixed with "this loop:" */
class Chrono {
 public:
  /** Constructor.
  @param[in]	name	chrono's name, used when showing the values */
  Chrono(const char* name) : m_name(name), m_show_from_destructor(true) {
    reset();
  }

  /** Resets the chrono (records the current time in it). */
  void reset() {
    gettimeofday(&m_tv, NULL);

    getrusage(RUSAGE_SELF, &m_ru);
  }

  /** Shows the time elapsed and usage statistics since the last reset. */
  void show() {
    struct rusage ru_now;
    struct timeval tv_now;
    struct timeval tv_diff;

    getrusage(RUSAGE_SELF, &ru_now);

    gettimeofday(&tv_now, NULL);

#ifndef timersub
#define timersub(a, b, r)                       \
  do {                                          \
    (r)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
    (r)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((r)->tv_usec < 0) {                     \
      (r)->tv_sec--;                            \
      (r)->tv_usec += 1000000;                  \
    }                                           \
  } while (0)
#endif /* timersub */

#define CHRONO_PRINT(type, tvp)                            \
  fprintf(stderr, "%s: %s% 5ld.%06ld sec\n", m_name, type, \
          static_cast<long>((tvp)->tv_sec), static_cast<long>((tvp)->tv_usec))

    timersub(&tv_now, &m_tv, &tv_diff);
    CHRONO_PRINT("real", &tv_diff);

    timersub(&ru_now.ru_utime, &m_ru.ru_utime, &tv_diff);
    CHRONO_PRINT("user", &tv_diff);

    timersub(&ru_now.ru_stime, &m_ru.ru_stime, &tv_diff);
    CHRONO_PRINT("sys ", &tv_diff);
  }

  /** Cause the timings not to be printed from the destructor. */
  void end() { m_show_from_destructor = false; }

  /** Destructor. */
  ~Chrono() {
    if (m_show_from_destructor) {
      show();
    }
  }

 private:
  /** Name of this chronometer. */
  const char* m_name;

  /** True if the current timings should be printed by the destructor. */
  bool m_show_from_destructor;

  /** getrusage() result as of the last reset(). */
  struct rusage m_ru;

  /** gettimeofday() result as of the last reset(). */
  struct timeval m_tv;
};

Test::Test(handlerton* hton, TABLE_SHARE* mysql_table_share, TABLE* mysql_table)
    : m_hton(hton),
      m_mysql_table_share(mysql_table_share),
      m_mysql_table(mysql_table) {}

void Test::correctness() {
  create_and_drop();
  scan_empty();
  scan_hash_index();
}

#define ut_a(expr) \
  do {             \
    if (!(expr)) { \
      abort();     \
    }              \
  } while (0)

template <class H>
void Test::sysbench_distinct_ranges_write_only(size_t number_of_rows_to_write) {
  // clang-format off
  char row[] = {
    "\xFF""1-2-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql"
  };
  // clang-format on

  H h(m_hton, m_mysql_table_share);

  HA_CREATE_INFO create_info;
  create_info.auto_increment_value = 0;

  ut_a(h.create("t1", m_mysql_table, &create_info, nullptr) == 0);

  ut_a(h.ha_open(m_mysql_table, "t1", 0, 0, nullptr) == 0);

  for (size_t n = 0; n < number_of_rows_to_write; ++n) {
    snprintf(row + 1, 119, "%016zx", n);
    row[17] = '-';
    memcpy(m_mysql_table->record[0], row, 120);

    const int ret = h.write_row(m_mysql_table->record[0]);
    ut_a(ret == 0);
  }

  ut_a(h.close() == 0);

  ut_a(h.delete_table("t1", nullptr) == 0);
}

template <class H>
void Test::sysbench_distinct_ranges() {
  // clang-format off
  static const char* rows[] = {
    "\xFF""1-2-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""2-3-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""3-4-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""4-5-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""5-6-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""6-7-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""7-8-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""8-9-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql",
    "\xFF""9-10-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysq",
    "\xFF""10-11-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""11-12-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""12-13-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""13-14-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""14-15-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""15-16-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""16-17-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""17-18-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""18-19-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""19-20-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""20-21-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""21-22-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""22-23-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""23-24-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""24-25-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""25-26-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""26-27-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""27-28-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""28-29-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""29-30-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""30-31-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""31-32-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""32-33-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""33-34-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""34-35-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""35-36-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""36-37-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""37-38-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""38-39-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""39-40-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""40-41-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""41-42-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""42-43-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""43-44-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""44-45-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""45-46-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""46-47-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""47-48-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""48-49-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""49-50-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""50-51-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""51-52-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""52-53-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""53-54-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""54-55-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""55-56-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""56-57-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""57-58-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""58-59-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""59-60-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""60-61-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""61-62-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""62-63-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""63-64-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""64-65-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""65-66-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""66-67-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""67-68-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""68-69-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""69-70-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""70-71-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""71-72-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""72-73-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""73-74-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""74-75-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""75-76-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""76-77-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""77-78-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""78-79-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""79-80-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""80-81-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""81-82-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""82-83-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""83-84-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""84-85-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""85-86-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""86-87-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""87-88-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""88-89-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""89-90-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""90-91-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""91-92-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""92-93-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""93-94-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""94-95-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""95-96-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""96-97-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""97-98-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""98-99-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmys",
    "\xFF""99-100-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmy",
    "\xFF""100-101-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonm",
    "\xFF""101-102-mysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonmysql-jsonm",
  };
  // clang-format on
  constexpr size_t n_rows = sizeof(rows) / sizeof(rows[0]);

  constexpr size_t number_of_iterations = 128;

  HA_CREATE_INFO create_info;
  create_info.auto_increment_value = 0;

#define ALSO_DO_READS

#ifdef ALSO_DO_READS
  std::array<unsigned char[sizeof(temptable::Storage::Iterator)], n_rows>
      positions;
#endif /* ALSO_DO_READS */

  for (size_t n = 0; n < number_of_iterations; ++n) {
    H h(m_hton, m_mysql_table_share);

#ifdef ALSO_DO_READS
    ut_a(h.ref_length <= sizeof(positions[0]));
#endif /* ALSO_DO_READS */

    ut_a(h.create("t1", m_mysql_table, &create_info, nullptr) == 0);

    ut_a(h.ha_open(m_mysql_table, "t1", 0, 0, nullptr) == 0);

    for (size_t i = 0; i < n_rows; ++i) {
      memcpy(m_mysql_table->record[0], rows[i], 120);

      const int ret = h.write_row(m_mysql_table->record[0]);
      if (ret != 0) {
        std::cerr << "write_row() failed with error " << ret << std::endl;
        abort();
      }
    }

#ifdef ALSO_DO_READS
    ut_a(h.rnd_init(true) == 0);

    size_t i = 0;
    while (h.rnd_next(m_mysql_table->record[0]) == 0) {
      h.position(nullptr);
      // std::cout << "ref: " << (void*)h.ref << ", ref_length: " <<
      // h.ref_length
      //          << ", ref val: " << *(uint64_t*)h.ref << "\n";
      memcpy(positions.at(i), h.ref, h.ref_length);
      ++i;
    }

    for (size_t j = 0; j < i; ++j) {
      ut_a(h.rnd_pos(m_mysql_table->record[0], positions.at(j)) == 0);
    }
#endif /* ALSO_DO_READS */

    ut_a(h.close() == 0);

    ut_a(h.delete_table("t1", nullptr) == 0);
  }
}

void Test::performance() {
  for (int i = 0; i < 1; ++i) {
    {
      Chrono chrono("temptable write only");
      sysbench_distinct_ranges_write_only<Handler>(1024);
    }

    {
      Chrono chrono("heap write only");
      sysbench_distinct_ranges_write_only<ha_heap>(1024);
    }

    {
      Chrono chrono("temptable full");
      sysbench_distinct_ranges<Handler>();
    }

    {
      Chrono chrono("heap full");
      sysbench_distinct_ranges<ha_heap>();
    }
  }
}

void Test::create_and_drop() {
  Handler h(m_hton, m_mysql_table_share);

  ut_a(h.create("t1", m_mysql_table, nullptr, nullptr) == 0);
  ut_a(h.create("t2", m_mysql_table, nullptr, nullptr) == 0);
  ut_a(h.delete_table("t1", nullptr) == 0);
  ut_a(h.delete_table("t2", nullptr) == 0);
}

void Test::scan_empty() {
  Handler h(m_hton, m_mysql_table_share);

  static const char* table_name = "test_scan_empty";

  ut_a(h.create(table_name, m_mysql_table, nullptr, nullptr) == 0);

  h.change_table_ptr(m_mysql_table, m_mysql_table_share);

  ut_a(h.open(table_name, 0, 0, nullptr) == 0);

  ut_a(h.rnd_init(true /* ignored */) == 0);
  ut_a(h.rnd_next(nullptr) == HA_ERR_END_OF_FILE);
  ut_a(h.rnd_end() == 0);

  ut_a(h.close() == 0);

  ut_a(h.delete_table(table_name, nullptr) == 0);
}

void Test::scan_hash_index() {
  Handler h(m_hton, m_mysql_table_share);

  static const char* table_name = "test_scan_hash_index";

  ut_a(h.create(table_name, m_mysql_table, nullptr, nullptr) == 0);

  h.change_table_ptr(m_mysql_table, m_mysql_table_share);

  ut_a(h.open(table_name, 0, 0, nullptr) == 0);

  // clang-format off
  const unsigned char* row1 = reinterpret_cast<const unsigned char*>("\xFF""aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const unsigned char* row2 = reinterpret_cast<const unsigned char*>("\xFF""bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  // clang-format on
  const size_t row_len = 121;

  memcpy(m_mysql_table->record[0], row1, row_len);
  ut_a(h.write_row(m_mysql_table->record[0]) == 0);

  memcpy(m_mysql_table->record[0], row2, row_len);
  ut_a(h.write_row(m_mysql_table->record[0]) == 0);

  ut_a(h.index_init(0, true /* ignored */) == 0);

  ut_a(h.index_read(m_mysql_table->record[0], row1 + 1, row_len - 1,
                     HA_READ_KEY_EXACT) == 0);
  ut_a(memcmp(m_mysql_table->record[0], row1, row_len) == 0);

  /* This could return either success or not found because hash indexes do not
   * have a predetermined order and we do not know if bbb... will follow a... */
  switch (h.index_next(m_mysql_table->record[0])) {
    case 0:
      ut_a(memcmp(m_mysql_table->record[0], row2, row_len) == 0);
      break;
    case HA_ERR_END_OF_FILE:
      break;
    default:
      abort();
  }

  ut_a(h.index_end() == 0);

  ut_a(h.close() == 0);

  ut_a(h.delete_table(table_name, nullptr) == 0);
}

#if 0

  /* Insert (15, 150), (16, 151), (17, 152). */
  for (uint32_t i = 0; i < 3; ++i) {
    *(uint8_t*)(table->record[0]) = 0xF9;
    *(uint32_t*)(table->record[0] + 1) = i + 15;
    *(uint32_t*)(table->record[0] + 5) = i + 150;
    ut_a(write_row(table->record[0]) == 0);
  }

  /* Test scan table - it should contain 3 records. */
  ut_a(rnd_init(true /* ignored */) == 0);
  for (uint32_t i = 0; i < 3; ++i) {
    ut_a(rnd_next(table->record[0]) == 0);
    ut_a(*(uint8_t*)(table->record[0]) == 0xF9);
    ut_a(*(uint32_t*)(table->record[0] + 1) == i + 15);
    ut_a(*(uint32_t*)(table->record[0] + 5) == i + 150);
  }
  ut_a(rnd_next(nullptr) == HA_ERR_END_OF_FILE);
  ut_a(rnd_end() == 0);

#if 0
  ut_a(index_init(0, true) == 0);

  uchar search_key[5];

  search_key[0] = 0;
  *(uint32_t*)(search_key + 1) = 14;
  ut_a(index_read(table->record[0], search_key, sizeof(search_key[5]),
                  HA_READ_KEY_EXACT) == 0);
#endif

#if 0
  /* Disabled because delete_row() crashes because it assumes some
  "retrieval plan" must have been set, but it is not. */

  /* Delete (15, 150), (16, 151), (17, 152). */
  for (int i = 0; i < 3; ++i) {
    *(uint8_t*)(table->record[0]) = 0xF9;
    *(uint32_t*)(table->record[0] + 1) = i + 15;
    *(uint32_t*)(table->record[0] + 5) = i + 150;
    ut_a(delete_row(table->record[0]) == 0);
  }

  /* Test scan table - it should be empty. */
  ut_a(rnd_init(true /* ignored */) == 0);
  ut_a(rnd_next(nullptr) == HA_ERR_END_OF_FILE);
  ut_a(rnd_end() == 0);
#endif

  ut_a(close() == 0);
#endif

} /* namespace temptable */

#endif /* TEMPTABLE_CPP_HOOKED_TESTS */
