/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

/**
  @file test_pfs_notification.cc

  Test component for the Performance Schema Notification service.
  Upon installation, this component registers 5 callback functions for each
  notification event:

    thread_create
    thread_destroy
    session_connect
    session_disconnect
    session_change_user

  These events are triggered externally, e.g. from an MTR script.
  Each callback functions logs a message to a predefined logfile and to stderr.

  @sa test_pfs_notification()
*/

REQUIRES_SERVICE_PLACEHOLDER(pfs_notification_v3);
REQUIRES_SERVICE_PLACEHOLDER(pfs_resource_group_v3);

/*
  User-defined data structure.
  Data values are simulated.
*/
struct User_data {
  User_data() : m_handle(0), m_priority(0), m_vcpu(0) {}
  User_data(int handle) : m_handle(handle), m_priority(0), m_vcpu(0) {}
  User_data(int handle, int priority, int vcpu)
      : m_handle(handle), m_priority(priority), m_vcpu(vcpu) {}
  int m_handle;
  int m_priority;
  int m_vcpu;
};

static User_data g_user_data;

/* Register callbacks */
class Registration {
 public:
  Registration(PSI_notification &cb) : m_cb(cb), m_handle(0) {}
  Registration(PSI_notification &cb, int handle) : m_cb(cb), m_handle(handle) {}
  PSI_notification m_cb;
  int m_handle;
};

std::vector<Registration> registrations;
static const int registration_count = 3;
static bool log_enabled = false;
static std::ofstream log_outfile;
static const std::string separator("===========================");

/* Handle for internal registration special use case */
static int internal_handle = 0;
/* Unique callback sequence identifier for special use case */
static const int internal_seq = 4;
/* True if internal registration succeeded. */
static bool internal_registration = false;
/* True if negative tests executed. */
static bool negative_tests = false;
/* Callback for special use case */
void session_connect_internal(const PSI_thread_attrs *thread_attrs);

void print_log(std::string msg);

/**
  Log file operations
*/
void open_log() {
  log_enabled = true;
  if (!log_outfile.is_open())
    log_outfile.open("test_pfs_notification.log", std::ofstream::out |
                                                      std::ofstream::trunc |
                                                      std::ofstream::binary);
  print_log("logfile opened");
}

void close_log() {
  print_log("logfile closed");
  log_enabled = false;
  if (log_outfile.is_open()) log_outfile.close();
}

void print_log(std::string msg) {
  if (!log_enabled) return;

  /* Write to both log file and stderr. */
  log_outfile << msg << std::endl;
  fprintf(stderr, "%s\n", msg.c_str());
  fflush(stderr);
}

void callback_print_log(uint handle, const char *callback,
                        const PSI_thread_attrs *attrs, int ret_code) {
  const PSI_thread_attrs *thread_attrs = attrs;
  PSI_thread_attrs my_thread_attrs;

  if (!log_enabled) return;

  std::string group, user, host;
  std::stringstream ss;

  if (!thread_attrs) {
    std::memset(&my_thread_attrs, 0, sizeof(my_thread_attrs));
    attrs = &my_thread_attrs;
  }

  if (thread_attrs->m_groupname_length > 0)
    group = std::string(thread_attrs->m_groupname,
                        thread_attrs->m_groupname_length);
  if (thread_attrs->m_username_length > 0)
    user =
        std::string(thread_attrs->m_username, thread_attrs->m_username_length);
  if (thread_attrs->m_hostname_length > 0)
    host =
        std::string(thread_attrs->m_hostname, thread_attrs->m_hostname_length);

  User_data user_data;
  if (thread_attrs->m_user_data)
    user_data = *static_cast<User_data *>(thread_attrs->m_user_data);

  ss << "***"
     << " callback= " << callback << " handle= " << handle
     << " ret_code= " << ret_code
     << " thread_id= " << thread_attrs->m_thread_internal_id
     << " plist_id= " << thread_attrs->m_processlist_id
     << " os_thread= " << thread_attrs->m_thread_os_id << " group= " << group
     << " user= " << user << " host= " << host << " vcpu= " << user_data.m_vcpu
     << " priority= " << user_data.m_priority;

  print_log(ss.str());
}

/**
  Callback for thread create.
*/
void thread_create_callback(int handle, const PSI_thread_attrs *thread_attrs) {
  callback_print_log(handle, "thread_create", thread_attrs, 0);
}

void thread_create_cb1(const PSI_thread_attrs *thread_attrs) {
  int handle = 1;
  thread_create_callback(handle, thread_attrs);
}

void thread_create_cb2(const PSI_thread_attrs *thread_attrs) {
  int handle = 2;
  thread_create_callback(handle, thread_attrs);
}

void thread_create_cb3(const PSI_thread_attrs *thread_attrs) {
  int handle = 3;
  thread_create_callback(handle, thread_attrs);
}

/**
  Callback for thread destroy.
*/
void thread_destroy_callback(int handle, const PSI_thread_attrs *thread_attrs) {
  callback_print_log(handle, "thread_destroy", thread_attrs, 0);
}

void thread_destroy_cb1(const PSI_thread_attrs *thread_attrs) {
  int handle = 1;
  thread_destroy_callback(handle, thread_attrs);
}

void thread_destroy_cb2(const PSI_thread_attrs *thread_attrs) {
  int handle = 2;
  thread_destroy_callback(handle, thread_attrs);
}

void thread_destroy_cb3(const PSI_thread_attrs *thread_attrs) {
  int handle = 3;
  thread_destroy_callback(handle, thread_attrs);
}

/* Check for approved username in MTR mode. */
bool check_user(std::string &user) {
  return (user == "PFS_MTR_MODE_ENABLE" || user == "PFS_MTR_MODE_DISABLE" ||
          user == "PFS_MTR_REGISTER_INTERNAL" ||
          user == "PFS_MTR_UNREGISTER_INTERNAL" ||
          user == "PFS_MTR_NEGAIVE_TEST_CASES" || user == "PFS_USER1" ||
          user == "PFS_USER2" || user == "PFS_USER3");
}

/**
  Callback for session connection.
*/
void session_connect_callback(int handle,
                              const PSI_thread_attrs *thread_attrs) {
  assert(thread_attrs != nullptr);

  /*
    There two primary test modes: MTR and RQG. Logging is only enabled in
    MTR mode, and must be disabled for high-concurrency performance and RQG
    testing. As a workaround, the test mode is controlled with pre-defined
    usernames until support for component system variables is available.
  */
  std::string user(thread_attrs->m_username, thread_attrs->m_username_length);

  if (user == "PFS_MTR_MODE_ENABLE" && !log_enabled) {
    open_log();
    return;
  }

  if (user == "PFS_MTR_MODE_DISABLE" && log_enabled) {
    close_log();
    return;
  }

  if (log_enabled) {
    /* Verify that this is an approved user name. */
    if (!check_user(user)) return;

    /*
      Test internal callback registration. Setting with_ref_count = false
      provides better performance but callbacks cannot be unregistered.
    */
    if (user == "PFS_MTR_REGISTER_INTERNAL") {
      if (!internal_registration) {
        PSI_notification callbacks;
        std::memset(&callbacks, 0, sizeof(callbacks));
        callbacks.session_connect = &session_connect_internal;
        internal_handle =
            mysql_service_pfs_notification_v3->register_notification(&callbacks,
                                                                     false);
        callback_print_log(handle, "register_notification_internal",
                           thread_attrs, internal_handle);
        internal_registration = true;
      }
      return;
    }

    /*
      Test unregistering of an 'internal' registration that does not use a ref
      count. Callbacks are disabled but not completely unregistered.
    */
    if (user == "PFS_MTR_UNREGISTER_INTERNAL") {
      if (internal_registration) {
        int ret = mysql_service_pfs_notification_v3->unregister_notification(
            internal_handle);
        callback_print_log(handle, "unregister_notification_internal",
                           thread_attrs, ret);
        internal_registration = false;
      }
      return;
    }

    /* Verify that the internal registration succeeded. */
    if (handle == internal_seq) {
      callback_print_log(handle, "session_connect(internal)", thread_attrs, 0);
      return;
    }

    /* Negative test cases. */
    if (user == "PFS_MTR_NEGATIVE_TEST_CASES") {
      if (negative_tests) return;

      PSI_notification callbacks;

      /* Register w/ bad callbacks. */
      std::memset(&callbacks, 0, sizeof(callbacks));
      int ret = mysql_service_pfs_notification_v3->register_notification(
          &callbacks, true);
      callback_print_log(handle, "register_notification(bad_cb)", thread_attrs,
                         ret);

      /* Register w/ null callbacks. */
      ret = mysql_service_pfs_notification_v3->register_notification(nullptr,
                                                                     true);
      callback_print_log(handle, "register_notification(nullptr)", thread_attrs,
                         ret);

      /* Unregister w/ invalid handle. */
      ret = mysql_service_pfs_notification_v3->unregister_notification(handle);
      callback_print_log(handle, "unregister_notification(bad_handle)",
                         thread_attrs, ret);
      negative_tests = true;
      return;
    }

    /*
      The Performance Schema reads the thread attributes only once per event,
      so thread_attrs won't reflect changes from previously invoked callbacks.
      Get the most recent set of thread attrs and append the handle to the
      resource group name such that it will eventually indicate each callback
      invoked for this event, e.g. "RESOURCE_GROUP_3_2_1"
    */
    PSI_thread_attrs my_thread_attrs;

    if (mysql_service_pfs_resource_group_v3->get_thread_system_attrs_by_id(
            nullptr, thread_attrs->m_thread_internal_id, &my_thread_attrs)) {
      print_log("get_thread_resource_group_by_id failed");
    }

    std::string group(my_thread_attrs.m_groupname,
                      my_thread_attrs.m_groupname_length);
    if (group.empty()) group = "RESOURCE_GROUP";
    group += "_" + std::to_string(handle);

    User_data *user_data = (User_data *)thread_attrs->m_user_data;
    if (user_data == nullptr) {
      g_user_data.m_handle = handle;
      g_user_data.m_priority = handle * 10;
      g_user_data.m_vcpu = handle * 2;
      user_data = &g_user_data;
    }

    /* Update resource group */
    if (mysql_service_pfs_resource_group_v3->set_thread_resource_group_by_id(
            nullptr, thread_attrs->m_thread_internal_id, group.c_str(),
            (int)group.length(), (void *)user_data)) {
      print_log("set_thread_resource_group_by_id failed");
    }

    /* Get thread attributes again to verify changes */
    if (mysql_service_pfs_resource_group_v3->get_thread_system_attrs_by_id(
            nullptr, thread_attrs->m_thread_internal_id, &my_thread_attrs)) {
      print_log("get_thread_resource_group_by_id failed");
    }

    callback_print_log(handle, "session_connect", &my_thread_attrs, 0);
  } else {
    /* Set resource group name. Do this once per connection. */
    if (handle == 1) {
      std::string group = "RESOURCE_GROUP_" + std::to_string(handle);
      if (mysql_service_pfs_resource_group_v3->set_thread_resource_group_by_id(
              nullptr, thread_attrs->m_thread_internal_id, group.c_str(),
              (int)group.length(), nullptr)) {
        print_log("set_thread_resource_group_by_id failed");
      }
    }
  }
}

void session_connect_cb1(const PSI_thread_attrs *thread_attrs) {
  int handle = 1;
  session_connect_callback(handle, thread_attrs);
}

void session_connect_cb2(const PSI_thread_attrs *thread_attrs) {
  int handle = 2;
  session_connect_callback(handle, thread_attrs);
}

void session_connect_cb3(const PSI_thread_attrs *thread_attrs) {
  int handle = 3;
  session_connect_callback(handle, thread_attrs);
}

void session_connect_internal(const PSI_thread_attrs *thread_attrs) {
  int handle = internal_seq;
  session_connect_callback(handle, thread_attrs);
}

/**
  Callback for session disconnect.
*/
void session_disconnect_callback(int handle,
                                 const PSI_thread_attrs *thread_attrs) {
  callback_print_log(handle, "session_disconnect", thread_attrs, 0);
}

void session_disconnect_cb1(const PSI_thread_attrs *thread_attrs) {
  int handle = 1;
  session_disconnect_callback(handle, thread_attrs);
}

void session_disconnect_cb2(const PSI_thread_attrs *thread_attrs) {
  int handle = 2;
  session_disconnect_callback(handle, thread_attrs);
}

void session_disconnect_cb3(const PSI_thread_attrs *thread_attrs) {
  int handle = 3;
  session_disconnect_callback(handle, thread_attrs);
}

/**
  Callback for session change user.
*/
void session_change_user_callback(int handle,
                                  const PSI_thread_attrs *thread_attrs) {
  callback_print_log(handle, "session_change_user", thread_attrs, 0);
}

void session_change_user_cb1(const PSI_thread_attrs *thread_attrs) {
  int handle = 1;
  session_change_user_callback(handle, thread_attrs);
}

void session_change_user_cb2(const PSI_thread_attrs *thread_attrs) {
  int handle = 2;
  session_change_user_callback(handle, thread_attrs);
}

void session_change_user_cb3(const PSI_thread_attrs *thread_attrs) {
  int handle = 3;
  session_change_user_callback(handle, thread_attrs);
}

/**
  Test the Notification service.
  Log messages are written to the console and log file.
  @return false for success
*/
bool test_pfs_notification() {
  bool result = false;
  std::stringstream ss;
  PSI_notification callbacks;

  for (auto r = 1; r <= registration_count; r++) {
    switch (r) {
      case 1:
        callbacks.thread_create = &thread_create_cb1;
        callbacks.thread_destroy = &thread_destroy_cb1;
        callbacks.session_connect = &session_connect_cb1;
        callbacks.session_disconnect = &session_disconnect_cb1;
        callbacks.session_change_user = &session_change_user_cb1;
        break;

      case 2:
        callbacks.thread_create = &thread_create_cb2;
        callbacks.thread_destroy = &thread_destroy_cb2;
        callbacks.session_connect = &session_connect_cb2;
        callbacks.session_disconnect = &session_disconnect_cb2;
        callbacks.session_change_user = &session_change_user_cb2;
        break;

      case 3:
        callbacks.thread_create = &thread_create_cb3;
        callbacks.thread_destroy = &thread_destroy_cb3;
        callbacks.session_connect = &session_connect_cb3;
        callbacks.session_disconnect = &session_disconnect_cb3;
        callbacks.session_change_user = &session_change_user_cb3;
        break;

      default:
        break;
    }

    auto handle = mysql_service_pfs_notification_v3->register_notification(
        &callbacks, true);

    if (handle == 0) {
      print_log("register_notification() failed");
    } else {
      registrations.push_back(Registration(callbacks, handle));
      ss << "register_notification " << handle;
      print_log(ss.str());
    }
  }

  return result;
}

/**
  Initialize the test component, open logfile, register callbacks.
  @return 0 for success
*/
mysql_service_status_t test_pfs_notification_init() {
  print_log("Test Performance Schema Notification Service\n");
  return mysql_service_status_t(test_pfs_notification());
}

/**
  Unregister callbacks, close logfile.
  @return 0 for success
*/
mysql_service_status_t test_pfs_notification_deinit() {
  print_log(separator);

  for (auto reg : registrations) {
    if (mysql_service_pfs_notification_v3->unregister_notification(
            reg.m_handle))
      print_log("unregister_notification failed");
    else {
      std::stringstream ss;
      ss << "unregister_notification " << reg.m_handle;
      print_log(ss.str());
    }
  }

  close_log();

  return mysql_service_status_t(0);
}

/* Empty list--no service provided. */
BEGIN_COMPONENT_PROVIDES(test_pfs_notification)
END_COMPONENT_PROVIDES();

/* Required services for this test component. */
BEGIN_COMPONENT_REQUIRES(test_pfs_notification)
REQUIRES_SERVICE(pfs_notification_v3), REQUIRES_SERVICE(pfs_resource_group_v3),
    END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_pfs_notification)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_pfs_notification", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_pfs_notification, "mysql:test_pfs_notification")
test_pfs_notification_init,
    test_pfs_notification_deinit END_DECLARE_COMPONENT();

/* Components contained in this library. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_pfs_notification)
    END_DECLARE_LIBRARY_COMPONENTS
