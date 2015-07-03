#ifndef AUTH_INTERNAL_INCLUDED
#define AUTH_INTERNAL_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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
/* Internals */

#include "my_global.h"                  /* NO_EMBEDDED_ACCESS_CHECKS */
#include "violite.h"                    /* SSL_type */

#include "auth_common.h"

class ACL_USER;
class ACL_PROXY_USER;
class GRANT_NAME;
class GRANT_TABLE;
class GRANT_COLUMN;
struct TABLE;

/* sql_authentication */
void optimize_plugin_compare_by_pointer(LEX_CSTRING *plugin_name);
bool auth_plugin_is_built_in(const char *plugin_name);
bool auth_plugin_supports_expiration(const char *plugin_name);


const ACL_internal_table_access *
get_cached_table_access(GRANT_INTERNAL_INFO *grant_internal_info,
                        const char *schema_name, const char *table_name);

/* sql_auth_cache */
ulong get_sort(uint count,...);


#ifndef NO_EMBEDDED_ACCESS_CHECKS

/*sql_authentication */
bool rsa_auth_status();

/* sql_auth_cache */
void rebuild_check_host(void);
ACL_USER * find_acl_user(const char *host,
                         const char *user,
                         my_bool exact);
ACL_PROXY_USER * acl_find_proxy_user(const char *user,
                                     const char *host,
                                     const char *ip,
                                     char *authenticated_as,
                                     bool *proxy_used);
bool set_user_salt(ACL_USER *acl_user);
void acl_insert_proxy_user(ACL_PROXY_USER *new_value);

void acl_update_user(const char *user, const char *host,
                     enum SSL_type ssl_type,
                     const char *ssl_cipher,
                     const char *x509_issuer,
                     const char *x509_subject,
                     USER_RESOURCES  *mqh,
                     ulong privileges,
                     const LEX_CSTRING &plugin,
                     const LEX_CSTRING &auth,
                     MYSQL_TIME password_change_time,
                     LEX_ALTER password_life,
                     ulong what_is_set);
void acl_insert_user(const char *user, const char *host,
                     enum SSL_type ssl_type,
                     const char *ssl_cipher,
                     const char *x509_issuer,
                     const char *x509_subject,
                     USER_RESOURCES *mqh,
                     ulong privileges,
                     const LEX_CSTRING &plugin,
                     const LEX_CSTRING &auth,
		     MYSQL_TIME password_change_time,
                     LEX_ALTER password_life);
void acl_update_proxy_user(ACL_PROXY_USER *new_value, bool is_revoke);
void acl_update_db(const char *user, const char *host, const char *db,
                   ulong privileges);
void acl_insert_db(const char *user, const char *host, const char *db,
                   ulong privileges);
bool update_sctx_cache(Security_context *sctx, ACL_USER *acl_user_ptr,
                       bool expired);

/* sql_user_table */
ulong get_access(TABLE *form,uint fieldnr, uint *next_field);
bool acl_end_trans_and_close_tables(THD *thd, bool rollback_transaction);
void acl_notify_htons(THD* thd, const char* query, size_t query_length);
int replace_db_table(TABLE *table, const char *db,
                     const LEX_USER &combo,
                     ulong rights, bool revoke_grant);
int replace_user_table(THD *thd, TABLE *table, LEX_USER *combo,
                       ulong rights, bool revoke_grant,
                       bool can_create_user, ulong what_to_replace);
int replace_proxies_priv_table(THD *thd, TABLE *table, const LEX_USER *user,
                               const LEX_USER *proxied_user,
                               bool with_grant_arg, bool revoke_grant);
int replace_column_table(GRANT_TABLE *g_t,
                         TABLE *table, const LEX_USER &combo,
                         List <LEX_COLUMN> &columns,
                         const char *db, const char *table_name,
                         ulong rights, bool revoke_grant);
int replace_table_table(THD *thd, GRANT_TABLE *grant_table,
                        TABLE *table, const LEX_USER &combo,
                        const char *db, const char *table_name,
                        ulong rights, ulong col_rights,
                        bool revoke_grant);
int replace_routine_table(THD *thd, GRANT_NAME *grant_name,
                          TABLE *table, const LEX_USER &combo,
                          const char *db, const char *routine_name,
                          bool is_proc, ulong rights, bool revoke_grant);
int open_grant_tables(THD *thd, TABLE_LIST *tables, bool *transactional_tables);
int handle_grant_table(TABLE_LIST *tables, uint table_no, bool drop,
                       LEX_USER *user_from, LEX_USER *user_to);

void acl_print_ha_error(TABLE *table, int handler_error);
/* sql_authorization */
bool is_privileged_user_for_credential_change(THD *thd);

#endif /* NO_EMBEDDED_ACCESS_CHECKS */

#endif /* AUTH_INTERNAL_INCLUDED */
