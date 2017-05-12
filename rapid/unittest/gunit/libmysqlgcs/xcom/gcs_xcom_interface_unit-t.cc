/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "gcs_xcom_interface.h"
#include "synode_no.h"

#include <vector>
#include <string>

using std::vector;

extern void do_cb_xcom_receive_data(synode_no message_id,
                                    Gcs_xcom_nodes *xcom_nodes, u_int size,
                                    char *data);

namespace gcs_interface_unittest
{

class GcsInterfaceTest : public ::testing::Test
{
protected:
  GcsInterfaceTest() {};


  virtual void SetUp()
  { }


  virtual void TearDown()
  { }

};


/**
  This test is primarily to run valgrind to make sure
  that the interface can be initialized and finalized
  multiple times without any leak.
 */
TEST_F(GcsInterfaceTest, DoubleInitFinalizeTest)
{
  Gcs_interface *gcs=
    Gcs_xcom_interface::get_interface();

  Gcs_interface_parameters if_params;
  if_params.add_parameter("group_name", "ola");
  if_params.add_parameter("peer_nodes", "127.0.0.1:24844");
  if_params.add_parameter("local_node", "127.0.0.1:24844");
  if_params.add_parameter("bootstrap_group", "true");

  // initialize the interface
  gcs->initialize(if_params);

  // finalize the interface
  gcs->finalize();

  // initialize the interface again
  gcs->initialize(if_params);

  // finalize it again interface
  gcs->finalize();

  // fake factory cleanup member function
  static_cast<Gcs_xcom_interface*>(gcs)->cleanup();
}

TEST_F(GcsInterfaceTest, ReceiveEmptyMessageTest)
{
  Gcs_interface *gcs=
    Gcs_xcom_interface::get_interface();

  Gcs_interface_parameters if_params;
  if_params.add_parameter("group_name", "ola");
  if_params.add_parameter("peer_nodes", "127.0.0.1:24844");
  if_params.add_parameter("local_node", "127.0.0.1:24844");
  if_params.add_parameter("bootstrap_group", "true");

  // initialize the interface
  gcs->initialize(if_params);

  // invoke the callback with a message with size zero
  do_cb_xcom_receive_data(null_synode, NULL, 0, NULL);

  // finalize the interface
  gcs->finalize();

  // fake factory cleanup member function
  static_cast<Gcs_xcom_interface*>(gcs)->cleanup();
}
}
