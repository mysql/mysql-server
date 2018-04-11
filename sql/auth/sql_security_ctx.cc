/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "sql/auth/sql_security_ctx.h"

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/psi/psi_base.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"
#include "sql/auth/auth_internal.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/auth/sql_authorization.h"
#include "sql/current_thd.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"

extern bool initialized;

void Security_context::init() {
  DBUG_ENTER("Security_context::init");

  m_user.set((const char *)0, 0, system_charset_info);
  m_host.set("", 0, system_charset_info);
  m_ip.set("", 0, system_charset_info);
  m_host_or_ip.set(STRING_WITH_LEN("connecting host"), system_charset_info);
  m_external_user.set("", 0, system_charset_info);
  m_priv_user[0] = m_priv_host[0] = m_proxy_user[0] = '\0';
  m_priv_user_length = m_priv_host_length = m_proxy_user_length = 0;
  m_master_access = 0;
  m_db_access = NO_ACCESS;
  m_acl_map = 0;
  m_map_checkout_count = 0;
  m_password_expired = false;
  m_is_locked = false;
  m_has_drop_policy = false;
  m_executed_drop_policy = false;
  DBUG_VOID_RETURN;
}

void Security_context::logout() {
  if (m_acl_map) {
    DBUG_PRINT("info",
               ("(logout) Security_context for %s@%s returns Acl_map to cache. "
                "Map reference count= %u",
                m_user.c_ptr(), m_host.c_ptr(), m_acl_map->reference_count()));
    get_global_acl_cache()->return_acl_map(m_acl_map);
    m_acl_map = 0;
    clear_active_roles();
  }
}

bool Security_context::has_drop_policy(void) { return m_has_drop_policy; }

void Security_context::execute_drop_policy(void) {
  if (m_has_drop_policy && !m_executed_drop_policy) {
    m_drop_policy(this);
    m_executed_drop_policy = true;
  }
}

void Security_context::set_drop_policy(
    const std::function<void(Security_context *)> &func) {
  m_drop_policy = func;
  m_has_drop_policy = true;
  m_executed_drop_policy = false;
}

void Security_context::destroy() {
  DBUG_ENTER("Security_context::destroy");
  if (m_has_drop_policy && !m_executed_drop_policy) {
    m_drop_policy(this);
  }
  if (m_acl_map) {
    DBUG_PRINT(
        "info",
        ("(destroy) Security_context for %s@%s returns Acl_map to cache. "
         "Map reference count= %u",
         m_user.c_ptr(), m_host.c_ptr(), m_acl_map->reference_count()));
    get_global_acl_cache()->return_acl_map(m_acl_map);
    clear_active_roles();
  }
  m_acl_map = 0;
  if (m_user.length()) m_user.set((const char *)0, 0, system_charset_info);

  if (m_host.length()) m_host.set("", 0, system_charset_info);

  if (m_ip.length()) m_ip.set("", 0, system_charset_info);

  if (m_host_or_ip.length()) m_host_or_ip.set("", 0, system_charset_info);

  if (m_external_user.length()) m_external_user.set("", 0, system_charset_info);

  m_priv_user[0] = m_priv_host[0] = m_proxy_user[0] = 0;
  m_priv_user_length = m_priv_host_length = m_proxy_user_length = 0;

  m_master_access = m_db_access = 0;
  m_password_expired = false;

  DBUG_VOID_RETURN;
}

void Security_context::skip_grants() {
  DBUG_ENTER("Security_context::skip_grants");

  /* privileges for the user are unknown everything is allowed */
  set_host_or_ip_ptr("", 0);
  assign_priv_user(C_STRING_WITH_LEN("skip-grants user"));
  assign_priv_host(C_STRING_WITH_LEN("skip-grants host"));
  m_master_access = ~NO_ACCESS;
  DBUG_VOID_RETURN;
}

/**
  Deep copy status of sctx object to this.

  @param[in]    src_sctx   Object from which status should be copied.
*/

void Security_context::copy_security_ctx(const Security_context &src_sctx) {
  DBUG_ENTER("Security_context::copy_security_ctx");

  assign_user(src_sctx.m_user.ptr(), src_sctx.m_user.length());
  assign_host(src_sctx.m_host.ptr(), src_sctx.m_host.length());
  assign_ip(src_sctx.m_ip.ptr(), src_sctx.m_ip.length());
  if (!strcmp(src_sctx.m_host_or_ip.ptr(), my_localhost))
    set_host_or_ip_ptr((char *)my_localhost, strlen(my_localhost));
  else
    set_host_or_ip_ptr();
  assign_external_user(src_sctx.m_external_user.ptr(),
                       src_sctx.m_external_user.length());
  assign_priv_user(src_sctx.m_priv_user, src_sctx.m_priv_user_length);
  assign_proxy_user(src_sctx.m_proxy_user, src_sctx.m_proxy_user_length);
  assign_priv_host(src_sctx.m_priv_host, src_sctx.m_priv_host_length);
  m_db_access = src_sctx.m_db_access;
  m_master_access = src_sctx.m_master_access;
  m_password_expired = src_sctx.m_password_expired;
  m_acl_map = 0;  // acl maps are reference counted we can't copy or share them!
  m_has_drop_policy = false;  // you cannot copy a drop policy
  m_executed_drop_policy = false;
  DBUG_VOID_RETURN;
}

/**
  Initialize this security context from the passed in credentials
  and activate it in the current thread.

  @param       thd
  @param       definer_user
  @param       definer_host
  @param       db
  @param[out]  backup  Save a pointer to the current security context
                       in the thread. In case of success it points to the
                       saved old context, otherwise it points to NULL.


  @note The Security_context_factory should be used as a replacement to this
    function at every opportunity.

  During execution of a statement, multiple security contexts may
  be needed:
  - the security context of the authenticated user, used as the
    default security context for all top-level statements
  - in case of a view or a stored program, possibly the security
    context of the definer of the routine, if the object is
    defined with SQL SECURITY DEFINER option.

  The currently "active" security context is parameterized in THD
  member security_ctx. By default, after a connection is
  established, this member points at the "main" security context
  - the credentials of the authenticated user.

  Later, if we would like to execute some sub-statement or a part
  of a statement under credentials of a different user, e.g.
  definer of a procedure, we authenticate this user in a local
  instance of Security_context by means of this method (and
  ultimately by means of acl_getroot), and make the
  local instance active in the thread by re-setting
  thd->m_security_ctx pointer.

  Note, that the life cycle and memory management of the "main" and
  temporary security contexts are different.
  For the main security context, the memory for user/host/ip is
  allocated on system heap, and the THD class frees this memory in
  its destructor. The only case when contents of the main security
  context may change during its life time is when someone issued
  CHANGE USER command.
  Memory management of a "temporary" security context is
  responsibility of the module that creates it.

  @retval true  there is no user with the given credentials. The erro
                is reported in the thread.
  @retval false success
*/

bool Security_context::change_security_context(THD *thd,
                                               const LEX_CSTRING &definer_user,
                                               const LEX_CSTRING &definer_host,
                                               LEX_STRING *db,
                                               Security_context **backup) {
  bool needs_change;

  DBUG_ENTER("Security_context::change_security_context");

  DBUG_ASSERT(definer_user.str && definer_host.str);

  *backup = NULL;
  needs_change =
      (strcmp(definer_user.str, thd->security_context()->priv_user().str) ||
       my_strcasecmp(system_charset_info, definer_host.str,
                     thd->security_context()->priv_host().str));
  if (needs_change) {
    if (acl_getroot(thd, this, const_cast<char *>(definer_user.str),
                    const_cast<char *>(definer_host.str),
                    const_cast<char *>(definer_host.str), db->str)) {
      my_error(ER_NO_SUCH_USER, MYF(0), definer_user.str, definer_host.str);
      DBUG_RETURN(true);
    }
    *backup = thd->security_context();
    thd->set_security_context(this);
  }

  DBUG_RETURN(false);
}

void Security_context::restore_security_context(THD *thd,
                                                Security_context *backup) {
  if (backup) thd->set_security_context(backup);
}

bool Security_context::user_matches(Security_context *them) {
  DBUG_ENTER("Security_context::user_matches");

  const char *them_user = them->user().str;

  DBUG_RETURN((m_user.ptr() != NULL) && (them_user != NULL) &&
              !strcmp(m_user.ptr(), them_user));
}

bool Security_context::check_access(ulong want_access, bool match_any) {
  DBUG_ENTER("Security_context::check_access");
  DBUG_RETURN((match_any ? (m_master_access & want_access)
                         : ((m_master_access & want_access) == want_access)));
}

/**
  This method pushes a role to the list of active roles. It requires
  Acl_cache_lock_guard.

  This method allocates memory which must be freed when the role is deactivated.

  @param role The role name
  @param role_host The role hostname-part.
  @param validate_access True if access validation should be performed.
    Default value is false.
*/
int Security_context::activate_role(LEX_CSTRING role, LEX_CSTRING role_host,
                                    bool validate_access) {
  auto res = std::find(m_active_roles.begin(), m_active_roles.end(),
                       create_authid_from(role, role_host));
  /* silently ignore requests of activating an already active role */
  if (res != m_active_roles.end()) return 0;
  LEX_CSTRING dup_role = {
      my_strdup(PSI_NOT_INSTRUMENTED, role.str, MYF(MY_WME)), role.length};
  LEX_CSTRING dup_role_host = {
      my_strdup(PSI_NOT_INSTRUMENTED, role_host.str, MYF(MY_WME)),
      role_host.length};
  if (validate_access && !check_if_granted_role(priv_user(), priv_host(),
                                                dup_role, dup_role_host)) {
    my_free(const_cast<char *>(dup_role.str));
    my_free(const_cast<char *>(dup_role_host.str));
    return ER_ACCESS_DENIED_ERROR;
  }
  m_active_roles.push_back(std::make_pair(dup_role, dup_role_host));
  return 0;
}

/**
  Subscribes to a cache entry of aggregated ACLs.
  A Security_context can only have one subscription at a time. If another one
  is requested, the former will be returned.
*/
void Security_context::checkout_access_maps(void) {
  DBUG_ENTER("Security_context::checkout_access_maps");

  /*
    If we're checkout out a map before we return it now, because we're only
    allowed to have one map at a time.
    However, if we've just authenticated we don't need to checkout a new map
    so we check if there has been any previous checkouts.
  */
  if (m_acl_map != 0) {
    DBUG_PRINT(
        "info",
        ("(checkout) Security_context for %s@%s returns Acl_map to cache. "
         "Map reference count= %u",
         m_user.c_ptr(), m_host.c_ptr(), m_acl_map->reference_count()));
    get_global_acl_cache()->return_acl_map(m_acl_map);
    m_acl_map = 0;
  }

  if (m_active_roles.size() == 0) DBUG_VOID_RETURN;
  ++m_map_checkout_count;
  Auth_id_ref uid;
  uid.first.str = this->m_user.ptr();
  uid.first.length = this->m_user.length();
  uid.second.str = this->m_host_or_ip.ptr();
  uid.second.length = this->m_host_or_ip.length();
  m_acl_map =
      get_global_acl_cache()->checkout_acl_map(this, uid, m_active_roles);
  if (m_acl_map != 0) {
    DBUG_PRINT("info", ("Roles are active and global access for %s@%s is set to"
                        " %lu",
                        user().str, host_or_ip().str, m_acl_map->global_acl()));
    set_master_access(m_acl_map->global_acl());
  } else {
    set_master_access(0);
  }
  DBUG_VOID_RETURN;
}

/**
  This helper method clears the active roles list and frees the allocated
  memory used for any previously activated roles.
*/
void Security_context::clear_active_roles(void) {
  for (List_of_auth_id_refs::iterator it = m_active_roles.begin();
       it != m_active_roles.end(); ++it) {
    my_free(const_cast<char *>(it->first.str));
    it->first.str = 0;
    it->first.length = 0;
    my_free(const_cast<char *>(it->second.str));
    it->second.str = 0;
    it->second.length = 0;
  }
  m_active_roles.clear();
  /*
    Clear does not actually free the memory as an optimization for reuse.
    This confuses valgrind, so we swap with an empty vector to ensure the
    memory is freed when testing with valgrind
  */
  List_of_auth_id_refs().swap(m_active_roles);
}

List_of_auth_id_refs *Security_context::get_active_roles(void) {
  return &m_active_roles;
}

ulong Security_context::db_acl(LEX_CSTRING db, bool use_pattern_scan) {
  DBUG_ENTER("Security_context::db_acl");
  if (m_acl_map == 0 || db.length == 0) DBUG_RETURN(0);

  Db_access_map::iterator it;
  std::string key(db.str, db.length);
  it = m_acl_map->db_acls()->find(key);
  if (it == m_acl_map->db_acls()->end()) {
    if (use_pattern_scan) {
      Db_access_map::iterator it = m_acl_map->db_wild_acls()->begin();
      ulong access = 0;
      for (; it != m_acl_map->db_wild_acls()->end(); ++it) {
        if (wild_case_compare(system_charset_info, db.str, db.length,
                              it->first.c_str(), it->first.size()) == 0) {
          DBUG_PRINT("info", ("Found matching db pattern %s for key %s",
                              it->first.c_str(), key.c_str()));
          access |= it->second;
        }
      }
      DBUG_RETURN(access);
    } else {
      DBUG_PRINT("info", ("Db %s not found in cache (no pattern matching)",
                          key.c_str()));
      DBUG_RETURN(0);
    }
  } else {
    DBUG_PRINT("info", ("Found exact match for db %s", key.c_str()));
    DBUG_RETURN(it->second);
  }
}

ulong Security_context::procedure_acl(LEX_CSTRING db,
                                      LEX_CSTRING procedure_name) {
  if (m_acl_map == 0)
    return 0;
  else {
    SP_access_map::iterator it;
    String q_name;
    append_identifier(&q_name, db.str, db.length);
    q_name.append(".");
    append_identifier(&q_name, procedure_name.str, procedure_name.length);
    it = m_acl_map->sp_acls()->find(q_name.c_ptr());
    if (it == m_acl_map->sp_acls()->end()) return 0;
    return it->second;
  }
}

ulong Security_context::function_acl(LEX_CSTRING db, LEX_CSTRING func_name) {
  if (m_acl_map == 0)
    return 0;
  else {
    String q_name;
    append_identifier(&q_name, db.str, db.length);
    q_name.append(".");
    append_identifier(&q_name, func_name.str, func_name.length);
    SP_access_map::iterator it;
    it = m_acl_map->func_acls()->find(q_name.c_ptr());
    if (it == m_acl_map->func_acls()->end()) return 0;
    return it->second;
  }
}

// return the entire element instead of just the acl?
Grant_table_aggregate Security_context::table_and_column_acls(
    LEX_CSTRING db, LEX_CSTRING table) {
  if (m_acl_map == 0) return Grant_table_aggregate();
  Table_access_map::iterator it;
  String q_name;
  append_identifier(&q_name, db.str, db.length);
  q_name.append(".");
  append_identifier(&q_name, table.str, table.length);
  it = m_acl_map->table_acls()->find(std::string(q_name.c_ptr_quick()));
  if (it == m_acl_map->table_acls()->end()) return Grant_table_aggregate();
  return it->second;
}

ulong Security_context::table_acl(LEX_CSTRING db, LEX_CSTRING table) {
  if (m_acl_map == 0) return 0;
  Grant_table_aggregate aggr = table_and_column_acls(db, table);
  return aggr.table_access;
}

bool Security_context::has_with_admin_acl(const LEX_CSTRING &role_name,
                                          const LEX_CSTRING &role_host) {
  DBUG_ENTER("Security_context::has_with_admin_acl");
  if (m_acl_map == 0) DBUG_RETURN(false);
  String q_name;
  append_identifier(&q_name, role_name.str, role_name.length);
  q_name.append("@");
  append_identifier(&q_name, role_host.str, role_host.length);
  Grant_acl_set::iterator it =
      m_acl_map->grant_acls()->find(std::string(q_name.c_ptr_quick()));
  if (it != m_acl_map->grant_acls()->end()) DBUG_RETURN(true);
  DBUG_RETURN(false);
}

bool Security_context::any_sp_acl(const LEX_CSTRING &db) {
  if ((db_acl(db, true) & PROC_ACLS) != 0) return true;
  SP_access_map::iterator it = m_acl_map->sp_acls()->begin();
  for (; it != m_acl_map->sp_acls()->end(); ++it) {
    String id_db;
    append_identifier(&id_db, db.str, db.length);
    if (it->first.compare(0, id_db.length(), id_db.c_ptr(), id_db.length()) ==
        0) {
      /* There's at least one SP with grants for this db */
      return true;
    }
  }
  return false;
}

bool Security_context::any_table_acl(const LEX_CSTRING &db) {
  if ((db_acl(db, true) & TABLE_ACLS) != 0) return true;
  Table_access_map::iterator table_it = m_acl_map->table_acls()->begin();
  for (; table_it != m_acl_map->table_acls()->end(); ++table_it) {
    String id_db;
    append_identifier(&id_db, db.str, db.length);
    if (table_it->first.compare(0, id_db.length(), id_db.c_ptr(),
                                id_db.length()) == 0) {
      /* There's at least one table with grants for this db*/
      return true;
    }
  }
  return false;
}

std::pair<bool, bool> Security_context::has_global_grant(const char *priv,
                                                         size_t priv_len) {
  /* server started with --skip-grant-tables */
  if (!initialized) return std::make_pair(true, true);
  std::string privilege(priv, priv_len);
  if (m_acl_map == 0) {
    Acl_cache_lock_guard acl_cache_lock(current_thd,
                                        Acl_cache_lock_mode::READ_MODE);
    if (!acl_cache_lock.lock(false)) return std::make_pair(false, false);
    Role_id key(&m_priv_user[0], m_priv_user_length, &m_priv_host[0],
                m_priv_host_length);
    User_to_dynamic_privileges_map::iterator it, it_end;
    std::tie(it, it_end) = get_dynamic_privileges_map()->equal_range(key);
    it = std::find(it, it_end, privilege);
    if (it != it_end) {
      return std::make_pair(true, it->second.second);
    }
    return std::make_pair(false, false);
  }
  Dynamic_privileges::iterator it =
      m_acl_map->dynamic_privileges()->find(privilege);
  if (it != m_acl_map->dynamic_privileges()->end()) {
    return std::make_pair(true, it->second);
  }

  return std::make_pair(false, false);
}

LEX_CSTRING Security_context::priv_user() const {
  LEX_CSTRING priv_user;
  DBUG_ENTER("Security_context::priv_user");
  priv_user.str = m_priv_user;
  priv_user.length = m_priv_user_length;
  DBUG_RETURN(priv_user);
}
