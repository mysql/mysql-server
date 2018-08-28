/*  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "mysql/components/services/clone_protocol_service.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/log_builtins.h"

#include "my_byteorder.h"
#include "mysql.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/sql_thd_internal_api.h"
#include "sql_common.h"

void clone_protocol_service_init() { return; }

DEFINE_METHOD(void, mysql_clone_start_statement,
              (THD * &thd, PSI_thread_key thread_key,
               PSI_statement_key statement_key)) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  bool thd_created = false;
#endif

  if (thd == nullptr) {
    /* Initialize Session */
    my_thread_init();

    /* Create thread with input key for PFS */
    thd = create_thd(true, true, true, thread_key);
#ifdef HAVE_PSI_THREAD_INTERFACE
    thd_created = true;
#endif
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  /* Create and set PFS thread key */
  if (thread_key != PSI_NOT_INSTRUMENTED) {
    if (thd_created) {
      PSI_THREAD_CALL(set_thread)(thd->get_psi());
    } else {
      if (thd->m_statement_psi != nullptr) {
        MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
        thd->m_statement_psi = nullptr;
        thd->m_digest = nullptr;
      }
      /* Set PSI information for thread */
      PSI_THREAD_CALL(delete_current_thread)();

      auto psi = PSI_THREAD_CALL(new_thread)(thread_key, thd, thd->thread_id());
      PSI_THREAD_CALL(set_thread_os_id)(psi);
      PSI_THREAD_CALL(set_thread)(psi);

      thd->set_psi(psi);
    }
  }
#endif

  /* Create and set PFS statement key */
  if (statement_key != PSI_NOT_INSTRUMENTED) {
    if (thd->m_statement_psi == nullptr) {
      thd->m_statement_psi = MYSQL_START_STATEMENT(
          &thd->m_statement_state, statement_key, thd->db().str,
          thd->db().length, thd->charset(), nullptr);
    } else {
      thd->m_statement_psi =
          MYSQL_REFINE_STATEMENT(thd->m_statement_psi, statement_key);
    }
  }
}

DEFINE_METHOD(void, mysql_clone_finish_statement, (THD * thd)) {
  DBUG_ASSERT(thd->m_statement_psi == nullptr);

  my_thread_end();
  destroy_thd(thd);
}

DEFINE_METHOD(MYSQL *, mysql_clone_connect,
              (THD * thd, const char *host, uint port, const char *user,
               const char *passwd, mysql_clone_ssl_context *ssl_ctx,
               MYSQL_SOCKET *socket)) {
  DBUG_ENTER("mysql_clone_connect");

  /* Set default as 5 seconds */
  uint net_read_timeout = 5;
  uint net_write_timeout = 5;

  /* Clean any previous Error and Warnings in THD */
  if (thd != nullptr) {
    thd->clear_error();
    thd->get_stmt_da()->reset_condition_info(thd);
    net_read_timeout = thd->variables.net_read_timeout;
    net_write_timeout = thd->variables.net_write_timeout;
  }

  MYSQL *mysql;
  MYSQL *ret_mysql;

  /* Connect using classic protocol */
  mysql = mysql_init(nullptr);

  auto client_ssl_mode = static_cast<enum mysql_ssl_mode>(ssl_ctx->m_ssl_mode);

  /* Get server public key for RSA key pair-based password exchange.*/
  bool get_key = true;
  mysql_options(mysql, MYSQL_OPT_GET_SERVER_PUBLIC_KEY, &get_key);

  if (client_ssl_mode != SSL_MODE_DISABLED) {
    /* Verify server's certificate */
    if (ssl_ctx->m_ssl_certificate_authority != nullptr) {
      client_ssl_mode = SSL_MODE_VERIFY_CA;
    }
    mysql_ssl_set(mysql, ssl_ctx->m_ssl_private_key, ssl_ctx->m_ssl_certificate,
                  ssl_ctx->m_ssl_certificate_authority, opt_ssl_capath,
                  opt_ssl_cipher);

    mysql_options(mysql, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
    mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
    mysql_options(mysql, MYSQL_OPT_TLS_VERSION, opt_tls_version);
  }

  mysql_options(mysql, MYSQL_OPT_SSL_MODE, &client_ssl_mode);

  auto timeout = static_cast<uint>(connect_timeout);
  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT,
                reinterpret_cast<char *>(&timeout));

  ret_mysql =
      mysql_real_connect(mysql, host, user, passwd, nullptr, port, 0, 0);

  if (ret_mysql == nullptr) {
    char err_buf[MYSYS_ERRMSG_SIZE + 64];
    snprintf(err_buf, sizeof(err_buf), "Connect failed: %u : %s",
             mysql_errno(mysql), mysql_error(mysql));

    my_error(ER_CLONE_REMOTE_ERROR, MYF(0), err_buf);
    LogErr(INFORMATION_LEVEL, ER_CLONE_INFO_CLIENT, err_buf);

    mysql_close(mysql);
    DBUG_RETURN(nullptr);
  }

  NET *net = &mysql->net;
  Vio *vio = net->vio;

  *socket = vio->mysql_socket;

  net_clear_error(net);
  net_clear(net, true);

  /* Set network read/write timeout */
  my_net_set_read_timeout(net, net_read_timeout);
  my_net_set_write_timeout(net, net_write_timeout);

  if (thd != nullptr) {
    /* Set current active vio so that shutdown and KILL
       signals can wake up current thread. */
    thd->set_clone_vio(net->vio);
  }

  /* Load clone plugin in remote */
  auto result = simple_command(mysql, COM_CLONE, nullptr, 0, 0);

  if (result) {
    if (thd != nullptr) {
      thd->clear_clone_vio();
    }
    char err_buf[MYSYS_ERRMSG_SIZE + 64];
    snprintf(err_buf, sizeof(err_buf), "%d : %s", net->last_errno,
             net->last_error);

    my_error(ER_CLONE_REMOTE_ERROR, MYF(0), err_buf);

    snprintf(err_buf, sizeof(err_buf), "COM_CLONE failed %d : %s",
             net->last_errno, net->last_error);
    LogErr(INFORMATION_LEVEL, ER_CLONE_INFO_CLIENT, err_buf);

    mysql_close(mysql);
    mysql = nullptr;
  }
  DBUG_RETURN(mysql);
}

DEFINE_METHOD(int, mysql_clone_send_command,
              (THD * thd, MYSQL *connection, bool set_active, uchar command,
               uchar *com_buffer, size_t buffer_length)) {
  DBUG_ENTER("mysql_clone_send_command");
  NET *net = &connection->net;

  if (net->last_errno != 0) {
    DBUG_RETURN(static_cast<int>(net->last_errno));
  }

  net_clear_error(net);
  net_clear(net, true);

  if (set_active && thd->killed != THD::NOT_KILLED) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    DBUG_RETURN(ER_QUERY_INTERRUPTED);
  }

  auto result =
      net_write_command(net, command, nullptr, 0, com_buffer, buffer_length);
  if (!result) {
    DBUG_RETURN(0);
  }

  int err = static_cast<int>(net->last_errno);

  /* Check if query is interrupted */
  if (set_active && thd->killed != THD::NOT_KILLED) {
    thd->clear_error();
    thd->get_stmt_da()->reset_condition_info(thd);
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    err = ER_QUERY_INTERRUPTED;
  }

  DBUG_ASSERT(err != 0);
  DBUG_RETURN(err);
}

DEFINE_METHOD(int, mysql_clone_get_response,
              (THD * thd, MYSQL *connection, bool set_active, uint32_t timeout,
               uchar **packet, size_t *length)) {
  DBUG_ENTER("mysql_clone_get_response");

  NET *net = &connection->net;

  if (net->last_errno != 0) {
    DBUG_RETURN(static_cast<int>(net->last_errno));
  }

  if (set_active && thd->killed != THD::NOT_KILLED) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    DBUG_RETURN(ER_QUERY_INTERRUPTED);
  }

  net_new_transaction(net);

  /* Adjust read timeout if specified. */
  if (timeout != 0) {
    my_net_set_read_timeout(net, timeout);
  }

  *length = my_net_read(net);

  /* Reset timeout back to default value. */
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);

  *packet = net->read_pos;

  if (*length != packet_error && *length != 0) {
    DBUG_RETURN(0);
  }

  int err = static_cast<int>(net->last_errno);
  /* Check if query is interrupted */
  if (set_active && thd->killed != THD::NOT_KILLED) {
    thd->clear_error();
    thd->get_stmt_da()->reset_condition_info(thd);
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    err = ER_QUERY_INTERRUPTED;
  }

  if (*length == 0 && err == 0) {
    net->last_errno = ER_NET_PACKETS_OUT_OF_ORDER;
    err = ER_NET_PACKETS_OUT_OF_ORDER;
    my_error(err, MYF(0));
  }

  DBUG_ASSERT(err != 0);
  DBUG_RETURN(err);
}

DEFINE_METHOD(int, mysql_clone_kill,
              (MYSQL * connection, MYSQL *kill_connection)) {
  DBUG_ENTER("mysql_clone_kill");

  auto kill_conn_id = kill_connection->thread_id;

  char kill_buffer[64];
  snprintf(kill_buffer, 64, "KILL CONNECTION %lu", kill_conn_id);

  auto err = mysql_real_query(connection, kill_buffer,
                              static_cast<ulong>(strlen(kill_buffer)));

  DBUG_RETURN(err);
}

DEFINE_METHOD(void, mysql_clone_disconnect,
              (THD * thd, MYSQL *mysql, bool is_fatal, bool clear_error)) {
  DBUG_ENTER("mysql_clone_disconnect");

  if (thd != nullptr) {
    thd->clear_clone_vio();

    /* clear any session error, if requested */
    if (clear_error) {
      thd->clear_error();
      thd->get_stmt_da()->reset_condition_info(thd);
    }
  }

  /* Make sure that the other end has switched back from clone protocol. */
  if (!is_fatal) {
    is_fatal = simple_command(mysql, COM_RESET_CONNECTION, nullptr, 0, 0);
  }

  if (is_fatal) {
    end_server(mysql);
  }

  /* Disconnect */
  mysql_close(mysql);
  DBUG_VOID_RETURN;
}

DEFINE_METHOD(int, mysql_clone_get_command,
              (THD * thd, uchar *command, uchar **com_buffer,
               size_t *buffer_length)) {
  DBUG_ENTER("mysql_clone_get_command");

  NET *net = thd->get_protocol_classic()->get_net();

  if (net->last_errno != 0) {
    DBUG_RETURN(static_cast<int>(net->last_errno));
  }

  /* flush any data in write buffer */
  if (!net_flush(net)) {
    net_new_transaction(net);

    /* Use idle timeout while waiting for commands */
    my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

    *buffer_length = my_net_read(net);

    my_net_set_read_timeout(net, thd->variables.net_read_timeout);

    if (*buffer_length != packet_error && *buffer_length != 0) {
      *com_buffer = net->read_pos;
      *command = **com_buffer;

      ++(*com_buffer);
      --(*buffer_length);

      DBUG_RETURN(0);
    }
  }

  int err = static_cast<int>(net->last_errno);

  if (*buffer_length == 0 && err == 0) {
    net->last_errno = ER_NET_PACKETS_OUT_OF_ORDER;
    err = ER_NET_PACKETS_OUT_OF_ORDER;
    my_error(err, MYF(0));
  }

  DBUG_ASSERT(err != 0);
  DBUG_RETURN(err);
}

DEFINE_METHOD(int, mysql_clone_send_response,
              (THD * thd, uchar *packet, size_t length)) {
  DBUG_ENTER("mysql_clone_send_response");

  NET *net = thd->get_protocol_classic()->get_net();

  if (net->last_errno != 0) {
    DBUG_RETURN(static_cast<int>(net->last_errno));
  }

  net_new_transaction(net);

  if (!my_net_write(net, packet, length) && !net_flush(net)) {
    DBUG_RETURN(0);
  }

  int err = static_cast<int>(net->last_errno);

  DBUG_ASSERT(err != 0);
  DBUG_RETURN(err);
}

DEFINE_METHOD(int, mysql_clone_send_error,
              (THD * thd, uchar err_cmd, bool is_fatal)) {
  DBUG_ENTER("mysql_clone_send_error");

  uchar err_packet[1 + 4 + MYSQL_ERRMSG_SIZE + 1];
  uchar *buf_ptr = &err_packet[0];
  size_t packet_length = 0;

  *buf_ptr = err_cmd;
  ++buf_ptr;
  ++packet_length;

  auto da = thd->get_stmt_da();

  char *bufp;

  if (da->is_error()) {
    int4store(buf_ptr, da->mysql_errno());
    buf_ptr += 4;
    packet_length += 4;

    bufp = reinterpret_cast<char *>(buf_ptr);
    packet_length +=
        snprintf(bufp, MYSQL_ERRMSG_SIZE, "%s", da->message_text());
    if (is_fatal) {
      mysql_mutex_lock(&thd->LOCK_thd_data);
      thd->shutdown_active_vio();
      mysql_mutex_unlock(&thd->LOCK_thd_data);

      DBUG_RETURN(da->mysql_errno());
    }
  } else {
    int4store(buf_ptr, ER_INTERNAL_ERROR);
    buf_ptr += 4;
    packet_length += 4;

    bufp = reinterpret_cast<char *>(buf_ptr);
    packet_length += snprintf(bufp, MYSQL_ERRMSG_SIZE, "%s", "Unknown Error");
  }

  NET *net = thd->get_protocol_classic()->get_net();

  if (net->last_errno != 0) {
    DBUG_RETURN(static_cast<int>(net->last_errno));
  }

  DBUG_ASSERT(!is_fatal);

  /* Clean error in THD */
  thd->clear_error();
  thd->get_stmt_da()->reset_condition_info(thd);
  net_new_transaction(net);

  if (my_net_write(net, &err_packet[0], packet_length) || net_flush(net)) {
    int err = static_cast<int>(net->last_errno);

    if (err == 0) {
      net->last_errno = ER_NET_PACKETS_OUT_OF_ORDER;
      err = ER_NET_PACKETS_OUT_OF_ORDER;
      my_error(err, MYF(0));
    }
    DBUG_RETURN(err);
  }
  DBUG_RETURN(0);
}
