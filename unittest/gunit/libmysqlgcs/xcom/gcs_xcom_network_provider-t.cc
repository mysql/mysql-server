/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include <atomic>
#include <chrono>
#include <thread>

#include "unittest/gunit/libmysqlgcs/include/gcs_base_test.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/xcom_network_provider.h"

namespace gcs_xcom_networkprovidertest {
class XComNetworkProviderTest : public GcsBaseTest {
 protected:
  XComNetworkProviderTest() = default;

  void SetUp() override {}

  void TearDown() override {}
};

class CountingXcomNetworkProvider : public Xcom_network_provider {
 public:
  int close_connection(const Network_connection &connection) override {
    const int result = Xcom_network_provider::close_connection(connection);
    m_last_closed_fd.store(connection.fd);
    m_last_close_result.store(result);
    m_close_count.fetch_add(1);
    return result;
  }

  unsigned int close_count() const { return m_close_count.load(); }

  int last_closed_fd() const { return m_last_closed_fd.load(); }

  int last_close_result() const { return m_last_close_result.load(); }

 private:
  std::atomic<unsigned int> m_close_count{0};
  std::atomic<int> m_last_closed_fd{-1};
  std::atomic<int> m_last_close_result{-1};
};

TEST_F(XComNetworkProviderTest, StartAndStopTestMissingPort) {
  Xcom_network_provider net_provider;

  ASSERT_TRUE(net_provider.start().first);
  net_provider.stop();
}

TEST_F(XComNetworkProviderTest, StartAndStopTest) {
  Xcom_network_provider net_provider;
  Network_configuration_parameters params;
  params.port = 12345;
  net_provider.configure(params);

  ASSERT_FALSE(net_provider.start().first);

  // Make sure that the first one has started correctly
  My_xp_util::sleep_seconds(5);

  net_provider.stop();
}

#ifndef _WIN32  // Windows is sensitive to successive binds operations.
                // To avoid adding big sleeps to the test, we will just skip
                // them as a whole.
TEST_F(XComNetworkProviderTest, StartAgainAndStopTest) {
  Xcom_network_provider net_provider;
  Network_configuration_parameters params;
  params.port = 12345;
  net_provider.configure(params);

  ASSERT_FALSE(net_provider.start().first);

  // Make sure that the first one has started correctly
  My_xp_util::sleep_seconds(5);

  ASSERT_TRUE(net_provider.start().first);

  net_provider.stop();
}

TEST_F(XComNetworkProviderTest, StartAndStopTestWithError) {
  Xcom_network_provider net_provider1, net_provider2;
  Network_configuration_parameters params;
  params.port = 12345;
  net_provider1.configure(params);
  net_provider2.configure(params);

  ASSERT_FALSE(net_provider1.start().first);

  // Make sure that the first one has started correctly
  My_xp_util::sleep_seconds(5);

  ASSERT_TRUE(net_provider2.start().first);

  net_provider1.stop();
}
#endif

TEST_F(XComNetworkProviderTest, CreateConnectionToSelfTest) {
  Xcom_network_provider net_provider;
  Network_configuration_parameters params;
  params.port = 12345;
  net_provider.configure(params);

  ASSERT_FALSE(net_provider.start().first);

  // Make sure that it has started correctly
  My_xp_util::sleep_seconds(5);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_TRUE(new_connection.get() != nullptr);

  int const close_connection_retval =
      net_provider.close_connection(*new_connection);

  ASSERT_EQ(0, close_connection_retval);

  net_provider.stop();
}

TEST_F(XComNetworkProviderTest, BusyHandoffRejectsAndClosesIncomingConnection) {
  CountingXcomNetworkProvider net_provider;
  Network_configuration_parameters params{};
  params.port = 12346;
  net_provider.configure(params);

  // Keep the handoff slot occupied so that the native acceptor must reject
  // the next incoming connection instead of waiting for a consumer.
  auto *pending_connection = new Network_connection(-1);
  ASSERT_EQ(Network_connection_handoff_status::ACCEPTED,
            net_provider.set_new_connection(pending_connection));
  ASSERT_FALSE(net_provider.start().first);

  auto client_connection =
      net_provider.open_connection("localhost", params.port, {"", "", false});

  int rejected_fd = -1;
  if (client_connection != nullptr && client_connection->fd >= 0) {
    for (int attempt = 0; attempt != 500; ++attempt) {
      if (net_provider.close_count() != 0) {
        rejected_fd = net_provider.last_closed_fd();
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  EXPECT_NE(nullptr, client_connection);
  if (client_connection != nullptr) {
    EXPECT_GE(client_connection->fd, 0);
    EXPECT_NE(client_connection->fd, rejected_fd);
  }
  EXPECT_GE(rejected_fd, 0);
  EXPECT_EQ(1U, net_provider.close_count());
  EXPECT_EQ(0, net_provider.last_close_result());

  Network_connection *retrieved_connection = net_provider.get_new_connection();
  EXPECT_EQ(pending_connection, retrieved_connection);
  delete retrieved_connection;

  if (client_connection != nullptr && client_connection->fd >= 0) {
    EXPECT_EQ(0, net_provider.close_connection(*client_connection));
  }
  EXPECT_EQ(2U, net_provider.close_count());
  EXPECT_EQ(0, net_provider.last_close_result());

  EXPECT_FALSE(net_provider.stop().first);
  EXPECT_EQ(2U, net_provider.close_count());
}

}  // namespace gcs_xcom_networkprovidertest
