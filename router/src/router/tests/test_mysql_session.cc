/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/mysql_session.h"

#include <gmock/gmock.h>
#include <gtest/gtest-param-test.h>

#include <mysql.h>  // SSL_MODE_DISABLED, ...

class MySQLSessionTest : public ::testing::Test {};

using mysqlrouter::MySQLSession;

TEST_F(MySQLSessionTest, pasrse_ssl_mode) {
  EXPECT_EQ(SSL_MODE_DISABLED,
            MySQLSession::parse_ssl_mode(MySQLSession::kSslModeDisabled));
  EXPECT_EQ(SSL_MODE_PREFERRED,
            MySQLSession::parse_ssl_mode(MySQLSession::kSslModePreferred));
  EXPECT_EQ(SSL_MODE_REQUIRED,
            MySQLSession::parse_ssl_mode(MySQLSession::kSslModeRequired));
  EXPECT_EQ(SSL_MODE_VERIFY_CA,
            MySQLSession::parse_ssl_mode(MySQLSession::kSslModeVerifyCa));
  EXPECT_EQ(SSL_MODE_VERIFY_IDENTITY,
            MySQLSession::parse_ssl_mode(MySQLSession::kSslModeVerifyIdentity));
  EXPECT_THROW(MySQLSession::parse_ssl_mode("bad"), std::logic_error);
}

TEST_F(MySQLSessionTest, ssl_mode_to_string) {
  EXPECT_EQ(MySQLSession::kSslModeDisabled,
            MySQLSession::ssl_mode_to_string(SSL_MODE_DISABLED));
  EXPECT_EQ(MySQLSession::kSslModePreferred,
            MySQLSession::ssl_mode_to_string(SSL_MODE_PREFERRED));
  EXPECT_EQ(MySQLSession::kSslModeRequired,
            MySQLSession::ssl_mode_to_string(SSL_MODE_REQUIRED));
  EXPECT_EQ(MySQLSession::kSslModeVerifyCa,
            MySQLSession::ssl_mode_to_string(SSL_MODE_VERIFY_CA));
  EXPECT_EQ(MySQLSession::kSslModeVerifyIdentity,
            MySQLSession::ssl_mode_to_string(SSL_MODE_VERIFY_IDENTITY));
}

template <typename T>
class MysqlSessionIntegerOptionTest : public ::testing::Test {};

using MysqlSessionIntegerOptionTypes =
    ::testing::Types<MySQLSession::ConnectTimeout, MySQLSession::ReadTimeout,
                     MySQLSession::WriteTimeout>;

TYPED_TEST_SUITE(MysqlSessionIntegerOptionTest, MysqlSessionIntegerOptionTypes);

TYPED_TEST(MysqlSessionIntegerOptionTest, set_and_get) {
  MySQLSession sess;

  using option_type = TypeParam;
  auto set_value = 1024;

  SCOPED_TRACE("// get current value");
  {
    option_type opt;
    auto res = sess.get_option(opt);
    ASSERT_TRUE(res);
  }

  SCOPED_TRACE("// set value");
  {
    auto res = sess.set_option(option_type(set_value));
    ASSERT_TRUE(res) << res.error().message();
  }

  SCOPED_TRACE("// get value again");
  {
    option_type opt;
    auto res = sess.get_option(opt);
    ASSERT_TRUE(res);
    EXPECT_EQ(opt.value(), set_value);
  }
}

template <class T>
T DefaultValueGetter(mysql_option /* opt */);

template <>
unsigned int DefaultValueGetter<unsigned int>(mysql_option opt) {
  // must be 1 as some types filter allowed values and only allow a range of 0-1
  // (like LocalFile)
  if (opt == MYSQL_OPT_LOCAL_INFILE) return 1;

  return 42;
}

template <>
const char *DefaultValueGetter<const char *>(mysql_option opt) {
  if (opt == MYSQL_OPT_TLS_VERSION) return "TLSv1.2";
  if (opt == MYSQL_OPT_LOAD_DATA_LOCAL_DIR) {
    // needs to be directory that exists.
    return
#ifdef _WIN32
        "C:\\"
#else
        "/"
#endif
        ;
  }

  return "test-value";
}

template <>
bool DefaultValueGetter<bool>(mysql_option /* opt */) {
  return true;
}

template <>
unsigned long DefaultValueGetter<unsigned long>(mysql_option /* opt */) {
  return 42;
}

template <typename T>
class MysqlSessionOptionTest : public ::testing::Test {};

using MysqlSessionOptionTypes =
    ::testing::Types<MySQLSession::DefaultAuthentication,  //
                     MySQLSession::EnableCleartextPlugin,  //
                     // InitCommand - set-only
                     MySQLSession::BindAddress,                //
                     MySQLSession::CanHandleExpiredPasswords,  //
                     MySQLSession::Compress,                   //
                     MySQLSession::ConnectTimeout,
                     // CompressionAlgorithms (Bug#32483980)
                     // ConnectAttributeReset - set-only
                     // ConnectAttributeDelete - set-only
                     MySQLSession::GetServerPublicKey,  //
                     MySQLSession::LoadDataLocalDir,    //
                     MySQLSession::LocalInfile,         //
                     MySQLSession::MaxAllowedPacket,    //
                     // NamedPipe - set-only
                     MySQLSession::NetBufferLength,  //
                     MySQLSession::Protocol,         //
                     MySQLSession::ReadTimeout,      //
                     MySQLSession::Reconnect,        //
                     MySQLSession::RetryCount,       //
                     MySQLSession::SslCa,            //
                     MySQLSession::SslCaPath,        //
                     MySQLSession::SslCert,          //
                     MySQLSession::SslCipher,        //
                     MySQLSession::SslCrl,           //
                     MySQLSession::SslCrlPath,       //
                     MySQLSession::SslKey,           //
                     MySQLSession::TlsCipherSuites,  //
                     MySQLSession::TlsVersion,       //
                     MySQLSession::WriteTimeout,     //
                     // ZstdCompressionLevel (Bug#32483980)
                     MySQLSession::PluginDir,         //
                     MySQLSession::ServerPluginKey,   //
                     MySQLSession::ReadDefaultFile,   //
                     MySQLSession::ReadDefaultGroup,  //
                     MySQLSession::CharsetDir,        //
                     MySQLSession::CharsetName        //
#ifdef _WIN32
                     ,
                     MySQLSession::SharedMemoryBasename
#endif
                     >;

TYPED_TEST_SUITE(MysqlSessionOptionTest, MysqlSessionOptionTypes);

TYPED_TEST(MysqlSessionOptionTest, get) {
  MySQLSession sess;

  using option_type = TypeParam;

  SCOPED_TRACE("// get current value");
  {
    option_type opt;
    auto res = sess.get_option(opt);
    ASSERT_TRUE(res);
  }
}

template <typename T>
void ExpectEq(T a, T b) {
  EXPECT_EQ(a, b);
}

template <>
void ExpectEq<const char *>(const char *a, const char *b) {
  EXPECT_STREQ(a, b);
}

// test if the value that's set can be read back.
TYPED_TEST(MysqlSessionOptionTest, set_and_get) {
  MySQLSession sess;

  using option_type = TypeParam;
  const auto set_value = DefaultValueGetter<typename option_type::value_type>(
      option_type().option());

  SCOPED_TRACE("// set value");
  {
    const auto res = sess.set_option(option_type(set_value));
    ASSERT_TRUE(res) << res.error().message();
  }

  SCOPED_TRACE("// get value again");
  {
    option_type opt;
    const auto res = sess.get_option(opt);
    ASSERT_TRUE(res);

    ExpectEq(opt.value(), set_value);
  }
}

/*
 * options that can be set, but not read back again.
 */
template <typename T>
class MysqlSessionGettableOptionTest : public ::testing::Test {};

using MysqlSessionGettableOptionTypes =
    ::testing::Types<MySQLSession::LoadDataLocalDir  // needs real directory
                     >;

TYPED_TEST_SUITE(MysqlSessionGettableOptionTest,
                 MysqlSessionGettableOptionTypes);

TYPED_TEST(MysqlSessionGettableOptionTest, get) {
  MySQLSession sess;

  using option_type = TypeParam;

  SCOPED_TRACE("// get current value");
  {
    option_type opt;
    const auto res = sess.get_option(opt);
    EXPECT_TRUE(res);
  }
}

/*
 * options that can be set, but reading them back leads to an error.
 */
template <typename T>
class MysqlSessionSettableOptionTest : public ::testing::Test {};

using MysqlSessionSettableOptionTypes =
    ::testing::Types<MySQLSession::InitCommand,             //
                     MySQLSession::ConnectAttributeReset,   //
                     MySQLSession::ConnectAttributeDelete,  //
                     MySQLSession::NamedPipe,               //
                     MySQLSession::ZstdCompressionLevel,    // Bug#32483980
                     MySQLSession::CompressionAlgorithms    // Bug#32483980
                     >;

TYPED_TEST_SUITE(MysqlSessionSettableOptionTest,
                 MysqlSessionSettableOptionTypes);

TYPED_TEST(MysqlSessionSettableOptionTest, get) {
  MySQLSession sess;

  using option_type = TypeParam;

  SCOPED_TRACE("// get current value");
  {
    option_type opt;
    const auto res = sess.get_option(opt);
    // get should fail
    EXPECT_FALSE(res);
  }
}

TYPED_TEST(MysqlSessionSettableOptionTest, set_and_get) {
  MySQLSession sess;

  using option_type = TypeParam;
  const auto set_value = DefaultValueGetter<typename option_type::value_type>(
      option_type().option());

  SCOPED_TRACE("// get current value");
  {
    option_type opt;
    auto res = sess.get_option(opt);
    // get should fail
    EXPECT_FALSE(res);
  }

  SCOPED_TRACE("// set value");
  {
    const auto res = sess.set_option(option_type(set_value));
    ASSERT_TRUE(res) << res.error().message();
  }

  SCOPED_TRACE("// get value again");
  {
    option_type opt;
    const auto res = sess.get_option(opt);
    // get should fail
    EXPECT_FALSE(res);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
