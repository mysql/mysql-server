/*
  Copyright (c) 2024, Oracle and/or its affiliates.
*/

#ifndef CONNECTION_CONTROL_PLUGIN_OPTION_USAGE_H
#define CONNECTION_CONTROL_PLUGIN_OPTION_USAGE_H

extern bool connection_control_plugin_option_usage_init();
extern bool connection_control_plugin_option_usage_deinit();
extern bool connection_control_plugin_option_usage_set(
    unsigned long every_nth_time = 100);

#endif /* CONNECTION_CONTROL_PLUGIN_OPTION_USAGE_H */
