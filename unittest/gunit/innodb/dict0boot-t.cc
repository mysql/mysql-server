/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

/* See http://code.google.com/p/googletest/wiki/Primer */

#include <gtest/gtest.h>

#include <chrono>
#include <random>
#include <thread>

#include "storage/innobase/include/dict0boot.h"
#include "storage/innobase/include/dict0dict.h"
#include "storage/innobase/include/sync0types.h"
#include "storage/innobase/include/ut0mutex.h"

constexpr int num_workers = 64;
constexpr int num_ids_per_worker = 1000;

/** Number of times the row_id is bumped */
constexpr int num_bumps = 64;
/** Bump the row_id by bump_size * {0, 1, 2, .. num_bumps} */
constexpr int bump_size = 100;

typedef std::vector<row_id_t> worker_result;

namespace innodb_dict0boot_unittest {
struct dict_sys_unittest_t : public dict_sys_t {
  void dict_hdr_flush_row_id() override {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(0, 1);
    if (dist(gen)) {
      std::this_thread::yield();
    }
  }
};

void generate_row_ids(worker_result &row_ids) {
  worker_result generated_row_ids;
  for (int i = 0; i < num_ids_per_worker; i++) {
    row_id_t new_row_id = dict_sys_get_new_row_id();
    generated_row_ids.push_back(new_row_id);
  }
  row_ids = std::move(generated_row_ids);
}

/* Generate (num_workers * num_ids_per_worker) row_ids in parallel */
std::vector<worker_result> do_work() {
  std::vector<std::thread> workers;
  std::vector<worker_result> row_ids(num_workers);

  for (int i = 0; i < num_workers; i++) {
    workers.emplace_back(generate_row_ids, std::ref(row_ids[i]));
  }

  for (auto &worker : workers) {
    worker.join();
  }
  return row_ids;
}

/* Validate that generated row_ids are unique */
void assert_unique(const std::vector<worker_result> &row_ids) {
  std::set<row_id_t> all_row_ids;
  for (const auto &worker_row_ids : row_ids) {
    for (const auto &row_id : worker_row_ids) {
      EXPECT_TRUE(all_row_ids.insert(row_id).second);
    }
  }
}

void bump_row_id() {
  for (int i = 0; i < num_bumps; i++) {
    dict_sys_set_min_next_row_id(i * bump_size);
  }
}

TEST(dict0boot, get_row_id) {
  dict_sys = new dict_sys_unittest_t;

  os_event_global_init();
  sync_check_init(num_workers);
  mutex_create(latch_id_t::LATCH_ID_DICT_SYS, &dict_sys->mutex);

  std::vector<worker_result> row_ids = do_work();

  assert_unique(row_ids);

  mutex_destroy(&dict_sys->mutex);
  sync_check_close();
  os_event_global_destroy();

  delete dict_sys;
}

TEST(dict0boot, bump_row_id) {
  dict_sys = new dict_sys_unittest_t;

  os_event_global_init();
  sync_check_init(num_workers + 1);
  mutex_create(latch_id_t::LATCH_ID_DICT_SYS, &dict_sys->mutex);

  std::vector<worker_result> row_ids;

  std::thread bumper{bump_row_id};
  row_ids = do_work();
  bumper.join();

  assert_unique(row_ids);

  mutex_destroy(&dict_sys->mutex);
  sync_check_close();
  os_event_global_destroy();

  delete dict_sys;
}
}  // namespace innodb_dict0boot_unittest
