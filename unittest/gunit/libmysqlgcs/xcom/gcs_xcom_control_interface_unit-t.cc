/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "synode_no.h"

namespace gcs_interface_unittest {

class GcsInterfaceTest : public GcsBaseTest {};

TEST_F(GcsInterfaceTest, NodeTooFarMessageUnit) {
  // Ideally, we should have a mock proxy and a mock control, but they are not
  // actually called by the test.
  Gcs_suspicions_manager *mgr = new Gcs_suspicions_manager(NULL, NULL);
  Gcs_xcom_nodes xcom_nodes;
  mgr->set_suspicions_processing_period(15u);
  mgr->set_non_member_expel_timeout_seconds(60ul);
  mgr->set_member_expel_timeout_seconds(60ul);

  // Build vector with suspect nodes
  std::vector<Gcs_member_identifier *> no_nodes, member_suspect_nodes;
  member_suspect_nodes.push_back(new Gcs_member_identifier("127.0.0.1:12346"));
  member_suspect_nodes.push_back(new Gcs_member_identifier("127.0.0.1:12347"));
  xcom_nodes.add_node(Gcs_xcom_node_information("127.0.0.1:12345", false));
  xcom_nodes.add_node(Gcs_xcom_node_information("127.0.0.1:12346", false));
  xcom_nodes.add_node(Gcs_xcom_node_information("127.0.0.1:12347", false));
  xcom_nodes.add_node(Gcs_xcom_node_information("127.0.0.1:12348", false));
  xcom_nodes.add_node(Gcs_xcom_node_information("127.0.0.1:12349", false));

  std::vector<Gcs_xcom_node_information>::iterator node_it;
  std::vector<Gcs_xcom_node_information> nodes = xcom_nodes.get_nodes();
  for (node_it = nodes.begin(); node_it != nodes.end(); ++node_it) {
    ASSERT_FALSE(node_it->has_lost_messages());
    ASSERT_TRUE(synode_eq(node_it->get_max_synode(), null_synode));
  }

  synode_no suspicion_synode = {1, 100, 0};
  // Insert suspicions into manager
  mgr->process_view(&xcom_nodes, no_nodes, no_nodes, member_suspect_nodes,
                    no_nodes, true, suspicion_synode);

  // Run thread and check that messages have not yet been lost since nothing
  // has yet been removed from the cache.
  mgr->run_process_suspicions(true);

  std::vector<Gcs_member_identifier *>::iterator it;
  for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
       ++it) {
    ASSERT_FALSE(mgr->get_suspicions().get_node(*(*it))->has_lost_messages());
    ASSERT_TRUE(
        synode_eq(mgr->get_suspicions().get_node(*(*it))->get_max_synode(),
                  suspicion_synode));
  }

  synode_no last_removed = {1, 200, 0};
  // Do it again with a higher last_removed_from_cache value
  mgr->update_last_removed(last_removed);
  mgr->run_process_suspicions(true);

  Gcs_xcom_node_information *node = NULL;
  for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
       ++it) {
    node = const_cast<Gcs_xcom_node_information *>(
        mgr->get_suspicions().get_node(*(*it)));
    ASSERT_TRUE(node->has_lost_messages());
    ASSERT_TRUE(synode_eq(node->get_max_synode(), suspicion_synode));
  }

  // Clear current suspicions...
  mgr->clear_suspicions();
  // ...and add them again to see if the message related vars are cleared.
  mgr->process_view(&xcom_nodes, no_nodes, no_nodes, member_suspect_nodes,
                    no_nodes, true, last_removed);

  for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
       ++it) {
    node = const_cast<Gcs_xcom_node_information *>(
        mgr->get_suspicions().get_node(*(*it)));
    ASSERT_FALSE(node->has_lost_messages());
    ASSERT_TRUE(synode_eq(node->get_max_synode(), last_removed));
  }

  for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
       ++it)
    delete (*it);
  member_suspect_nodes.clear();
}

}  // namespace gcs_interface_unittest
