/* Copyright (c) 2009, 2016, Oracle and/or its affiliates. All rights reserved.

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

/* support for Services */
#include <service_versions.h>
#include "mysql/services.h"

struct st_service_ref {
  const char *name;
  uint version;
  void *service;
};

#ifndef EMBEDDED_LIBRARY
static struct srv_session_service_st srv_session_service_handler= {
  srv_session_init_thread,
  srv_session_deinit_thread,
  srv_session_open,
  srv_session_detach,
  srv_session_close,
  srv_session_server_is_available
};

static struct command_service_st command_handler= {
  command_service_run_command,
};

static struct srv_session_info_service_st srv_session_info_handler= {
  srv_session_info_get_thd,
  srv_session_info_get_session_id,
  srv_session_info_get_current_db,
  srv_session_info_get_client_port,
  srv_session_info_set_client_port,
  srv_session_info_set_connection_type,
  srv_session_info_killed,
  srv_session_info_session_count,
  srv_session_info_thread_count
};
#endif

static struct my_snprintf_service_st my_snprintf_handler = {
  my_snprintf,
  my_vsnprintf
};

static struct thd_alloc_service_st thd_alloc_handler= {
  thd_alloc,
  thd_calloc,
  thd_strdup,
  thd_strmake,
  thd_memdup,
  thd_make_lex_string
};

static struct thd_wait_service_st thd_wait_handler= {
  thd_wait_begin,
  thd_wait_end
};

static struct my_thread_scheduler_service my_thread_scheduler_handler= {
  my_connection_handler_set,
  my_connection_handler_reset
};

static struct my_plugin_log_service my_plugin_log_handler= {
  my_plugin_log_message
};

static struct mysql_string_service_st mysql_string_handler= {
  mysql_string_convert_to_char_ptr,
  mysql_string_get_iterator,
  mysql_string_iterator_next,
  mysql_string_iterator_isupper,
  mysql_string_iterator_islower,
  mysql_string_iterator_isdigit,
  mysql_string_to_lowercase,
  mysql_string_free,
  mysql_string_iterator_free,
};

static struct mysql_malloc_service_st mysql_malloc_handler=
{
  my_malloc,
  my_realloc,
  my_claim,
  my_free,
  my_memdup,
  my_strdup,
  my_strndup
};


static struct mysql_password_policy_service_st mysql_password_policy_handler= {
  my_validate_password_policy,
  my_calculate_password_strength
};

static struct mysql_parser_service_st parser_handler=
{
  mysql_parser_current_session,
  mysql_parser_open_session,
  mysql_parser_start_thread,
  mysql_parser_join_thread,
  mysql_parser_set_current_database,
  mysql_parser_parse,
  mysql_parser_get_statement_type,
  mysql_parser_get_statement_digest,
  mysql_parser_get_number_params,
  mysql_parser_extract_prepared_params,
  mysql_parser_visit_tree,
  mysql_parser_item_string,
  mysql_parser_free_string,
  mysql_parser_get_query,
  mysql_parser_get_normalized_query
};

static struct rpl_transaction_ctx_service_st rpl_transaction_ctx_handler= {
  set_transaction_ctx,
};

static struct transaction_write_set_service_st transaction_write_set_handler= {
  get_transaction_write_set,
};

static struct mysql_locking_service_st locking_service_handler=
{
  mysql_acquire_locking_service_locks,
  mysql_release_locking_service_locks
};

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static struct security_context_service_st security_context_handler={
  thd_get_security_context,
  thd_set_security_context,
  security_context_create,
  security_context_destroy,
  security_context_copy,
  security_context_lookup,
  security_context_get_option,
  security_context_set_option
};
#endif

static struct mysql_keyring_service_st mysql_keyring_handler= {
  my_key_store,
  my_key_fetch,
  my_key_remove,
  my_key_generate
};

static struct st_service_ref list_of_services[]=
{
#ifndef EMBEDDED_LIBRARY
  { "srv_session_service",
    VERSION_srv_session_service,&srv_session_service_handler },
  { "command_service",     VERSION_command, &command_handler },
  { "srv_session_info_service",
     VERSION_srv_session_info_service, &srv_session_info_handler },
#endif
  { "my_snprintf_service", VERSION_my_snprintf, &my_snprintf_handler },
  { "thd_alloc_service",   VERSION_thd_alloc,   &thd_alloc_handler },
  { "thd_wait_service",    VERSION_thd_wait,    &thd_wait_handler },
  { "my_thread_scheduler_service",
    VERSION_my_thread_scheduler, &my_thread_scheduler_handler },
  { "my_plugin_log_service", VERSION_my_plugin_log, &my_plugin_log_handler },
  { "mysql_string_service",
    VERSION_mysql_string, &mysql_string_handler },
  { "mysql_malloc_service", VERSION_mysql_malloc, &mysql_malloc_handler },
  { "mysql_password_policy_service", VERSION_mysql_password_policy, &mysql_password_policy_handler },
  { "mysql_parser_service", VERSION_parser, &parser_handler },
  { "rpl_transaction_ctx_service",
    VERSION_rpl_transaction_ctx_service, &rpl_transaction_ctx_handler },
  { "transaction_write_set_service",
    VERSION_transaction_write_set_service, &transaction_write_set_handler },
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  { "security_context_service",
    VERSION_security_context_service, &security_context_handler },
#endif
  { "mysql_locking_service", VERSION_locking_service, &locking_service_handler },
  { "mysql_keyring_service", VERSION_mysql_keyring_service, &mysql_keyring_handler}
};

