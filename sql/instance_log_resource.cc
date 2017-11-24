/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/instance_log_resource.h"


int MY_ATTRIBUTE((visibility("default")))
Instance_log_resource::dummy_function_to_ensure_we_are_linked_into_the_server()
{
  return 1;
}


void Instance_log_resource_mi_wrapper::lock()
{
  mysql_mutex_lock(&mi->data_lock);
};


void Instance_log_resource_mi_wrapper::unlock()
{
  mysql_mutex_unlock(&mi->data_lock);
};


bool Instance_log_resource_mi_wrapper::collect_info()
{
  bool error= false;

  mysql_mutex_assert_owner(&mi->data_lock);

  Json_array *json_channels= static_cast<Json_array *>(get_json());

  Json_string json_channel_name(mi->get_channel());

  LOG_INFO log_info;
  mi->get_flushed_relay_log_info(&log_info);
  size_t dir_len = dirname_length(log_info.log_file_name);
  Json_string json_log_file(log_info.log_file_name + dir_len);
  Json_int json_log_pos(log_info.pos);

  Json_object json_channel;
  error= json_channel.add_clone("channel_name",
                                (const Json_dom *)&json_channel_name);
  if (!error)
    error= json_channel.add_clone("relay_log_file", &json_log_file);
  if (!error)
    error= json_channel.add_clone("relay_log_position", &json_log_pos);
  if (!error)
    json_channels->append_clone(&json_channel);

  return error;
};


void Instance_log_resource_binlog_wrapper::lock()
{
  mysql_mutex_lock(binlog->get_log_lock());
};


void Instance_log_resource_binlog_wrapper::unlock()
{
  mysql_mutex_unlock(binlog->get_log_lock());
};


bool Instance_log_resource_binlog_wrapper::collect_info()
{
  bool error= false;

  mysql_mutex_assert_owner(binlog->get_log_lock());

  if (binlog->is_open())
  {
    Json_object *json_master= static_cast<Json_object *>(get_json());

    LOG_INFO log_info;
    binlog->get_current_log(&log_info, false);
    size_t dir_len = dirname_length(log_info.log_file_name);
    Json_string json_log_file(log_info.log_file_name + dir_len);
    Json_int json_log_pos(log_info.pos);

    error= json_master->add_clone("binary_log_file", &json_log_file);
    if (!error)
      json_master->add_clone("binary_log_position", &json_log_pos);
  }
  return error;
};


void Instance_log_resource_gtid_state_wrapper::lock()
{
  global_sid_lock->wrlock();
};


void Instance_log_resource_gtid_state_wrapper::unlock()
{
  global_sid_lock->unlock();
};


bool Instance_log_resource_gtid_state_wrapper::collect_info()
{
  bool error= false;
  global_sid_lock->assert_some_wrlock();

  char *gtid_executed_string;
  Json_object *json_master= static_cast<Json_object *>(get_json());
  int len= gtid_state->get_executed_gtids()->to_string(&gtid_executed_string);
  if (!(error= (len < 0)))
  {
    Json_string json_gtid_executed(gtid_executed_string);
    error= json_master->add_clone("gtid_executed", &json_gtid_executed);
  }
  my_free(gtid_executed_string);
  return error;
};


Instance_log_resource *
Instance_log_resource_factory::get_wrapper(Master_info *mi, Json_dom *json)
{
  Instance_log_resource_mi_wrapper *wrapper;
  wrapper= new Instance_log_resource_mi_wrapper(mi, json);
  return wrapper;
}


Instance_log_resource *
Instance_log_resource_factory::get_wrapper(MYSQL_BIN_LOG *binlog,
                                           Json_dom *json)
{
  Instance_log_resource_binlog_wrapper *wrapper;
  wrapper= new Instance_log_resource_binlog_wrapper(binlog, json);
  return wrapper;
}


Instance_log_resource *
Instance_log_resource_factory::get_wrapper(Gtid_state *gtid_state,
                                           Json_dom *json)
{
  Instance_log_resource_gtid_state_wrapper *wrapper;
  wrapper= new Instance_log_resource_gtid_state_wrapper(gtid_state, json);
  return wrapper;
}
