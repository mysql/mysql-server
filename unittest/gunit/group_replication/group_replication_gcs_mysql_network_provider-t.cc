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

#include "plugin/group_replication/include/gcs_mysql_network_provider.h"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

Gr_incoming_connection_status handle_group_replication_incoming_connection(
    THD *thd, int fd, SSL *ssl_ctx) {
  (void)thd;
  (void)fd;
  (void)ssl_ctx;

  return Gr_incoming_connection_status::ACCEPTED;
}

// To fool the compiler
ulong get_components_stop_timeout_var() { return 0; }

static bool fake_group_replication_force_pqc = false;
static bool fake_group_replication_use_pqc_sign = false;
static const char *fake_group_replication_tls_kex = "";

bool get_group_replication_force_pqc_var() {
  return fake_group_replication_force_pqc;
}

bool get_group_replication_use_pqc_sign_var() {
  return fake_group_replication_use_pqc_sign;
}

const char *get_group_replication_tls_kex_var() {
  return fake_group_replication_tls_kex;
}

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
  MOCK_METHOD(unsigned int, mysql_errno, (MYSQL * mysql), (override));
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
    fake_group_replication_force_pqc = false;
    fake_group_replication_use_pqc_sign = false;
    fake_group_replication_tls_kex = "";
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

MATCHER_P(IsBoolOption, expected, "") {
  return arg != nullptr && *static_cast<const bool *>(arg) == expected;
}

MATCHER_P(IsCStringOption, expected, "") {
  const char *value = static_cast<const char *>(arg);
  return (value == nullptr && expected == nullptr) ||
         (value != nullptr && expected != nullptr &&
          std::strcmp(value, expected) == 0);
}

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
  fake_group_replication_force_pqc = true;
  fake_group_replication_use_pqc_sign = true;
  fake_group_replication_tls_kex = "X25519MLKEM768";

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
  EXPECT_CALL(*native_interface,
              mysql_options(testing::_, testing::_, testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(0));
  EXPECT_CALL(*native_interface, mysql_options(testing::_, MYSQL_OPT_FORCE_PQC,
                                               IsBoolOption(true)))
      .Times(1);
  EXPECT_CALL(
      *native_interface,
      mysql_options(testing::_, MYSQL_OPT_USE_PQC_SIGN, IsBoolOption(true)))
      .Times(1);
  EXPECT_CALL(*native_interface,
              mysql_options(testing::_, MYSQL_OPT_TLS_KEX,
                            IsCStringOption("X25519MLKEM768")))
      .Times(1);

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

  EXPECT_CALL(*native_interface, mysql_errno(testing::_))
      .Times(1)
      .WillOnce(testing::Return(ER_UNKNOWN_COM_ERROR));

  EXPECT_CALL(*native_interface, mysql_close(testing::_));

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto new_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_EQ(-1, new_connection.get()->fd);

  net_provider.stop();

  free(fake_conn);
}

TEST_F(MySQLNetworkProviderTest, BusyConnectionHandoffCanBeRetried) {
  EXPECT_CALL(*auth_interface, get_credentials(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly(testing::Return(false));

  MYSQL *busy_mysql = (MYSQL *)malloc(sizeof(MYSQL));
  MYSQL *successful_mysql = (MYSQL *)malloc(sizeof(MYSQL));
  constexpr int successful_fd = 43;
  successful_mysql->net.fd = successful_fd;

  EXPECT_CALL(*native_interface, mysql_init(testing::_))
      .Times(2)
      .WillOnce(testing::Return(busy_mysql))
      .WillOnce(testing::Return(successful_mysql));

  EXPECT_CALL(
      *native_interface,
      mysql_real_connect(testing::_, testing::_, testing::_, testing::_,
                         testing::_, testing::_, testing::_, testing::_))
      .Times(2)
      .WillOnce(testing::Return(busy_mysql))
      .WillOnce(testing::Return(successful_mysql));

  EXPECT_CALL(
      *native_interface,
      send_command(testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(2)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(*native_interface, mysql_errno(busy_mysql))
      .Times(1)
      .WillOnce(testing::Return(ER_GRP_RPL_CONNECTION_HANDOFF_BUSY));

  EXPECT_CALL(*native_interface, mysql_close(busy_mysql)).Times(1);
  EXPECT_CALL(*native_interface, mysql_close(successful_mysql)).Times(1);
  EXPECT_CALL(*native_interface, mysql_free(successful_mysql)).Times(1);

  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  auto busy_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_EQ(-1, busy_connection->fd);

  auto successful_connection =
      net_provider.open_connection("localhost", 12345, {"", "", false});

  ASSERT_EQ(successful_fd, successful_connection->fd);
  ASSERT_FALSE(successful_connection->has_error);
  EXPECT_EQ(0, net_provider.close_connection(*successful_connection));

  net_provider.stop();

  free(busy_mysql);
  free(successful_mysql);
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

  ASSERT_EQ(Gr_incoming_connection_status::ACCEPTED,
            net_provider.set_new_connection(&fake_thd, &fake_network_conn));

  Network_connection *retrieved_network_connection =
      net_provider.get_new_connection();

  ASSERT_NE(retrieved_network_connection, nullptr);
  ASSERT_EQ(fake_network_conn.fd, retrieved_network_connection->fd);

  net_provider.stop();

  fake_thd.clear_active_vio();
  vio_delete(active_vio);
}

TEST_F(MySQLNetworkProviderTest, BusyServerConnectionIsRejectedAndCanRetry) {
  Gcs_mysql_network_provider net_provider(auth_interface, native_interface);

  ASSERT_FALSE(net_provider.start().first);

  constexpr int first_socket = 42;
  constexpr int second_socket = 43;

  THD first_thd(false);
  MYSQL_VIO first_vio = vio_new(first_socket, VIO_TYPE_TCPIP, 0);
  first_vio->mysql_socket.fd = first_socket;
  first_vio->vioshutdown = [](Vio *) { return 0; };
  first_thd.set_active_vio(first_vio);

  THD second_thd(false);
  MYSQL_VIO second_vio = vio_new(second_socket, VIO_TYPE_TCPIP, 0);
  second_vio->mysql_socket.fd = second_socket;
  second_vio->vioshutdown = [](Vio *) { return 0; };
  second_thd.set_active_vio(second_vio);

  Network_connection first_connection(first_socket);
  Network_connection second_connection(second_socket);

  ASSERT_EQ(Gr_incoming_connection_status::ACCEPTED,
            net_provider.set_new_connection(&first_thd, &first_connection));
  const Gr_incoming_connection_status second_result =
      net_provider.set_new_connection(&second_thd, &second_connection);
  EXPECT_EQ(Gr_incoming_connection_status::BUSY, second_result);

  Network_connection *retrieved_connection = net_provider.get_new_connection();
  ASSERT_EQ(&first_connection, retrieved_connection);

  if (second_result == Gr_incoming_connection_status::ACCEPTED) {
    // Clean up if this test is run against a queueing implementation.
    retrieved_connection = net_provider.get_new_connection();
    ASSERT_EQ(&second_connection, retrieved_connection);
  } else {
    // Rejection must remove the THD registration so the same connection can
    // be accepted after the pending connection is consumed.
    ASSERT_EQ(Gr_incoming_connection_status::ACCEPTED,
              net_provider.set_new_connection(&second_thd, &second_connection));
    retrieved_connection = net_provider.get_new_connection();
    ASSERT_EQ(&second_connection, retrieved_connection);
  }

  net_provider.stop();

  first_thd.clear_active_vio();
  second_thd.clear_active_vio();
  vio_delete(first_vio);
  vio_delete(second_vio);
}

TEST_F(MySQLNetworkProviderTest, LogMappingTest) {
  // Test for default return
  int coded_log_level = ERROR_LEVEL;
  network_provider_dynamic_log_level provided_log_level =
      network_provider_dynamic_log_level::PROVIDED;

  ASSERT_EQ(ERROR_LEVEL, Gcs_mysql_network_provider_util::log_level_adaptation(
                             coded_log_level, provided_log_level));

  // Test for transformed return
  coded_log_level = ERROR_LEVEL;
  provided_log_level = network_provider_dynamic_log_level::WARNING;

  ASSERT_EQ(WARNING_LEVEL,
            Gcs_mysql_network_provider_util::log_level_adaptation(
                coded_log_level, provided_log_level));

  // Test for out of range transformed return
  coded_log_level = ERROR_LEVEL;
  provided_log_level = network_provider_dynamic_log_level::DEBUG;

  ASSERT_EQ(Gcs_mysql_network_provider_util::OUT_OF_RANGE_LOG_LEVEL,
            Gcs_mysql_network_provider_util::log_level_adaptation(
                coded_log_level, provided_log_level));
}

TEST_F(MySQLNetworkProviderTest, RetryableConnectionHandoffErrorTest) {
  EXPECT_TRUE(
      Gcs_mysql_network_provider_util::is_retryable_connection_handoff_error(
          ER_GRP_RPL_CONNECTION_HANDOFF_BUSY));
  EXPECT_FALSE(
      Gcs_mysql_network_provider_util::is_retryable_connection_handoff_error(
          ER_UNKNOWN_COM_ERROR));
}

}  // namespace group_replication_gcs_mysql_networkprovidertest
