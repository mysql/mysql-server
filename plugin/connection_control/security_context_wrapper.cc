/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "plugin/connection_control/security_context_wrapper.h"

#include <mysql/service_security_context.h> /* Security context service */

#include <mysql/components/my_service.h>
#include <mysql/components/services/dynamic_privilege.h>
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_acl.h" /* SUPER_ACL */
#include "sql/sql_class.h"    /* THD, Security context */
#include "string_with_len.h"

namespace connection_control {
/**
  Security_context_wrapper constructor.

  @param [in] thd    Handle to THD

  Get security context from thd.
*/
Security_context_wrapper::Security_context_wrapper(MYSQL_THD thd) {
  m_valid = thd_get_security_context(thd, &m_sctx) ? false : true;
}

/**
  Get value for given property from security context

  @param [in] property    Property to be checked
  @param [out] value      Value of the property

  @returns status of property check
    @retval true Error fetching property value
    @retval false value contains valid value for given property
*/

bool Security_context_wrapper::get_property(const char *property,
                                            LEX_CSTRING *value) {
  value->length = 0;
  value->str = nullptr;

  if (!m_valid)
    return true;
  else
    return security_context_get_option(m_sctx, property, value);
}

/**  Get proxy user information from security context */

const char *Security_context_wrapper::get_proxy_user() {
  MYSQL_LEX_CSTRING proxy_user;
  if (get_property("proxy_user", &proxy_user)) return nullptr;
  return proxy_user.str;
}

/** Get priv user information from security context */

const char *Security_context_wrapper::get_priv_user() {
  MYSQL_LEX_CSTRING priv_user;
  if (get_property("priv_user", &priv_user)) return nullptr;
  return priv_user.str;
}

/** Get priv host information from security context */

const char *Security_context_wrapper::get_priv_host() {
  MYSQL_LEX_CSTRING priv_host;
  if (get_property("priv_host", &priv_host)) return nullptr;
  return priv_host.str;
}

/** Get connected user information from security context */

const char *Security_context_wrapper::get_user() {
  MYSQL_LEX_CSTRING user;
  if (get_property("user", &user)) return nullptr;
  return user.str;
}

/** Get connected host information from security context */

const char *Security_context_wrapper::get_host() {
  /*
    We can't use thd->security_ctx->priv_host_name()
    because it returns "%" if hostname is empty.
    However, thd->security_ctx->proxy_user won't have
    "%" if hostname was empty.

    To be consistent, we will always use
    'user'@'host'/''@'host'/''@'' type of representation.
  */
  MYSQL_LEX_CSTRING host;
  if (get_property("host", &host)) return nullptr;
  return host.str;
}

/** Get connected ip information from security context */

const char *Security_context_wrapper::get_ip() {
  MYSQL_LEX_CSTRING ip;
  if (get_property("ip", &ip)) return nullptr;
  return ip.str;
}

/** Check if valid security context exists for give THD or not */

bool Security_context_wrapper::security_context_exists() { return m_valid; }

/** Check whether user has required privilege or not */

bool Security_context_wrapper::is_super_user() {
  if (!m_valid) return false;

  bool has_super = false;
  if (security_context_get_option(m_sctx, "privilege_super", &has_super))
    return false;

  return has_super;
}

/** Check whether user has the connection admin privilege or not */

bool Security_context_wrapper::is_connection_admin() {
  if (!m_valid) return false;
  SERVICE_TYPE(registry) *r = mysql_plugin_registry_acquire();
  bool access_granted = false;
  {
    my_service<SERVICE_TYPE(global_grants_check)> service(
        "global_grants_check.mysql_server", r);
    if (service.is_valid()) {
      access_granted = service->has_global_grant(
          reinterpret_cast<Security_context_handle>(m_sctx),
          STRING_WITH_LEN("CONNECTION_ADMIN"));
    }
  }  // scope exit destroys my_service
  mysql_plugin_registry_release(r);
  return access_granted;
}
}  // namespace connection_control
