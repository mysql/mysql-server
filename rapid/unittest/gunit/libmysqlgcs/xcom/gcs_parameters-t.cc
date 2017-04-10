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
#include "gcs_message_stage_lz4.h"

#include <vector>
#include <string>

using std::vector;

namespace gcs_parameters_unittest
{

class GcsParametersTest : public ::testing::Test
{
protected:
  GcsParametersTest() : m_gcs(NULL) {};


  virtual void SetUp()
  {
    m_gcs= Gcs_xcom_interface::get_interface();

    // convenience alias to specialized version of Gcs_interface.
    m_xcs= static_cast<Gcs_xcom_interface*>(m_gcs);

    // These are all parameters and are all valid
    m_params.add_parameter("group_name", "ola");
    m_params.add_parameter("local_node", "127.0.0.1:24844");
    m_params.add_parameter("peer_nodes", "127.0.0.1:24844,127.0.0.1:24845");
    m_params.add_parameter("bootstrap_group", "true");
    m_params.add_parameter("poll_spin_loops", "100");
    m_params.add_parameter("compression", "on");
    m_params.add_parameter("compression_threshold", "1024");
    m_params.add_parameter("ip_whitelist", "127.0.0.1,192.168.1.0/24");
  }

  virtual void TearDown()
  {
    // fake factory cleanup member function
    static_cast<Gcs_xcom_interface*>(m_gcs)->cleanup();
  }


  Gcs_interface *m_gcs;
  Gcs_xcom_interface *m_xcs;
  Gcs_interface_parameters m_params;

  void do_check_params()
  {
    enum_gcs_error err= m_gcs->initialize(m_params);
    ASSERT_EQ(err, GCS_NOK);
    err= m_gcs->finalize();
    // initialization failed, and thus so will finalization
    ASSERT_EQ(err, GCS_NOK);
  }

  void do_check_ok_params()
  {
    enum_gcs_error err= m_gcs->initialize(m_params);
    ASSERT_EQ(err, GCS_OK);
    err= m_gcs->finalize();
    // initialization succeeded, and thus so will finalization
    ASSERT_EQ(err, GCS_OK);
  }

};

/*
 This file contains a bunch of test for BUG#22901408.

 Checks default values for compression, does sanity checks, etc.
 */
TEST_F(GcsParametersTest, ParametersCompression)
{
  enum_gcs_error err;

  // --------------------------------------------------------
  // Compression default values
  // --------------------------------------------------------
  Gcs_interface_parameters implicit_values;
  implicit_values.add_parameter("group_name", "ola");
  implicit_values.add_parameter("peer_nodes", "127.0.0.1:24844,127.0.0.1:24845");
  implicit_values.add_parameter("local_node", "127.0.0.1:24844");
  implicit_values.add_parameter("bootstrap_group", "true");
  implicit_values.add_parameter("poll_spin_loops", "100");

  err= m_gcs->initialize(implicit_values);

  ASSERT_EQ(err, GCS_OK);

  const Gcs_interface_parameters &init_params=
    m_xcs->get_initialization_parameters();

  // compression is ON by default
  ASSERT_TRUE (init_params.get_parameter("compression")->
                  compare("on") == 0);

  // compression_threshold is set to the default
  std::stringstream ss;
  ss << Gcs_message_stage_lz4::DEFAULT_THRESHOLD;
  ASSERT_TRUE(init_params.get_parameter("compression_threshold")->
                  compare(ss.str()) == 0);

  // finalize the interface
  err= m_gcs->finalize();

  ASSERT_EQ(err, GCS_OK);

  // --------------------------------------------------------
  // Compression explicit values
  // --------------------------------------------------------
  std::string compression= "off";
  std::string compression_threshold= "1";

  Gcs_interface_parameters explicit_values;
  explicit_values.add_parameter("group_name", "ola");
  explicit_values.add_parameter("peer_nodes", "127.0.0.1:24844,127.0.0.1:24845");
  explicit_values.add_parameter("local_node", "127.0.0.1:24844");
  explicit_values.add_parameter("bootstrap_group", "true");
  explicit_values.add_parameter("poll_spin_loops", "100");
  explicit_values.add_parameter("compression", compression);
  explicit_values.add_parameter("compression_threshold", compression_threshold);

  err= m_gcs->initialize(explicit_values);

  const Gcs_interface_parameters &init_params2=
    m_xcs->get_initialization_parameters();

  ASSERT_EQ(err, GCS_OK);

  // compression is ON by default
  ASSERT_TRUE(init_params2.get_parameter("compression")->
                compare(compression) == 0);

  // compression is set to the value we explicitly configured
  ASSERT_TRUE(init_params2.get_parameter("compression_threshold")->
                compare(compression_threshold) == 0);

  err= m_gcs->finalize();

  ASSERT_EQ(err, GCS_OK);
}


TEST_F(GcsParametersTest, SanityParameters)
{
  // initialize the interface
  enum_gcs_error err= m_gcs->initialize(m_params);

  ASSERT_EQ(err, GCS_OK);

  // finalize the interface
  err= m_gcs->finalize();

  ASSERT_EQ(err, GCS_OK);
}

TEST_F(GcsParametersTest, AbsentGroupName)
{
  Gcs_interface_parameters params;
  params.add_parameter("peer_nodes", "127.0.0.1:24844,127.0.0.1:24845");
  params.add_parameter("local_node", "127.0.0.1:24844");
  params.add_parameter("bootstrap_group", "true");
  params.add_parameter("poll_spin_loops", "100");

  enum_gcs_error err= m_gcs->initialize(params);
  ASSERT_EQ(err, GCS_NOK);
  err= m_gcs->finalize();
  // initialization failed, and thus so will finalization
  ASSERT_EQ(err, GCS_NOK);
}


TEST_F(GcsParametersTest, AbsentPeerNodes)
{
  Gcs_interface_parameters params;
  params.add_parameter("group_name", "ola");
  params.add_parameter("local_node", "127.0.0.1:24844");
  params.add_parameter("bootstrap_group", "true");
  params.add_parameter("poll_spin_loops", "100");

  enum_gcs_error err= m_gcs->initialize(params);
  ASSERT_EQ(err, GCS_NOK);
  err= m_gcs->finalize();
  // initialization failed, and thus so will finalization
  ASSERT_EQ(err, GCS_NOK);
}

TEST_F(GcsParametersTest, AbsentLocalNode)
{
  Gcs_interface_parameters params;
  params.add_parameter("group_name", "ola");
  params.add_parameter("peer_nodes", "127.0.0.1:24844,127.0.0.1:24845");
  params.add_parameter("bootstrap_group", "true");
  params.add_parameter("poll_spin_loops", "100");

  enum_gcs_error err= m_gcs->initialize(params);
  ASSERT_EQ(err, GCS_NOK);
  err= m_gcs->finalize();
  // initialization failed, and thus so will finalization
  ASSERT_EQ(err, GCS_NOK);
}

TEST_F(GcsParametersTest, InvalidPeerNodes)
{
  std::string *p= (std::string*) m_params.get_parameter("peer_nodes");
  std::string save= *p;

  // invalid peer
  *p= "127.0.0.1 24844,127.0.0.1 24845";

  enum_gcs_error err= m_gcs->initialize(m_params);
  ASSERT_EQ(err, GCS_NOK);
  err= m_gcs->finalize();
  // initialization failed, and thus so will finalization
  ASSERT_EQ(err, GCS_NOK);

  *p= save;
}

TEST_F(GcsParametersTest, InvalidLocalNode)
{
  std::string *p= (std::string*) m_params.get_parameter("local_node");
  std::string save= *p;

  // invalid peer
  *p= "127.0.0.1 24844";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidPollSpinLoops)
{
  std::string *p= (std::string*) m_params.get_parameter("poll_spin_loops");
  std::string save= *p;

  *p= "OLA";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidCompressionThreshold)
{
  std::string *p= (std::string*) m_params.get_parameter("compression_threshold");
  std::string save= *p;

  *p= "OLA";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidLocalNodeAddress)
{
  std::string *p= (std::string*) m_params.get_parameter("local_node");
  std::string save= *p;

  *p= "127.0";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidWhitelistIPMask)
{
  std::string *p= (std::string*) m_params.get_parameter("ip_whitelist");
  std::string save= *p;

  *p= "192.168.1.1/33";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidWhitelistIP)
{
  std::string *p= (std::string*) m_params.get_parameter("ip_whitelist");
  std::string save= *p;

  *p= "192.168.1.256/24";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidWhitelistIPs)
{
  std::string *p= (std::string*) m_params.get_parameter("ip_whitelist");
  std::string save= *p;

  *p= "192.168.1.222/24,255.257.256.255";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, HalfBakedIP)
{
  std::string *p= (std::string*) m_params.get_parameter("ip_whitelist");
  std::string save= *p;

  *p= "192.168.";
  do_check_params();
  *p= save;
}

TEST_F(GcsParametersTest, InvalidLocalNode_IP_not_found)
{
  std::string *p= (std::string*) m_params.get_parameter("local_node");
  std::string save= *p;

  *p= "8.8.8.8:24844";
  do_check_params();

  *p= "128.0.3.4:12345";
  do_check_params();

  *p= "localhost:12345";
  do_check_ok_params();

  *p= save;
}

}
