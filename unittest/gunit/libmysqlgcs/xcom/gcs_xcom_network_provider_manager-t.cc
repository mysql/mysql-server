/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/include/network_provider.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/network_provider_manager.h"
#include "unittest/gunit/libmysqlgcs/include/gcs_base_test.h"

namespace gcs_xcom_networkprovidermamangertest {

class mock_network_provider : public Network_provider {
 public:
  MOCK_METHOD((std::pair<bool, int>), start, (), (override));
  MOCK_METHOD((std::pair<bool, int>), stop, (), (override));
  MOCK_METHOD(enum_transport_protocol, get_communication_stack, (),
              (const, override));
  MOCK_METHOD(bool, configure, (const Network_configuration_parameters &params),
              (override));
  MOCK_METHOD(bool, configure_secure_connections,
              (const Network_configuration_parameters &params), (override));
  MOCK_METHOD(std::unique_ptr<Network_connection>, open_connection,
              (const std::string &address, const unsigned short port,
               const Network_security_credentials &security_credentials,
               int connection_timeout),
              (override));
  MOCK_METHOD(int, close_connection, (const Network_connection &connection),
              (override));
  MOCK_METHOD(void, cleanup_secure_connections_context, (), (override));
  MOCK_METHOD(std::function<void()>, get_secure_connections_context_cleaner, (),
              (override));
  MOCK_METHOD(bool, finalize_secure_connections_context, (), (override));
};

/**
 * @brief This is the test suite for the Network Provider Manager. Each test
 *         will create a mock provider and exercise the Manager's behaviour.
 *
 *         Note that the test subject is a singleton. Meaning that the state
 *         must be cleansed after each individual test.
 *
 * XComNetworkProviderManagerTest.BasicManagerTest
 *   Adds a network provider, starts and stops it and removes
 *
 * XComNetworkProviderManagerTest.ManagerShortcutMethodsTest
 *   Uses add_and_start, stops and removes
 *
 * XComNetworkProviderManagerTest.DoubleAddManagerTest
 *   Tries to add the same provider twice. It will do so, but the side effect
 *   is calling stop() twice
 *
 * XComNetworkProviderManagerTest.RemoveAndStartAndStopProviderManagerTest
 *   Adds and removes a Provider and then tries to start and stop.
 *   start and stop will never be called.
 *
 * XComNetworkProviderManagerTest.RemoveAllAndStartAndStopProviderManagerTest
 *   Adds and removes all providers and then tries to start and stop.
 *   start and stop will never be called.
 *
 * XComNetworkProviderManagerTest.BasicManagerActiveProviderTest
 *    Adds a network provider with XCOM which is the default active provider,
 *    starts, stops it and remove.
 *
 * XComNetworkProviderManagerTest.BasicManagerActiveProviderWithSSLTest
 *    Adds a network provider with XCOM which is the default active provider,
 *    SSL enabled, starts, stops it and remove.
 *
 * XComNetworkProviderManagerTest.BasicManagerActiveFailProviderTest
 *    Adds a network provider with MYSQL which is NOT the default active
 * provider, start and stop must fail and remove.
 *
 * XComNetworkProviderManagerTest.BasicManagerActiveProviderInterfaceTest
 *   Does the whole path for a provider: add, open a connection, receive a
 * connection, stop and remove.
 *
 */

class XComNetworkProviderManagerTest : public GcsBaseTest {
 protected:
  XComNetworkProviderManagerTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}
};

TEST_F(XComNetworkProviderManagerTest, BasicManagerTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));
  EXPECT_CALL(*mock_provider, start())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, stop())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  ASSERT_FALSE(Network_provider_manager::getInstance().start_network_provider(
      XCOM_PROTOCOL));
  ASSERT_FALSE(Network_provider_manager::getInstance().stop_network_provider(
      XCOM_PROTOCOL));

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);
}

TEST_F(XComNetworkProviderManagerTest, ManagerShortcutMethodsTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));
  EXPECT_CALL(*mock_provider, start())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, stop())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));

  Network_provider_manager::getInstance().add_and_start_network_provider(
      mock_provider);

  ASSERT_FALSE(Network_provider_manager::getInstance().stop_network_provider(
      XCOM_PROTOCOL));

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);
}

TEST_F(XComNetworkProviderManagerTest, DoubleAddManagerTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));
  EXPECT_CALL(*mock_provider, start())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, stop())
      .Times(2)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);
  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  ASSERT_FALSE(Network_provider_manager::getInstance().start_network_provider(
      XCOM_PROTOCOL));
  ASSERT_FALSE(Network_provider_manager::getInstance().stop_network_provider(
      XCOM_PROTOCOL));

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);
}

TEST_F(XComNetworkProviderManagerTest,
       RemoveAndStartAndStopProviderManagerTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);

  EXPECT_TRUE(Network_provider_manager::getInstance().start_network_provider(
      XCOM_PROTOCOL));
  EXPECT_TRUE(Network_provider_manager::getInstance().stop_network_provider(
      XCOM_PROTOCOL));
}

TEST_F(XComNetworkProviderManagerTest,
       RemoveAllAndStartAndStopProviderManagerTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  Network_provider_manager::getInstance().remove_all_network_provider();

  EXPECT_TRUE(Network_provider_manager::getInstance().start_network_provider(
      XCOM_PROTOCOL));
  EXPECT_TRUE(Network_provider_manager::getInstance().stop_network_provider(
      XCOM_PROTOCOL));
}

TEST_F(XComNetworkProviderManagerTest, BasicManagerActiveProviderTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));
  EXPECT_CALL(*mock_provider, start())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, stop())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, configure(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  ASSERT_FALSE(
      Network_provider_manager::getInstance().start_active_network_provider());
  ASSERT_FALSE(
      Network_provider_manager::getInstance().stop_active_network_provider());

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);
}

TEST_F(XComNetworkProviderManagerTest, BasicManagerActiveProviderWithSSLTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));
  EXPECT_CALL(*mock_provider, start())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, stop())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, configure(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_provider, configure_secure_connections(testing::_))
      .WillRepeatedly(testing::Return(false));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  Network_provider_manager::getInstance().xcom_set_ssl_mode(SSL_REQUIRED);

  ASSERT_FALSE(
      Network_provider_manager::getInstance().start_active_network_provider());
  ASSERT_FALSE(
      Network_provider_manager::getInstance().stop_active_network_provider());

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);

  Network_provider_manager::getInstance().xcom_set_ssl_mode(SSL_DISABLED);
}

TEST_F(XComNetworkProviderManagerTest, BasicManagerActiveFailProviderTest) {
  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(MYSQL_PROTOCOL));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  EXPECT_TRUE(
      Network_provider_manager::getInstance().start_active_network_provider());
  EXPECT_TRUE(
      Network_provider_manager::getInstance().stop_active_network_provider());

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);
}

TEST_F(XComNetworkProviderManagerTest,
       BasicManagerActiveProviderInterfaceTest) {
  constexpr int fd_number = 42;

  std::shared_ptr<mock_network_provider> mock_provider =
      std::make_shared<mock_network_provider>();
  EXPECT_CALL(*mock_provider, get_communication_stack())
      .WillRepeatedly(testing::Return(XCOM_PROTOCOL));
  EXPECT_CALL(*mock_provider, start())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, stop())
      .Times(1)
      .WillRepeatedly(testing::Return(std::make_pair(false, 0)));
  EXPECT_CALL(*mock_provider, configure(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_provider,
              open_connection(testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(
          testing::ByMove(std::make_unique<Network_connection>(fd_number))));

  Network_provider_manager::getInstance().add_network_provider(mock_provider);

  ASSERT_FALSE(
      Network_provider_manager::getInstance().start_active_network_provider());

  auto connection_to =
      Network_provider_manager::getInstance().open_xcom_connection("", 12345,
                                                                   false);
  ASSERT_NE(connection_to, nullptr);
  ASSERT_EQ(connection_to->fd, fd_number);

  auto *fake_incoming = new Network_connection(fd_number);
  mock_provider->set_new_connection(fake_incoming);

  connection_descriptor *incoming_from_manager =
      Network_provider_manager::getInstance().incoming_connection();

  ASSERT_NE(incoming_from_manager, nullptr);
  ASSERT_EQ(incoming_from_manager->fd, fd_number);

  free(incoming_from_manager);

  connection_descriptor *incoming_from_manager_must_be_null =
      Network_provider_manager::getInstance().incoming_connection();

  ASSERT_EQ(incoming_from_manager_must_be_null, nullptr);

  ASSERT_FALSE(
      Network_provider_manager::getInstance().stop_active_network_provider());

  Network_provider_manager::getInstance().remove_network_provider(
      XCOM_PROTOCOL);

  free(connection_to);
}
}  // namespace gcs_xcom_networkprovidermamangertest