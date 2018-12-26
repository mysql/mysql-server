/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef SQL_SERVICE_COMMAND_INCLUDE
#define SQL_SERVICE_COMMAND_INCLUDE

#include "sql_service_interface.h"
#include "plugin_utils.h"

#define GR_PLUGIN_SESSION_THREAD_TIMEOUT 10

/**
  What is the policy when creation a new server session for SQL execution.
*/
enum enum_plugin_con_isolation
{
  PSESSION_USE_THREAD,      ///< Use the current thread
  PSESSION_INIT_THREAD,     ///< Use the current thread but initialize it
  PSESSION_DEDICATED_THREAD ///< Use a dedicated thread to open a session
};

class Sql_service_commands
{
public:
  /**
    Internal method to set the super read only mode.

    @param sql_interface the server session interface for query execution

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long internal_set_super_read_only(Sql_service_interface *sql_interface);

  /**
    Internal method to set the read only mode.

    @param sql_interface the server session interface for query execution

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long internal_set_read_only(Sql_service_interface *sql_interface);

  /**
    Internal method to reset the super read only mode.

    @param sql_interface the server session interface for query execution

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long internal_reset_super_read_only(Sql_service_interface *sql_interface);

  /**
    Internal method to reset the super read only mode.

    @param sql_interface the server session interface for query execution

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long internal_reset_read_only(Sql_service_interface *sql_interface);

  /**
   Internal method to get the super read only mode.

   @param sql_interface the server session interface for query execution

   @retval -1  Error reading the value
   @retval  0  Not in super read mode
   @retval  1  In read super mode
  */
  long internal_get_server_super_read_only(Sql_service_interface *sql_interface);

  /**
    Internal method to get the super read only mode.

    @param sql_interface the server session interface for query execution

    @retval -1  Error reading the value
    @retval  0  Not in super read mode
    @retval  1  In read super mode
  */
  long internal_get_server_read_only(Sql_service_interface *sql_interface);

  /**
    Method to return the server gtid_executed by executing the corresponding
    sql query.

    @param sql_interface        the server session interface for query execution
    @param [out] gtid_executed  The string where the result will be appended

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error
  */
  int internal_get_server_gtid_executed(Sql_service_interface *sql_interface,
                                        std::string& gtid_executed);

  /**
    Method to wait for the server gtid_executed to match the given GTID string

    @param sql_interface the server session interface for query execution
    @param [in] gtid_executed  The GTID string to check
    @param [in] timeout        The timeout after which the method should break

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error when executed or timeout.
  */
  long internal_wait_for_server_gtid_executed(Sql_service_interface *sql_interface,
                                              std::string& gtid_executed,
                                              int timeout= 0);

};

struct st_session_method
{
  long (Sql_service_commands::*method)(Sql_service_interface*);
  bool terminated;
};

class Session_plugin_thread
{
public:

  Session_plugin_thread(Sql_service_commands* command_interface);

  ~Session_plugin_thread();

  /**
    Launch a new thread that will create a new server session.

    @param plugin_pointer_var the plugin pointer for session creation

    @return the operation was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int launch_session_thread(void *plugin_pointer_var);

  /**
    Terminate the thread and close the session.

    @return the operation was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int terminate_session_thread();

  /**
    Thread handler for session creation.
  */
  int session_thread_handler();

  /**
     Method to submit a new method into execution on the session thread
     @param method    method to executed
     @param terminate termination flag to the class
  */
  void queue_new_method_for_application(long (Sql_service_commands::*method)(Sql_service_interface*),
                                        bool terminate=false);

  /**
    Wait for the queued method to return.
    @return the return value of the submitted method
  */
  long wait_for_method_execution();

  Sql_service_interface *get_service_interface();

private:
  Sql_service_commands *command_interface;

  Sql_service_interface *m_server_interface;

  Synchronized_queue<st_session_method*> *incoming_methods;

  void *m_plugin_pointer;

  /** Session thread handle */
  my_thread_handle m_plugin_session_pthd;
  /* run conditions and locks */
  mysql_mutex_t m_run_lock;
  mysql_cond_t m_run_cond;
  /* method completion conditions and locks */
  mysql_mutex_t m_method_lock;
  mysql_cond_t m_method_cond;

  /** Session thread method completion flag */
  bool m_method_execution_completed;
  /** The method return value */
  long m_method_execution_return_value;
  /** Session thread running flag */
  bool m_session_thread_running;
  /** Session thread starting flag */
  bool m_session_thread_starting;
  /** Session termination flag */
  bool m_session_thread_terminate;
  /** Session tread error flag */
  int m_session_thread_error;
};

class Sql_service_command_interface
{
public:
  Sql_service_command_interface();
  ~Sql_service_command_interface();

  /**
    Establishes the connection to the server.

    @param isolation_param  session creation requirements: use current thread,
                            use thread but initialize it or create it in a
                            dedicated thread
    @param plugin_pointer   the plugin pointer for threaded connections

    @return the connection was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int establish_session_connection(enum_plugin_con_isolation isolation_param,
                                   void *plugin_pointer= NULL);

  /**
    Returns the SQL service interface associated to this class

    @return the sql service interface field
  */
  Sql_service_interface* get_sql_service_interface();

  /**
    Sets the SQL API user to be used on security checks

    @param user the user to be used

    @return the operation was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int set_interface_user(const char* user);

  /**
   Method to kill the session identified by the given sesion id in those
   cases where the server hangs while executing the sql query.

   @param session_id  id of the session to be killed.
   @param session  the session to be killed

   @return the error value returned
      @retval 0  - success
      @retval >0 - Failure
 */
  long kill_session(uint32_t session_id, MYSQL_SESSION session);

  /**
    Method to set the super_read_only variable "ON".

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long set_super_read_only();

  /**
    Method to set the read_only variable "ON" on the server.

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long set_read_only();

  /**
    Method to reset the super_read_only mode back to "OFF" on the
    server.

    @return error code during execution of the sql query.
       @retval 0  - success
       @retval >0 - failure
  */
  long reset_super_read_only();

  /**
    Method to reset the read_only mode back to "OFF" on the
    server.

    @return error code during execution of the sql query.
      @retval 0  -  success
      @retval >0 - failure
  */
  long reset_read_only();

  /**
    Method to return the server gtid_executed by executing the corresponding
    sql query.

    @param [out] gtid_executed The string where the result will be appended

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error
  */
  int get_server_gtid_executed(std::string& gtid_executed);

  /**
    Method to wait for the server gtid_executed to match the given GTID string

    @param [in] gtid_executed  The GTID string to check
    @param [in] timeout        The timeout after which the method should break

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error when executed or timeout.
  */
  long wait_for_server_gtid_executed(std::string& gtid_executed, int timeout= 0);

  /**
    Method to get the value of the super_read_only variable on the server.

    @retval -1  Error reading the value
    @retval  0  Not in super read mode
    @retval  1  In read super mode
  */
  long get_server_super_read_only();

  /**
    Method to get the value of the read_only variable on the server.

    @retval -1  Error reading the value
    @retval  0  Not in super read mode
    @retval  1  In read super mode
  */
  long get_server_read_only();
private:

  enum_plugin_con_isolation connection_thread_isolation;

  Sql_service_commands sql_service_commands;

  /** The internal SQL session service interface to the server */
  Sql_service_interface *m_server_interface;

  /* The thread where the connection leaves if isolation is needed*/
  Session_plugin_thread* m_plugin_session_thread;
};

#endif //SQL_SERVICE_COMMAND_INCLUDE
