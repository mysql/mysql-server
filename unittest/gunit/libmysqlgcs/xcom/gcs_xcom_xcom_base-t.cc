/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <string>
#include <vector>

#include "gcs_base_test.h"

#include "xcom_base.h"

namespace gcs_xcom_xcom_unittest {

class GcsXComXCom : public GcsBaseTest {
 protected:
  GcsXComXCom() {}
  ~GcsXComXCom() {}
};

TEST_F(GcsXComXCom, XcomSendClientAppDataUpgradeScenario) {
  app_data a;
  std::string address("127.0.0.1:12345");

  char *names[] = {const_cast<char *>(address.c_str())};
  node_address *na = new_node_address(1, names);

  a.body.c_t = add_node_type;
  a.body.app_u_u.nodes.node_list_len = 1;
  a.body.app_u_u.nodes.node_list_val = na;

  int result = 0;
  result = are_we_allowed_to_upgrade_to_v6(&a);

  ASSERT_EQ(result, 1);
  delete_node_address(1, na);
}

TEST_F(GcsXComXCom, XcomSendClientAppDataUpgradeScenarioV6) {
  app_data a;
  std::string address("[::1]:12345");

  char *names[] = {const_cast<char *>(address.c_str())};
  node_address *na = new_node_address(1, names);

  a.body.c_t = add_node_type;
  a.body.app_u_u.nodes.node_list_len = 1;
  a.body.app_u_u.nodes.node_list_val = na;

  int result = 0;
  result = are_we_allowed_to_upgrade_to_v6(&a);

  ASSERT_EQ(result, 0);

  delete_node_address(1, na);
}

TEST_F(GcsXComXCom, XcomSendClientAppDataUpgradeScenarioMalformed) {
  app_data a;
  std::string address("::1]:12345");

  char *names[] = {const_cast<char *>(address.c_str())};
  node_address *na = new_node_address(1, names);

  a.body.c_t = add_node_type;
  a.body.app_u_u.nodes.node_list_len = 1;
  a.body.app_u_u.nodes.node_list_val = na;

  int result = 0;
  result = are_we_allowed_to_upgrade_to_v6(&a);

  ASSERT_EQ(result, 0);

  delete_node_address(1, na);
}

TEST_F(GcsXComXCom, XcomNewClientEligibleDowngradeScenario) {
  std::string address("127.0.0.1:12345");

  char *names[] = {const_cast<char *>(address.c_str())};
  node_address *na = new_node_address(1, names);

  site_def *sd = new_site_def();
  init_site_def(1, na, sd);

  xcom_proto incoming = x_1_4;
  int result = is_new_node_eligible_for_ipv6(incoming, sd);

  ASSERT_EQ(result, 0);

  free_site_def(sd);
  delete_node_address(1, na);
}

TEST_F(GcsXComXCom, XcomNewClientEligibleDowngradeScenarioFail) {
  std::string address("[::1]:12345");

  char *names[] = {const_cast<char *>(address.c_str())};
  node_address *na = new_node_address(1, names);

  site_def *sd = new_site_def();
  init_site_def(1, na, sd);

  xcom_proto incoming = x_1_4;
  int result = is_new_node_eligible_for_ipv6(incoming, sd);

  ASSERT_EQ(result, 1);

  free_site_def(sd);
  delete_node_address(1, na);
}

TEST_F(GcsXComXCom, XcomNewClientEligibleDowngradeScenarioNullSiteDef) {
  xcom_proto incoming = x_1_4;
  int result = is_new_node_eligible_for_ipv6(incoming, NULL);

  ASSERT_EQ(result, 0);
}

TEST_F(GcsXComXCom, XcomNewClientEligibleDowngradeScenarioVersionSame) {
  xcom_proto incoming = my_xcom_version;
  int result = is_new_node_eligible_for_ipv6(incoming, NULL);

  ASSERT_EQ(result, 0);
}

}  // namespace gcs_xcom_xcom_unittest
