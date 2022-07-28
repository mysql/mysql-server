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

#include "unittest/gunit/libmysqlgcs/include/gcs_base_test.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/xcom_network_provider.h"

namespace gcs_xcom_networkprovidertest {
class XComNetworkProviderTest : public GcsBaseTest {
 protected:
  XComNetworkProviderTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}
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

  int close_connection_retval = net_provider.close_connection(*new_connection);

  ASSERT_EQ(0, close_connection_retval);

  net_provider.stop();
}

}  // namespace gcs_xcom_networkprovidertest