/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef REMOTE_CLONE_HANDLER_INCLUDED
#define REMOTE_CLONE_HANDLER_INCLUDED

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/include/sql_service/sql_service_command.h"

/**
  Class that hold the logic to decide if we should or not execute a clone
  operation and the logic to launch it.
*/
class Remote_clone_handler : public Group_event_observer {
 public:
  /** The possible results when checking what is the recovery strategy */
  enum enum_clone_check_result {
    DO_CLONE = 0,              // Do a remote clone
    DO_RECOVERY = 1,           // Don't clone, use distributed recovery
    CHECK_ERROR = 2,           // Error when checking
    NO_RECOVERY_POSSIBLE = 3,  // No available members for clone or recovery
    CLONE_CHECKS_END = 4       // Enum end
  };

  /**
    Constructor for the remote cloning handler

    @param threshold                The threshold for clone activation
    @param components_stop_timeout  The stop timeout in error cases
  */
  Remote_clone_handler(ulonglong threshold, ulong components_stop_timeout);

  /**
    The destructor
  */
  ~Remote_clone_handler() override;

  /**
    Set the class threshold for clone activation
    @param threshold   The threshold for clone activation
  */
  void set_clone_threshold(ulonglong threshold) {
    m_clone_activation_threshold = threshold;
  }

  /*
    Check what are the valid donors present and how many transactions
    this member is missing related to them.this

    @param donor_info  a tuple that has the info. It contains
                       number of valid clone donors
                       number of valid recovery donors
                       number of valid recovering donors
                       whether clone activation threshold was breached or not

    @return whether or not we managed to get the info
      @retval 0    the info was retrieved
      @retval != 0 some error occurred
  */
  int extract_donor_info(std::tuple<uint, uint, uint, bool> *donor_info);

  /**
    Check if clone or distributed recovery shall be used for provisioning
    @return What is the clone strategy to follow or shall we error out
      @retval DO_CLONE              Do a remote clone
      @retval DO_RECOVERY           Don't clone, use distributed recovery
      @retval CHECK_ERROR           Error when choosing the strategy
      @retval NO_RECOVERY_POSSIBLE  No available members for clone or recovery
  */
  enum_clone_check_result check_clone_preconditions();

  /**
    Launch the clone process with some preliminary checks.

    @param group_name  The group name
    @param view_id     The view id when clone started

    @note: the given parameters are used when falling back to
           distributed recovery in case of a clone issue.

    @return whether or not we managed to launch the clone thread.
      @retval 0    the thread launched successfully
      @retval != 0 for some reason we did not launch the thread
  */
  int clone_server(const std::string &group_name, const std::string &view_id);

  /**
    Terminate the clone process

    @param rejoin  Are we terminating or rejoining in the plugin

    @note: the flag tells the method if the clone query should be killed or not
           Usually on rejoins the clone query is not killed.
           When stopping GR, then the query is terminated.
           No guarantees are made about what the server state is after that
  */
  void terminate_clone_process(bool rejoin);

  /**
    Lock when trying to set the read mode and a clone might be running
  */
  void lock_gr_clone_read_mode_lock() {
    mysql_mutex_lock(&m_clone_read_mode_lock);
  }

  /**
    Unlock when trying to set the read mode and a clone might be running
  */
  void unlock_gr_clone_read_mode_lock() {
    mysql_mutex_unlock(&m_clone_read_mode_lock);
  }

 private:
  /** What is the result when we check if the clone plugin is present*/
  enum enum_clone_presence_query_result {
    CLONE_PLUGIN_NOT_PRESENT = 0,  // the plugin is not there or not active
    CLONE_PLUGIN_PRESENT = 1,      // the plugin is there and active
    CLONE_CHECK_QUERY_ERROR = 2,   // error when checking
  };

  /** What are the states of the clone execution query */
  enum enum_clone_query_status {
    CLONE_QUERY_NOT_EXECUTING = 0,  // Not yet executed
    CLONE_QUERY_EXECUTING = 1,      // Executing query
    CLONE_QUERY_EXECUTED = 2,       // Executed already
  };

  // The listeners for group events

  int after_view_change(const std::vector<Gcs_member_identifier> &joining,
                        const std::vector<Gcs_member_identifier> &leaving,
                        const std::vector<Gcs_member_identifier> &group,
                        bool is_leaving, bool *skip_election,
                        enum_primary_election_mode *election_mode,
                        std::string &suggested_primary) override;
  int after_primary_election(
      std::string primary_uuid,
      enum_primary_election_primary_change_status primary_change_status,
      enum_primary_election_mode election_mode, int error) override;
  int before_message_handling(const Plugin_gcs_message &message,
                              const std::string &message_origin,
                              bool *skip_message) override;

  /**
    The thread callback passed onto mysql_thread_create.

    @param[in] arg a pointer to a Remote_clone_handler instance.

    @return Does not return.
  */
  static void *launch_thread(void *arg);

  /**
    The clone thread process.
  */
  [[noreturn]] void clone_thread_handle();

  /**
    Check if clone plugin is present
    @return is the clone present or error
      @retval CLONE_PLUGIN_NOT_PRESENT  The plugin is not present or active
      @retval CLONE_PLUGIN_PRESENT      The plugin is present and active
      @retval CLONE_CHECK_QUERY_ERROR   There was an error when checking
  */
  enum_clone_presence_query_result check_clone_plugin_presence();

  /**
    Get all the valid members for cloning

    @param[out] suitable_donors the list of possible donors
  */
  void get_clone_donors(std::list<Group_member_info *> &suitable_donors);

  /**
    Configure the SSL options for the clone plugin

    @param[in] sql_command_interface the connection to use
  */
  int set_clone_ssl_options(
      Sql_service_command_interface *sql_command_interface);

  /**
    In error fall back to recovery or error out

    @param[in] critical_error         the error prevent distributed recovery
  */
  int fallback_to_recovery_or_leave(bool critical_error = false);

  /**
    Executes the query to change the allowed donor list for clone

    @param[in] sql_command_interface the connection to use
    @param[in] hostname the hostname to set
    @param[in] port     the port to set

    @return whether or not we managed to set the value
      @retval 0    the value was set
      @retval != 0 some error occurred
  */
  int update_donor_list(Sql_service_command_interface *sql_command_interface,
                        std::string &hostname, std::string &port);

  /**
    Checks if the server connection was not killed.
    If so, establish a new one.

    @param[in] sql_command_interface the server connection

    @return did we manage to reconnect
      @retval 0    yes
      @retval != 0 some error occurred
  */
  int evaluate_server_connection(
      Sql_service_command_interface *sql_command_interface);

  /**
    Executes the query to remotely clone a server

    @param[in] sql_command_interface the connection to use
    @param[in] hostname the hostname to use
    @param[in] port     the port to use
    @param[in] username the username to use
    @param[in] password the password to use
    @param[in] use_ssl  make clone use SSL

    @return whether or not we managed to clone the server
      @retval 0    the clone was successful
      @retval != 0 some error occurred
  */
  int run_clone_query(Sql_service_command_interface *sql_command_interface,
                      std::string &hostname, std::string &port,
                      std::string &username, std::string &password,
                      bool use_ssl);

  /**
    Kill the current query executing a clone

    @return whether or not we managed to kill the clone query
      @retval 0    the kill query was successful
      @retval != 0 some error occurred
  */
  int kill_clone_query();

  /**
    Given a error code it evaluates if the error is critical or not.
    Basically it tells us if there is still data in the server.

    @param[in] error_code the clone returned error

    @retval true  error is critical
    @retval false error is not critical
  */
  bool evaluate_error_code(int error_code);

#ifndef NDEBUG
  /**
    Function for debug points
    @note this function can have a parameter for different debug points
  */
  void gr_clone_debug_point();
#endif /* NDEBUG */

  // Settings to fall back to recovery
  /** The group to which the recovering member belongs */
  std::string m_group_name;
  /** The view id when the clone started */
  std::string m_view_id;

  /** the THD handle. */
  THD *m_clone_thd;
  /** the state of the thread. */
  thread_state m_clone_process_thd_state;
  /** the thread handle. */
  my_thread_handle m_thd_handle;
  /** the mutex for controlling access to the thread itself. */
  mysql_mutex_t m_run_lock;
  /** the cond_var used to signal the thread. */
  mysql_cond_t m_run_cond;
  /** the mutex for the clone process query. */
  mysql_mutex_t m_clone_query_lock;
  /** the mutex for the clone external running status/read mode*/
  mysql_mutex_t m_clone_read_mode_lock;

  /** Is the process being terminated*/
  bool m_being_terminated;
  /** What is the status on the read only mode enabling query */
  enum_clone_query_status m_clone_query_status;
  /** The session id for the clone query*/
  unsigned long m_clone_query_session_id;

  /**The threshold after which the clone process is invoked*/
  ulonglong m_clone_activation_threshold;

  /** the mutex for donor list accesses. */
  mysql_mutex_t m_donor_list_lock;

  /** The list of available donors */
  std::list<Group_member_info *> m_suitable_donors;

  /** The current donor address*/
  Gcs_member_identifier *m_current_donor_address;

  /** Timeout on shutdown */
  ulong m_stop_wait_timeout;
};

#endif /* REMOTE_CLONE_HANDLER_INCLUDED */
