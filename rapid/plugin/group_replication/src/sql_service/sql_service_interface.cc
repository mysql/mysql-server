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

#include "sql_service_interface.h"
#include "plugin_log.h"

/* keep it in sync with enum_server_command in my_command.h */
const LEX_STRING command_name[]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("Init DB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("Field List") },
  { C_STRING_WITH_LEN("Create DB") },
  { C_STRING_WITH_LEN("Drop DB") },
  { C_STRING_WITH_LEN("Refresh") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Statistics") },
  { C_STRING_WITH_LEN("Processlist") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Kill") },
  { C_STRING_WITH_LEN("Debug") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Time") },
  { C_STRING_WITH_LEN("Delayed insert") },
  { C_STRING_WITH_LEN("Change user") },
  { C_STRING_WITH_LEN("Binlog Dump") },
  { C_STRING_WITH_LEN("Table Dump") },
  { C_STRING_WITH_LEN("Connect Out") },
  { C_STRING_WITH_LEN("Register Slave") },
  { C_STRING_WITH_LEN("Prepare") },
  { C_STRING_WITH_LEN("Execute") },
  { C_STRING_WITH_LEN("Long Data") },
  { C_STRING_WITH_LEN("Close stmt") },
  { C_STRING_WITH_LEN("Reset stmt") },
  { C_STRING_WITH_LEN("Set option") },
  { C_STRING_WITH_LEN("Fetch") },
  { C_STRING_WITH_LEN("Daemon") },
  { C_STRING_WITH_LEN("Binlog Dump GTID") },
  { C_STRING_WITH_LEN("Reset Connection") },
  { C_STRING_WITH_LEN("Error") }  // Last command number
};


/* Sql_service_interface constructor */
Sql_service_interface::Sql_service_interface(enum cs_text_or_binary cs_txt_bin,
                                             const CHARSET_INFO *charset)
: m_plugin(NULL),
  m_txt_or_bin(cs_txt_bin),
  m_charset(charset)
{
}

Sql_service_interface::~Sql_service_interface()
{
  /* close server session */
  if (m_session)
    srv_session_close(m_session);

  /* if thread was initialized deinitialized it */
  if (m_plugin)
    srv_session_deinit_thread();
}

int Sql_service_interface::open_session()
{
  DBUG_ENTER("Sql_service_interface::open_session");

  m_session= NULL;
  /* open a server session after server is in operating state */
  if (!wait_for_session_server(SESSION_WAIT_TIMEOUT))
  {
    m_session= srv_session_open(NULL, NULL);
    if (m_session == NULL)
      DBUG_RETURN(1); /* purecov: inspected */
  }
  else
  {
    DBUG_RETURN(1); /* purecov: inspected */
  }

  DBUG_RETURN(0);
}

int Sql_service_interface::open_thread_session(void *plugin_ptr)
{
  DBUG_ASSERT(plugin_ptr != NULL);

  m_session= NULL;
  /* open a server session after server is in operating state */
  if (!wait_for_session_server(SESSION_WAIT_TIMEOUT))
  {
    /* initalize new thread to be used with server session */
    if (srv_session_init_thread(plugin_ptr))
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL, "Error when initializing a session thread for"
                                  "internal server connection.");
      return 1;
      /* purecov: end */
    }

    m_session= srv_session_open(NULL, NULL);
    if (m_session == NULL)
      return 1; /* purecov: inspected */
  }
  else
  {
    return 1; /* purecov: inspected */
  }

  m_plugin= plugin_ptr;
  return 0;
}

long Sql_service_interface::execute_internal(Sql_resultset *rset,
                                             enum cs_text_or_binary cs_txt_bin,
                                             const CHARSET_INFO *cs_charset,
                                             COM_DATA cmd,
                                             enum enum_server_command cmd_type)
{
  DBUG_ENTER("Sql_service_interface::execute_internal");
  int err= 0;

  if (!m_session)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL, "Error, the internal server communication "
                                "session is not initialized.");
    DBUG_RETURN(-1);
    /* purecov: end */
  }

  if (is_session_killed(m_session))
  {
    /* purecov: begin inspected */
    log_message(MY_INFORMATION_LEVEL, "Error, the internal server communication "
                                      "session is killed or server is shutting"
                                      " down.");
    DBUG_RETURN(-1);
    /* purecov: end */
  }

  Sql_service_context_base *ctx= new Sql_service_context(rset);

  /* execute sql command */
  if (command_service_run_command(m_session, cmd_type, &cmd,
                                  cs_charset,
                                  &Sql_service_context_base::sql_service_callbacks,
                                  cs_txt_bin, ctx))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL, "Error running internal command type: %s."
                                "Got error: %s(%d)", command_name[cmd_type].str,
                                rset->sql_errno(), rset->err_msg().c_str());

    delete ctx;
    DBUG_RETURN(rset->sql_errno());
    /* purecov: end */
  }

  err= rset->sql_errno();
  delete ctx;
  DBUG_RETURN(err);
}


long Sql_service_interface::execute_query(std::string sql_string)
{
  DBUG_ENTER("Sql_service_interface::execute");
  DBUG_ASSERT(sql_string.length() <= UINT_MAX);
  COM_DATA cmd;
  Sql_resultset rset;

  cmd.com_query.query= (char *) sql_string.c_str();
  cmd.com_query.length= static_cast<unsigned int>(sql_string.length());

  long err= execute_internal(&rset, m_txt_or_bin,
                             m_charset, cmd, COM_QUERY);

  DBUG_RETURN(err);
}


long Sql_service_interface::execute_query(std::string sql_string,
                                          Sql_resultset *rset,
                                          enum cs_text_or_binary cs_txt_or_bin,
                                          const CHARSET_INFO *cs_charset)
{
  DBUG_ENTER("Sql_service_interface::execute");
  DBUG_ASSERT(sql_string.length() <= UINT_MAX);
  COM_DATA cmd;
  cmd.com_query.query= (char *) sql_string.c_str();
  cmd.com_query.length= static_cast<unsigned int>(sql_string.length());

  long err= execute_internal(rset, cs_txt_or_bin,
                            cs_charset, cmd, COM_QUERY);

  DBUG_RETURN(err);
}


long Sql_service_interface::execute(COM_DATA cmd,
				    enum enum_server_command cmd_type,
                                    Sql_resultset *rset,
                                    enum cs_text_or_binary cs_txt_or_bin,
                                    const CHARSET_INFO *cs_charset)
{
  DBUG_ENTER("Sql_service_interface::execute");

  long err= execute_internal(rset, cs_txt_or_bin,
                             cs_charset, cmd, cmd_type);

  DBUG_RETURN(err);
}

int Sql_service_interface::wait_for_session_server(ulong total_timeout)
{
  int number_of_tries= 0;
  ulong wait_retry_sleep= total_timeout * 1000000 / MAX_NUMBER_RETRIES;
  int err= 0;

  while (!srv_session_server_is_available())
  {
    /* purecov: begin inspected */
    if (number_of_tries >= MAX_NUMBER_RETRIES)
    {
      log_message(MY_ERROR_LEVEL,
                  "Error, maximum number of retries exceeded when waiting for "
                  "the internal server session state to be operating");
      err= 1;
      break;
    }
    else
    {
      /*
        If we have more tries.
        Then sleep before new attempts are made.
      */
      my_sleep(wait_retry_sleep);
      ++number_of_tries;
    }
    /* purecov: end */
  }

  return err;
}

int Sql_service_interface::set_session_user(const char *user)
{
  MYSQL_SECURITY_CONTEXT sc;
  if (thd_get_security_context(srv_session_info_get_thd(m_session), &sc)) {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Error when trying to fetch security context when contacting the"
                " server for internal plugin requests.");
    return 1;
    /* purecov: end */
  }
  if (security_context_lookup(sc, user, "localhost", NULL, NULL))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "There was an error when trying to access the server with user:"
                " %s. Make sure the user is present in the server and that"
                " mysql_upgrade was ran after a server update.", user);
    return 1;
    /* purecov: end */
  }
  return 0;
}

bool Sql_service_interface::is_acl_disabled()
{
  MYSQL_SECURITY_CONTEXT scontext;

  if (thd_get_security_context(srv_session_info_get_thd(m_session), &scontext))
    return false; /* purecov: inspected */

  MYSQL_LEX_CSTRING value;
  if (false != security_context_get_option(scontext, "priv_user", &value))
    return false; /* purecov: inspected */

  return 0 != value.length && NULL != strstr(value.str, "skip-grants ");
}
