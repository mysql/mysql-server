/*
Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <chrono>
#include <thread>

#include <gmock/gmock.h>

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;

Path g_origin_path;

struct Credentials {
  std::string username;
  std::string password_hash;
};

struct Auth_data {
  Credentials credentials;
  std::string privileges;
  std::string auth_method{"modular_crypt_format"};
};

struct Http_response_details {
  HttpStatusCode::key_type code;
  std::string type;
};

class MetadataHttpAuthTest : public RouterComponentTest {
 protected:
  std::string get_metadata_cache_section(
      const std::chrono::milliseconds ttl = kTTL,
      const std::chrono::milliseconds auth_cache_ttl = kAuthCacheTTL,
      const std::chrono::milliseconds auth_cache_refresh_interval =
          kAuthCacheRefreshRate) const {
    const auto ttl_str =
        std::to_string(std::chrono::duration<double>(ttl).count());
    const auto auth_cache_ttl_str =
        std::to_string(std::chrono::duration<double>(auth_cache_ttl).count());
    const auto auth_cache_refresh_interval_str = std::to_string(
        std::chrono::duration<double>(auth_cache_refresh_interval).count());

    return kMetadataCacheSectionBase + "ttl=" + ttl_str +
           "\n"
           "auth_cache_ttl=" +
           auth_cache_ttl_str +
           "\n"
           "auth_cache_refresh_interval=" +
           auth_cache_refresh_interval_str +
           "\n"
           "\n";
  }

  std::string get_metadata_cache_routing_section(
      const uint16_t router_port) const {
    const std::string result = "[routing:test_default" +
                               std::to_string(router_port) +
                               "]\n"
                               "bind_port=" +
                               std::to_string(router_port) + "\n" +
                               "destinations=metadata-cache://test/"
                               "default?role=PRIMARY\nprotocol=classic\n"
                               "routing_strategy=first-available\n";

    return result;
  }

  virtual std::string auth_backend_settings() const {
    return "[http_auth_backend:somebackend]\n"
           "backend=metadata_cache\n";
  }

  std::string get_rest_section() const {
    const std::string result =
        "[http_server]\n"
        "port=" +
        std::to_string(http_server_port) +
        "\n"
        "[rest_router]\n"
        "require_realm = somerealm\n"
        "[rest_api]\n"
        "[http_auth_realm:somerealm]\n"
        "backend = somebackend\n"
        "method = basic\n"
        "name = test\n" +
        auth_backend_settings() +
        "[rest_routing]\n"
        "require_realm = somerealm\n";

    return result;
  }

  std::string create_state_file_content(const std::string &cluster_id,
                                        const unsigned view_id,
                                        const uint16_t &metadata_server_port) {
    const std::string metadata_servers =
        "\"mysql://127.0.0.1:" + std::to_string(metadata_server_port) + "\"";
    // clang-format off
    const std::string result =
      "{"
         R"("version": "1.0.0",)"
         R"("metadata-cache": {)"
           R"("group-replication-id": ")" + cluster_id + R"(",)"
           R"("cluster-metadata-servers": [)" + metadata_servers + "],"
           R"("view-id":)" + std::to_string(view_id) +
          "}"
        "}";
    // clang-format on

    return result;
  }

  auto &launch_router(
      const std::string &metadata_cache_section,
      const int expected_errorcode = EXIT_SUCCESS,
      const std::chrono::milliseconds wait_for_notify_ready = 30s) {
    const std::string &temp_test_dir_str = temp_test_dir.name();

    const auto &routing_section =
        get_metadata_cache_routing_section(router_port);

    const auto &rest_section = get_rest_section();

    SCOPED_TRACE("// Create a router state file");
    const std::string state_file = create_state_file(
        temp_test_dir_str,
        create_state_file_content(cluster_id, view_id, cluster_node_port));

    const std::string masterkey_file =
        Path(temp_test_dir_str).join("master.key").str();
    const std::string keyring_file =
        Path(temp_test_dir_str).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // launch the router with metadata-cache configuration
    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    default_section["dynamic_state"] = state_file;
    const std::string conf_file = create_config_file(
        temp_test_dir_str,
        metadata_cache_section + routing_section + rest_section,
        &default_section);

    return ProcessManager::launch_router(
        {"-c", conf_file}, expected_errorcode, /*catch_stderr=*/true,
        /*with_sudo=*/false, wait_for_notify_ready);
  }

  void set_mock_metadata(
      const std::vector<Auth_data> &auth_data_collection,
      const uint16_t http_port, const std::string &gr_id,
      const uint16_t cluster_node_port, const bool error_on_md_query = false,
      const unsigned primary_id = 0, const uint64_t view_id = 0,
      const mysqlrouter::MetadataSchemaVersion md_version = {2, 0, 3}) const {
    auto json_doc = mock_GR_metadata_as_json(
        gr_id, {cluster_node_port}, primary_id, view_id, error_on_md_query);

    JsonAllocator allocator;
    JsonValue nodes(rapidjson::kArrayType);
    for (const auto &auth_data : auth_data_collection) {
      JsonValue node(rapidjson::kArrayType);
      node.PushBack(JsonValue(auth_data.credentials.username.c_str(),
                              auth_data.credentials.username.size(), allocator),
                    allocator);

      node.PushBack(
          JsonValue(auth_data.credentials.password_hash.c_str(),
                    auth_data.credentials.password_hash.size(), allocator),
          allocator);

      node.PushBack(JsonValue(auth_data.privileges.c_str(),
                              auth_data.privileges.size(), allocator),
                    allocator);

      node.PushBack(JsonValue(auth_data.auth_method.c_str(),
                              auth_data.auth_method.size(), allocator),
                    allocator);

      nodes.PushBack(node, allocator);
    }

    json_doc.AddMember("rest_user_credentials", nodes, allocator);

    JsonValue metadata_version_node(rapidjson::kArrayType);
    metadata_version_node.PushBack(md_version.major, allocator);
    metadata_version_node.PushBack(md_version.minor, allocator);
    metadata_version_node.PushBack(md_version.patch, allocator);
    json_doc.AddMember("metadata_version", metadata_version_node, allocator);

    const auto json_str = json_to_string(json_doc);

    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
  }

  int get_rest_auth_queries_count(const std::string &json_string) const {
    rapidjson::Document json_doc;
    json_doc.Parse(json_string.c_str());
    if (json_doc.HasMember("rest_auth_query_count")) {
      EXPECT_TRUE(json_doc["rest_auth_query_count"].IsInt());
      return json_doc["rest_auth_query_count"].GetInt();
    }
    return 0;
  }

  int wait_for_rest_auth_query(int expected_rest_auth_query_count,
                               uint16_t http_port) const {
    int rest_auth_queries_count{0}, retries{0};
    do {
      std::this_thread::sleep_for(50ms);
      const std::string server_globals =
          MockServerRestClient(http_port).get_globals_as_json_string();
      rest_auth_queries_count = get_rest_auth_queries_count(server_globals);
    } while (rest_auth_queries_count < expected_rest_auth_query_count &&
             retries++ < 100);

    return rest_auth_queries_count;
  }

  void SetUp() override {
    RouterComponentTest::SetUp();
    ProcessManager::set_origin(g_origin_path);

    cluster_node_port = port_pool_.get_next_available();
    cluster_http_port = port_pool_.get_next_available();
    http_server_port = port_pool_.get_next_available();

    SCOPED_TRACE("// Launch a server mock that will act as our cluster member");
    const auto trace_file =
        get_data_dir().join("metadata_http_auth_backend.js").str();

    cluster_node = &ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_node_port, EXIT_SUCCESS, false, cluster_http_port);

    router_port = port_pool_.get_next_available();

    uri = std::string(rest_api_basepath) + "/routes/test_default" +
          std::to_string(router_port) + "/status";
  }

  const std::string kMetadataCacheSectionBase =
      "[metadata_cache:test]\n"
      "cluster_type=gr\n"
      "router_id=1\n"
      "user=mysql_router1_user\n"
      "metadata_cluster=test\n"
      "connect_timeout=1\n";

  static const std::chrono::milliseconds kTTL;
  static const std::chrono::milliseconds kAuthCacheTTL;
  static const std::chrono::milliseconds kAuthCacheRefreshRate;
  static const std::string cluster_id;
  TempDirectory temp_test_dir;
  uint64_t view_id = 1;

  ProcessWrapper *cluster_node;
  uint16_t cluster_node_port;
  uint16_t cluster_http_port;
  uint16_t http_server_port;
  uint16_t router_port;

  std::string uri;
};

const std::chrono::milliseconds MetadataHttpAuthTest::kTTL = 200ms;
const std::chrono::milliseconds MetadataHttpAuthTest::kAuthCacheTTL = -1s;
const std::chrono::milliseconds MetadataHttpAuthTest::kAuthCacheRefreshRate =
    500ms;
const std::string MetadataHttpAuthTest::cluster_id =
    "3a0be5af-0022-11e8-9655-0800279e6a88";

const Credentials kTestUser1{
    "foobar",
    // hash for password="password"
    {0x24, 0x41, 0x24, 0x30, 0x30, 0x35, 0x24, 0x58, 0x54, 0x72, 0x6F, 0x7D,
     0x7D, 0x78, 0x6A, 0x62, 0x26, 0x7C, 0x65, 0x5C, 0x11, 0x3E, 0x0C, 0x09,
     0x04, 0x33, 0x25, 0x33, 0x79, 0x53, 0x35, 0x4F, 0x55, 0x33, 0x79, 0x45,
     0x6D, 0x53, 0x6D, 0x74, 0x46, 0x30, 0x64, 0x62, 0x6E, 0x6C, 0x69, 0x46,
     0x75, 0x6F, 0x33, 0x39, 0x7A, 0x49, 0x48, 0x77, 0x58, 0x35, 0x78, 0x59,
     0x62, 0x51, 0x53, 0x55, 0x41, 0x5A, 0x37, 0x49, 0x31, 0x43}};
const Credentials kTestUser2{
    "testuser",
    // hash for password="secret"
    {0x24, 0x41, 0x24, 0x30, 0x30, 0x35, 0x24, 0x3F, 0x44, 0x62, 0x49, 0x71,
     0x15, 0x52, 0x18, 0x71, 0x27, 0x42, 0x06, 0x04, 0x3E, 0x1E, 0x61, 0x08,
     0x40, 0x42, 0x29, 0x2E, 0x68, 0x4D, 0x33, 0x4B, 0x76, 0x4C, 0x41, 0x74,
     0x4C, 0x6C, 0x6F, 0x54, 0x43, 0x4F, 0x4B, 0x64, 0x2E, 0x4A, 0x69, 0x34,
     0x74, 0x53, 0x63, 0x4E, 0x6E, 0x79, 0x6A, 0x65, 0x38, 0x55, 0x4B, 0x68,
     0x4F, 0x2F, 0x63, 0x70, 0x71, 0x79, 0x68, 0x36, 0x54, 0x2E}};

const Http_response_details ResponseUnauthorized{HttpStatusCode::Unauthorized,
                                                 kContentTypeHtmlCharset};
const Http_response_details ResponseForbidden{HttpStatusCode::Forbidden,
                                              kContentTypeHtmlCharset};
const Http_response_details ResponseOk{HttpStatusCode::Ok, kContentTypeJson};

struct BasicMetadataHttpAuthTestParams {
  std::string username;
  std::string password;
  Auth_data cached_info;
  Http_response_details http_response;
};

class BasicMetadataHttpAuthTest
    : public MetadataHttpAuthTest,
      public ::testing::WithParamInterface<BasicMetadataHttpAuthTestParams> {};

TEST_F(BasicMetadataHttpAuthTest, MetadataHttpAuthDefaultConfig) {
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, false, 0, view_id);

  SCOPED_TRACE("// Launch the router with the initial state file");
  launch_router(kMetadataCacheSectionBase);

  ASSERT_TRUE(wait_for_rest_endpoint_ready(uri, http_server_port));
  EXPECT_GT(wait_for_rest_auth_query(2, cluster_http_port), 0);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, "127.0.0.1", http_server_port, "foobar",
                         "password");

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                       HttpStatusCode::Ok, json_doc,
                                       kContentTypeJson));
}

TEST_F(BasicMetadataHttpAuthTest, UnsupportedMetadataSchemaVersion) {
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, false, 0, view_id, {1, 0, 0});

  SCOPED_TRACE("// Launch the router with the initial state file");
  launch_router(kMetadataCacheSectionBase);

  ASSERT_TRUE(wait_for_rest_endpoint_ready(uri, http_server_port));

  IOContext io_ctx;
  RestClient rest_client(io_ctx, "127.0.0.1", http_server_port, "foobar",
                         "password");

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                       HttpStatusCode::Unauthorized, json_doc,
                                       kContentTypeHtmlCharset));
}

TEST_P(BasicMetadataHttpAuthTest, BasicMetadataHttpAuth) {
  set_mock_metadata(
      {{GetParam().cached_info.credentials, GetParam().cached_info.privileges,
        GetParam().cached_info.auth_method}},
      cluster_http_port, cluster_id, cluster_node_port, false, 0, view_id);

  SCOPED_TRACE("// Launch the router with the initial state file");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, kAuthCacheTTL, kAuthCacheRefreshRate);
  launch_router(metadata_cache_section);

  ASSERT_TRUE(wait_for_rest_endpoint_ready(uri, http_server_port));
  EXPECT_GT(wait_for_rest_auth_query(2, cluster_http_port), 0);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, "127.0.0.1", http_server_port,
                         GetParam().username, GetParam().password);

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                       GetParam().http_response.code, json_doc,
                                       GetParam().http_response.type));
}

INSTANTIATE_TEST_SUITE_P(
    BasicMetadataHttpAuth, BasicMetadataHttpAuthTest,
    ::testing::Values(
        // matching user and password
        BasicMetadataHttpAuthTestParams{
            "foobar", "password", {kTestUser1, ""}, ResponseOk},
        // not matching username
        BasicMetadataHttpAuthTestParams{
            "foobar", "password", {kTestUser2, ""}, ResponseUnauthorized},
        // matching username, wrong password
        BasicMetadataHttpAuthTestParams{
            "foobar", "ooops", {kTestUser1, ""}, ResponseUnauthorized},
        // empty username
        BasicMetadataHttpAuthTestParams{
            "", "secret", {kTestUser2, ""}, ResponseUnauthorized},
        // empty password
        BasicMetadataHttpAuthTestParams{
            "nopwd", "", {{"nopwd", ""}, ""}, ResponseOk},
        // username too long
        BasicMetadataHttpAuthTestParams{std::string(260, 'x'),
                                        "secret",
                                        {kTestUser2, ""},
                                        ResponseUnauthorized},
        // matching user and password, but with privileges
        BasicMetadataHttpAuthTestParams{
            "foobar", "password", {kTestUser1, "{}"}, ResponseForbidden},
        // invalid JSON string, user not added to auth cache
        BasicMetadataHttpAuthTestParams{
            "foobar", "password", {kTestUser1, "xy{}z"}, ResponseUnauthorized},
        // unsupported authentication_method
        BasicMetadataHttpAuthTestParams{
            "foobar",
            "password",
            {kTestUser1, "", "mysql_native_password"},
            ResponseUnauthorized},
        // MCF missing rounds
        BasicMetadataHttpAuthTestParams{
            "x",
            "secret",
            {{"x",
              "$A$$1=>5szy1\\':\\`\\'yv!@v0ZZkRT04EOc."
              "sCRxFmoV30RhdtDdvt1N8rtZwmNO4re8"},
             ""},
            ResponseUnauthorized},
        // MCF missing digest
        BasicMetadataHttpAuthTestParams{
            "x",
            "secret",
            {{"x", "$A$005$1=>5szy1\\':\\`\\'yv!@v"}, ""},
            ResponseUnauthorized},
        // MCF missing salt and digest
        BasicMetadataHttpAuthTestParams{
            "x", "secret", {{"x", "$A$005$"}, ""}, ResponseUnauthorized},
        // MCF with unsupported identifier
        BasicMetadataHttpAuthTestParams{
            "x",
            "secret",
            {{"x",
              "$_$005$1=>5szy1\\':\\`\\'yv!@v0ZZkRT04EOc."
              "sCRxFmoV30RhdtDdvt1N8rtZwmNO4re8"},
             ""},
            ResponseUnauthorized}));

class FileAuthBackendWithMetadataAuthSettings : public MetadataHttpAuthTest {
 public:
  const mysql_harness::Path passwd_file =
      mysql_harness::Path(temp_test_dir.name()).join("passwd");

  std::string auth_backend_settings() const override {
    return "[http_auth_backend:somebackend]\n"
           "backend=file\n"
           "filename=" +
           passwd_file.str() + "\n";
  }
};

TEST_F(FileAuthBackendWithMetadataAuthSettings, MixedBackendSettings) {
  ProcessWrapper::OutputResponder responder{
      [](const std::string &line) -> std::string {
        if (line == "Please enter password: ")
          return std::string(kRestApiPassword) + "\n";

        return "";
      }};

  auto &cmd = launch_command(
      ProcessManager::get_origin().join("mysqlrouter_passwd").str(),
      {"set", passwd_file.str(), kRestApiUsername}, EXIT_SUCCESS, true, -1ms,
      responder);
  EXPECT_EQ(cmd.wait_for_exit(), 0) << cmd.get_full_output();

  set_mock_metadata({}, cluster_http_port, cluster_id, cluster_node_port, false,
                    0, view_id);

  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, kAuthCacheTTL, kAuthCacheRefreshRate);
  // It should be possible to launch router with backend=file and with
  // additional metadata_cache auth settings
  launch_router(metadata_cache_section);
  ASSERT_TRUE(wait_for_port_ready(router_port));
}

class InvalidMetadataHttpAuthTimersTest
    : public MetadataHttpAuthTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(InvalidMetadataHttpAuthTimersTest, InvalidMetadataHttpAuthTimers) {
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, false, 0, view_id);

  SCOPED_TRACE("// Launch the router with the initial state file");
  auto &router =
      launch_router(kMetadataCacheSectionBase + GetParam(), EXIT_FAILURE, -1s);
  check_exit_code(router, EXIT_FAILURE);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidMetadataHttpAuthTimers, InvalidMetadataHttpAuthTimersTest,
    ::testing::Values(
        std::string{"auth_cache_ttl=2.5\nauth_cache_refresh_interval=2.51\n"},
        std::string{"auth_cache_ttl=2\nttl=3\n"},
        std::string{"auth_cache_refresh_interval=1\nttl=2\n"},
        std::string{"auth_cache_ttl=3600.01\n"},
        std::string{"auth_cache_ttl=0.0001\n"},
        std::string{"auth_cache_ttl=-0.1\n"},
        std::string{"auth_cache_ttl=-1.1\n"},
        std::string{"auth_cache_ttl=xxx\n"},
        std::string{"auth_cache_refresh_interval=3600.01\n"},
        std::string{"auth_cache_refresh_interval=0.0001\n"},
        std::string{"auth_cache_refresh_interval=yyy\n"}));

class ValidMetadataHttpAuthTimersTest
    : public MetadataHttpAuthTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(ValidMetadataHttpAuthTimersTest, ValidMetadataHttpAuthTimers) {
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, false, 0, view_id);

  SCOPED_TRACE("// Launch the router with the initial state file");
  launch_router(kMetadataCacheSectionBase + "ttl=0.001\n" + GetParam());
  ASSERT_TRUE(wait_for_port_ready(router_port));
}

INSTANTIATE_TEST_SUITE_P(
    ValidMetadataHttpAuthTimers, ValidMetadataHttpAuthTimersTest,
    ::testing::Values(
        std::string{
            "auth_cache_ttl=0.001\nauth_cache_refresh_interval=0.001\n"},
        std::string{"auth_cache_ttl=3600\n"},
        std::string{"auth_cache_ttl=3600.00\n"},
        std::string{"auth_cache_refresh_interval=0.001\n"},
        std::string{"auth_cache_refresh_interval=3600\n"},
        std::string{"auth_cache_refresh_interval=3600.00\n"}));

class MetadataHttpAuthTestCustomTimers
    : public MetadataHttpAuthTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(MetadataHttpAuthTestCustomTimers, MetadataHttpAuthCustomTimers) {
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, false, 0, view_id);

  SCOPED_TRACE("// Launch the router with the initial state file");
  launch_router(kMetadataCacheSectionBase + GetParam());

  ASSERT_TRUE(wait_for_port_ready(router_port));
  ASSERT_TRUE(wait_for_rest_endpoint_ready(uri, http_server_port));
  EXPECT_GT(wait_for_rest_auth_query(2, cluster_http_port), 0);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, "127.0.0.1", http_server_port, "foobar",
                         "password");

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                       HttpStatusCode::Ok, json_doc,
                                       kContentTypeJson));
}

INSTANTIATE_TEST_SUITE_P(
    MetadataHttpAuthCustomTimers, MetadataHttpAuthTestCustomTimers,
    ::testing::Values(
        std::string{"auth_cache_ttl=3600\nauth_cache_refresh_interval=2\n"},
        std::string{"auth_cache_ttl=3\n"}, std::string{"auth_cache_ttl=-1\n"},
        std::string{"auth_cache_refresh_interval=1\n"},
        std::string{"auth_cache_refresh_interval=1.567\n"},
        std::string{"auth_cache_ttl=2.567\n"}));

TEST_F(MetadataHttpAuthTest, ExpiredAuthCacheTTL) {
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, false, 0, view_id);

  std::chrono::milliseconds cache_ttl = kAuthCacheRefreshRate * 4;
  SCOPED_TRACE("// Launch the router with the initial state file");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, cache_ttl, kAuthCacheRefreshRate);
  launch_router(metadata_cache_section);

  ASSERT_TRUE(wait_for_rest_endpoint_ready(uri, http_server_port));
  EXPECT_GT(wait_for_rest_auth_query(2, cluster_http_port), 0);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, "127.0.0.1", http_server_port, "foobar",
                         "password");

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                       HttpStatusCode::Ok, json_doc,
                                       kContentTypeJson));

  const bool fail_on_md_query = true;
  // Start to fail metadata cache updates
  set_mock_metadata({{kTestUser1, ""}}, cluster_http_port, cluster_id,
                    cluster_node_port, fail_on_md_query, 0, view_id);

  // wait long enough for the auth cache to expire
  std::this_thread::sleep_for(cache_ttl);

  ASSERT_NO_FATAL_FAILURE(request_json(rest_client, uri, HttpMethod::Get,
                                       HttpStatusCode::Unauthorized, json_doc,
                                       kContentTypeHtmlCharset));
}

struct MetadataAuthCacheUpdateParams {
  std::vector<Auth_data> first_auth_cache_data_set;
  Http_response_details first_http_response;
  std::vector<Auth_data> second_auth_cache_data_set;
  Http_response_details second_http_response;
};

class MetadataAuthCacheUpdate
    : public MetadataHttpAuthTest,
      public ::testing::WithParamInterface<MetadataAuthCacheUpdateParams> {};

TEST_P(MetadataAuthCacheUpdate, AuthCacheUpdate) {
  set_mock_metadata(GetParam().first_auth_cache_data_set, cluster_http_port,
                    cluster_id, cluster_node_port, false, 0, view_id);

  SCOPED_TRACE("// Launch the router with the initial state file");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, kAuthCacheTTL, kAuthCacheRefreshRate);
  launch_router(metadata_cache_section);

  ASSERT_TRUE(wait_for_rest_endpoint_ready(uri, http_server_port));
  EXPECT_GT(wait_for_rest_auth_query(2, cluster_http_port), 0);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, "127.0.0.1", http_server_port, "foobar",
                         "password");

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(
      rest_client, uri, HttpMethod::Get, GetParam().first_http_response.code,
      json_doc, GetParam().first_http_response.type));

  // Update authentication metadata
  set_mock_metadata(GetParam().second_auth_cache_data_set, cluster_http_port,
                    cluster_id, cluster_node_port, false, 0, view_id);

  // auth_cache is updated
  EXPECT_GT(wait_for_rest_auth_query(2, cluster_http_port), 0);

  ASSERT_NO_FATAL_FAILURE(request_json(
      rest_client, uri, HttpMethod::Get, GetParam().second_http_response.code,
      json_doc, GetParam().second_http_response.type));
}

INSTANTIATE_TEST_SUITE_P(
    AuthCacheUpdate, MetadataAuthCacheUpdate,
    ::testing::Values(
        // add user
        MetadataAuthCacheUpdateParams{{{kTestUser2, ""}},
                                      ResponseUnauthorized,
                                      {{kTestUser1, ""}, {kTestUser2, ""}},
                                      ResponseOk},
        // add user privileges
        MetadataAuthCacheUpdateParams{{{kTestUser1, ""}},
                                      ResponseOk,
                                      {{kTestUser1, "{\"foo\": \"bar\"}"}},
                                      ResponseForbidden},
        // change password
        MetadataAuthCacheUpdateParams{
            {{kTestUser1, ""}},
            ResponseOk,
            {{{kTestUser1.username, kTestUser2.password_hash}, ""}},
            ResponseUnauthorized},
        // rm user
        MetadataAuthCacheUpdateParams{{{kTestUser1, ""}, {kTestUser2, ""}},
                                      ResponseOk,
                                      {{kTestUser2, ""}},
                                      ResponseUnauthorized}));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
