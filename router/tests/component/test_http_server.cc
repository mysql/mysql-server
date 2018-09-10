/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifdef _WIN32
// ensure windows.h doesn't expose min() nor max()
#define NOMINMAX
#endif

#include <gtest/gtest.h>

#include "config_builder.h"
#include "dim.h"
#include "gtest_testname.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "temp_dir.h"

Path g_origin_path;

// base-class to init RouterComponentTest before we launch_* anything
class HttpServerTestBase : public RouterComponentTest {
 public:
  HttpServerTestBase() {
    set_origin(g_origin_path);

    RouterComponentTest::init();
  }
};

using HttpServerStaticFilesParams =
    std::tuple<std::string, std::string, unsigned int>;

class HttpServerStaticFilesTest
    : public HttpServerTestBase,
      public ::testing::Test,
      public ::testing::WithParamInterface<HttpServerStaticFilesParams> {
 public:
  HttpServerStaticFilesTest()
      : port_pool_{},
        http_port_{port_pool_.get_next_available()},
        conf_dir_{},
        conf_file_{create_config_file(
            conf_dir_.name(),
            ConfigBuilder::build_section(
                "http_server", {{"port", std::to_string(http_port_)},
                                {"static_folder", get_data_dir().str()}}))},
        http_server_{launch_router({"-c", conf_file_})} {}

 protected:
  TcpPortPool port_pool_;
  uint16_t http_port_;
  std::string http_hostname_ = "127.0.0.1";
  TempDirectory conf_dir_;
  std::string conf_file_;
  CommandHandle http_server_;
};

/**
 * ensure GET requests for static files work.
 *
 * - start the http-server component
 * - make a client connect to the http-server
 */
TEST_P(HttpServerStaticFilesTest, ensure) {
  SCOPED_TRACE("// wait http port connectable");
  ASSERT_TRUE(wait_for_port_ready(http_port_, 1000))
      << http_server_.get_full_output();

  std::string http_uri = std::get<1>(GetParam());
  SCOPED_TRACE("// connecting " + http_hostname_ + ":" +
               std::to_string(http_port_) + " for " + http_uri);

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname_, http_port_);
  auto req = rest_client.request_sync(HttpMethod::Get, http_uri);
  ASSERT_TRUE(req);
  ASSERT_EQ(req.get_response_code(), std::get<2>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    http_static, HttpServerStaticFilesTest,
    ::testing::Values(
        std::make_tuple("dir, no index-file", "/", 403),
        std::make_tuple("file exists", "/my_port.js", 200),
        std::make_tuple("not leave root", "/../my_port.js",
                        200)  // assumes my_root.js is only in datadir
        ),
    [](const ::testing::TestParamInfo<HttpServerStaticFilesParams> &info) {
      return gtest_sanitize_param_name(std::get<0>(info.param) + " " +
                                       std::to_string(std::get<2>(info.param)));
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
