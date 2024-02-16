/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "my_dbug.h"                                /* assert */
#include "mysql/components/services/log_builtins.h" /* LogErr */
#include "mysql/status_var.h"                       /* SHOW_VAR */
#include "mysqld_error.h"                           /* Error/Warning macros */
#include "sql/ssl_acceptor_context_status.h"        /* Status functions */

#include "sql/ssl_acceptor_context_operator.h"

Ssl_acceptor_context_container *mysql_main;
Ssl_acceptor_context_container *mysql_admin;

Ssl_acceptor_context_container::Ssl_acceptor_context_container(
    Ssl_acceptor_context_data *data)
    : lock_(nullptr) {
  lock_ = new Ssl_acceptor_context_data_lock(data);
}

Ssl_acceptor_context_container ::~Ssl_acceptor_context_container() {
  if (lock_ != nullptr) delete lock_;
  lock_ = nullptr;
}

void Ssl_acceptor_context_container::switch_data(
    Ssl_acceptor_context_data *new_data) {
  if (lock_ != nullptr) lock_->write_wait_and_delete(new_data);
}

bool TLS_channel::singleton_init(Ssl_acceptor_context_container **out,
                                 std::string channel,
                                 Ssl_init_callback *callbacks, bool db_init) {
  if (out == nullptr || callbacks == nullptr) return true;
  *out = nullptr;
  /*
    No need to take the ssl_ctx_lock lock here since it's being called
    from singleton_init().
  */
  if (callbacks->provision_certs()) return true;

  enum enum_ssl_init_error error = SSL_INITERR_NOERROR;
  Ssl_acceptor_context_data *news =
      new Ssl_acceptor_context_data(channel, callbacks, true, &error);
  Ssl_acceptor_context_container *new_container =
      new Ssl_acceptor_context_container(news);
  if (news == nullptr || new_container == nullptr) {
    LogErr(WARNING_LEVEL, ER_SSL_LIBRARY_ERROR,
           "Error initializing the SSL context system structure");
    if (new_container) delete new_container;
    return true;
  }

  if (opt_tls_certificates_enforced_validation &&
      error != SSL_INITERR_NOERROR) {
    LogErr(ERROR_LEVEL, ER_FAILED_TO_VALIDATE_CERTIFICATES_SERVER_EXIT);
    delete new_container;
    return true;
  }

  if (news->have_ssl() && callbacks->warn_self_signed_ca()) {
    /* This would delete Ssl_acceptor_context_data too */
    delete new_container;
    return true;
  }

  if (!db_init && news->have_ssl())
    LogErr(SYSTEM_LEVEL, ER_TLS_CONFIGURED_FOR_CHANNEL, channel.c_str());

  *out = new_container;
  return false;
}

void TLS_channel::singleton_deinit(Ssl_acceptor_context_container *container) {
  if (container == nullptr) return;
  delete container;
}

void TLS_channel::singleton_flush(Ssl_acceptor_context_container *container,
                                  std::string channel,
                                  Ssl_init_callback *callbacks,
                                  enum enum_ssl_init_error *error, bool force) {
  if (container == nullptr) return;
  Ssl_acceptor_context_data *news =
      new Ssl_acceptor_context_data(channel, callbacks, false, error);
  if (*error != SSL_INITERR_NOERROR && !force) {
    delete news;
    return;
  }
  (void)container->switch_data(news);
  return;
}

std::string Lock_and_access_ssl_acceptor_context::show_property(
    Ssl_acceptor_context_property_type property_type) {
  const Ssl_acceptor_context_data *data = read_lock_;
  return (data != nullptr ? data->show_property(property_type) : std::string{});
}

std::string Lock_and_access_ssl_acceptor_context::channel_name() {
  const Ssl_acceptor_context_data *data = read_lock_;
  return (data != nullptr ? data->channel_name() : std::string{});
}

bool Lock_and_access_ssl_acceptor_context::have_ssl() {
  const Ssl_acceptor_context_data *data = read_lock_;
  return (data != nullptr ? data->have_ssl() : false);
}

bool have_ssl() {
  if (mysql_main != nullptr) {
    Lock_and_access_ssl_acceptor_context context(mysql_main);
    if (context.have_ssl()) return true;
  }
  if (mysql_admin != nullptr) {
    Lock_and_access_ssl_acceptor_context context(mysql_admin);
    if (context.have_ssl()) return true;
  }
  return false;
}

/* Helpers */
static int show_long_status(SHOW_VAR *var, char *buff,
                            Ssl_acceptor_context_property_type property_type) {
  std::string property;
  if (mysql_main != nullptr) {
    Lock_and_access_ssl_acceptor_context main(mysql_main);
    property = main.show_property(property_type);
  }
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) = std::stol(property);

  return 0;
}

static int show_char_status(SHOW_VAR *var, char *buff,
                            Ssl_acceptor_context_property_type property_type) {
  std::string property;
  if (mysql_main != nullptr) {
    Lock_and_access_ssl_acceptor_context main(mysql_main);
    property = main.show_property(property_type);
  }
  var->type = SHOW_CHAR;
  strncpy(buff, property.c_str(), SHOW_VAR_FUNC_BUFF_SIZE);
  buff[SHOW_VAR_FUNC_BUFF_SIZE - 1] = 0;
  var->value = buff;

  return 0;
}
/* Helpers end */

/* Status functions for mysql_main TLS context */

int Ssl_mysql_main_status::show_ssl_ctx_sess_accept(THD *, SHOW_VAR *var,
                                                    char *buff) {
  return show_long_status(var, buff,
                          Ssl_acceptor_context_property_type::accepts);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_accept_good(THD *, SHOW_VAR *var,
                                                         char *buff) {
  return show_long_status(var, buff,
                          Ssl_acceptor_context_property_type::finished_accepts);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_connect_good(THD *, SHOW_VAR *var,
                                                          char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::finished_connects);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_accept_renegotiate(THD *,
                                                                SHOW_VAR *var,
                                                                char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::callback_cache_hits);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_connect_renegotiate(THD *,
                                                                 SHOW_VAR *var,
                                                                 char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::callback_cache_hits);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_cb_hits(THD *, SHOW_VAR *var,
                                                     char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::callback_cache_hits);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_hits(THD *, SHOW_VAR *var,
                                                  char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_hits);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_cache_full(THD *, SHOW_VAR *var,
                                                        char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_overflows);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_misses(THD *, SHOW_VAR *var,
                                                    char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_misses);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_timeouts(THD *, SHOW_VAR *var,
                                                      char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_timeouts);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_timeout(THD *, SHOW_VAR *var,
                                                     char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_timeout);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_number(THD *, SHOW_VAR *var,
                                                    char *buff) {
  return show_long_status(
      var, buff,
      Ssl_acceptor_context_property_type::used_session_cache_entries);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_connect(THD *, SHOW_VAR *var,
                                                     char *buff) {
  return show_long_status(var, buff,
                          Ssl_acceptor_context_property_type::client_connects);
}

int Ssl_mysql_main_status::show_ssl_ctx_sess_get_cache_size(THD *,
                                                            SHOW_VAR *var,
                                                            char *buff) {
  return show_long_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_size);
}

int Ssl_mysql_main_status::show_ssl_ctx_get_verify_mode(THD *, SHOW_VAR *var,
                                                        char *buff) {
  return show_long_status(var, buff,
                          Ssl_acceptor_context_property_type::ctx_verify_mode);
}

int Ssl_mysql_main_status::show_ssl_ctx_get_verify_depth(THD *, SHOW_VAR *var,
                                                         char *buff) {
  return show_long_status(var, buff,
                          Ssl_acceptor_context_property_type::ctx_verify_depth);
}

int Ssl_mysql_main_status::show_ssl_ctx_get_session_cache_mode(THD *,
                                                               SHOW_VAR *var,
                                                               char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::session_cache_mode);
}

int Ssl_mysql_main_status::show_ssl_get_server_not_before(THD *, SHOW_VAR *var,
                                                          char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::server_not_before);
}

int Ssl_mysql_main_status::show_ssl_get_server_not_after(THD *, SHOW_VAR *var,
                                                         char *buff) {
  return show_char_status(var, buff,
                          Ssl_acceptor_context_property_type::server_not_after);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_ca(THD *, SHOW_VAR *var,
                                               char *buff) {
  return show_char_status(var, buff,
                          Ssl_acceptor_context_property_type::current_tls_ca);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_capath(THD *, SHOW_VAR *var,
                                                   char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::current_tls_capath);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_cert(THD *, SHOW_VAR *var,
                                                 char *buff) {
  return show_char_status(var, buff,
                          Ssl_acceptor_context_property_type::current_tls_cert);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_key(THD *, SHOW_VAR *var,
                                                char *buff) {
  return show_char_status(var, buff,
                          Ssl_acceptor_context_property_type::current_tls_key);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_cipher(THD *, SHOW_VAR *var,
                                                   char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::current_tls_cipher);
}

int Ssl_mysql_main_status::show_ssl_get_tls_ciphersuites(THD *, SHOW_VAR *var,
                                                         char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::current_tls_ciphersuites);
}

int Ssl_mysql_main_status::show_ssl_get_tls_version(THD *, SHOW_VAR *var,
                                                    char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::current_tls_version);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_crl(THD *, SHOW_VAR *var,
                                                char *buff) {
  return show_char_status(var, buff,
                          Ssl_acceptor_context_property_type::current_tls_crl);
}

int Ssl_mysql_main_status::show_ssl_get_ssl_crlpath(THD *, SHOW_VAR *var,
                                                    char *buff) {
  return show_char_status(
      var, buff, Ssl_acceptor_context_property_type::current_tls_crlpath);
}
