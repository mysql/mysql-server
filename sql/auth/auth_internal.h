/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef AUTH_INTERNAL_INCLUDED
#define AUTH_INTERNAL_INCLUDED

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "auth_common.h"
#include "mysql_time.h"                 /* MYSQL_TIME */
#include "partitioned_rwlock.h"
#include "table.h"
#include "violite.h"                    /* SSL_type */
#include "dynamic_privilege_table.h"



class ACL_USER;
class ACL_PROXY_USER;
class GRANT_NAME;
class GRANT_TABLE;
class GRANT_COLUMN;
struct TABLE;
typedef struct user_resources USER_RESOURCES;
void append_identifier(THD *thd, String *packet, const char *name,
                       size_t length);
typedef std::map<std::string, unsigned long > Column_map;
struct Grant_table_aggregate
{
  Grant_table_aggregate() : table_access(0l), cols(0l){}
  ulong table_access;
  ulong cols;
  Column_map columns;
};
typedef std::map<std::string, unsigned long> SP_access_map;
typedef std::map<std::string, unsigned long > Db_access_map;
typedef std::map<std::string, Grant_table_aggregate>
  Table_access_map_storage;
class Table_access_map
{
public:
  Table_access_map() : m_thd(0) {}
  ~Table_access_map() {}
  typedef Table_access_map_storage::iterator iterator;
  typedef Table_access_map_storage::value_type value_type;
  typedef Table_access_map_storage::mapped_type mapped_type;
  mapped_type &operator[](const Table_access_map_storage::key_type &key)
  { return m_values[key]; }
  iterator begin() { return m_values.begin(); }
  iterator end() { return m_values.end(); }
  iterator find(const Table_access_map_storage::key_type &key)
  { return m_values.find(key); }
  void set_thd(THD* thd) { m_thd= thd; }
  THD *get_thd() { return m_thd; }
private:
  THD *m_thd;
  Table_access_map_storage m_values;
};
typedef std::unordered_set<std::string> Grant_acl_set;

std::string create_authid_str_from(const LEX_USER *user);
std::string create_authid_str_from(const ACL_USER *user);
std::string create_authid_str_from(const LEX_CSTRING &user,
                                   const LEX_CSTRING &host);
std::string create_authid_str_from(const Auth_id_ref &user);
Auth_id_ref create_authid_from(const LEX_USER *user);
Auth_id_ref create_authid_from(const ACL_USER *user);

/* sql_authentication */
void optimize_plugin_compare_by_pointer(LEX_CSTRING *plugin_name);
bool auth_plugin_is_built_in(const char *plugin_name);
bool auth_plugin_supports_expiration(const char *plugin_name);


const ACL_internal_table_access *
get_cached_table_access(GRANT_INTERNAL_INFO *grant_internal_info,
                        const char *schema_name, const char *table_name);

/* sql_auth_cache */
ulong get_sort(uint count,...);
bool assert_acl_cache_read_lock(THD *thd);
bool assert_acl_cache_write_lock(THD *thd);

/*sql_authentication */
bool rsa_auth_status();

/* sql_auth_cache */
void rebuild_check_host(void);
ACL_USER * find_acl_user(const char *host,
                         const char *user,
                         bool exact);
ACL_PROXY_USER * acl_find_proxy_user(const char *user,
                                     const char *host,
                                     const char *ip,
                                     char *authenticated_as,
                                     bool *proxy_used);
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
void clear_and_init_db_cache();

/* sql_user_table */
ulong get_access(TABLE *form,uint fieldnr, uint *next_field);
int replace_db_table(THD *thd, TABLE *table, const char *db,
                     const LEX_USER &combo,
                     ulong rights, bool revoke_grant);
int replace_user_table(THD *thd, TABLE *table, LEX_USER *combo,
                       ulong rights, bool revoke_grant,
                       bool can_create_user, ulong what_to_replace);
int replace_proxies_priv_table(THD *thd, TABLE *table, const LEX_USER *user,
                               const LEX_USER *proxied_user,
                               bool with_grant_arg, bool revoke_grant);
int replace_column_table(THD *thd, GRANT_TABLE *g_t,
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
int replace_roles_priv_table(THD *thd, TABLE *table, const LEX_USER *user,
                             const LEX_USER *role,
                             bool with_grant_arg,
                             bool revoke_grant);

void acl_print_ha_error(int handler_error);
bool check_acl_tables(TABLE_LIST *tables, bool report_error);
bool log_and_commit_acl_ddl(THD *thd,
                            bool transactional_tables,
                            std::set<LEX_USER *> *extra_users= NULL,
                            bool extra_error= false,
                            bool log_to_binlog= true,
                            bool notify_htons= true);
/* sql_authorization */
bool is_privileged_user_for_credential_change(THD *thd);
void rebuild_vertex_index(THD *thd);
void roles_init_graph(void);
void roles_delete_graph(void);
/**
  Storage container for default role ids. Default roles are only weakly
  depending on ACL_USERs. You can retain a default role even if the
  corresponding ACL_USER is missing in the acl_cache.
*/
class Role_id
{
public:
  Role_id(const char *user, int user_len, const char *host,
          int host_len)
  {
    m_user.append(user, user_len);
    m_host.append(host, host_len);
    append_identifier(&m_auth_str, user, user_len);
    m_auth_str.append('@');
    append_identifier(&m_auth_str, host, host_len);
  }

  Role_id(const Auth_id_ref &id)
  {
    m_user.append(id.first.str, id.first.length);
    m_host.append(id.second.str, id.second.length);
    append_identifier(&m_auth_str, id.first.str, id.first.length);
    m_auth_str.append('@');
    append_identifier(&m_auth_str, id.second.str, id.second.length);
  }

  Role_id(const LEX_CSTRING &user, const LEX_CSTRING &host)
  {
    m_user.append(user.str, user.length);
    m_host.append(host.str, host.length);
    append_identifier(&m_auth_str, user.str, user.length);
    m_auth_str.append('@');
    append_identifier(&m_auth_str, host.str, host.length);
  }

  Role_id(const std::string &user, const std::string &host)
  : m_user(user), m_host(host)
  {
    append_identifier(&m_auth_str, user.c_str(), user.length());
    m_auth_str.append('@');
    append_identifier(&m_auth_str, host.c_str(), host.length());
  }

  ~Role_id() {}

  Role_id(const Role_id &id)
  {
    m_user= id.m_user;
    m_host= id.m_host;
    m_auth_str.copy(id.m_auth_str);
  }

  bool operator<(const Role_id &id) const
  {
    std::string tmp,tmp2;
    tmp.append(m_user);
    tmp.append(m_host);
    tmp2.append(id.m_user);
    tmp2.append(id.m_host);
    return (tmp < tmp2);
  }

  void auth_str(std::string *out) const
  {
    out->append(m_auth_str.ptr());
  }

  const std::string &user() const { return m_user; }
  const std::string &host() const { return m_host; }
private:
  std::string m_user;
  std::string m_host;
  String m_auth_str;
};

void dynamic_privileges_init(void);
void dynamic_privileges_delete(void);
bool grant_dynamic_privilege(const LEX_CSTRING &str_priv,
                             const LEX_CSTRING &str_user,
                             const LEX_CSTRING &str_host,
                             bool with_grant_option,
                             Update_dynamic_privilege_table &func);
bool revoke_dynamic_privilege(const LEX_CSTRING &str_priv,
                              const LEX_CSTRING &str_user,
                              const LEX_CSTRING &str_host,
                              Update_dynamic_privilege_table &update_table);
bool revoke_all_dynamic_privileges(const LEX_CSTRING &user,
                                   const LEX_CSTRING &host,
                                   Update_dynamic_privilege_table &func);
bool rename_dynamic_grant(const LEX_CSTRING &old_user,
                          const LEX_CSTRING &old_host,
                          const LEX_CSTRING &new_user,
                          const LEX_CSTRING &new_host,
                          Update_dynamic_privilege_table &update_table);
bool operator==(const Role_id &a, const Auth_id_ref &b);
bool operator==(const Auth_id_ref &a, const Role_id &b);
bool operator==(const std::pair<const Role_id, const Role_id> &a,
                const Auth_id_ref &b);
bool operator==(const Role_id &a, const Role_id &b);
bool operator==(std::pair<const Role_id, std::pair<std::string, bool> > &a,
                const std::string &b);
typedef std::vector<std::pair<Role_id, bool> > List_of_granted_roles;

struct role_id_hash
{
  std::size_t operator()(const Role_id& k) const
  {
    using std::size_t;
    using std::hash;
    using std::string;
    return ((hash<string>()(k.user()) ^ (hash<string>()(k.host()) << 1)) >> 1);
  }
};

typedef std::unordered_multimap<const Role_id, const Role_id, role_id_hash>
  Default_roles;
typedef std::map<std::string, bool> Dynamic_privileges;

void get_privilege_access_maps(ACL_USER *acl_user,
                               const List_of_auth_id_refs *using_roles,
                               ulong *access,
                               Db_access_map *db_map,
                               Db_access_map *db_wild_map,
                               Table_access_map *table_map,
                               SP_access_map *sp_map,
                               SP_access_map *func_map,
                               List_of_granted_roles *granted_roles,
                               Grant_acl_set *with_admin_acl,
                               Dynamic_privileges *dynamic_acl);
bool clear_default_roles(THD *thd, TABLE *table,
                         const Auth_id_ref &user_auth_id,
                         std::vector<Role_id > *default_roles);
void get_granted_roles(THD *thd, LEX_USER *user,
                       List_of_granted_roles *granted_roles);
void revoke_role(THD *thd, ACL_USER *role, ACL_USER *user);
bool revoke_all_roles_from_user(THD *thd, TABLE *edge_table,
                                TABLE *defaults_table, LEX_USER *user);
bool drop_role(THD *thd, TABLE *edge_table, TABLE *defaults_table,
               const Auth_id_ref &authid_user);
bool modify_role_edges_in_table(THD *thd, TABLE *table,
                                const Auth_id_ref &from_user,
                                const Auth_id_ref &to_user,
                                bool with_admin_option,
                                bool delete_option);
Auth_id_ref create_authid_from(const Role_id &user);
bool roles_rename_authid(THD *thd, TABLE *edge_table, TABLE *defaults_table,
                         LEX_USER *user_from, LEX_USER *user_to);
bool set_and_validate_user_attributes(THD *thd,
                                      LEX_USER *Str,
                                      ulong &what_to_set,
                                      bool is_privileged_user,
                                      bool is_role);
typedef std::pair<std::string, bool> Grant_privilege;
typedef std::unordered_multimap<const Role_id, Grant_privilege,
                                role_id_hash >
  User_to_dynamic_privileges_map;
User_to_dynamic_privileges_map *get_dynamic_privileges_map();
User_to_dynamic_privileges_map *
swap_dynamic_privileges_map(User_to_dynamic_privileges_map *map);
bool populate_roles_caches(THD *thd, TABLE_LIST * tablelst);
#endif /* AUTH_INTERNAL_INCLUDED */
