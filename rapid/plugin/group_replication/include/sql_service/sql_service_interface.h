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

#ifndef SQL_SERVICE_INTERFACE_INCLUDE
#define SQL_SERVICE_INTERFACE_INCLUDE

#include "sql_service_context.h"

#define MAX_NUMBER_RETRIES 100
#define SESSION_WAIT_TIMEOUT 2

class Sql_service_interface
{
private:

  /** Pointer to Srv_session class */
  MYSQL_SESSION m_session;

  /** Pointer to the group_replication plugin structure */
  void *m_plugin;

  /** send result in string or native types */
  enum cs_text_or_binary m_txt_or_bin;

  /* The charset for the input string data */
  const CHARSET_INFO * m_charset;

  /**
    Executes a server command in a session.

    @param rset          resulted obtained after executing query
    @param cs_txt_bin    send result in string or native types
                         i.e. to CS_TEXT_REPRESENTATION or
                         CS_BINARY_REPRESENTATION
    @param cs_charset    charset for the string data input
    @param cmd           command service data input structure containing
                         command details (query/database/session id..).
                         Check include/mysql/com_data.h for more details.
    @param cmd_type      command type default set is COM_QUERY

    @return the sql error number
      @retval  0    OK
      @retval >0    SQL Error Number returned from MySQL Service API
      @retval -1    Internal server session failed or was killed
      @retval -2    Internal API failure
  */
  long
  execute_internal(Sql_resultset *rset,
                   enum cs_text_or_binary cs_txt_bin,
                   const CHARSET_INFO *cs_charset,
                   COM_DATA cmd,
                   enum enum_server_command cmd_type);

  /**
    Wait for server to be in SERVER_OPERATING state

    @param total_timeout    number of seconds to wait for
                            session server

    @return session server availability
      @retval   0   session server is in SERVER_OPERATING state
      @retval   1   timeout
      @retval  -1   session server shutdown in progress
  */
  int wait_for_session_server(ulong total_timeout);

public:

  /**
    Sql_service_interface constructor - Non-threaded version

    Initializes sql_service_context class

    @param cs_txt_bin    send result in string or native types
                         i.e. to CS_TEXT_REPRESENTATION or
                         CS_BINARY_REPRESENTATION
    @param cs_charset    charset for the string data input
  */
  Sql_service_interface(
                    enum cs_text_or_binary cs_txt_bin= CS_TEXT_REPRESENTATION,
                    const CHARSET_INFO *cs_charset= &my_charset_utf8_general_ci);


  /**
    Sql_service_interface destructor
  */
  ~Sql_service_interface();

  /**
    Opens an server session for internal server connection.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int open_session();

  /**
    Opens an threaded server session for internal server connection.

    @param plugin_ptr a plugin pointer passed the connection thread.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int open_thread_session(void *plugin_ptr);

  /**
    Executes a server command in a session.

    @note the command type here is COM_QUERY

    @param query_string  query to be executed

    @return the sql error number
      @retval  0    OK
      @retval >0    SQL Error Number returned from MySQL Service API
      @retval <0    local errors
  */
  long execute_query(std::string query_string);


  /**
    Executes a server command in a session.

    @param sql_string    query to be executed
    @param rset          resulted obtained after executing query
    @param cs_txt_bin    send result in string or native types
                         i.e. to CS_TEXT_REPRESENTATION or
                         CS_BINARY_REPRESENTATION
    @param cs_charset    charset for the string data input

    @note the command type here is COM_QUERY

    @return the sql error number
      @retval  0    OK
      @retval >0    SQL Error Number returned from MySQL Service API
      @retval <0    local errors
  */
  long execute_query(std::string sql_string,
                     Sql_resultset *rset,
                     enum cs_text_or_binary cs_txt_bin= CS_TEXT_REPRESENTATION,
                     const CHARSET_INFO *cs_charset= &my_charset_utf8_general_ci);

  /**
    Executes a server command in a session.

    @param cmd           command service data input structure containing
                         command details (query/database/session id..).
                         Check include/mysql/com_data.h for more details.
    @param cmd_type      command type default set is COM_QUERY
    @param rset          resulted obtained after executing query
    @param cs_txt_bin    send result in string or native types
                         i.e. to CS_TEXT_REPRESENTATION or
                         CS_BINARY_REPRESENTATION
    @param cs_charset    charset for the string data input

    @return the sql error number
      @retval  0    OK
      @retval >0    SQL Error Number returned from MySQL Service API
      @retval <0    local errors
  */
  long execute(COM_DATA cmd,
               enum enum_server_command cmd_type,
               Sql_resultset *rset,
               enum cs_text_or_binary cs_txt_bin= CS_TEXT_REPRESENTATION,
               const CHARSET_INFO *cs_charset= &my_charset_utf8_general_ci);


  /**
    Set send result type to CS_TEXT_REPRESENTATION or
    CS_BINARY_REPRESENTATION

    @param field_type  send result in string or native types
                       i.e. to CS_TEXT_REPRESENTATION or
                       CS_BINARY_REPRESENTATION
  */
  void set_send_resulttype(enum cs_text_or_binary field_type)
  {
    m_txt_or_bin= field_type;
  }

  /**
    set charset for the string data input(com_query for example)

    @param charset    charset for the string data input
  */
  void set_charset(const CHARSET_INFO * charset)
  {
    m_charset= charset;
  }

  /**
    Returns whether the session was killed

    @return
      @retval  0   not killed
      @retval  1   killed
  */
  int is_session_killed(MYSQL_SESSION session)
  {
    return srv_session_info_killed(session);
  }


  /**
    Returns the ID of a session.

    @return thread ID
  */
  uint64_t get_session_id()
  {
    return srv_session_info_get_session_id(m_session);
  }

  /**
    Returns the MYSQL_SESSION object.

    @return thread ID
  */
  MYSQL_SESSION get_session()
  {
    return m_session;
  }

  /**
    Set the session associated user.

    @param user the user to change to

    @return
      @retval  0   all ok
      @retval  1   error
  */
  int set_session_user(const char *user);

  /**
   Check if the server is running without user privileges

   @return
     @retval  true   the server is skipping the grant table
     @retval  false  user privileges are working normally
  */
  bool is_acl_disabled();
};

#endif //SQL_SERVICE_INTERFACE_INCLUDE
