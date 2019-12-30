/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * Tests the metadata cache plugin implementation.
 */

#include "mock_metadata.h"
#include "mysqlrouter/metadata_cache.h"
#include "tcp_address.h"
#include "test/helpers.h"

#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include "gmock/gmock.h"

/**
 * Constants that are used throughout the test cases.
 */

const std::string replication_group_id = "0000-0000-0001";
const std::string kDefaultTestReplicaset_1 = "replicaset-1";  // replicaset-1
const std::string kDefaultTestReplicaset_2 = "replicaset-2";  // replicaset-2
const std::string kDefaultTestReplicaset_3 = "replicaset-3";  // replicaset-3

const std::string kDefaultMetadataHost = "127.0.0.1";  // 127.0.0.1
const std::string kDefaultMetadataUser = "admin";      // admin
const std::string kDefaultMetadataPassword = "";       //
const int kDefaultMetadataPort = 32275;                // 32275
const std::chrono::milliseconds kDefaultMetadataTTL = std::chrono::seconds(1);
const std::string kDefaultMetadataReplicaset = "replicaset-1";

const mysql_harness::TCPAddress bootstrap_server(kDefaultMetadataHost,
                                                 kDefaultMetadataPort);
const std::vector<mysql_harness::TCPAddress> metadata_server_vector = {
    bootstrap_server};

using metadata_cache::ManagedInstance;
using std::thread;

class MetadataCachePluginTest : public ::testing::Test {
 public:
  MockNG mf;

  MetadataCachePluginTest()
      : mf(kDefaultMetadataUser, kDefaultMetadataPassword, 1, 1, 1,
           kDefaultMetadataTTL) {}

  void SetUp() override {
    std::vector<ManagedInstance> instance_vector_1;
    metadata_cache::MetadataCacheAPI::instance()->cache_init(
        replication_group_id, metadata_server_vector,
        {kDefaultMetadataUser, kDefaultMetadataPassword}, kDefaultMetadataTTL,
        mysqlrouter::SSLOptions(), kDefaultMetadataReplicaset, 1, 1);
    metadata_cache::MetadataCacheAPI::instance()->cache_start();
    int count = 1;
    /**
     * Wait until the plugin is completely initialized. Since
     * the plugin initialization is started on a separate thread,
     * we are required to wait until the cache is populated.
     */
    while (instance_vector_1.size() != 3) {
      try {
        instance_vector_1 =
            cache_api_->lookup_replicaset(kDefaultTestReplicaset_1)
                .instance_vector;
      } catch (const std::runtime_error &exc) {
        /**
         * If the lookup fails after 5 attempts it points to an error
         * in the cache initialization. This is an exception situation.
         */
        if (count++ >= 5) throw exc;
      }
      /**
       * Sleep before retrying the lookup.
       */
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }

  void TearDown() override {
    metadata_cache::MetadataCacheAPI::instance()->cache_stop();
  }

  metadata_cache::MetadataCacheAPIBase *cache_api_{
      metadata_cache::MetadataCacheAPI::instance()};
};

/**
 * Test that looking up an invalid replicaset returns a empty list.
 */
TEST_F(MetadataCachePluginTest, InvalidReplicasetTest) {
  EXPECT_TRUE(cache_api_->lookup_replicaset("InvalidReplicaset")
                  .instance_vector.empty());
}

/**
 * Test that the list of servers that are part of a replicaset is accurate.
 */
TEST_F(MetadataCachePluginTest, ValidReplicasetTest_1) {
  std::vector<ManagedInstance> instance_vector_1 =
      cache_api_->lookup_replicaset(kDefaultTestReplicaset_1).instance_vector;

  EXPECT_EQ(instance_vector_1[0], mf.ms1);
  EXPECT_EQ(instance_vector_1[1], mf.ms2);
  EXPECT_EQ(instance_vector_1[2], mf.ms3);
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
