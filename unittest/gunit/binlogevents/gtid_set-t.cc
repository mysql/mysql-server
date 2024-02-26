/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
#include <gtest/gtest.h>
#include "sql/rpl_gtid.h"

class GtidSetTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(GtidSetTest, Gtid_set_create_destroy) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);
  set1._add_gtid(sidno, 3);
  set1._add_gtid(sidno, 4);

  ASSERT_TRUE(set1.contains_gtid(sidno, 1));
  ASSERT_TRUE(set1.contains_gtid(sidno, 2));
  ASSERT_TRUE(set1.contains_gtid(sidno, 3));
  ASSERT_TRUE(set1.contains_gtid(sidno, 4));
  ASSERT_FALSE(set1.contains_gtid(sidno, 5));
}

TEST_F(GtidSetTest, Gtid_set_add_sets) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);

  Gtid_set set3(&sm, nullptr);

  smap_lock.wrlock();
  set3.add_gtid_set(&set1);
  set3.add_gtid_set(&set2);
  smap_lock.unlock();
  set3._add_gtid(sidno, 5);

  ASSERT_TRUE(set3.contains_gtid(sidno, 1));
  ASSERT_TRUE(set3.contains_gtid(sidno, 2));
  ASSERT_TRUE(set3.contains_gtid(sidno, 3));
  ASSERT_TRUE(set3.contains_gtid(sidno, 4));
  ASSERT_TRUE(set3.contains_gtid(sidno, 5));
  ASSERT_FALSE(set3.contains_gtid(sidno, 6));
}

TEST_F(GtidSetTest, Gtid_set_remove_sets) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 5);

  smap_lock.wrlock();
  set2.remove_gtid_set(&set1);
  smap_lock.unlock();

  ASSERT_FALSE(set2.contains_gtid(sidno, 1));
  ASSERT_FALSE(set2.contains_gtid(sidno, 2));
  ASSERT_TRUE(set2.contains_gtid(sidno, 3));
  ASSERT_TRUE(set2.contains_gtid(sidno, 4));
  ASSERT_TRUE(set2.contains_gtid(sidno, 5));
  ASSERT_FALSE(set2.contains_gtid(sidno, 6));
}

TEST_F(GtidSetTest, Gtid_set_remove_add_sets) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 5);

  smap_lock.wrlock();
  set2.remove_gtid_set(&set1);
  set2.add_gtid_set(&set1);
  smap_lock.unlock();

  ASSERT_TRUE(set2.contains_gtid(sidno, 1));
  ASSERT_TRUE(set2.contains_gtid(sidno, 2));
  ASSERT_TRUE(set2.contains_gtid(sidno, 3));
  ASSERT_TRUE(set2.contains_gtid(sidno, 4));
  ASSERT_TRUE(set2.contains_gtid(sidno, 5));
  ASSERT_FALSE(set2.contains_gtid(sidno, 6));
}

TEST_F(GtidSetTest, Gtid_set_is_subset_true) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 5);

  ASSERT_TRUE(set1.is_subset(&set2));
}

TEST_F(GtidSetTest, Gtid_set_is_subset_false) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);
  set1._add_gtid(sidno, 6);

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 5);

  ASSERT_FALSE(set1.is_subset(&set2));
}

TEST_F(GtidSetTest, Gtid_set_is_subset_not_equals_false) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);
  set1._add_gtid(sidno, 3);
  set1._add_gtid(sidno, 4);
  set1._add_gtid(sidno, 5);

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 5);

  ASSERT_FALSE(set1.is_subset_not_equals(&set2));
}

TEST_F(GtidSetTest, Gtid_set_is_subset_not_equals_true) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  smap_lock.unlock();

  set1._add_gtid(sidno, 1);
  set1._add_gtid(sidno, 2);
  set1._add_gtid(sidno, 5);

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 5);

  ASSERT_TRUE(set1.is_subset_not_equals(&set2));
}

TEST_F(GtidSetTest, Gtid_set_add_gtid_text) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:1");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:2");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:3");
  smap_lock.unlock();

  ASSERT_TRUE(set1.contains_gtid(sidno, 1));
  ASSERT_TRUE(set1.contains_gtid(sidno, 2));
  ASSERT_TRUE(set1.contains_gtid(sidno, 3));
  ASSERT_FALSE(set1.contains_gtid(sidno, 4));
}

TEST_F(GtidSetTest, Gtid_set_add_gtid_text_interval) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:1");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:3-6");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:8");
  smap_lock.unlock();

  ASSERT_TRUE(set1.contains_gtid(sidno, 1));
  ASSERT_FALSE(set1.contains_gtid(sidno, 2));
  ASSERT_TRUE(set1.contains_gtid(sidno, 3));
  ASSERT_TRUE(set1.contains_gtid(sidno, 4));
  ASSERT_TRUE(set1.contains_gtid(sidno, 6));
  ASSERT_FALSE(set1.contains_gtid(sidno, 7));
  ASSERT_TRUE(set1.contains_gtid(sidno, 8));
  ASSERT_FALSE(set1.contains_gtid(sidno, 9));
}

TEST_F(GtidSetTest, Gtid_set_add_gtid_text_memory) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  static const int PREALLOCATED_INTERVAL_COUNT = 64;
  Gtid_set::Interval iv[PREALLOCATED_INTERVAL_COUNT];
  set1.add_interval_memory(PREALLOCATED_INTERVAL_COUNT, iv);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:1");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:2");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:3");
  smap_lock.unlock();

  ASSERT_TRUE(set1.contains_gtid(sidno, 1));
  ASSERT_TRUE(set1.contains_gtid(sidno, 2));
  ASSERT_TRUE(set1.contains_gtid(sidno, 3));
  ASSERT_FALSE(set1.contains_gtid(sidno, 4));
}

TEST_F(GtidSetTest, Gtid_set_add_remove_gtid_text_memory) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  Gtid_set set1(&sm, nullptr);
  static const int PREALLOCATED_INTERVAL_COUNT = 64;
  Gtid_set::Interval iv[PREALLOCATED_INTERVAL_COUNT];
  set1.add_interval_memory(PREALLOCATED_INTERVAL_COUNT, iv);

  Gtid_set set2(&sm, nullptr);

  smap_lock.wrlock();
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:1");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:2");
  set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:3-10");
  smap_lock.unlock();

  set2._add_gtid(sidno, 1);
  set2._add_gtid(sidno, 2);
  set2._add_gtid(sidno, 3);
  set2._add_gtid(sidno, 4);
  set2._add_gtid(sidno, 10);

  set1.remove_gtid_set(&set2);

  ASSERT_FALSE(set1.contains_gtid(sidno, 1));
  ASSERT_FALSE(set1.contains_gtid(sidno, 2));
  ASSERT_FALSE(set1.contains_gtid(sidno, 3));
  ASSERT_FALSE(set1.contains_gtid(sidno, 4));
  ASSERT_TRUE(set1.contains_gtid(sidno, 5));
  ASSERT_TRUE(set1.contains_gtid(sidno, 6));
  ASSERT_FALSE(set1.contains_gtid(sidno, 10));
  ASSERT_FALSE(set1.contains_gtid(sidno, 11));
}

TEST_F(GtidSetTest, Gtid_set_add_remove_gtid_text_memory_loop) {
  Checkable_rwlock smap_lock;
  Sid_map sm(&smap_lock);
  rpl_sid sid;

  smap_lock.wrlock();
  sid.parse("d3a98502-756b-4b08-bdd2-a3d3938ba90f", 36);
  rpl_sidno sidno = sm.add_sid(sid);
  smap_lock.unlock();

  for (int i = 0; i < 1000; i++) {
    Gtid_set set1(&sm, nullptr);
    static const int PREALLOCATED_INTERVAL_COUNT = 64;
    Gtid_set::Interval iv[PREALLOCATED_INTERVAL_COUNT];
    set1.add_interval_memory(PREALLOCATED_INTERVAL_COUNT, iv);

    Gtid_set set2(&sm, nullptr);

    smap_lock.wrlock();
    set1.ensure_sidno(sidno);
    set2.ensure_sidno(sidno);
    set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:1");
    set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:2");
    set1.add_gtid_text("d3a98502-756b-4b08-bdd2-a3d3938ba90f:3-10");
    smap_lock.unlock();

    set2._add_gtid(sidno, 1);
    set2._add_gtid(sidno, 2);
    set2._add_gtid(sidno, 3);
    set2._add_gtid(sidno, 4);
    set2._add_gtid(sidno, 10);

    set1.remove_gtid_set(&set2);

    ASSERT_FALSE(set1.contains_gtid(sidno, 1));
    ASSERT_FALSE(set1.contains_gtid(sidno, 2));
    ASSERT_FALSE(set1.contains_gtid(sidno, 3));
    ASSERT_FALSE(set1.contains_gtid(sidno, 4));
    ASSERT_TRUE(set1.contains_gtid(sidno, 5));
    ASSERT_TRUE(set1.contains_gtid(sidno, 6));
    ASSERT_FALSE(set1.contains_gtid(sidno, 10));
    ASSERT_FALSE(set1.contains_gtid(sidno, 11));
  }
}
