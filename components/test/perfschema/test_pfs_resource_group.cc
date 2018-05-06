/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_notification.h>
#include <mysql/components/services/pfs_resource_group.h>
#include <string.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

/**
  @file test_pfs_resource_group.cc

  Test component for the Performance Schema Resource Group service.

  Upon installation, this component registers callback functions for these
  session events:
    session_connect
    session_disconnect
    session_change_user

  These events are triggered externally from an MTR script.

  When a new session connects, the callback function invokes
  set_thread_resource_group() with test data that varies according to
  which of the predefined usernames is associated with the connection.

  Results are logged to a logfile and to stderr.

  @sa test_pfs_resource_group()
*/

REQUIRES_SERVICE_PLACEHOLDER(pfs_notification);
REQUIRES_SERVICE_PLACEHOLDER(pfs_resource_group);

/* True if user "PFS_DEBUG_MODE" connects. */
static bool debug_mode = false;

struct User_data {
  User_data() : thread_priority(0), thread_vcpu(0) {}
  User_data(int priority, int vcpu)
      : thread_priority(priority), thread_vcpu(vcpu) {}
  int thread_priority;
  int thread_vcpu;
};

enum event_type { SESSION_CONNECT, SESSION_DISCONNECT };
const char *event_name[] = {"SESSION_CONNECT", "SESSION_DISCONNECT"};

struct Event_info {
  Event_info(event_type type, const PSI_thread_attrs *attrs)
      : m_type(type), m_attrs(*attrs) {}
  event_type m_type;
  PSI_thread_attrs m_attrs;
};

static int handle = 0;
static std::ofstream log_outfile;
static std::string separator("===========================");

void print_log(std::string msg) {
  log_outfile << msg << std::endl;
  fprintf(stderr, "%s\n", msg.c_str());
  fflush(stderr);
}

void print_event(const Event_info &event, std::string &msg) {
  PSI_thread_attrs thread_attrs = event.m_attrs;
  event_type type = event.m_type;
  std::string event_type_name = event_name[type];
  std::string group, user, host;

  if (thread_attrs.m_groupname_length > 0)
    group =
        std::string(thread_attrs.m_groupname, thread_attrs.m_groupname_length);
  if (thread_attrs.m_username_length > 0)
    user = std::string(thread_attrs.m_username, thread_attrs.m_username_length);
  if (thread_attrs.m_hostname_length > 0)
    host = std::string(thread_attrs.m_hostname, thread_attrs.m_hostname_length);

  User_data user_data;
  if (thread_attrs.m_user_data)
    user_data = *static_cast<User_data *>(thread_attrs.m_user_data);

  std::stringstream ss;
  ss << "*** " << event_type_name;
  if (debug_mode)
    ss << " thread_id= " << thread_attrs.m_thread_internal_id
       << " plist_id= " << thread_attrs.m_processlist_id
       << " os_thread= " << thread_attrs.m_thread_os_id;
  else
    ss << " group= " << group << " user= " << user << " host= " << host
       << " vcpu= " << user_data.thread_vcpu
       << " priority= " << user_data.thread_priority;
  ss << std::endl << msg;
  print_log(ss.str());
}

void session_event(const Event_info &event_info);

/**
  Callback for session connection.
*/
void session_connect_callback(const PSI_thread_attrs *thread_attrs) {
  assert(thread_attrs != NULL);
  session_event(Event_info(SESSION_CONNECT, thread_attrs));
}

/**
  Callback for session disconnect.
*/
void session_disconnect_callback(const PSI_thread_attrs *thread_attrs) {
  assert(thread_attrs != NULL);
  session_event(Event_info(SESSION_DISCONNECT, thread_attrs));
}

/**
  Test the Resource Group service.
  Log messages are written to the console and log file.
  @return NULL for success
*/
void session_event(const Event_info &event) {
  PSI_thread_attrs attrs = event.m_attrs;

  switch (event.m_type) {
    case SESSION_CONNECT: {
      std::string user_name(attrs.m_username, attrs.m_username_length);

      /* Test API based on user name */
      auto thread_id = attrs.m_thread_internal_id;
      std::string group_name;
      int ret = 0;

      if (user_name == "PFS_DEBUG_MODE") {
        debug_mode = true;
        print_log("DEBUG MODE ON");
      } else if (user_name == "PFS_TEST_INVALID_THREAD_ID") {
        thread_id = 9999;
        group_name = "PFS_INVALID_THREAD_ID";
      } else if (user_name == "PFS_TEST_INVALID_GROUP_NAME") {
        int invalid_size = sizeof(PSI_thread_attrs::m_groupname) + 10;
        group_name = std::string(invalid_size, 'X');
      } else {
        group_name = "PFS_VALID_GROUP_NAME";
      }

      /* Set the resource group name for a thread. */
      ret = mysql_service_pfs_resource_group->set_thread_resource_group_by_id(
          NULL, thread_id, group_name.c_str(), (int)group_name.length(),
          (void *)attrs.m_user_data);

      std::string msg("set_thread_resource_group(");
      if (debug_mode || user_name == "PFS_TEST_INVALID_THREAD_ID")
        msg += std::to_string(thread_id);
      else
        msg += "tid";
      msg += ", " + group_name + ") returned " + std::to_string(ret);
      print_event(event, msg);
      break;
    }

    case SESSION_DISCONNECT: {
      std::string user_name(attrs.m_username, attrs.m_username_length);
      if (user_name == "PFS_DEBUG_MODE") {
        debug_mode = false;
        print_log("DEBUG MODE OFF");
      }
      break;
    }

    default:
      break;
  }
}

/**
  Initialize the test component, open logfile, register callbacks.
  Launch a thread to process session event callbacks.
  @return 0 for success
*/
mysql_service_status_t test_pfs_resource_group_init() {
  int ret = 0;
  log_outfile.open("test_pfs_resource_group.log", std::ofstream::out |
                                                      std::ofstream::trunc |
                                                      std::ofstream::binary);
  print_log("Test Performance Schema Resource Group Service\n");

  PSI_notification callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.session_connect = &session_connect_callback;
  callbacks.session_disconnect = &session_disconnect_callback;

  std::string group_name("PFS_CURRENT_THREAD");
  std::string msg("set_thread_resource_group(");

  handle =
      mysql_service_pfs_notification->register_notification(&callbacks, true);
  if (handle == 0) {
    print_log("register_notification failed");
    goto error;
  }

  /* Set the resource group for the current thread. */
  ret = mysql_service_pfs_resource_group->set_thread_resource_group(
      group_name.c_str(), (int)group_name.length(), nullptr);
  msg += group_name + ") returned " + std::to_string(ret);
  print_log(msg);

  return mysql_service_status_t(0);

error:
  log_outfile.close();
  return mysql_service_status_t(1);
}

/**
  Unregister callbacks, close logfile.
  @return 0 for success
*/
mysql_service_status_t test_pfs_resource_group_deinit() {
  if (mysql_service_pfs_notification->unregister_notification(handle))
    print_log("unregister_notification failed");
  log_outfile.close();
  return mysql_service_status_t(0);
}

/* Empty list--no service provided. */
BEGIN_COMPONENT_PROVIDES(test_pfs_resource_group)
END_COMPONENT_PROVIDES();

/* Required services for this test component. */
BEGIN_COMPONENT_REQUIRES(test_pfs_resource_group)
REQUIRES_SERVICE(pfs_notification), REQUIRES_SERVICE(pfs_resource_group),
    END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_pfs_resource_group)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_pfs_resource_group", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_pfs_resource_group, "mysql:test_pfs_resource_group")
test_pfs_resource_group_init,
    test_pfs_resource_group_deinit END_DECLARE_COMPONENT();

/* Components contained in this library. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_pfs_resource_group)
    END_DECLARE_LIBRARY_COMPONENTS
