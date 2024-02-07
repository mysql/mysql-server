/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
#include <gtest/gtest.h>
#include "sql/rpl_gtid.h"

class GtidSetTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(GtidSetTest, Gtid_set_create_destroy) {
  Checkable_rwlock smap_lock;
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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
  Tsid_map sm(&smap_lock);
  mysql::gtid::Tsid tsid;

  smap_lock.wrlock();
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
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

TEST_F(GtidSetTest, GtidSetParsingTestFormat) {
  auto valid_sets = {
      "11111111-1111-1111-1111-111111111111:tag_1 : 1-2 , "
      "11111111-1111-1111-1111-111111111111, "
      "11111111-1111-1111-1111-111111111111:tag_1 ",
      "11111111-1111-1111-1111-111111111112:tag_1 : 1-2 , "
      "11111111-1111-1111-1111-111111111111:1-2, "
      "11111111-1111-1111-1111-111111111112:tag_1 ",
      "11111111-1111-1111-1111-111111111112:tag_1 : 1-2 : 3-4 : tag_2: 1-2 , "
      "11111111-1111-1111-1111-111111111111:1-2, "
      "11111111-1111-1111-1111-111111111112:tag_1  ,,, ",
      "11111111-1111-1111-1111-111111111111:tag_1 : 2 ,, "
      "11111111-1111-1111-1111-111111111111:tag_1:1, "
      "11111111-1111-1111-1111-111111111111:tag_1 ",
      "11111111-1111-1111-1111-111111111112:tag_1 : 1-2 , "
      "11111111-1111-1111-1111-111111111111:1-2, "
      "11111111-1111-1111-1111-111111111111 ",
      "11111111-1111-1111-1111-111111111112:tag_1 : 1 : 2 : 3 : 4 : tag_2: "
      "1-2 , 11111111-1111-1111-1111-111111111111:1-2, "
      "11111111-1111-1111-1111-111111111112:tag_1  ,,, ",
      "11111111-1111-1111-1111-111111111111:1-2,"
      "11111111-1111-1111-1111-111111111112:tag_1:1-4:tag_2:1-2, "
      "11111111-1111-1111-1111-111111111112:tag_1  ,,, "};

  mysql::gtid::Tsid tsid_tmp;
  std::vector<mysql::gtid::Tsid> tsids;
  ASSERT_TRUE(tsid_tmp.from_cstring("11111111-1111-1111-1111-111111111111") >
              0);  // 0
  tsids.push_back(tsid_tmp);
  ASSERT_TRUE(tsid_tmp.from_cstring(
                  "11111111-1111-1111-1111-111111111111:tag_1") > 0);  // 1
  tsids.push_back(tsid_tmp);
  ASSERT_TRUE(tsid_tmp.from_cstring(
                  "11111111-1111-1111-1111-111111111111:tag_2") > 0);  // 2
  tsids.push_back(tsid_tmp);
  ASSERT_TRUE(tsid_tmp.from_cstring("11111111-1111-1111-1111-111111111112") >
              0);  // 3
  tsids.push_back(tsid_tmp);
  ASSERT_TRUE(tsid_tmp.from_cstring(
                  "11111111-1111-1111-1111-111111111112:tag_1") > 0);  // 4
  tsids.push_back(tsid_tmp);
  ASSERT_TRUE(tsid_tmp.from_cstring(
                  "11111111-1111-1111-1111-111111111112:tag_2") > 0);  // 5
  tsids.push_back(tsid_tmp);

  std::vector<std::unique_ptr<Tsid_map>> sid_maps_expected;

  std::size_t num_sets = 3;
  std::vector<std::unique_ptr<Gtid_set>> gtid_sets_expected;
  for (std::size_t id = 0; id < num_sets; ++id) {
    sid_maps_expected.emplace_back(std::make_unique<Tsid_map>(nullptr));
    gtid_sets_expected.emplace_back(
        std::make_unique<Gtid_set>(sid_maps_expected.at(id).get()));
  }

  rpl_sidno sidno;
  // prepare 0
  {
    std::size_t current = 0;
    auto &gtid_set = gtid_sets_expected.at(current);
    auto *tsid_map = gtid_set->get_tsid_map();
    sidno = tsid_map->add_tsid(tsids.at(1));
    gtid_set->ensure_sidno(sidno);
    gtid_set->_add_gtid(sidno, 1);
    gtid_set->_add_gtid(sidno, 2);
    sidno = tsid_map->add_tsid(tsids.at(0));
    gtid_set->ensure_sidno(sidno);
  }
  // prepare 1
  {
    std::size_t current = 1;
    auto &gtid_set = gtid_sets_expected.at(current);
    auto *tsid_map = gtid_set->get_tsid_map();
    assert(tsid_map != nullptr);

    sidno = tsid_map->add_tsid(tsids.at(4));
    gtid_set->ensure_sidno(sidno);
    gtid_set->_add_gtid(sidno, 1);
    gtid_set->_add_gtid(sidno, 2);
    sidno = tsid_map->add_tsid(tsids.at(0));
    gtid_set->ensure_sidno(sidno);
    gtid_set->_add_gtid(sidno, 1);
    gtid_set->_add_gtid(sidno, 2);
  }
  // prepare 2
  {
    std::size_t current = 2;
    auto &gtid_set = gtid_sets_expected.at(current);
    auto *tsid_map = gtid_set->get_tsid_map();

    sidno = tsid_map->add_tsid(tsids.at(4));
    gtid_set->ensure_sidno(sidno);
    gtid_set->_add_gtid(sidno, 1);
    gtid_set->_add_gtid(sidno, 2);
    gtid_set->_add_gtid(sidno, 3);
    gtid_set->_add_gtid(sidno, 4);
    sidno = tsid_map->add_tsid(tsids.at(5));
    gtid_set->ensure_sidno(sidno);
    gtid_set->_add_gtid(sidno, 1);
    gtid_set->_add_gtid(sidno, 2);
    sidno = tsid_map->add_tsid(tsids.at(0));
    gtid_set->ensure_sidno(sidno);
    gtid_set->_add_gtid(sidno, 1);
    gtid_set->_add_gtid(sidno, 2);
    sidno = tsid_map->add_tsid(tsids.at(1));
    gtid_set->ensure_sidno(sidno);
  }

  std::vector<std::size_t> gtid_set_verification;
  gtid_set_verification.push_back(0);
  gtid_set_verification.push_back(1);
  gtid_set_verification.push_back(2);
  gtid_set_verification.push_back(0);
  gtid_set_verification.push_back(1);
  gtid_set_verification.push_back(2);
  gtid_set_verification.push_back(2);

  std::size_t id = 0;
  for (const auto &valid_str : valid_sets) {
    Tsid_map tsid_map(nullptr);
    Gtid_set gtid_set(&tsid_map);
    auto status = gtid_set.add_gtid_text(valid_str);
    ASSERT_TRUE(status == RETURN_STATUS_OK);
    std::cout << "comparing : " << id << " text: " << valid_str << std::endl;
    EXPECT_TRUE(gtid_set.equals(
        (gtid_sets_expected.at(gtid_set_verification.at(id++)).get())));
  }
}
