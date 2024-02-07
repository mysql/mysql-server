/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef GMS_LISTENER_TEST_H
#define GMS_LISTENER_TEST_H

#include <mysql/components/service_implementation.h>
#include "plugin/group_replication/include/thread/mysql_thread.h"

#define GMS_LISTENER_EXAMPLE_NAME "group_membership_listener.gr_example"
#define GMST_LISTENER_EXAMPLE_NAME "group_member_status_listener.gr_example"

void register_listener_service_gr_example();
void unregister_listener_service_gr_example();

/**
  An example implementation of the group_membership_listener
  service. It is actually used for testing purposes.
*/
class group_membership_listener_example_impl {
 public:
  /**
  @c notify_view_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_view_change, (const char *));

  /**
  @c notify_quorum_lost(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_quorum_lost, (const char *));
};

/**
  An example implementation of the group_member_status_listener
  service. It is actually used for testing purposes.
*/
class group_member_status_listener_example_impl {
 public:
  /**
  @c notify_member_role_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_member_role_change, (const char *));

  /**
  @c notify_member_state_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_member_state_change, (const char *));
};

class Gms_listener_test_parameters : public Mysql_thread_body_parameters {
 public:
  /**
    Gms_listener_test_parameters constructor.

    @param [in]  message  Message to add to the table
    */
  Gms_listener_test_parameters(const std::string &message)
      : m_message(message), m_error(1){};
  virtual ~Gms_listener_test_parameters(){};

  /**
    Get value for class private member error.

    @return the error value returned
    @retval 0      OK
    @retval !=0    Error
    */
  int get_error();

  /**
    Set value for class private member error.

    @param [in] error Set value of error
    */
  void set_error(int error);

  /**
    Get message to add to the table.

    @return the message
    */
  const std::string &get_message();

 private:
  const std::string m_message{""};
  int m_error{1};
};

class Gms_listener_test : Mysql_thread_body {
 public:
  Gms_listener_test() = default;

  virtual ~Gms_listener_test() override = default;

  /**
    Method that will be run on mysql_thread.

    @param [in, out] parameters Values used by method to get service variable.

    */
  void run(Mysql_thread_body_parameters *parameters) override;

  /**
    Log the notification message to the test table.
    This method will run on the MySQL session of the global
    `mysql_thread_handler`, which is already prepared to be
    directly use by the `Sql_service_interface`.

    @param [in]  message  Message to add to the table

    @return the error value returned
    @retval false   OK
    @retval true    Error
    */
  bool log_notification_to_test_table(const std::string &message);
};

#endif /* GMS_LISTENER_TEST_H */
