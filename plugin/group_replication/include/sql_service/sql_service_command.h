/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_SERVICE_COMMAND_INCLUDE
#define SQL_SERVICE_COMMAND_INCLUDE

#include <stddef.h>

#include "my_inttypes.h"
#include "plugin/group_replication/include/plugin_utils.h"
#include "plugin/group_replication/include/sql_service/sql_service_interface.h"

#define GR_PLUGIN_SESSION_THREAD_TIMEOUT 10

/**
  What is the policy when creation a new server session for SQL execution.
*/
enum enum_plugin_con_isolation {
  PSESSION_USE_THREAD,       ///< Use the current thread
  PSESSION_INIT_THREAD,      ///< Use the current thread but initialize it
  PSESSION_DEDICATED_THREAD  ///< Use a dedicated thread to open a session
};

class Sql_service_commands {
 public:
  /**
   Method to kill the session identified by the given session id in those
   cases where the server hangs while executing the sql query.

   @param sql_interface  the server session interface for query execution
   @param session_id  id of the session to be killed.

   @return the error value returned
    @retval 0  - success
    @retval >0 - Failure
  */
  long internal_kill_session(Sql_service_interface *sql_interface,
                             void *session_id = nullptr);

  /**
    Method to remotely clone a server

    @param[in] sql_interface  The connection where to execute the query
    @param[in] variable_args  Tuple <string,string,string,string,bool,string>

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long internal_clone_server(Sql_service_interface *sql_interface,
                             void *variable_args = nullptr);

  /**
    Method to execute a given query

    @param[in] sql_interface  The connection where to execute the query
    @param[in] variable_args  Tuple <string, string>

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long internal_execute_query(Sql_service_interface *sql_interface,
                              void *variable_args = nullptr);

  /**
    Method to execute a given conditional query

    @param[in] sql_interface  The connection where to execute the query
    @param[in] variable_args  Tuple <string, bool, string>

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long internal_execute_conditional_query(Sql_service_interface *sql_interface,
                                          void *variable_args = nullptr);
};

struct st_session_method {
  long (Sql_service_commands::*method)(Sql_service_interface *, void *);
  bool terminated;
};

class Session_plugin_thread {
 public:
  Session_plugin_thread(Sql_service_commands *command_interface);

  ~Session_plugin_thread();

  /**
    Launch a new thread that will create a new server session.

    @param plugin_pointer_var the plugin pointer for session creation
    @param user               the user for the connection

    @return the operation was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int launch_session_thread(void *plugin_pointer_var, const char *user);

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
  void queue_new_method_for_application(
      long (Sql_service_commands::*method)(Sql_service_interface *, void *),
      bool terminate = false);

  /**
    Wait for the queued method to return.
    @return the return value of the submitted method
  */
  long wait_for_method_execution();

  Sql_service_interface *get_service_interface();

  /**
    Sets a pointer that the next queued method will use to return a value
    @param pointer the pointer where the method will store some return value
  */
  void set_return_pointer(void *pointer) { return_object = pointer; }

 private:
  Sql_service_commands *command_interface;

  Sql_service_interface *m_server_interface;

  Synchronized_queue<st_session_method *> *incoming_methods;

  void *m_plugin_pointer;

  /** The value for returning on methods */
  void *return_object;

  /** Session thread handle */
  my_thread_handle m_plugin_session_pthd;
  /* run conditions and locks */
  mysql_mutex_t m_run_lock;
  mysql_cond_t m_run_cond;
  /* method completion conditions and locks */
  mysql_mutex_t m_method_lock;
  mysql_cond_t m_method_cond;

  /**The user for the session connection*/
  const char *session_user;
  /** Session thread method completion flag */
  bool m_method_execution_completed;
  /** The method return value */
  long m_method_execution_return_value;
  /** Session thread state */
  thread_state m_session_thread_state;
  /** Session termination flag */
  bool m_session_thread_terminate;
  /** Session thread error flag */
  int m_session_thread_error;
};

class Sql_service_command_interface {
 public:
  Sql_service_command_interface();
  ~Sql_service_command_interface();

  /**
    Establishes the connection to the server.

    @param isolation_param  session creation requirements: use current thread,
                            use thread but initialize it or create it in a
                            dedicated thread
    @param user             the user for the connection
    @param plugin_pointer   the plugin pointer for threaded connections

    @return the connection was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int establish_session_connection(enum_plugin_con_isolation isolation_param,
                                   const char *user,
                                   void *plugin_pointer = nullptr);

  /**
    Terminates the old connection and creates a new one to the server.

    @param isolation_param  session creation requirements: use current thread,
                            use thread but initialize it or create it in a
                            dedicated thread
    @param user             the user for the connection
    @param plugin_pointer   the plugin pointer for threaded connections

    @return the connection was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int reestablish_connection(enum_plugin_con_isolation isolation_param,
                             const char *user, void *plugin_pointer = nullptr);
  /**
    Was this session killed?

    @retval true   session was killed
    @retval false  session was not killed
  */
  bool is_session_killed();

  /**
    Stops and deletes all connection related structures
  */
  void terminate_connection_fields();

  /**
    Returns the SQL service interface associated to this class

    @return the sql service interface field
  */
  Sql_service_interface *get_sql_service_interface();

  /**
    Sets the SQL API user to be used on security checks

    @param user the user to be used

    @return the operation was successful
      @retval 0      OK
      @retval !=0    Error
  */
  int set_interface_user(const char *user);

  /**
   Method to kill the session identified by the given session id in those
   cases where the server hangs while executing the sql query.

   @param session_id  id of the session to be killed.

   @return the error value returned
      @retval 0  - success
      @retval >0 - Failure
  */
  long kill_session(unsigned long session_id);

  /**
    Checks if there is an existing session

    @return the error value returned
      @retval true  valid
      @retval false some issue prob happened on connection
  */
  bool is_session_valid();

  /**
    Method to remotely clone a server

    @param [in] host      The host to clone
    @param [in] port      The host port
    @param [in] username  The username to authenticate in the remote server
    @param [in] password  The password to authenticate in the remote server
    @param [in] use_ssl   Is ssl configured for the clone process
    @param [out] error    The error message in case of error

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long clone_server(std::string &host, std::string &port, std::string &username,
                    std::string &password, bool use_ssl, std::string &error);

  /**
    Execute a query passed as parameter.

    @param [in] query      The query to execute

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long execute_query(std::string &query);

  /**
    Execute a query passed as parameter.

    @param [in] query      The query to execute
    @param [out] error     The error message in case of error

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long execute_query(std::string &query, std::string &error);

  /**
    Execute a conditional query passed as parameter.

    @param [in] query      The query to execute
    @param [in] result     The result of the query

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long execute_conditional_query(std::string &query, bool *result);

  /**
    Execute a conditional query passed as parameter.

    @param [in] query      The query to execute
    @param [in] result     The result of the query
    @param [out] error     The error message in case of error

    @return the error value returned
      @retval 0      OK
      @retval !=0    Error on execution
  */
  long execute_conditional_query(std::string &query, bool *result,
                                 std::string &error);

 private:
  enum_plugin_con_isolation connection_thread_isolation;

  Sql_service_commands sql_service_commands;

  /** The internal SQL session service interface to the server */
  Sql_service_interface *m_server_interface;

  /* The thread where the connection leaves if isolation is needed*/
  Session_plugin_thread *m_plugin_session_thread;
};

#endif  // SQL_SERVICE_COMMAND_INCLUDE
