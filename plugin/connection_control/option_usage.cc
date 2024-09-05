/*
  Copyright (c) 2024, Oracle and/or its affiliates.
*/

#include "option_usage.h"
#include <string>
#include "connection_control.h"
#include "mysql/components/component_implementation.h"
#include "mysql/components/library_mysys/option_usage_data.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/util/weak_service_reference.h"

static const std::string c_name("connection_control plugin"),
    opt_name("mysql_option_tracker_option"),
    c_option_name("Connection DoS control");
typedef weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                               c_name, opt_name>
    weak_option;
static Option_usage_data *option_usage{nullptr};

bool connection_control_plugin_option_usage_init() {
  assert(option_usage == nullptr);
  option_usage =
      new (std::nothrow) Option_usage_data(c_option_name.c_str(), reg_srv);
  return weak_option::init(
      reg_srv, reg_reg, [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->define(
                        c_option_name.c_str(), c_name.c_str(),
                        g_variables.failed_connections_threshold > 0 ? 1 : 0);
      });
}

bool connection_control_plugin_option_usage_deinit() {
  if (option_usage) {
    delete option_usage;
    option_usage = nullptr;
  }
  return weak_option::deinit(
      reg_srv, reg_reg, [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->undefine(c_option_name.c_str());
      });
}

bool connection_control_plugin_option_usage_set(unsigned long every_nth_time) {
  return option_usage->set_sampled(true, every_nth_time);
}
