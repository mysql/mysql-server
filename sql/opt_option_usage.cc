/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/opt_option_usage.h"

#include <string>
#include "mysql/components/library_mysys/option_tracker_usage.h"

static const std::string container_mysql_server_name("mysql_server"),
    traditional_optimizer_option_name("Traditional Optimizer"),
    hypergraph_optimizer_option_name("Hypergraph Optimizer");

unsigned long long option_tracker_traditional_optimizer_usage_count = 0;
unsigned long long option_tracker_hypergraph_optimizer_usage_count = 0;

static bool set_option_tracker_traditional_optimizer_usage_count(
    unsigned long long new_value) {
  option_tracker_traditional_optimizer_usage_count = new_value;
  return false;
}

static bool set_option_tracker_hypergraph_optimizer_usage_count(
    unsigned long long new_value) {
  option_tracker_hypergraph_optimizer_usage_count = new_value;
  return false;
}

static bool traditional_optimizer_option_callback_define_failed = false;
static bool hypergraph_optimizer_option_callback_define_failed = false;

bool optimizer_options_usage_init(SERVICE_TYPE(mysql_option_tracker_option) *
                                      opt,
                                  SERVICE_TYPE(registry) * srv_registry) {
  bool traditional_err = false, hypergraph_err = false;
#ifdef WITH_HYPERGRAPH_OPTIMIZER
  bool with_hypergraph_optimizer = true;
#else
  bool with_hypergraph_optimizer = false;
#endif
  unsigned long long temp_usage_counter = 0;
  // Traditional Optimizer option.
  // Option definition
  traditional_err |=
      (0 != opt->define(traditional_optimizer_option_name.c_str(),
                        container_mysql_server_name.c_str(), true));

  // Fetch usage data from database
  traditional_err |=
      option_usage_read_counter(traditional_optimizer_option_name.c_str(),
                                &temp_usage_counter, srv_registry);
  if (!traditional_err) {
    option_tracker_traditional_optimizer_usage_count = temp_usage_counter;
  }
  temp_usage_counter = 0;

  // Register callback to update usage data
  traditional_err |=
      (traditional_optimizer_option_callback_define_failed =
           option_usage_register_callback(
               traditional_optimizer_option_name.c_str(),
               set_option_tracker_traditional_optimizer_usage_count,
               srv_registry));

  // Hypergraph Optimizer option
  // Option definition
  hypergraph_err |= (0 != opt->define(hypergraph_optimizer_option_name.c_str(),
                                      container_mysql_server_name.c_str(),
                                      with_hypergraph_optimizer));

  // Fetch usage data from database
  hypergraph_err |=
      option_usage_read_counter(hypergraph_optimizer_option_name.c_str(),
                                &temp_usage_counter, srv_registry);
  if (!hypergraph_err) {
    option_tracker_hypergraph_optimizer_usage_count = temp_usage_counter;
  }
  temp_usage_counter = 0;

  // Register callback to update usage data
  hypergraph_err |=
      (hypergraph_optimizer_option_callback_define_failed =
           option_usage_register_callback(
               hypergraph_optimizer_option_name.c_str(),
               set_option_tracker_hypergraph_optimizer_usage_count,
               srv_registry));
  return traditional_err || hypergraph_err;
}

bool optimizer_options_usage_deinit(SERVICE_TYPE(mysql_option_tracker_option) *
                                        opt,
                                    SERVICE_TYPE(registry) * srv_registry) {
  bool err = false;
  err |=
      !traditional_optimizer_option_callback_define_failed &&
      option_usage_unregister_callback(
          traditional_optimizer_option_name.c_str(),
          set_option_tracker_traditional_optimizer_usage_count, srv_registry);
  err |= (0 != opt->undefine(traditional_optimizer_option_name.c_str()));
  err |= !hypergraph_optimizer_option_callback_define_failed &&
         option_usage_unregister_callback(
             hypergraph_optimizer_option_name.c_str(),
             set_option_tracker_hypergraph_optimizer_usage_count, srv_registry);
  err |= (0 != opt->undefine(hypergraph_optimizer_option_name.c_str()));
  return err;
}
