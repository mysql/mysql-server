/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef SSL_ACCEPTOR_CONTEXT_STATUS
#define SSL_ACCEPTOR_CONTEXT_STATUS

/* Functions to show mysql_main interface's TLS properties */
class Ssl_mysql_main_status {
 public:
  static int show_ssl_ctx_sess_accept(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_accept_good(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_connect_good(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_accept_renegotiate(THD *, SHOW_VAR *var,
                                                  char *buff);
  static int show_ssl_ctx_sess_connect_renegotiate(THD *, SHOW_VAR *var,
                                                   char *buff);
  static int show_ssl_ctx_sess_cb_hits(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_hits(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_cache_full(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_misses(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_timeouts(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_number(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_connect(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_get_cache_size(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_get_verify_mode(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_get_verify_depth(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_get_session_cache_mode(THD *, SHOW_VAR *var,
                                                 char *buff);
  static int show_ssl_get_server_not_before(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_server_not_after(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_ca(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_capath(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_cert(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_key(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_cipher(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_tls_ciphersuites(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_tls_version(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_crl(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_get_ssl_crlpath(THD *, SHOW_VAR *var, char *buff);
  static int show_ssl_ctx_sess_timeout(THD *, SHOW_VAR *var, char *buff);
};
#endif  // !SSL_ACCEPTOR_CONTEXT_STATUS
