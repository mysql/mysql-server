/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/gcs_mysql_network_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

void handle_group_replication_incoming_connection(THD *thd, int fd,
                                                  SSL *ssl_ctx) {
  (void)thd;
  (void)fd;
  (void)ssl_ctx;
}

// To fool the compiler
ulong get_components_stop_timeout_var() { return 0; }

namespace group_replication_gcs_mysql_networkprovidertest {

class mock_gcs_mysql_network_provider_auth_interface
    : public Gcs_mysql_network_provider_auth_interface {
 public:
  MOCK_METHOD(bool, get_credentials,
              (std::string & username, std::string &password), (override));
};

class mock_gcs_mysql_network_provider_native_interface
    : public Gcs_mysql_network_provider_native_interface {
 public:
  MOCK_METHOD(MYSQL *, mysql_real_connect,
              (MYSQL * mysql, const char *host, const char *user,
               const char *passwd, const char *db, unsigned int port,
               const char *unix_socket, unsigned long clientflag),
              (override));
  MOCK_METHOD(bool, send_command,
              (MYSQL * mysql, enum enum_server_command command,
               const unsigned char *arg, size_t length, bool skip_check),
              (override));
  MOCK_METHOD(MYSQL *, mysql_init, (MYSQL * sock), (override));
  MOCK_METHOD(void, mysql_close, (MYSQL * sock), (override));
  MOCK_METHOD(void, mysql_free, (void *ptr), (override));
  MOCK_METHOD(int, channel_get_network_namespace, (std::string & net_ns),
              (override));
  MOCK_METHOD(bool, set_network_namespace,
              (const std::string &network_namespace), (override));
  MOCK_METHOD(bool, restore_original_network_namespace, (), (override));

  MOCK_METHOD(int, mysql_options,
              (MYSQL * mysql, enum mysql_option option, const void *arg),
              (override));
  MOCK_METHOD(bool, mysql_ssl_set,
              (MYSQL * mysql, const char *key, const char *cert, const char *ca,
               const char *capath, const char *cipher),
              (override));
};

/**
 * @brief Unit Test Fixture for Gcs_mysql_network_provider.
 *
 * It is intended to provide as much coverage as possible to
 * Gcs_mysql_network_provider. For that, it uses a series of mocks and
 * dependecy injection mechanisms to reach its goal.
 *
 * The tests are:
 *
 * MySQLNetworkProviderTest.StartAndStopTest
 *   Tests if we are able to start and stop the provider
 *
 * MySQLNetworkProviderTest.StartAgainAndStopTest
 *   Tests if we are able to start the provider twice and stop it
 *
 * MySQLNetworkProviderTest.CreateConnectionToSelfTest
 *   Test if one is able to connect successfully
 *
 * MySQLNetworkProviderTest.CreateConnectionToSelfWithSSLTest
 *   Test if one is able to connect successfully using SSL
 *
 * MySQLNetworkProviderTest.CreateConnectionToSelfCredentialsErrorTest
 *   Test error case when getting credentials
 *
 * MySQLNetworkProviderTest.CreateConnectionToSelfRealConnectErrorTest
 *   Test error case when connecting
 *
 * MySQLNetworkProviderTest.CreateConnectionToSelfSendCommandErrorTest
 *   Test error case when sending a command
 *
 * MySQLNetworkProviderTest.NewServerConnectionTest
 *   Test receiving a new connection from the outside
 */

class MySQLNetworkProviderTest : public ::testing::Test {
 protected:
  MySQLNetworkProviderTest() {}

  virtual void SetUp() {
    auth_interface = new mock_gcs_mysql_network_provider_auth_interface();
    native_interface = new mock_gcs_mysql_network_provider_native_interface();
  }

  virtual void TearDown() {
    delete auth_interface;
    delete native_interface;
  }

  mock_gcs_mysql_network_provider_auth_interface *auth_interface;
  mock_gcs_mysql_network_provider_native_interface *native_interface;
};

TEST_F(MySQLNetworkProviderTest, StartAndStopTest) {
  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);
  net_provider.stop();
}

TEST_F(MySQLNetworkProviderTest, StartAgainAndStopTest) {
  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);
  ASSERT_FALSE(net_provider.start().first);

  net_provider.stop();
}

TEST_F(MySQLNetworkProviderTest, CreateConnectionToSelfTest) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  MYSQL *fake_conn = (MYSQL *)malloc(sizeof(MYSQL));

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      mysql_real_connect(testing::_, testing::_, testing::_, testing::_,
                         testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      send_command(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*native_interface, channel_get_network_namespace(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*native_interface, set_network_namespace(testing::_)).Times(0);

  EXPECT_CALL(*native_interface, restore_original_network_namespace()).Times(0);

  EXPECT_CALL(*native_interface, mysql_close(testing::_));

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_NE(-1, new_connection.get()->fd);

  int close_connection_retval = net_provider.close_connection(*new_connection);

  ASSERT_EQ(0, close_connection_retval);

  net_provider.stop();

  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, CreateConnectionToSelfWithNameSpaceTest) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  MYSQL *fake_conn = (MYSQL *)malloc(sizeof(MYSQL));

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      mysql_real_connect(testing::_, testing::_, testing::_, testing::_,
                         testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      send_command(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*native_interface, channel_get_network_namespace(testing::_))
      .Times(1)
      .WillRepeatedly(
          testing::DoAll([](std::string &net_ns) { net_ns.assign("test_ns"); },
                         testing::Return(false)));

  EXPECT_CALL(*native_interface,
              set_network_namespace(testing::StrEq("test_ns")))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*native_interface, restore_original_network_namespace())
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*native_interface, mysql_close(testing::_));

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_NE(-1, new_connection.get()->fd);

  int close_connection_retval = net_provider.close_connection(*new_connection);

  ASSERT_EQ(0, close_connection_retval);

  net_provider.stop();

  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, CreateConnectionToSelfWithSSLTest) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  MYSQL *fake_conn = (MYSQL *)malloc(sizeof(MYSQL));

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      mysql_real_connect(testing::_, testing::_, testing::_, testing::_,
                         testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::DoAll(
          [](MYSQL *mysql, const char *host, const char *user,
             const char *passwd, const char *db, unsigned int port,
             const char *unix_socket, unsigned long clientflag) {
            (void)host;
            (void)user;
            (void)passwd;
            (void)db;
            (void)port;
            (void)unix_socket;
            (void)clientflag;
            mysql->net.vio = (Vio *)malloc(sizeof(Vio));
            SSL *fake_ssl_connection = (SSL *)malloc(sizeof(SSL *));
            mysql->net.vio->ssl_arg = fake_ssl_connection;
          },
          testing::Return(fake_conn)));

  EXPECT_CALL(
      *native_interface,
      send_command(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(*native_interface, mysql_close(testing::_));

  Network_configuration_parameters net_provider_security_params;
  net_provider_security_params.ssl_params = {
      SSL_REQUIRED, nullptr, nullptr, nullptr, nullptr,
      nullptr,      nullptr, nullptr, nullptr, nullptr};
  net_provider_security_params.tls_params = {nullptr, nullptr};
  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  net_provider.configure_secure_connections(net_provider_security_params);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", true});

  ASSERT_NE(-1, new_connection.get()->fd);

  int close_connection_retval = net_provider.close_connection(*new_connection);

  ASSERT_EQ(0, close_connection_retval);

  net_provider.stop();

  free(fake_conn->net.vio->ssl_arg);
  free(fake_conn->net.vio);
  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, CreateConnectionToSelfCredentialsErrorTest) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));

  MYSQL *fake_conn = (MYSQL *)malloc(sizeof(MYSQL));

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_EQ(-1, new_connection.get()->fd);

  net_provider.stop();

  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, CreateConnectionToSelfRealConnectErrorTest) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  MYSQL *fake_conn = (MYSQL *)malloc(sizeof(MYSQL));

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      mysql_real_connect(testing::_, testing::_, testing::_, testing::_,
                         testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(nullptr));

  EXPECT_CALL(*native_interface, mysql_close(testing::_)).Times(1);

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_EQ(-1, new_connection.get()->fd);

  net_provider.stop();

  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, CreateConnectionToSelfSendCommandErrorTest) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(false));

  MYSQL *fake_conn = (MYSQL *)malloc(sizeof(MYSQL));

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      mysql_real_connect(testing::_, testing::_, testing::_, testing::_,
                         testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(fake_conn));

  EXPECT_CALL(
      *native_interface,
      send_command(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*native_interface, mysql_close(testing::_));

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_EQ(-1, new_connection.get()->fd);

  net_provider.stop();

  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, NewServerConnectionTest) {
  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  constexpr int socket_to_use = 42;

  THD fake_thd(false);
  // Vio *active_vio = (Vio *)malloc(sizeof(Vio));
  MYSQL_VIO active_vio = vio_new(socket_to_use, VIO_TYPE_TCPIP, 0);
  active_vio->mysql_socket.fd = socket_to_use;
  active_vio->vioshutdown = [](Vio *) { return 0; };

  fake_thd.set_active_vio(active_vio);

  Network_connection fake_network_conn(socket_to_use);

  net_provider.set_new_connection(&fake_thd, &fake_network_conn);

  Network_connection *retrieved_network_connection =
      net_provider.get_new_connection();

  ASSERT_NE(retrieved_network_connection, nullptr);
  ASSERT_EQ(fake_network_conn.fd, retrieved_network_connection->fd);

  net_provider.stop();

  fake_thd.clear_active_vio();
  vio_delete(active_vio);
}

}  // namespace group_replication_gcs_mysql_networkprovidertest