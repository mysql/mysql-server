/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <vector>
#include <string>

#include "gcs_base_test.h"

using std::vector;

namespace gcs_whitelist_unittest
{

class GcsWhitelist : public GcsBaseTest
{
protected:
  GcsWhitelist() {}
};

TEST_F(GcsWhitelist, ValidIPs)
{
  Gcs_ip_whitelist wl;
  ASSERT_TRUE(wl.is_valid("192.168.1.1"));
  ASSERT_TRUE(wl.is_valid("192.168.1.2"));
  ASSERT_TRUE(wl.is_valid("192.168.1.254"));

  ASSERT_TRUE(wl.is_valid("::1"));
  ASSERT_TRUE(wl.is_valid("::1/2"));
  ASSERT_TRUE(wl.is_valid("::1/64,192.168.1.2/24"));
  ASSERT_TRUE(wl.is_valid("::1/64,192.168.1.2/24,192.168.1.1"));
  ASSERT_TRUE(wl.is_valid("::1/64,192.168.1.2/24,192.168.1.1,10.1.1.1"));
}

TEST_F(GcsWhitelist, InvalidConfiguration)
{
  Gcs_ip_whitelist wl;
  ASSERT_FALSE(wl.is_valid("192.168.1"));
  ASSERT_FALSE(wl.is_valid("192.168.1/24"));
  ASSERT_FALSE(wl.is_valid("192.168.1.0/33"));
  ASSERT_FALSE(wl.is_valid("192.168.1.0/24,192.168.2.0/33"));
}

TEST_F(GcsWhitelist, ValidListIPv6)
{
  Gcs_ip_whitelist wl;
  std::string list= "::1/128,::ffff:192.168.1.1/24,fe80::2ab2:bdff:fe16:8d07/67";
  wl.configure(list);

  ASSERT_FALSE(wl.shall_block("::1"));
  ASSERT_FALSE(wl.shall_block("fe80::2ab2:bdff:fe16:8d07"));
  ASSERT_FALSE(wl.shall_block("::ffff:192.168.1.10"));
  ASSERT_TRUE(wl.shall_block("192.168.1.10"));
}

TEST_F(GcsWhitelist, ValidListIPv4)
{
  Gcs_ip_whitelist wl;
  wl.configure("192.168.1.0/31");

  ASSERT_FALSE(wl.shall_block("192.168.1.1"));
  ASSERT_TRUE(wl.shall_block("192.168.2.1"));
  ASSERT_TRUE(wl.shall_block("192.168.1.2"));

  wl.configure("192.168.1.0/32");
  ASSERT_TRUE(wl.shall_block("192.168.1.1"));

  wl.configure("192.168.1.1/32");
  ASSERT_FALSE(wl.shall_block("192.168.1.1"));

  // never block localhost
  ASSERT_FALSE(wl.shall_block("127.0.0.1"));

  wl.configure("192.168.1.0/24,192.168.2.0/24");
  ASSERT_FALSE(wl.shall_block("127.0.0.1"));
  ASSERT_FALSE(wl.shall_block("192.168.1.2"));
  ASSERT_FALSE(wl.shall_block("192.168.1.254"));
  ASSERT_FALSE(wl.shall_block("192.168.2.2"));
  ASSERT_FALSE(wl.shall_block("192.168.2.254"));
}

TEST_F(GcsWhitelist, DefaultList)
{
  Gcs_ip_whitelist wl;

  wl.configure(Gcs_ip_whitelist::DEFAULT_WHITELIST);
  ASSERT_FALSE(wl.shall_block("127.0.0.1"));
  ASSERT_TRUE(wl.shall_block("::1"));
  ASSERT_FALSE(wl.shall_block("192.168.1.2"));
  ASSERT_FALSE(wl.shall_block("192.168.2.2"));
  ASSERT_FALSE(wl.shall_block("10.0.0.1"));
  ASSERT_TRUE(wl.shall_block("172.15.0.1"));
  ASSERT_FALSE(wl.shall_block("172.16.0.1"));
  ASSERT_FALSE(wl.shall_block("172.24.0.1"));
  ASSERT_FALSE(wl.shall_block("172.31.0.1"));
  ASSERT_TRUE(wl.shall_block("172.38.0.1"));
}

TEST_F(GcsWhitelist, ListAsText)
{
  Gcs_ip_whitelist wl;

  wl.configure(Gcs_ip_whitelist::DEFAULT_WHITELIST);
  ASSERT_STRCASEEQ(Gcs_ip_whitelist::DEFAULT_WHITELIST.c_str(),
                   wl.get_configured_ip_whitelist().c_str());
}

TEST_F(GcsWhitelist, AbsentList)
{
  Gcs_interface_parameters params;
  params.add_parameter("group_name", "ola");
  params.add_parameter("peer_nodes", "127.0.0.1:24844");
  params.add_parameter("local_node", "127.0.0.1:24844");
  params.add_parameter("bootstrap_group", "true");
  params.add_parameter("poll_spin_loops", "100");

  Gcs_interface *gcs= Gcs_xcom_interface::get_interface();
  enum_gcs_error err= gcs->initialize(params);
  ASSERT_EQ(err, GCS_OK);

  // verify that a whitelist was provided by default
  Gcs_xcom_interface *xcs= static_cast<Gcs_xcom_interface*>(gcs);
  MYSQL_GCS_LOG_INFO("Whitelist as string with collected IP addresses: " <<
                     xcs->get_ip_whitelist().to_string());
  ASSERT_FALSE(xcs->get_ip_whitelist().get_configured_ip_whitelist().empty());
  ASSERT_FALSE(xcs->get_ip_whitelist().to_string().empty());

  // this finalizes the m_logger, so be careful to not add a call to
  // MYSQL_GCS_LOG after this line
  err= gcs->finalize();

  // claim interface memory back
  xcs->cleanup();

  // initialization failed, and thus so will finalization
  ASSERT_EQ(err, GCS_OK);
}

}
