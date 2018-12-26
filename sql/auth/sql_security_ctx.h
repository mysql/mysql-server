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
#ifndef SQL_SECURITY_CTX_INCLUDED
#define SQL_SECURITY_CTX_INCLUDED
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <utility>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_dbug.h"
#include "mysql_com.h"
#include "sql/auth/auth_common.h"
#include "sql/sql_const.h"
#include "sql_string.h"

/* Forward declaration. Depends on sql_auth_cache.h (which depends on this file)
 */
class Acl_map;
class THD;
struct Grant_table_aggregate;

/**
  @class Security_context
  @brief A set of THD members describing the current authenticated user.
*/

class Security_context {
 public:
  Security_context() { init(); }
  ~Security_context() { destroy(); }

  Security_context(const Security_context &src_sctx) {
    copy_security_ctx(src_sctx);
  }

  Security_context &operator=(const Security_context &src_sctx) {
    DBUG_ENTER("Security_context::operator =");

    if (this != &src_sctx) {
      destroy();
      copy_security_ctx(src_sctx);
    }

    DBUG_RETURN(*this);
  }

  void skip_grants();

  /**
    Getter method for member m_user.

    @retval LEX_CSTRING object having constant pointer to m_user.Ptr
    and its length.
  */

  LEX_CSTRING user() const {
    LEX_CSTRING user;

    DBUG_ENTER("Security_context::user");

    user.str = m_user.ptr();
    user.length = m_user.length();

    DBUG_RETURN(user);
  }

  inline void set_user_ptr(const char *user_arg, const size_t user_arg_length);

  inline void assign_user(const char *user_arg, const size_t user_arg_length);

  std::pair<bool, bool> has_global_grant(const char *priv, size_t priv_len);
  int activate_role(LEX_CSTRING user, LEX_CSTRING host,
                    bool validate_access = false);
  void clear_active_roles(void);
  List_of_auth_id_refs *get_active_roles();
  void checkout_access_maps(void);
  ulong db_acl(LEX_CSTRING db, bool use_pattern_scan = true);
  ulong procedure_acl(LEX_CSTRING db, LEX_CSTRING procedure_name);
  ulong function_acl(LEX_CSTRING db, LEX_CSTRING procedure_name);
  ulong table_acl(LEX_CSTRING db, LEX_CSTRING table);
  Grant_table_aggregate table_and_column_acls(LEX_CSTRING db,
                                              LEX_CSTRING table);
  bool has_with_admin_acl(const LEX_CSTRING &role_name,
                          const LEX_CSTRING &role_host);
  bool any_sp_acl(const LEX_CSTRING &db);
  bool any_table_acl(const LEX_CSTRING &db);

  /**
    Getter method for member m_host.

    @retval LEX_CSTRING object having constant pointer to m_host.Ptr
    and its length.
  */

  LEX_CSTRING host() const {
    LEX_CSTRING host;

    DBUG_ENTER("Security_context::host");

    host.str = m_host.ptr();
    host.length = m_host.length();

    DBUG_RETURN(host);
  }

  inline void set_host_ptr(const char *host_arg, const size_t host_arg_length);

  inline void assign_host(const char *host_arg, const size_t host_arg_length);

  /**
    Getter method for member m_ip.

    @retval LEX_CSTRING object having constant pointer to m_ip.Ptr
    and its length
  */

  LEX_CSTRING ip() const {
    LEX_CSTRING ip;

    DBUG_ENTER("Security_context::ip");

    ip.str = m_ip.ptr();
    ip.length = m_ip.length();

    DBUG_RETURN(ip);
  }

  inline void set_ip_ptr(const char *ip_arg, const int ip_arg_length);

  inline void assign_ip(const char *ip_arg, const int ip_arg_length);

  /**
    Getter method for member m_host_or_ip.

    @retval LEX_CSTRING object having constant pointer to m_host_or_ip.Ptr
    and its length
  */

  LEX_CSTRING host_or_ip() const {
    LEX_CSTRING host_or_ip;

    DBUG_ENTER("Security_context::host_or_ip");

    host_or_ip.str = m_host_or_ip.ptr();
    host_or_ip.length = m_host_or_ip.length();

    DBUG_RETURN(host_or_ip);
  }

  /**
    Setter method for member m_host_or_ip.
  */

  void set_host_or_ip_ptr() {
    DBUG_ENTER("Security_context::set_host_or_ip_ptr");

    /*
      Set host_or_ip to either host or ip if they are available else set it to
      empty string.
    */
    const char *host_or_ip =
        m_host.length() ? m_host.ptr() : (m_ip.length() ? m_ip.ptr() : "");

    m_host_or_ip.set(host_or_ip, strlen(host_or_ip), system_charset_info);

    DBUG_VOID_RETURN;
  }

  /**
    Setter method for member m_host_or_ip.

    @param[in]    host_or_ip_arg         New user value for m_host_or_ip.
    @param[in]    host_or_ip_arg_length  Length of "host_or_ip_arg" param.
  */

  void set_host_or_ip_ptr(const char *host_or_ip_arg,
                          const int host_or_ip_arg_length) {
    DBUG_ENTER("Security_context::set_host_or_ip_ptr");

    m_host_or_ip.set(host_or_ip_arg, host_or_ip_arg_length,
                     system_charset_info);

    DBUG_VOID_RETURN;
  }

  /**
    Getter method for member m_external_user.

    @retval LEX_CSTRING object having constant pointer to m_external_host.Ptr
    and its length
  */

  LEX_CSTRING external_user() const {
    LEX_CSTRING ext_user;

    DBUG_ENTER("Security_context::external_user");

    ext_user.str = m_external_user.ptr();
    ext_user.length = m_external_user.length();

    DBUG_RETURN(ext_user);
  }

  inline void set_external_user_ptr(const char *ext_user_arg,
                                    const int ext_user_arg_length);

  inline void assign_external_user(const char *ext_user_arg,
                                   const int ext_user_arg_length);

  /**
    Getter method for member m_priv_user.

    @retval LEX_CSTRING object having constant pointer to m_priv_user.Ptr
    and its length
  */

  LEX_CSTRING priv_user() const;

  inline void assign_priv_user(const char *priv_user_arg,
                               const size_t priv_user_arg_length);

  /**
    Getter method for member m_proxy_user.

    @retval LEX_CSTRING object having constant pointer to m_proxy_user.Ptr
    and its length
  */

  LEX_CSTRING proxy_user() const {
    LEX_CSTRING proxy_user;

    DBUG_ENTER("Security_context::proxy_user");

    proxy_user.str = m_proxy_user;
    proxy_user.length = m_proxy_user_length;

    DBUG_RETURN(proxy_user);
  }

  inline void assign_proxy_user(const char *proxy_user_arg,
                                const size_t proxy_user_arg_length);

  /**
    Getter method for member m_priv_host.

    @retval LEX_CSTRING object having constant pointer to m_priv_host.Ptr
    and its length
  */

  LEX_CSTRING priv_host() const {
    LEX_CSTRING priv_host;

    DBUG_ENTER("Security_context::priv_host");

    priv_host.str = m_priv_host;
    priv_host.length = m_priv_host_length;

    DBUG_RETURN(priv_host);
  }

  inline void assign_priv_host(const char *priv_host_arg,
                               const size_t priv_host_arg_length);

  const char *priv_host_name() const {
    return (*m_priv_host ? m_priv_host : (char *)"%");
  }

  /**
    Getter method for member m_master_access.
  */

  ulong master_access() const { return m_master_access; }

  void set_master_access(ulong master_access) {
    DBUG_ENTER("set_master_access");
    m_master_access = master_access;
    DBUG_PRINT("info", ("Cached master access is %lu", m_master_access));
    DBUG_VOID_RETURN;
  }

  /**
    Check if a an account has been assigned to the security context

    The account assigment to the security context is always executed in the
    following order:
    1) assign user's name to the context
    2) assign user's hostname to the context
    Whilst user name can be null, hostname cannot. This is why we can say that
    the full account has been assigned to the context when hostname is not
    equal to empty string.

    @return Account assignment status
      @retval true account has been assigned to the security context
      @retval false account has not yet been assigned to the security context
  */

  bool has_account_assigned() const { return m_priv_host[0] != '\0'; }

  /**
    Check permission against m_master_access
  */

  /**
    Check global access
    @param want_access The required privileges
    @param match_any if the security context must match all or any of the req.
   *                 privileges.
    @return True if the security context fulfills the access requirements.
  */
  bool check_access(ulong want_access, bool match_any = false);

  /**
   Returns the schema level effective privileges (with applied roles)
   for the currently active schema.
  */
  ulong current_db_access() const { return m_db_access; }

  /**
    Cache the schema level effective privileges (apply roles first!) for the
    currently active schema.
    @param db_access
  */
  void cache_current_db_access(ulong db_access) { m_db_access = db_access; }

  /**
    Getter method for member m_password_expired.
  */

  bool password_expired() const { return m_password_expired; }

  void set_password_expired(bool password_expired) {
    m_password_expired = password_expired;
  }

  bool change_security_context(THD *thd, const LEX_CSTRING &definer_user,
                               const LEX_CSTRING &definer_host, LEX_STRING *db,
                               Security_context **backup);

  void restore_security_context(THD *thd, Security_context *backup);

  bool user_matches(Security_context *);

  void logout();
  void init();
  /**
    Locked account can still be used as routine definers and when they are
    there shouldn't be any checks for expired passwords.
  */
  bool account_is_locked() { return m_is_locked; }

  void lock_account(bool is_locked) { m_is_locked = is_locked; }

  void set_drop_policy(const std::function<void(Security_context *)> &func);

  bool has_drop_policy(void);

  bool has_executed_drop_policy(void);

  void execute_drop_policy(void);

 private:
  void destroy();
  void copy_security_ctx(const Security_context &src_sctx);

 private:
  /**
    m_user - user of the client, set to NULL until the user has been read from
             the connection
  */
  String m_user;

  /** m_host - host of the client */
  String m_host;

  /** m_ip - client IP */
  String m_ip;

  /**
    m_host_or_ip - points to host if host is available, otherwise points to ip
  */
  String m_host_or_ip;

  String m_external_user;

  /**
    m_priv_user - The user privilege we are using. May be "" for anonymous user.
  */
  char m_priv_user[USERNAME_LENGTH];
  size_t m_priv_user_length;

  char m_proxy_user[USERNAME_LENGTH + MAX_HOSTNAME + 5];
  size_t m_proxy_user_length;

  /**
    The host privilege we are using
  */
  char m_priv_host[MAX_HOSTNAME];
  size_t m_priv_host_length;

  /**
    Global privileges from mysql.user.
  */
  ulong m_master_access;

  /**
    Privileges for current db
  */
  ulong m_db_access;

  /**
    password expiration flag.

    This flag is set according to connecting user's context and not the
    effective user.
  */
  bool m_password_expired;
  List_of_auth_id_refs m_active_roles;
  Acl_map *m_acl_map;
  int m_map_checkout_count;
  /**
    True if this account can't be logged into.
  */
  bool m_is_locked;

  bool m_executed_drop_policy;
  bool m_has_drop_policy;
  std::unique_ptr<std::function<void(Security_context *)>> m_drop_policy;
};

/**
  Setter method for member m_user.
  Function just sets the user_arg pointer value to the
  m_user, user_arg value is *not* copied.

  @param[in]    user_arg         New user value for m_user.
  @param[in]    user_arg_length  Length of "user_arg" param.
*/

void Security_context::set_user_ptr(const char *user_arg,
                                    const size_t user_arg_length) {
  DBUG_ENTER("Security_context::set_user_ptr");

  if (user_arg == m_user.ptr()) DBUG_VOID_RETURN;

  // set new user value to m_user.
  m_user.set(user_arg, user_arg_length, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_user.

  Copies user_arg value to the m_user if it is not null else m_user is set
  to NULL.

  @param[in]    user_arg         New user value for m_user.
  @param[in]    user_arg_length  Length of "user_arg" param.
*/

void Security_context::assign_user(const char *user_arg,
                                   const size_t user_arg_length) {
  DBUG_ENTER("Security_context::assign_user");

  if (user_arg == m_user.ptr()) DBUG_VOID_RETURN;

  if (user_arg)
    m_user.copy(user_arg, user_arg_length, system_charset_info);
  else
    m_user.set((const char *)0, 0, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_host.
  Function just sets the host_arg pointer value to the
  m_host, host_arg value is *not* copied.
  host_arg value must not be NULL.

  @param[in]    host_arg         New user value for m_host.
  @param[in]    host_arg_length  Length of "host_arg" param.
*/

void Security_context::set_host_ptr(const char *host_arg,
                                    const size_t host_arg_length) {
  DBUG_ENTER("Security_context::set_host_ptr");

  DBUG_ASSERT(host_arg != nullptr);

  if (host_arg == m_host.ptr()) DBUG_VOID_RETURN;

  // set new host value to m_host.
  m_host.set(host_arg, host_arg_length, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_host.

  Copies host_arg value to the m_host if it is not null else m_user is set
  to empty string.


  @param[in]    host_arg         New user value for m_host.
  @param[in]    host_arg_length  Length of "host_arg" param.
*/

void Security_context::assign_host(const char *host_arg,
                                   const size_t host_arg_length) {
  DBUG_ENTER("Security_context::assign_host");

  if (host_arg == nullptr) {
    m_host.set("", 0, system_charset_info);
    goto end;
  } else if (host_arg == m_host.ptr()) {
    goto end;
  } else if (*host_arg) {
    m_host.copy(host_arg, host_arg_length, system_charset_info);
    goto end;
  }

end:
  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_ip.
  Function just sets the ip_arg pointer value to the
  m_ip, ip_arg value is *not* copied.

  @param[in]    ip_arg         New user value for m_ip.
  @param[in]    ip_arg_length  Length of "ip_arg" param.
*/

void Security_context::set_ip_ptr(const char *ip_arg, const int ip_arg_length) {
  DBUG_ENTER("Security_context::set_ip_ptr");

  if (ip_arg == m_ip.ptr()) DBUG_VOID_RETURN;

  // set new ip value to m_ip.
  m_ip.set(ip_arg, ip_arg_length, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_ip.

  Copies ip_arg value to the m_ip if it is not null else m_ip is set
  to NULL.


  @param[in]    ip_arg         New user value for m_ip.
  @param[in]    ip_arg_length  Length of "ip_arg" param.
*/

void Security_context::assign_ip(const char *ip_arg, const int ip_arg_length) {
  DBUG_ENTER("Security_context::assign_ip");

  if (ip_arg == m_ip.ptr()) DBUG_VOID_RETURN;

  if (ip_arg)
    m_ip.copy(ip_arg, ip_arg_length, system_charset_info);
  else
    m_ip.set((const char *)0, 0, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_external_user.
  Function just sets the ext_user_arg pointer to the
  m_external_user, ext_user_arg is *not* copied.

  @param[in]    ext_user_arg         New user value for m_external_user.
  @param[in]    ext_user_arg_length  Length of "ext_user_arg" param.
*/

void Security_context::set_external_user_ptr(const char *ext_user_arg,
                                             const int ext_user_arg_length) {
  DBUG_ENTER("Security_context::set_external_user_ptr");

  if (ext_user_arg == m_external_user.ptr()) DBUG_VOID_RETURN;

  // set new ip value to m_ip.
  m_external_user.set(ext_user_arg, ext_user_arg_length, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_external_user.

  Copies ext_user_arg value to the m_external_user if it is not null
  else m_external_user is set to NULL.

  @param[in]    ext_user_arg         New user value for m_external_user.
  @param[in]    ext_user_arg_length  Length of "ext_user_arg" param.
*/

void Security_context::assign_external_user(const char *ext_user_arg,
                                            const int ext_user_arg_length) {
  DBUG_ENTER("Security_context::assign_external_user");

  if (ext_user_arg == m_external_user.ptr()) DBUG_VOID_RETURN;

  if (ext_user_arg)
    m_external_user.copy(ext_user_arg, ext_user_arg_length,
                         system_charset_info);
  else
    m_external_user.set((const char *)0, 0, system_charset_info);

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_priv_user.

  @param[in]    priv_user_arg         New user value for m_priv_user.
  @param[in]    priv_user_arg_length  Length of "priv_user_arg" param.
*/

void Security_context::assign_priv_user(const char *priv_user_arg,
                                        const size_t priv_user_arg_length) {
  DBUG_ENTER("Security_context::assign_priv_user");

  if (priv_user_arg_length) {
    m_priv_user_length =
        std::min(priv_user_arg_length, sizeof(m_priv_user) - 1);
    strmake(m_priv_user, priv_user_arg, m_priv_user_length);
  } else {
    *m_priv_user = 0;
    m_priv_user_length = 0;
  }

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_proxy_user.

  @param[in]    proxy_user_arg         New user value for m_proxy_user.
  @param[in]    proxy_user_arg_length  Length of "proxy_user_arg" param.
*/

void Security_context::assign_proxy_user(const char *proxy_user_arg,
                                         const size_t proxy_user_arg_length) {
  DBUG_ENTER("Security_context::assign_proxy_user");

  if (proxy_user_arg_length) {
    m_proxy_user_length =
        std::min(proxy_user_arg_length, sizeof(m_proxy_user) - 1);
    strmake(m_proxy_user, proxy_user_arg, m_proxy_user_length);
  } else {
    *m_proxy_user = 0;
    m_proxy_user_length = 0;
  }

  DBUG_VOID_RETURN;
}

/**
  Setter method for member m_priv_host.

  @param[in]    priv_host_arg         New user value for m_priv_host.
  @param[in]    priv_host_arg_length  Length of "priv_host_arg" param.
*/

void Security_context::assign_priv_host(const char *priv_host_arg,
                                        const size_t priv_host_arg_length) {
  DBUG_ENTER("Security_context::assign_priv_host");

  if (priv_host_arg_length) {
    m_priv_host_length =
        std::min(priv_host_arg_length, sizeof(m_priv_host) - 1);
    strmake(m_priv_host, priv_host_arg, m_priv_host_length);
  } else {
    *m_priv_host = 0;
    m_priv_host_length = 0;
  }

  DBUG_VOID_RETURN;
}

#endif /* SQL_SECURITY_CTX_INCLUDED */
