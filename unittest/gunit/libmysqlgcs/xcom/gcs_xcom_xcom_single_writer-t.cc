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

#include <array>
#include <cstring>
#include <string>
#include <vector>
#include "app_data.h"
#include "get_synode_app_data.h"
#include "pax_msg.h"
#include "xcom/site_struct.h"
#include "xcom_base.h"
#include "xcom_cache.h"
#include "xcom_memory.h"
#include "xcom_transport.h"

extern void recompute_node_set(node_set const *old_set,
                               node_list const *old_nodes, node_set *new_set,
                               node_list const *new_nodes);

extern void recompute_timestamps(detector_state const old_timestamp,
                                 node_list const *old_nodes,
                                 detector_state new_timestamp,
                                 node_list const *new_nodes);

extern void analyze_leaders(site_def *site);
extern int is_active_leader(node_no x, site_def *site);
extern node_no found_active_leaders(site_def *site);
extern bool unsafe_leaders(app_data *a);
extern bool_t handle_set_leaders(app_data *a);
extern bool_t handle_leaders(app_data *a);

namespace xcom_base_unittest {

uint32_t constexpr const test_group_id{0xbaadcafe};

class XcomSingleWriter : public GcsBaseTest {
 protected:
  XcomSingleWriter() {}
  ~XcomSingleWriter() override {}
};

auto xprintf = [](std::ostream &os, auto... args) {
  int dummy[] = {(os << args, 0)...};
  (void)dummy;
};

TEST_F(XcomSingleWriter, test_xcom_client_set_max_leaders) {
  std::array<uint32_t, 4> numbers = {std::numeric_limits<uint32_t>::min(), 0, 1,
                                     std::numeric_limits<uint32_t>::max()};
  for (auto n : numbers) {  // Should always return 0 because of nullptr
    ASSERT_EQ(0, xcom_client_set_max_leaders(nullptr, n, test_group_id));
  }
}

TEST_F(XcomSingleWriter, test_xcom_client_set_leaders) {
  std::array<char const *, 5> a = {nullptr, "hostname", "xxfunnyxx.nohost.foo",
                                   "localhost", "127.0.0.1"};
  for (auto name : a) {  // Should always return 0 because of nullptr
    ASSERT_EQ(0, xcom_client_set_leaders(nullptr, 1, &name, test_group_id));
  }
}

struct test_node_address : public node_address {
  test_node_address(char const *s) {
    address = strdup(s);
    uuid = {};
    proto = {};
    services = 0;
  }
};

TEST_F(XcomSingleWriter, test_recompute_node_set) {
  node_set old_set{};
  node_set new_set{};
  test_node_address node_a{"node_a:12345"};
  test_node_address node_b{"node_b:67890"};
  test_node_address list_1[]{node_a, node_b};
  test_node_address list_2[]{node_b, node_a};
  node_list old_nodes{2, list_1};
  node_list new_nodes{2, list_2};
  bool failed{false};

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    (void)p;
    xdr_free((xdrproc_t)xdr_node_address, (char *)&node_a);
    xdr_free((xdrproc_t)xdr_node_address, (char *)&node_b);
    xdr_free((xdrproc_t)xdr_node_set, (char *)&old_set);
    xdr_free((xdrproc_t)xdr_node_set, (char *)&new_set);
  }};
  std::unique_ptr<node_set, decltype(cleanup)> guard(&old_set, cleanup);

  // Note failure and print message
  auto fail{[&failed](auto... args) {
    xprintf(std::cerr, "test_recompute_node_set FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  // Two nodes, one alive,and one not
  init_node_set(&old_set, 2);
  init_node_set(&new_set, 2);
  add_node(old_set, 0);  // Node 0 (node_a) is alive

  auto assert_set{[&](u_int i) {
    if (!is_set(new_set, i))
      fail("new_set[", i, "] should be set after ::recompute_node_set");
  }};

  auto assert_not_set{[&](u_int i) {
    if (is_set(new_set, i))
      fail("new_set[", i, "] should not be set after ::recompute_node_set");
  }};

  // Recompute node set after having swapped position of a and b in node list
  ::recompute_node_set(&old_set, &old_nodes, &new_set, &new_nodes);
  assert_not_set(0);
  assert_set(1);

  // Recompute node set after having removed node a
  new_nodes.node_list_len = 1;
  new_set.node_set_len = 1;
  ::recompute_node_set(&old_set, &old_nodes, &new_set, &new_nodes);
  assert_not_set(0);

  // Recompute node set after having removed node b
  new_nodes.node_list_val[0] = node_a;
  ::recompute_node_set(&old_set, &old_nodes, &new_set, &new_nodes);
  assert_set(0);
}

TEST_F(XcomSingleWriter, test_recompute_timestamps) {
  detector_state const old_ts{1.0, 2.0};
  detector_state new_ts{};
  test_node_address node_a{"node_a:12345"};
  test_node_address node_b{"node_b:67890"};
  node_address list_1[]{node_a, node_b};
  node_address list_2[]{node_b, node_a};
  node_list old_nodes{2, list_1};
  node_list new_nodes{2, list_2};
  bool failed{false};

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    (void)p;
    xdr_free((xdrproc_t)xdr_node_address, (char *)&node_a);
    xdr_free((xdrproc_t)xdr_node_address, (char *)&node_b);
  }};
  std::unique_ptr<node_address, decltype(cleanup)> guard(&node_a, cleanup);

  // Note failure and print message
  auto fail{[&failed](auto... args) {
    xprintf(std::cerr, "test_recompute_timestamps FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  auto assert_ts{[&](u_int i, double ts) {
    if (!(new_ts[i] == ts))
      fail("new_ts[", i, "] should be ", ts, " after recompute_timestamps");
  }};

  // Recompute timestamps after having swapped position of a and b in node list
  ::recompute_timestamps(old_ts, &old_nodes, new_ts, &new_nodes);
  assert_ts(0, 2.0);
  assert_ts(1, 1.0);

  // Recompute node ts after having removed node a
  new_nodes.node_list_len = 1;
  ::recompute_timestamps(old_ts, &old_nodes, new_ts, &new_nodes);
  assert_ts(0, 2.0);

  // Recompute node ts after having removed node b
  new_nodes.node_list_val[0] = node_a;
  ::recompute_timestamps(old_ts, &old_nodes, new_ts, &new_nodes);
  assert_ts(0, 1.0);
}

/* Simple multiplicative hash */
static uint32_t mhash(unsigned char const *buf, size_t length) {
  size_t i = 0;
  uint32_t sum = 0;
  for (i = 0; i < length; i++) {
    sum += 0x811c9dc5 * (uint32_t)buf[i];
  }
  return sum;
}

static blob uuid_blob(char const *arg) {
  blob uuid_tmp;
  unsigned int hash = mhash((unsigned char const *)arg, strlen(arg));
  G_MESSAGE("hash %x", hash);
  uuid_tmp.data.data_len = sizeof(hash);
  uuid_tmp.data.data_val = (char *)calloc(1, uuid_tmp.data.data_len);
  memcpy(uuid_tmp.data.data_val, &hash, uuid_tmp.data.data_len);
  return uuid_tmp;
}

/* Initialize address list from command line */
static void init_me(node_list *nl, char const *arg) {
  /* Synthesize (hopefully) unique identifier and add to address list. */
  /* The uuid may be anything, it is not used by xcom */
  u_int j = 0;
  blob uuid_tmp = uuid_blob(arg);
  nl->node_list_len = 1;
  nl->node_list_val = new_node_address_uuid(nl->node_list_len, &arg, &uuid_tmp);
  GET_GOUT;
  ADD_GOUT("init_me uuid");
  for (j = 0; j < uuid_tmp.data.data_len; j++) {
    ADD_F_GOUT(" %x", *((unsigned char *)&uuid_tmp.data.data_val[j]));
  }
  ADD_GOUT(" services");
  ADD_F_GOUT(" %x", nl->node_list_val->services);
  PRINT_GOUT;
  FREE_GOUT;
  free(uuid_tmp.data.data_val);
}

TEST_F(XcomSingleWriter, test_analyze_leaders) {
  bool failed{false};
  char const *node0{"iamthegreatest:12345"};
  char const *node1{"node1:12346"};
  blob uuid{uuid_blob(node1)};
  site_def site;

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    uuid.data.data_len = 0;
    free(uuid.data.data_val);
    free_site_def_body(p);
  }};
  std::unique_ptr<site_def, decltype(cleanup)> guard(&site, cleanup);

  // Note failure and print message
  auto fail{[&failed](auto... args) {
    xprintf(std::cerr, "test_analyze_leaders FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  auto assert_cached_leaders{[&]() {
    if (!site.cached_leaders)
      fail("site.cached_leaders should be set if max_active_leaders == 1");
  }};

  auto assert_no_cached_leaders{[&]() {
    if (site.cached_leaders)
      fail(
          "site.cached_leaders should not be set if max_active_leaders == "
          "active_leaders_all");
  }};

  auto assert_leader{[&](node_no n, char const *why) {
    if (!site.active_leader[n])
      fail("site.active_leader[", n, "] should be set if ", why);
    if (!::is_active_leader(n, &site))
      fail("::is_active_leader(", n, ") should return 1 if ", why);
  }};

  auto assert_not_leader{[&](node_no n, char const *why) {
    if (site.active_leader[n])
      fail("site.active_leader[", n, "] should not be set if ", why);
    if (::is_active_leader(n, &site))
      fail("::is_active_leader(", n, ") should return 0 if ", why);
  }};

  auto assert_found_leaders{[&](node_no n) {
    if (::found_active_leaders(&site) != n)
      fail("::found_active_leaders(site) should return ", n);
  }};

  // Completely empty config with all nodes as leaders
  site.max_active_leaders = 0;
  ::analyze_leaders(&site);
  assert_no_cached_leaders();
  assert_found_leaders(0);

  // Single writer, but no nodes in global node set
  site.nodeno = 0;  // I am node 0
  site.max_active_leaders = 1;
  init_me(&site.nodes, node0);
  init_node_set(&site.global_node_set, 1);

  ::analyze_leaders(&site);
  assert_cached_leaders();
  // Node 0 wil be leader if node set empty
  assert_leader(0, "no nodes in global node set");
  assert_found_leaders(1);

  // Single writer, all nodes in global node set
  site.cached_leaders = 0;
  set_node_set(&site.global_node_set);

  ::analyze_leaders(&site);
  assert_cached_leaders();
  assert_leader(0, "all nodes in global node set");
  assert_found_leaders(1);

  // Add second node as leader
  node_address *addr = new_node_address_uuid(1, &node1, &uuid);
  add_site_def(1, addr, &site);
  delete_node_address(1, addr);
  site.leaders = alloc_leader_array(1);
  site.leaders.leader_array_val[0].address = strdup(node1);

  site.cached_leaders = 0;
  reset_node_set(&site.global_node_set);  // Mark all as down

  ::analyze_leaders(&site);
  assert_cached_leaders();
  assert_not_leader(1, "all are down");
  assert_leader(0, "all are down");
  assert_found_leaders(1);

  site.cached_leaders = 0;
  set_node_set(&site.global_node_set);  // Mark all as present

  ::analyze_leaders(&site);
  assert_cached_leaders();
  assert_not_leader(0, "node1 is leader");
  assert_leader(1, "node1 is leader");
  assert_found_leaders(1);

  // Remove leader from global node set
  site.global_node_set.node_set_val[1] = 0;
  site.cached_leaders = 0;

  ::analyze_leaders(&site);
  assert_cached_leaders();
  assert_leader(0, "node1 is down");
  assert_not_leader(1, "node1 is down");
  assert_found_leaders(1);
}

TEST_F(XcomSingleWriter, test_unsafe_leaders) {
  bool failed = false;
  char const *node0 = "iamthegreatest:12345";
  app_data a;
  memset(&a, 0, sizeof a);
  site_def *site = new_site_def();
  char const *node1{"node1:12346"};
  blob uuid{uuid_blob(node1)};

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    (void)p;
    uuid.data.data_len = 0;
    free(uuid.data.data_val);
    free_site_defs();
    xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  }};
  std::unique_ptr<site_def, decltype(cleanup)> guard(site, cleanup);

  // Note failure and print message
  auto fail{[&failed](auto... args) {
    xprintf(std::cerr, "test_unsafe_leaders FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  // Null app_data
  a.body.c_t = add_node_type;

  if (::unsafe_leaders(&a)) fail("empty app_data should be all right");

  // Single node, compatible protocol version and max_leaders == all
  node_list &nl = a.body.app_u_u.nodes;
  init_me(&nl, node0);
  push_site_def(site);
  if (::unsafe_leaders(&a))
    fail("Compatible protocol version and max_active_leaders == all");

  // Single node, compatible protocol version and max_active_leaders == 1
  site->max_active_leaders = 1;
  if (::unsafe_leaders(&a))
    fail("Compatible protocol version and max_active_leaders == 1");

  // Single node, incompatible protocol version and max_active_leaders == all
  site->max_active_leaders = active_leaders_all;
  nl.node_list_val[0].proto.max_proto = x_1_8;
  if (::unsafe_leaders(&a))
    fail("Incompatible protocol version and max_active_leaders == all");

  // Single node, incompatible protocol version and max_active_leaders == 1
  site->max_active_leaders = 1;
  if (!::unsafe_leaders(&a))
    fail("Incompatible protocol version and max_active_leaders == 1");

  // Add second node with compatible protocol version
  node_address *addr = new_node_address_uuid(1, &node1, &uuid);
  add_site_def(1, addr, site);
  delete_node_address(1, addr);
  if (!::unsafe_leaders(&a))
    fail(
        "Two nodes, 1 incompatible protocol version and max_active_leaders == "
        "1");

  // Switch to max_active_leaders == all
  site->max_active_leaders = active_leaders_all;
  if (::unsafe_leaders(&a))
    fail(
        "Two nodes, 1 incompatible protocol version and max_active_leaders == "
        "all");

  // Make both protocol versions compatible
  nl.node_list_val[0].proto.max_proto = x_1_9;
  if (::unsafe_leaders(&a))
    fail(
        "Two nodes, compatible protocol versions and max_active_leaders == "
        "all");

  // Make both protocol versions compatible
  site->max_active_leaders = 1;
  if (::unsafe_leaders(&a))
    fail("Two nodes, compatible protocol versions and max_active_leaders == 1");
}

TEST_F(XcomSingleWriter, test_handle_max_leaders) {
  bool failed = false;
  char const *node0 = "iamthegreatest:12345";
  app_data a;
  memset(&a, 0, sizeof a);
  site_def *site = new_site_def();
  char const *node1{"node1:12346"};
  blob uuid{uuid_blob(node1)};

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    (void)p;
    uuid.data.data_len = 0;
    free(uuid.data.data_val);
    free_site_defs();
    xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  }};
  std::unique_ptr<site_def, decltype(cleanup)> guard(site, cleanup);

  // Note failure and print message
  auto fail{[&failed](auto... args) {
    xprintf(std::cerr, "test_handle_max_leaders FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  // Single node, compatible protocol version and max_leaders == all
  init_me(&site->nodes, node0);
  alloc_node_set(&site->global_node_set, 1);
  alloc_node_set(&site->local_node_set, 1);
  site->nodeno = 0;
  site->event_horizon = EVENT_HORIZON_MIN;
  site_install_action(site, unified_boot_type);
  init_set_max_leaders(test_group_id, &a, active_leaders_all);
  if (!handle_max_leaders(&a))
    fail("Compatible protocol version and new max_leaders == all");

  // Single node, compatible protocol version and max_leaders == 1
  a.body.app_u_u.max_leaders = 1;
  if (!handle_max_leaders(&a))
    fail("Compatible protocol version and new max_leaders == 1");

  init_set_max_leaders(test_group_id, &a, 2);
  if (handle_max_leaders(&a))
    fail("Compatible protocol version and new max_leaders == 2");

  // Add second node with incompatible protocol version, max_leaders == all
  site = clone_site_def(get_site_def());
  node_address *addr = new_node_address_uuid(1, &node1, &uuid);
  addr->proto.max_proto = x_1_8;
  add_site_def(1, addr, site);
  site_install_action(site, add_node_type);
  delete_node_address(1, addr);
  init_set_max_leaders(test_group_id, &a, active_leaders_all);
  if (!handle_max_leaders(&a))
    fail("Incompatible protocol version and max_leaders == all");

  // Two nodes, incompatible protocol version and max_leaders == 1
  init_set_max_leaders(test_group_id, &a, 1);
  if (handle_max_leaders(&a))
    fail("Incompatible protocol version and max_leaders == 1");

  // Two nodes, incompatible protocol version and max_leaders == 2
  init_set_max_leaders(test_group_id, &a, 2);
  if (handle_max_leaders(&a))
    fail("Incompatible protocol version and max_leaders == 2");

  // Two nodes, incompatible protocol version and max_leaders == 3
  init_set_max_leaders(test_group_id, &a, 3);
  if (handle_max_leaders(&a))
    fail("Incompatible protocol version and max_leaders == 3");
}

TEST_F(XcomSingleWriter, test_handle_set_leaders) {
  bool failed = false;
  char const *nodes[]{"iamthegreatest:12345", "node1:12346"};
  app_data a;
  memset(&a, 0, sizeof a);
  site_def *site = new_site_def();
  blob uuid{uuid_blob(nodes[1])};

  auto free_app{[&]() { xdr_free((xdrproc_t)xdr_app_data, (char *)&a); }};

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    (void)p;
    uuid.data.data_len = 0;
    free(uuid.data.data_val);
    free_site_defs();
    free_app();
  }};
  std::unique_ptr<site_def, decltype(cleanup)> guard(site, cleanup);

  // Note failure and print message
  auto fail{[&](auto... args) {
    xprintf(std::cerr, "test_handle_set_leaders FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  init_me(&site->nodes, nodes[0]);
  alloc_node_set(&site->global_node_set, 1);
  alloc_node_set(&site->local_node_set, 1);
  site->nodeno = 0;
  site->event_horizon = EVENT_HORIZON_MIN;
  site_install_action(site, unified_boot_type);

  // Single node, compatible protocol version and leaders node0
  init_set_leaders(test_group_id, &a, 1, nodes);
  if (!handle_set_leaders(&a))
    fail("Compatible protocol version and leaders node0");
  free_app();

  // Compatible protocol version and leaders == node0, node1
  init_set_leaders(test_group_id, &a, 2, nodes);
  if (!handle_set_leaders(&a))
    fail("Compatible protocol version and leaders node0, node1");
  free_app();

  // Add second node with incompatible protocol version
  site = clone_site_def(get_site_def());
  node_address *addr = new_node_address_uuid(1, &nodes[1], &uuid);
  addr->proto.max_proto = x_1_8;
  add_site_def(1, addr, site);
  site_install_action(site, add_node_type);
  delete_node_address(1, addr);

  init_set_leaders(test_group_id, &a, 1, nodes);
  if (handle_set_leaders(&a))
    fail("Both compatible and incompatible protocol versions");
  free_app();

  init_set_leaders(test_group_id, &a, 2, nodes);
  if (handle_set_leaders(&a))
    fail("Both compatible and incompatible protocol versions");
  free_app();

  // Two nodes, incompatible protocol version and leaders == 3
  a.body.app_u_u.max_leaders = 3;
  if (handle_set_leaders(&a))
    fail("Incompatible protocol version and leaders == 3");
}

TEST_F(XcomSingleWriter, test_handle_leaders) {
  bool failed = false;
  char const *nodes[]{"iamthegreatest:12345", "node1:12346"};
  app_data leader_app;
  memset(&leader_app, 0, sizeof leader_app);
  app_data max_app;
  memset(&max_app, 0, sizeof max_app);
  site_def *site = nullptr;
  blob uuid{uuid_blob(nodes[1])};

  auto free_app{[&]() {
    // leader_app and max_app have been linked, so unlink
    // to avoid deallocating the stack objects.
    leader_app.next = nullptr;
    max_app.next = nullptr;
    xdr_free((xdrproc_t)xdr_app_data, (char *)&leader_app);
    xdr_free((xdrproc_t)xdr_app_data, (char *)&max_app);
  }};

  // Deallocate on scope exit
  auto cleanup{[&](auto p) {
    (void)p;
    uuid.data.data_len = 0;
    free(uuid.data.data_val);
    free_site_defs();
    free_app();
  }};

  // Note failure and print message
  auto fail{[&](auto... args) {
    xprintf(std::cerr, "test_handle_leaders FAILED: ", args..., "\n");
    failed = true;
    ASSERT_FALSE(failed);
  }};

  site = new_site_def();
  std::unique_ptr<site_def, decltype(cleanup)> guard(site, cleanup);

  init_me(&site->nodes, nodes[0]);
  alloc_node_set(&site->global_node_set, 1);
  alloc_node_set(&site->local_node_set, 1);
  site->nodeno = 0;
  site->event_horizon = EVENT_HORIZON_MIN;
  site_install_action(site, unified_boot_type);

  // Single node, compatible protocol version and leaders node0
  init_set_leaders(test_group_id, &leader_app, 1, nodes, &max_app, 1);
  if (!::handle_leaders(&leader_app))
    fail("Compatible protocol version and leaders node0, max_leaders == 1");
  free_app();

  // Compatible protocol version and leaders == node0, node1
  init_set_leaders(test_group_id, &leader_app, 2, nodes, &max_app, 1);
  if (!::handle_leaders(&leader_app))
    fail(
        "Compatible protocol version and leaders node0, node1, max_leaders == "
        "1");
  free_app();

  // Single node, compatible protocol version and leaders node0
  init_set_leaders(test_group_id, &leader_app, 1, nodes, &max_app, 2);
  if (::handle_leaders(&leader_app))
    fail("Compatible protocol version and leaders node0, max_leaders == 2");
  free_app();

  // Compatible protocol version and leaders == node0, node1
  init_set_leaders(test_group_id, &leader_app, 2, nodes, &max_app, 2);
  if (::handle_leaders(&leader_app))
    fail(
        "Compatible protocol version and leaders node0, node1, max_leaders == "
        "2");
  free_app();

  // Single node, compatible protocol version and leaders node0
  init_set_leaders(test_group_id, &leader_app, 1, nodes, &max_app, 3);
  if (::handle_leaders(&leader_app))
    fail("Compatible protocol version and leaders node0, max_leaders == 3");
  free_app();

  // Compatible protocol version and leaders == node0, node1
  init_set_leaders(test_group_id, &leader_app, 2, nodes, &max_app, 3);
  if (::handle_leaders(&leader_app))
    fail(
        "Compatible protocol version and leaders node0, node1, max_leaders == "
        "3");
  free_app();

  // Add second node with incompatible protocol version
  site = clone_site_def(get_site_def());
  node_address *addr = new_node_address_uuid(1, &nodes[1], &uuid);
  addr->proto.max_proto = x_1_8;
  add_site_def(1, addr, site);
  site_install_action(site, add_node_type);
  delete_node_address(1, addr);

  init_set_leaders(test_group_id, &leader_app, 1, nodes, &max_app, 1);
  if (::handle_leaders(&leader_app))
    fail(
        "Both compatible and incompatible protocol versions, max_leaders == 1");
  free_app();

  init_set_leaders(test_group_id, &leader_app, 2, nodes, &max_app, 1);
  if (::handle_leaders(&leader_app))
    fail(
        "Both compatible and incompatible protocol versions, max_leaders == 1");
  free_app();

  init_set_leaders(test_group_id, &leader_app, 1, nodes, &max_app, 2);
  if (::handle_leaders(&leader_app))
    fail(
        "Both compatible and incompatible protocol versions, max_leaders == 2");
  free_app();

  init_set_leaders(test_group_id, &leader_app, 2, nodes, &max_app, 2);
  if (::handle_leaders(&leader_app))
    fail(
        "Both compatible and incompatible protocol versions, max_leaders == 2");
  free_app();

  init_set_leaders(test_group_id, &leader_app, 1, nodes, &max_app, 3);
  if (::handle_leaders(&leader_app))
    fail(
        "Both compatible and incompatible protocol versions, max_leaders == 3");
  free_app();

  init_set_leaders(test_group_id, &leader_app, 2, nodes, &max_app, 3);
  if (::handle_leaders(&leader_app))
    fail(
        "Both compatible and incompatible protocol versions, max_leaders == 3");
  free_app();
}

}  // namespace xcom_base_unittest
