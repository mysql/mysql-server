/*
  Copyright (c) 2016, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * Tests the metadata cache plugin implementation.
 */

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gmock/gmock.h>

#include "cluster_metadata_gr.h"
#include "mock_metadata.h"
#include "mock_metadata_factory.h"
#include "mysql/harness/tls_reload_component.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/metadata_cache.h"
#include "test/helpers.h"
#include "test/temp_directory.h"

/**
 * Constants that are used throughout the test cases.
 */
const unsigned kRouterId = 2;
const std::string kDefaultMetadataHost = "127.0.0.1";  // 127.0.0.1
const std::string kDefaultMetadataUser = "admin";      // admin
const std::string kDefaultMetadataPassword = "";       //
const int kDefaultMetadataPort = 32275;                // 32275
const std::chrono::milliseconds kDefaultMetadataTTL = std::chrono::seconds(1);
const std::chrono::milliseconds kDefaultAuthCacheTTL = std::chrono::seconds(2);
const std::chrono::milliseconds kDefaultAuthCacheRefreshInterval =
    std::chrono::milliseconds(-1);
const std::string kDefaultClusterName = "cluster_name_1";

const mysql_harness::TcpDestination bootstrap_server(kDefaultMetadataHost,
                                                     kDefaultMetadataPort);
const std::vector<mysql_harness::TcpDestination> metadata_server_vector = {
    bootstrap_server};

class ClusterMetadataTlsReloadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    reloader_.stop();
    reloader_.init();
  }

  void TearDown() override { reloader_.stop(); }

  mysql_harness::TlsReloadComponent &reloader_{
      mysql_harness::TlsReloadComponent::get_instance()};
};

class TestClusterMetadata : public GRClusterMetadata {
 public:
  using GRClusterMetadata::GRClusterMetadata;

  bool connect_with(mysqlrouter::MySQLSession &session) {
    return do_connect(session, {"127.0.0.1", 3306});
  }
};

class RecordingMySQLSession : public mysqlrouter::MySQLSession {
 public:
  using mysqlrouter::MySQLSession::connect;

  void set_ssl_options(mysql_ssl_mode mode, const std::string &tls_version,
                       const std::string &cipher, const std::string &ca,
                       const std::string &capath, const std::string &crl,
                       const std::string &crlpath) override {
    mode_ = mode;
    tls_version_ = tls_version;
    cipher_ = cipher;
    ca_ = ca;
    capath_ = capath;
    crl_ = crl;
    crlpath_ = crlpath;
  }

  void connect(const std::string &, unsigned int, const std::string &,
               const mysql_harness::SecureString &, const std::string &,
               const std::string &, int, int, unsigned long) override {
    connected_ = true;
  }

  mysql_ssl_mode mode_{};
  std::string tls_version_;
  std::string cipher_;
  std::string ca_;
  std::string capath_;
  std::string crl_;
  std::string crlpath_;
  bool connected_{false};
};

TEST_F(ClusterMetadataTlsReloadTest, RejectsInvalidCa) {
  TempDirectory directory;
  const auto ca = directory.name() + "/metadata-ca.pem";
  std::filesystem::copy_file(SSL_TEST_DATA_DIR "/cacert.pem", ca);

  metadata_cache::MetadataCacheMySQLSessionConfig session_config{};
  mysqlrouter::SSLOptions ssl_options;
  ssl_options.mode = "REQUIRED";
  ssl_options.ca = ca;

  GRClusterMetadata metadata{session_config, ssl_options};
  ASSERT_TRUE(std::filesystem::remove(ca));
  EXPECT_THROW(metadata.reload_tls(), std::system_error);
}

TEST_F(ClusterMetadataTlsReloadTest, RejectsInvalidCrl) {
  TempDirectory directory;
  const auto crl = directory.name() + "/metadata-crl.pem";
  std::filesystem::copy_file(SSL_TEST_DATA_DIR "/crl-client-revoked.crl", crl);

  metadata_cache::MetadataCacheMySQLSessionConfig session_config{};
  mysqlrouter::SSLOptions ssl_options;
  ssl_options.mode = "VERIFY_CA";
  ssl_options.ca = SSL_TEST_DATA_DIR "/cacert.pem";
  ssl_options.crl = crl;

  GRClusterMetadata metadata{session_config, ssl_options};
  ASSERT_TRUE(std::filesystem::remove(crl));
  EXPECT_THROW(metadata.reload_tls(), std::system_error);
}

TEST_F(ClusterMetadataTlsReloadTest, AcceptsNoOptionalTrustMaterial) {
  metadata_cache::MetadataCacheMySQLSessionConfig session_config{};
  mysqlrouter::SSLOptions ssl_options;
  ssl_options.mode = "REQUIRED";

  GRClusterMetadata metadata{session_config, ssl_options};
  EXPECT_NO_THROW(metadata.reload_tls());
}

TEST_F(ClusterMetadataTlsReloadTest, AcceptsCaWithoutCrl) {
  metadata_cache::MetadataCacheMySQLSessionConfig session_config{};
  mysqlrouter::SSLOptions ssl_options;
  ssl_options.mode = "REQUIRED";
  ssl_options.ca = SSL_TEST_DATA_DIR "/cacert.pem";

  GRClusterMetadata metadata{session_config, ssl_options};
  EXPECT_NO_THROW(metadata.reload_tls());
}

TEST_F(ClusterMetadataTlsReloadTest,
       CallbackPublishesCaAndCrlForNewConnection) {
  TempDirectory directory;
  const auto ca = directory.name() + "/metadata-ca.pem";
  const auto crl = directory.name() + "/metadata-crl.pem";
  std::filesystem::copy_file(SSL_TEST_DATA_DIR "/cacert.pem", ca);
  std::filesystem::copy_file(SSL_TEST_DATA_DIR "/crl-client-revoked.crl", crl);

  metadata_cache::MetadataCacheMySQLSessionConfig session_config{};
  mysqlrouter::SSLOptions ssl_options;
  ssl_options.mode = "VERIFY_CA";
  ssl_options.ca = ca;
  ssl_options.crl = crl;
  TestClusterMetadata metadata{session_config, ssl_options};

  std::filesystem::copy_file(SSL_TEST_DATA_DIR "/crl-ca-cert.pem", ca,
                             std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(SSL_TEST_DATA_DIR "/crl-server-revoked.crl", crl,
                             std::filesystem::copy_options::overwrite_existing);

  std::mutex mutex;
  std::condition_variable cv;
  bool callback_complete{false};
  bool callback_success{false};
  const auto id = reloader_.register_callback(
      "metadata_cache", [&]() -> mysql_harness::TlsReloadComponent::Result {
        try {
          metadata.reload_tls();
          callback_success = true;
        } catch (...) {
          callback_success = false;
        }
        {
          std::lock_guard<std::mutex> lock{mutex};
          callback_complete = true;
        }
        cv.notify_one();
        if (callback_success)
          return mysql_harness::TlsReloadComponent::Result{};
        return stdx::unexpected(std::string{});
      });

  reloader_.request_reload();
  {
    std::unique_lock<std::mutex> lock{mutex};
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds{2},
                            [&] { return callback_complete; }));
  }
  ASSERT_TRUE(callback_success);

  RecordingMySQLSession session;
  ASSERT_TRUE(metadata.connect_with(session));
  EXPECT_TRUE(session.connected_);
  EXPECT_EQ(SSL_MODE_VERIFY_CA, session.mode_);
  EXPECT_EQ(ca, session.ca_);
  EXPECT_EQ(crl, session.crl_);

  reloader_.unregister_callback(id);
}

using metadata_cache::ManagedInstance;

class MetadataCachePluginTest : public ::testing::Test {
 public:
  MockNG mf;

  MetadataCachePluginTest()
      : mf({{kDefaultMetadataUser, kDefaultMetadataPassword}, 1, 1, 1}) {}

  void SetUp() override {
    std::vector<ManagedInstance> instance_vector_1;

    metadata_cache::MetadataCacheAPI::instance()->set_instance_factory(
        &mock_metadata_factory_get_instance);

    metadata_cache::MetadataCacheAPI::instance()->cache_init(
        mysqlrouter::ClusterType::GR_V2, kRouterId, "", metadata_server_vector,
        {kDefaultMetadataTTL, kDefaultAuthCacheTTL,
         kDefaultAuthCacheRefreshInterval},
        mysqlrouter::SSLOptions(),
        {mysqlrouter::TargetCluster::TargetType::ByName, kDefaultClusterName},
        {{kDefaultMetadataUser, kDefaultMetadataPassword}, 2, 1, 0},
        metadata_cache::RouterAttributes{});
    metadata_cache::MetadataCacheAPI::instance()->cache_start();
    int count = 1;
    /**
     * Wait until the plugin is completely initialized. Since
     * the plugin initialization is started on a separate thread,
     * we are required to wait until the cache is populated.
     */
    while (instance_vector_1.size() != 3) {
      try {
        instance_vector_1 = cache_api_->get_cluster_nodes();
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
 * Test that the list of servers that are part of a cluster is accurate.
 */
TEST_F(MetadataCachePluginTest, ValidCluserTest_1) {
  std::vector<ManagedInstance> instance_vector_1 =
      cache_api_->get_cluster_nodes();

  EXPECT_EQ(instance_vector_1[0], mf.ms1);
  EXPECT_EQ(instance_vector_1[1], mf.ms2);
  EXPECT_EQ(instance_vector_1[2], mf.ms3);
}

int main(int argc, char *argv[]) {
  init_test_logger();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
