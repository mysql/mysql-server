/*  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef SRV_SESSION_H
#define SRV_SESSION_H

#include <stdint.h>

#include "lex_string.h"
#include "m_ctype.h"
#include "my_command.h"
#include "my_psi_config.h"
#include "my_thread_local.h"
#include "mysql/components/services/bits/psi_statement_bits.h"
#include "mysql/service_command.h"
#include "mysql/service_srv_session.h"
#include "sql/protocol_callback.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "violite.h" /* enum_vio_type */

struct st_plugin_int;

/**
  @file
  Header file for the Srv_session class that wraps THD, DA in one bundle
  for easy use of internal APIs.
  Srv_session also provides means for physical thread initialization and
  respective deinitialization.
*/

#ifdef HAVE_PSI_STATEMENT_INTERFACE
extern PSI_statement_info stmt_info_new_packet;
#endif

class Srv_session {
 public:
  /**
    Initializes the module.


    This method has to be called at server startup.

    @return
     false  success
     true   failure
  */
  static bool module_init();

  /**
    Deinitializes the module


    This method has to be called at server shutdown.

    @return
      false  success
      true   failure
  */
  static bool module_deinit();

  /**
    Initializes the current physical thread for use with this class.

    @param plugin Pointer to the plugin structure, passed to the plugin over
                  the plugin init function.

    @return
      false  success
      true   failure
  */
  static bool init_thread(const void *plugin);

  /**
    Deinitializes the current physical thread for use with session service
  */
  static void deinit_thread();

  /**
    Checks if a plugin has left threads and sessions

    @param plugin  The plugin to be checked
  */
  static void check_for_stale_threads(const st_plugin_int *plugin);

  /**
    Checks if the session is valid.

    Checked is if session is NULL, or in the list of opened sessions. If the
    session is not in this list it was either closed or the address is invalid.

    @return
      true  valid
      false not valid
  */
  static bool is_valid(const Srv_session *session);

  /**
    Returns the number opened sessions in thread initialized by this class.
  */
  static unsigned int session_count();

  /**
    Returns the number currently running threads initialized by this class.
  */
  static unsigned int thread_count(const void *plugin_name);

  /**
    Check if current physical thread was created to be used with this class.
  */
  static bool is_srv_session_thread();

  /* Non-static members follow */

  /**
    Enum for the state of the session
  */
  enum srv_session_state {
    SRV_SESSION_CREATED,
    SRV_SESSION_OPENED,
    SRV_SESSION_ATTACHED,
    SRV_SESSION_DETACHED,
    /*
      Following are the states
      while using the existing
      THD with the session.
    */
    SRV_SESSION_ASSOCIATE,     /* session using the THD provided explicitly */
    SRV_SESSION_ASSOCIATED,    /* explicit THD, is installed */
    SRV_SESSION_DISASSOCIATED, /* Changes the state of a session
                                 to disassociate */
    SRV_SESSION_CLOSED
  };

  /**
    Constructs a server session. That means This session object owns the THD.

    @note May throw if it fails to construct server session.

    @param err_cb         Default completion callback
    @param err_cb_ctx     Plugin's context, opaque pointer that would
                          be provided to callbacks. Might be NULL.
  */
  Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx);

  /**
   Have a THD object and wish to associate the same to session object.
   Session object will use the thd. It won't own it.

   @param err_cb         Default completion callback
   @param err_cb_ctx     Plugin's context, opaque pointer that would
                          be provided to callbacks. Might be NULL.
   @param thd   A valid THD
  */
  Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx, THD *thd);

  ~Srv_session();

  /**
    Opens a server session

    @return
      session  on success
      NULL     on failure
  */
  bool open();

  /**
    Attaches the session to the current physical thread

    @returns
      false   success
      true    failure
  */
  bool attach();

  /**
    Detaches the session from current physical thread.

    @returns
      false success
      true  failure
  */
  bool detach();

  /**
    Closes the session

    @returns
      false Session successfully closed
      true  Session wasn't found or key doesn't match
  */
  bool close();

  /**
    Returns if the session is in attached state

    @returns
      false   Not attached
      true    Attached
  */
  inline bool is_attached() const { return m_state == SRV_SESSION_ATTACHED; }

  /**
    Executes a server command.

    @param command  Command to be executed
    @param data     Command's arguments
    @param client_cs  The charset for the string data input (COM_QUERY
                      for example)
    @param command_callbacks  Callbacks to be used by the server to encode data
                              and to communicate with the client (plugin) side.
    @param text_or_binary     See enum cs_text_or_binary
    @param callbacks_context  Context passed to the callbacks

    @returns
      1   error
      0   success
  */
  int execute_command(enum enum_server_command command,
                      const union COM_DATA *data, const CHARSET_INFO *client_cs,
                      const struct st_command_service_cbs *command_callbacks,
                      enum cs_text_or_binary text_or_binary,
                      void *callbacks_context);

  /**
    Returns the internal THD object
  */
  inline THD *get_thd() { return m_thd; }

  /**
    Returns the ID of a session.

    The value returned from THD::thread_id()
  */
  my_thread_id get_session_id() const { return m_thd->thread_id(); }

  /**
    Returns the client port.

    @note The client port in SHOW PROCESSLIST, INFORMATION_SCHEMA.PROCESSLIST.
    This port is NOT shown in PERFORMANCE_SCHEMA.THREADS.
  */
  uint16_t get_client_port() const { return m_thd->peer_port; }

  /**
    Sets the client port.

    @note The client port in SHOW PROCESSLIST, INFORMATION_SCHEMA.PROCESSLIST.
    This port is NOT shown in PERFORMANCE_SCHEMA.THREADS.

    @param port  Port number
  */
  void set_client_port(uint16_t port);

  /**
    Returns the current database of a session.

    @note This call is not thread-safe. Don't invoke the method from a thread
          different than the one in which the invocation happens. This means
          that the call should NOT happen during run_command(). The value
          returned is valid until the next run_command() call, which may
          change it.
  */
  LEX_CSTRING get_current_database() const { return m_thd->db(); }

  /**
    Sets the connection type.

    @see enum_vio_type

    @note If NO_VIO_TYPE passed as type the call will fail.

    @return
      false success
      true  failure
  */
  bool set_connection_type(enum_vio_type type);

  struct st_err_protocol_ctx {
    st_err_protocol_ctx(srv_session_error_cb h, void *h_ctx)
        : handler(h), handler_context(h_ctx) {}

    srv_session_error_cb handler;
    void *handler_context;
  };

 private:
  Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx,
              srv_session_state state, bool free_resources, THD *thd);

  /**
    Sets session's state to attached

    @param stack  New stack address
  */
  void set_attached(const char *stack);

  /**
    Changes the state of a session to detached
  */
  void set_detached();

  /**
    Changes the state of a session to associate
  */
  void set_associate();

  /**
    Changes the state of a session to disassociate
  */
  void set_disassociate();

  /**
    Check is the session state is associate.
    In other words, session is using the THD provided explicitly.

    @returns
      true   session state is associate
      false  Otherwise
  */
  bool is_associate() const;

  /**
    Check if the session state is associated.
    In other words, explicit thd which pointed by this, is installed

    @returns
      true   session state is associated
      false  Otherwise
  */
  bool is_associated() const;

  /**
    Installs the thd pointed by the session object as the current_thd

    @return
      false success
      true  failure
  */
  bool associate();

  /**
   Uninstall the thd pointed by the session object as the current_thd

   @return
     false success
     true  failure
  */
  bool disassociate();

  Diagnostics_area m_da;
  st_err_protocol_ctx m_err_protocol_ctx;
  Protocol_callback m_protocol_error;

  srv_session_state m_state;
  enum_vio_type m_vio_type;
  THD *m_thd;
  const bool m_free_resources;

  class Session_backup_and_attach {
   public:
    /**
      Constructs a session state object. Saves state then attaches a session.
      Uses RAII.

      @param sess Session to backup
      @param is_in_close_session Whether session needs to be closed.
    */
    Session_backup_and_attach(Srv_session *sess, bool is_in_close_session);

    /**
      Destructs the session state object. In other words it restores to
      previous state.
    */
    ~Session_backup_and_attach();

   private:
    Srv_session *session;
    Srv_session *old_session; /* used in srv_session threads */
    THD *backup_thd;
    bool in_close_session;

   public:
    bool attach_error;
  };
};

#endif /* SRV_SESSION_H */
