/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

namespace xcom_transport_unittest {

class XcomTransport : public GcsBaseTest {
 protected:
  XcomTransport() = default;
  ~XcomTransport() override = default;
};

TEST_F(XcomTransport, SerializeTooManySynodes) {
  u_int constexpr nr_synodes = MAX_SYNODE_ARRAY + 1;

  app_data_ptr a = new_app_data();
  a->body.c_t = get_synode_app_data_type;
  a->body.app_u_u.synodes.synode_no_array_len = nr_synodes;
  a->body.app_u_u.synodes.synode_no_array_val =
      static_cast<synode_no *>(std::calloc(nr_synodes, sizeof(synode_no)));

  pax_msg *p = pax_msg_new(null_synode, nullptr);
  p->a = a;
  p->to = VOID_NODE_NO;
  p->op = client_msg;

  uint32_t buflen = 0;
  char *buf = nullptr;
  ASSERT_EQ(serialize_msg(p, x_1_6, &buflen, &buf), 0);

  p->refcnt = 1;
  unchecked_replace_pax_msg(&p, nullptr);

  std::free(buf);
}

TEST_F(XcomTransport, is_new_node_eligible_for_ipv6) {
  char const *invalid_address = "127.0.0.257:123456";
  node_address node{const_cast<char *>(invalid_address),
                    {{0, nullptr}},
                    {x_1_0, x_1_0},
                    P_PROP | P_ACC | P_LEARN};
  site_def site{null_synode,
                null_synode,
                VOID_NODE_NO,
                {1, &node},
                {nullptr},
                {0},
                0,
                {0, nullptr},
                {0, nullptr},
                0,
                x_1_0,
                {null_synode},
                0.0,
                EVENT_HORIZON_MIN,
                0,
                {0, nullptr},
                nullptr};
  /* Any protocol version besides x_1_6 is okay as long as it is older than
     MY_XCOM_PROTO */
  ASSERT_EQ(1, is_new_node_eligible_for_ipv6(x_1_6, &site));
}

TEST_F(XcomTransport, get_ip_and_port) {
  /* Has 512 characters + \0. */
  char const *hostname_that_is_too_big =
      "uDmNoeWItHSKUkullwFTkTYclXzEAZwcOKvezkHTxCaoCBkrrMFJfARWdmnpvVHSokbOKcHf"
      "TKZqkZFysAFvTMoGsqBMkTUvcSFFosMSeQqYCtqOtOtCNxMVAonZlFosAxIFWzATRzIUAKGQ"
      "WFEHEJDkWqJYTSOBGLIUJTqrxDbCOGYPSiCymVxeZPmuXHCpcFHzEiGfsHxHffvuDPyMIgfp"
      "YfSFRhylIYwTrafXooTigiDdNhVkMrtJRmNGUCPHFMCBXxhioyEydKNZhVUROJmYrqQMQZaC"
      "iueRmJKatxHiiWYqshHxNiHHShxRURWiymUXRIPMOHOBUhXjqfJIyqtygobpDmVGbAqynnRR"
      "ukByXEegTTFfyHsvKiFJixFttmxHrxKZblGmkPhcUHzPVJcpzmWPXiPtatPxVTmOioqvmAom"
      "cFUQEufzYBrxVneufgdJOlvlPaBgiyPlAzmXDzwYyxXujyKATWBjiGWatqiYCgiSGWkcIoAS"
      "uYsTnWeR";
  char ip[IP_MAX_SIZE];
  xcom_port port = 0;
  ASSERT_EQ(1, get_ip_and_port(const_cast<char *>(hostname_that_is_too_big), ip,
                               &port));

  char const *malformed_ipv6_1 = "[ ]";
  ASSERT_EQ(1,
            get_ip_and_port(const_cast<char *>(malformed_ipv6_1), ip, &port));

  char const *malformed_ipv6_2 = "[::::::::]";
  ASSERT_EQ(1,
            get_ip_and_port(const_cast<char *>(malformed_ipv6_2), ip, &port));
}

/* Validate that we can correctly deserialize a `gcs_snapshot_op` message
   serialized by the XCom implementation that only knows up to protocol x_1_6.

   Note that this test embeds the byte array `buf` of the serialized message
   directly into the test source code. Nevertheless the source contains the code
   that was used to generate the embedded array commented out.
 */
TEST_F(XcomTransport, GcsSnapshotOpCrossVersionSerialization) {
  char const b_str[] = "uuid";
  // blob b;
  // b.data.data_len = sizeof b_str / sizeof b_str[0];
  // b.data.data_val = b_str;
  //
  // x_proto_range xpr;
  // xpr.min_proto = x_1_0;
  // xpr.max_proto = x_1_6;
  //
  // node_address na;
  char const na_str[] = "127.0.0.1:12345";
  // na.address = na_str;
  // na.uuid = b;
  // na.proto = xpr;
  //
  // node_list nl;
  // nl.node_list_len = 1;
  // nl.node_list_val = &na;
  //
  synode_no const start = {2, 2, 2};
  synode_no const boot_key = {1, 1, 1};
  xcom_event_horizon const event_horizon = 42;
  // config c;
  // c.start = start;
  // c.boot_key = boot_key;
  // c.nodes = nl;
  // c.event_horizon = event_horizon;

  // config_ptr cs_val[] = {&c};
  // configs cs;
  // cs.configs_len = sizeof cs_val / sizeof cs_val[0];
  // cs.configs_val = cs_val;
  //
  char const as_str[] = "app_snap";
  // blob as;
  // as.data.data_len = sizeof as_str / sizeof as_str[0];
  // as.data.data_val = as_str;
  //
  synode_no const log_start = {3, 3, 3};
  // gcs_snapshot gs;
  // gs.log_start = log_start;
  // gs.cfg = cs;
  // gs.app_snap = as;

  // pax_msg *p = pax_msg_new_0(null_synode);
  // p->op = gcs_snapshot_op;
  // p->gcs_snap = &gs;

  // uint32_t buflen = 0;
  // unsigned char *buf = 0;
  // int ok = serialize_msg(p, x_1_6, &buflen, (char **)&buf);
  // ASSERT_TRUE(ok);

  // std::printf("buflen=%" PRIu32 "\n", buflen);
  // std::printf("begin\n");
  // for (uint32_t i = 0; i < buflen; i++) {
  //   std::printf(" 0x%hhx,", *(buf + i));
  // }
  // std::printf("\nend\n");

  unsigned char buf[] = {
      0x0,  0x0,  0x0,  0x7,  0x0,  0x0,  0x0,  0xf4, 0x0,  0x0,  0x0,  0x0,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x0,  0x0,  0x0,  0x15, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0x3,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x3,  0x0,  0x0,  0x0,  0x3,  0x0,  0x0,  0x0,  0x1,
      0x0,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0x2,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x2,  0x0,  0x0,  0x0,  0x2,  0x0,  0x0,  0x0,  0x1,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0x1,
      0x0,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0xf,  0x31, 0x32, 0x37, 0x2e,
      0x30, 0x2e, 0x30, 0x2e, 0x31, 0x3a, 0x31, 0x32, 0x33, 0x34, 0x35, 0x0,
      0x0,  0x0,  0x0,  0x5,  0x75, 0x75, 0x69, 0x64, 0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0x7,  0x0,  0x0,  0x0,  0x2a,
      0x0,  0x0,  0x0,  0x9,  0x61, 0x70, 0x70, 0x5f, 0x73, 0x6e, 0x61, 0x70,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0xa,
      0x0,  0x0,  0x0,  0x0};
  uint32_t buflen = 256;

  xcom_proto proto = read_protoversion(VERS_PTR(buf));
  ASSERT_EQ(proto, x_1_6);
  uint32_t msgsize = 0;
  x_msg_type x_type = x_normal;
  unsigned int tag = 0;
  get_header_1_0(buf, &msgsize, &x_type, &tag);
  ASSERT_EQ(msgsize + MSG_HDR_SIZE, buflen);
  ASSERT_EQ(x_type, x_normal);

  pax_msg *p_received = pax_msg_new_0(null_synode);
  int deserialize_ok =
      deserialize_msg(p_received, x_1_6, (char *)buf + MSG_HDR_SIZE, msgsize);
  ASSERT_TRUE(deserialize_ok);

  ASSERT_EQ(p_received->op, gcs_snapshot_op);
  ASSERT_TRUE(synode_eq(p_received->gcs_snap->log_start, log_start));
  ASSERT_EQ(p_received->gcs_snap->cfg.configs_len, 1);
  ASSERT_TRUE(
      synode_eq(p_received->gcs_snap->cfg.configs_val[0]->start, start));
  ASSERT_TRUE(
      synode_eq(p_received->gcs_snap->cfg.configs_val[0]->boot_key, boot_key));
  ASSERT_EQ(p_received->gcs_snap->cfg.configs_val[0]->nodes.node_list_len, 1);
  ASSERT_EQ(strcmp(p_received->gcs_snap->cfg.configs_val[0]
                       ->nodes.node_list_val[0]
                       .address,
                   na_str),
            0);
  ASSERT_EQ(strcmp(p_received->gcs_snap->cfg.configs_val[0]
                       ->nodes.node_list_val[0]
                       .uuid.data.data_val,
                   b_str),
            0);
  ASSERT_EQ(p_received->gcs_snap->cfg.configs_val[0]
                ->nodes.node_list_val[0]
                .proto.min_proto,
            x_1_0);
  ASSERT_EQ(p_received->gcs_snap->cfg.configs_val[0]
                ->nodes.node_list_val[0]
                .proto.max_proto,
            x_1_6);
  ASSERT_EQ(p_received->gcs_snap->cfg.configs_val[0]->event_horizon,
            event_horizon);
  ASSERT_EQ(strcmp(p_received->gcs_snap->app_snap.data.data_val, as_str), 0);

  delete_pax_msg(p_received);
}
}  // namespace xcom_transport_unittest
