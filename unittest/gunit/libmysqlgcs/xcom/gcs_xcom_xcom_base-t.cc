/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_base_test.h"

#include <cstring>
#include <string>
#include <vector>
#include "app_data.h"
#include "get_synode_app_data.h"
#include "pax_msg.h"
#include "xcom_cache.h"
#include "xcom_memory.h"
#include "xcom_transport.h"

namespace xcom_base_unittest {

class XcomBase : public GcsBaseTest {
 protected:
  XcomBase() { ::init_cache(); }
  ~XcomBase() { ::deinit_cache(); }
};

TEST_F(XcomBase, XcomSendClientAppDataUpgradeScenario) {
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

TEST_F(XcomBase, XcomSendClientAppDataUpgradeScenarioV6) {
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

TEST_F(XcomBase, XcomSendClientAppDataUpgradeScenarioMalformed) {
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

TEST_F(XcomBase, XcomNewClientEligibleDowngradeScenario) {
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

TEST_F(XcomBase, XcomNewClientEligibleDowngradeScenarioFail) {
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

TEST_F(XcomBase, XcomNewClientEligibleDowngradeScenarioNullSiteDef) {
  xcom_proto incoming = x_1_4;
  int result = is_new_node_eligible_for_ipv6(incoming, NULL);

  ASSERT_EQ(result, 0);
}

TEST_F(XcomBase, XcomNewClientEligibleDowngradeScenarioVersionSame) {
  xcom_proto incoming = my_xcom_version;
  int result = is_new_node_eligible_for_ipv6(incoming, NULL);

  ASSERT_EQ(result, 0);
}

TEST_F(XcomBase, GetSynodeAppDataNotCached) {
  synode_no synode;
  synode.group_id = 12345;
  synode.msgno = 0;
  synode.node = 0;

  synode_no_array synodes;
  synodes.synode_no_array_len = 1;
  synodes.synode_no_array_val = &synode;

  synode_app_data_array result;
  result.synode_app_data_array_len = 0;
  result.synode_app_data_array_val = NULL;

  auto error_code = ::xcom_get_synode_app_data(&synodes, &result);
  ASSERT_EQ(error_code, XCOM_GET_SYNODE_APP_DATA_NOT_CACHED);
}

TEST_F(XcomBase, GetSynodeAppDataNotDecided) {
  synode_no synode;
  synode.group_id = 12345;
  synode.msgno = 0;
  synode.node = 0;

  synode_no_array synodes;
  synodes.synode_no_array_len = 1;
  synodes.synode_no_array_val = &synode;

  synode_app_data_array result;
  result.synode_app_data_array_len = 0;
  result.synode_app_data_array_val = NULL;

  /* Add the synode to the cache, but it is undecided. */
  get_cache(synode);

  auto error_code = ::xcom_get_synode_app_data(&synodes, &result);
  ASSERT_EQ(error_code, XCOM_GET_SYNODE_APP_DATA_NOT_DECIDED);
}

TEST_F(XcomBase, GetSynodeAppDataSuccessful) {
  synode_no synode;
  synode.group_id = 12345;
  synode.msgno = 0;
  synode.node = 0;

  synode_no_array synodes;
  synodes.synode_no_array_len = 1;
  synodes.synode_no_array_val = &synode;

  synode_app_data_array result;
  result.synode_app_data_array_len = 0;
  result.synode_app_data_array_val = NULL;

  /* Add the synode to the cache, and set it as decided. */
  char const *const payload = "Message in a bottle";
  app_data_ptr a = new_app_data();
  a->body.c_t = app_type;
  a->body.app_u_u.data.data_len = std::strlen(payload) + 1;
  a->body.app_u_u.data.data_val = const_cast<char *>(payload);

  pax_msg *p = pax_msg_new_0(synode);
  p->op = learn_op;
  p->a = a;
  p->refcnt = 1;

  pax_machine *paxos = get_cache(synode);
  paxos->learner.msg = p;

  auto error_code = ::xcom_get_synode_app_data(&synodes, &result);
  ASSERT_EQ(error_code, XCOM_GET_SYNODE_APP_DATA_OK);

  ASSERT_EQ(result.synode_app_data_array_len, 1);
  ASSERT_EQ(synode_eq(result.synode_app_data_array_val[0].synode, synode), 1);
  ASSERT_EQ(result.synode_app_data_array_val[0].data.data_len,
            p->a->body.app_u_u.data.data_len);
  ASSERT_EQ(std::strcmp(result.synode_app_data_array_val[0].data.data_val,
                        p->a->body.app_u_u.data.data_val),
            0);

  /* Cleanup */
  a->body.app_u_u.data.data_len = 0;
  a->body.app_u_u.data.data_val = nullptr;

  unchecked_replace_pax_msg(&paxos->learner.msg, nullptr);

  my_xdr_free(reinterpret_cast<xdrproc_t>(xdr_synode_app_data_array),
              reinterpret_cast<char *>(&result));
}

TEST_F(XcomBase, GetSynodeAppDataTooManySynodes) {
  /*
   Bypass protocol negotiation because we are not actually connected to
   anything.
  */
  connection_descriptor con;
  con.connected_ = CON_PROTO;
  con.x_proto = get_latest_common_proto();

  constexpr uint32_t group_id = 1;
  /* Unserializable message. Exceeds array size. */
  u_int constexpr nr_synodes = MAX_SYNODE_ARRAY + 1;

  synode_no_array synodes;
  synodes.synode_no_array_len = nr_synodes;
  synodes.synode_no_array_val =
      static_cast<synode_no *>(std::calloc(nr_synodes, sizeof(synode_no)));
  ASSERT_NE(synodes.synode_no_array_val, nullptr);

  synode_app_data_array reply;
  ASSERT_EQ(xcom_client_get_synode_app_data(&con, group_id, &synodes, &reply),
            0);

  std::free(synodes.synode_no_array_val);
}

TEST_F(XcomBase, ProposerBatchDeserialization) {
  pax_msg *p = nullptr;
  unchecked_replace_pax_msg(&p, pax_msg_new_0(null_synode));

  for (auto i = 0; i < MAX_BATCH_APP_DATA; i++) {
    app_data_ptr a = new_app_data();
    a->body.c_t = app_type;
    a->next = p->a;

    p->a = a;
  }

  char *buffer = nullptr;
  uint32_t buffer_len = 0;

  int const serialized =
      serialize_msg(p, get_latest_common_proto(), &buffer_len, &buffer);
  ASSERT_EQ(serialized, 1);

  unchecked_replace_pax_msg(&p, pax_msg_new_0(null_synode));

  int const deserialized =
      deserialize_msg(p, get_latest_common_proto(), buffer, buffer_len);
  ASSERT_EQ(deserialized, 1);

  std::free(p);
  std::free(buffer);
}

}  // namespace xcom_base_unittest
