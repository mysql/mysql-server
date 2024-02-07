// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include "plugin/group_replication/include/certification/gtid_generator.h"

using namespace mysql::gtid;

namespace gr::unittest {

class GtidGeneratorTest : public ::testing::Test {
 public:
  GtidGeneratorTest() = default;

  void SetUp() override {}

  void TearDown() override {}

  struct Id {
    Id(const std::string &uuid, Tsid_map &tsid_map, Gtid_set &set)
        : m_uuid(uuid) {
      [[maybe_unused]] auto ret = m_tsid.from_cstring(uuid.c_str());
      assert(ret > 0);
      m_sidno = tsid_map.add_tsid(m_tsid);
      assert(m_sidno >= 1);
      set.ensure_sidno(m_sidno);  // insert sidno to gtid set
    }
    std::string m_uuid;  // uuid string
    Tsid m_tsid;         // uuid string converted to tsid
    rpl_sidno m_sidno;   // tsid converted to sidno using tsid_map
  };

  // This test verifies automatic GTID generation performed by the
  // Gtid_generator class and used during certification process in GR. It
  // executes testing for various GTID assignment block sizes.
  // R1. For automatic GTID generation, the source shall automatically generate
  //     a transaction sequence number that is unique
  //     for a pair of UUID and a tag.
  // R2. For automatic GTID generation, the source shall not produce gaps
  //     in generation of a GTID for any UUID:Tag pair, where the Tag can be
  //     empty.
  // block_size - Size of blocks of GTIDs assigned to specific members
  static void test_generated_gtids(size_t block_size) {
    Gtid_generator gen;
    gen.initialize(block_size);

    Checkable_rwlock tsid_map_lock;
    Tsid_map tsid_map(&tsid_map_lock);

    Gtid_set set(&tsid_map, &tsid_map_lock);

    tsid_map_lock.wrlock();

    // define member's uuids
    std::vector<Id> members_tids = {
        Id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", tsid_map, set),
        Id("aaaaaaaa-aaaa-aaaa-bbbb-aaaaaaaaaaaa", tsid_map, set)};

    // define transactions' uuids (custom uuids set with UUID:AUTOMATIC)
    std::vector<Id> trx_tids = {
        Id("aaaaaaaa-aaaa-aaaa-cccc-aaaaaaaaaaaa", tsid_map, set),
        Id("aaaaaaaa-aaaa-aaaa-cccc-aaaaaaaaaaaa:aa", tsid_map, set),
        Id("aaaaaaaa-aaaa-aaaa-cccc-aaaaaaaaaaaa:bb", tsid_map, set),
        Id("aaaaaaaa-aaaa-aaaa-cccc-aaaaaaaaaaaa:cc", tsid_map, set),
        Id("aaaaaaaa-aaaa-aaaa-cccc-aaaaaaaaaaaa:dd", tsid_map, set),
        Id("aaaaaaaa-aaaa-aaaa-cccc-aaaaaaaaaaaa:ee", tsid_map, set)};

    assert(set.get_max_sidno() >= trx_tids.at(trx_tids.size() - 1).m_sidno);

    tsid_map_lock.unlock();

    std::mutex mt;

    // define job for each thread
    auto thread_job =
        [&members_tids, &trx_tids, &set, &gen, &tsid_map_lock, &
         mt ](size_t thread_id) -> auto {
      size_t member_id = 0;  // member id
      if (thread_id > 2) {
        member_id = 1;
      }
      auto member_tid = members_tids.at(member_id);
      auto trx_tid = trx_tids.at(thread_id);

      size_t trx_num = (thread_id + 1) * 100 + thread_id;

      for (size_t id = 0; id < trx_num; ++id) {
        std::scoped_lock lock(mt);  // certifier is sequential
        tsid_map_lock.rdlock();     // required by Gtid_set
        auto [gno, res] = gen.get_next_available_gtid(member_tid.m_uuid.c_str(),
                                                      trx_tid.m_sidno, set);
        set._add_gtid(trx_tid.m_sidno, gno);
        tsid_map_lock.unlock();
      }
      return 0;
    };

    std::vector<std::thread> threads;
    std::size_t num_threads = trx_tids.size();

    // run threads
    for (size_t tid = 0; tid < num_threads; ++tid) {
      threads.push_back(std::thread(thread_job, tid));
    }

    // join all threads
    for (auto &thread : threads) {
      thread.join();
    }

    // prepare validation gtid set
    Gtid_set target_gtid_set(&tsid_map, &tsid_map_lock);
    for (const auto &member_tid : members_tids) {
      tsid_map_lock.rdlock();
      target_gtid_set.ensure_sidno(member_tid.m_sidno);
      tsid_map_lock.unlock();
    }
    for (const auto &trx_tid : trx_tids) {
      tsid_map_lock.rdlock();
      target_gtid_set.ensure_sidno(trx_tid.m_sidno);
      tsid_map_lock.unlock();
    }

    // Check that the current state of the generator is correct
    // for each member and each transaction uuid
    for (size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
      size_t member_id = 0;  // member id
      if (thread_id > 2) {
        member_id = 1;
      }
      auto member_tid = members_tids.at(member_id);
      auto trx_tid = trx_tids.at(thread_id);
      size_t trx_num = (thread_id + 1) * 100 + thread_id;
      tsid_map_lock.rdlock();
      for (size_t trx_id = 0; trx_id < trx_num; ++trx_id) {
        target_gtid_set._add_gtid(trx_tid.m_sidno, trx_id + 1);
      }
      auto [gno, res] = gen.get_next_available_gtid(member_tid.m_uuid.c_str(),
                                                    trx_tid.m_sidno, set);
      tsid_map_lock.unlock();
      ASSERT_EQ(gno, trx_num + 1);
    }

    // check gtid executed set
    tsid_map_lock.wrlock();
    ASSERT_EQ(set.equals(&target_gtid_set), true);
    tsid_map_lock.unlock();
  }

  // This test verifies automatic GTID generation performed by the
  // Gtid_generator class and used during certification process in GR.
  // Corner case - gno exhaustion, recompute fails
  static void test_gno_exhaustion() {
    Gtid_generator gen;
    gen.initialize(1000);

    Checkable_rwlock tsid_map_lock;
    Tsid_map tsid_map(&tsid_map_lock);
    Gtid_set set(&tsid_map, &tsid_map_lock);

    tsid_map_lock.wrlock();
    Id member_id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", tsid_map, set);

    set.add_gtid_text(
        "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1-9223372036854775806");
    gen.recompute(set);
    auto [gno, res] = gen.get_next_available_gtid(member_id.m_uuid.c_str(),
                                                  member_id.m_sidno, set);
    ASSERT_EQ(res, mysql::utils::Return_status::error);
    ASSERT_EQ(gno, -1);
    tsid_map_lock.unlock();
  }

  // This test verifies automatic GTID generation performed by the
  // Gtid_generator class and used during certification process in GR.
  // Corner case - gno exhaustion, get_next_available_gtid fails
  static void test_gno_exhaustion_2() {
    Gtid_generator gen;
    gen.initialize(1);

    Checkable_rwlock tsid_map_lock;
    Tsid_map tsid_map(&tsid_map_lock);
    Gtid_set set(&tsid_map, &tsid_map_lock);

    tsid_map_lock.wrlock();
    Id member_id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", tsid_map, set);

    set.add_gtid_text(
        "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1-9223372036854775806");
    gen.recompute(set);
    auto [gno, res] = gen.get_next_available_gtid(member_id.m_uuid.c_str(),
                                                  member_id.m_sidno, set);
    ASSERT_EQ(res, mysql::utils::Return_status::error);
    ASSERT_EQ(gno, -1);
    tsid_map_lock.unlock();
  }
};

TEST_F(GtidGeneratorTest, CheckGeneratedGtids) {
  // test generated GTIDs with a generator parametrized with a custom block size
  GtidGeneratorTest::test_generated_gtids(1);
  GtidGeneratorTest::test_generated_gtids(10);
  GtidGeneratorTest::test_generated_gtids(15);
  GtidGeneratorTest::test_generated_gtids(53);
  GtidGeneratorTest::test_generated_gtids(500);
  GtidGeneratorTest::test_generated_gtids(5000);
  GtidGeneratorTest::test_gno_exhaustion();
  GtidGeneratorTest::test_gno_exhaustion_2();
}

}  // namespace gr::unittest
