/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <chrono>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "router_component_clusterset.h"
#include "router_component_testutils.h"

#ifndef _ROUTER_COMPONENT_METADATA_H_
#define _ROUTER_COMPONENT_METADATA_H_

class RouterComponentMetadataTest : public RouterComponentClusterSetTest {
 protected:
  std::string get_metadata_cache_section(
      ClusterType cluster_type = ClusterType::GR_V2,
      const std::string &ttl = "0.5", const std::string &cluster_name = "test",
      const std::string &ssl_mode = "");

  std::string get_metadata_cache_routing_section(
      uint16_t router_port, const std::string &role,
      const std::string &strategy, const std::string &section_name = "default",
      const std::string &protocol = "classic",
      const std::vector<std::pair<std::string, std::string>>
          &additional_options = {});

  std::vector<std::string> get_array_field_value(const std::string &json_string,
                                                 const std::string &field_name);

  int get_ttl_queries_count(const std::string &json_string) {
    return get_int_field_value(json_string, "md_query_count");
  }

  int get_update_attributes_count(const std::string &json_string) {
    return get_int_field_value(json_string, "update_attributes_count");
  }

  int get_update_last_check_in_count(const std::string &json_string) {
    return get_int_field_value(json_string, "update_last_check_in_count");
  }

  bool wait_metadata_read(const ProcessWrapper &router,
                          const std::chrono::milliseconds timeout);

  ProcessWrapper &launch_router(
      const std::string &metadata_cache_section,
      const std::string &routing_section,
      std::vector<uint16_t> metadata_server_ports, const int expected_exitcode,
      std::chrono::milliseconds wait_for_notify_ready =
          std::chrono::seconds(30));

  std::string setup_router_config(const std::string &metadata_cache_section,
                                  const std::string &routing_section,
                                  std::vector<uint16_t> metadata_server_ports);

  std::string state_file_;
  const std::string router_metadata_username{"mysql_router1_user"};
};

#endif  // _ROUTER_COMPONENT_METADATA_H_
