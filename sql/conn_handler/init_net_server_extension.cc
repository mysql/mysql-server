/*
   Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "init_net_server_extension.h"

#include "violite.h"                    // Vio
#include "channel_info.h"               // Channel_info
#include "connection_handler_manager.h" // Connection_handler_manager
#include "mysqld.h"                     // key_socket_tcpip
#include "log.h"                        // sql_print_error
#include "sql_class.h"                  // THD

#include <pfs_idle_provider.h>
#include <mysql/psi/mysql_idle.h>

#ifdef HAVE_PSI_STATEMENT_INTERFACE
extern PSI_statement_info stmt_info_new_packet;
#endif

static void net_before_header_psi(struct st_net *net, void *user_data,
                                  size_t /* unused: count */) {
  THD *thd;
  thd = static_cast<THD *>(user_data);
  assert(thd != NULL);

  if (thd->m_server_idle) {
    /*
      The server is IDLE, waiting for the next command.
      Technically, it is a wait on a socket, which may take a long time,
      because the call is blocking.
      Disable the socket instrumentation, to avoid recording a SOCKET event.
      Instead, start explicitly an IDLE event.
    */
    MYSQL_SOCKET_SET_STATE(net->vio->mysql_socket, PSI_SOCKET_STATE_IDLE);
    MYSQL_START_IDLE_WAIT(thd->m_idle_psi, &thd->m_idle_state);
  }
}

static void net_after_header_psi(struct st_net *net, void *user_data,
                                 size_t /* unused: count */, my_bool rc) {
  THD *thd;
  thd = static_cast<THD *>(user_data);
  assert(thd != NULL);

  if (thd->m_server_idle) {
    /*
      The server just got data for a network packet header,
      from the network layer.
      The IDLE event is now complete, since we now have a message to process.
      We need to:
      - start a new STATEMENT event
      - start a new STAGE event, within this statement,
      - start recording SOCKET WAITS events, within this stage.
      The proper order is critical to get events numbered correctly,
      and nested in the proper parent.
    */
    MYSQL_END_IDLE_WAIT(thd->m_idle_psi);

    if (!rc) {
      assert(thd->m_statement_psi == NULL);
      thd->m_statement_psi = MYSQL_START_STATEMENT(
          &thd->m_statement_state, stmt_info_new_packet.m_key, thd->db().str,
          thd->db().length, thd->charset(), NULL);

      /*
        Starts a new stage in performance schema, if compiled in and enabled.
        Also sets THD::proc_info (used by SHOW PROCESSLIST, column STATE)
      */
      THD_STAGE_INFO(thd, stage_starting);
    }

    /*
      TODO: consider recording a SOCKET event for the bytes just read,
      by also passing count here.
    */
    MYSQL_SOCKET_SET_STATE(net->vio->mysql_socket, PSI_SOCKET_STATE_ACTIVE);
    thd->m_server_idle = false;
  }
}

void init_net_server_extension(THD *thd) {
  /* Start with a clean state for connection events. */
  thd->m_idle_psi = NULL;
  thd->m_statement_psi = NULL;
  thd->m_server_idle = false;

  /* Hook up the NET_SERVER callback in the net layer. */
  thd->m_net_server_extension.m_user_data = thd;
  thd->m_net_server_extension.m_before_header = net_before_header_psi;
  thd->m_net_server_extension.m_after_header = net_after_header_psi;
  thd->m_net_server_extension.timeout_on_full_packet = FALSE;

  /* Activate this private extension for the mysqld server. */
  thd->get_protocol_classic()->get_net()->extension =
      &thd->m_net_server_extension;
}
