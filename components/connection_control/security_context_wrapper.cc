/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "security_context_wrapper.h"
#include <mysqld_error.h>
#include <string_with_len.h>
#include "connection_control.h"

namespace connection_control {
/**
  Security_context_wrapper constructor.

  @param [in] thd    Handle to THD

  Get security context from thd.
*/
Security_context_wrapper::Security_context_wrapper(MYSQL_THD thd) {
  if (mysql_service_mysql_thd_security_context->get(thd, &m_sctx) == 0) {
    m_valid = true;
  } else {
    LogComponentErr(ERROR_LEVEL,
                    ER_CONNECTION_CONTROL_FAILED_TO_GET_SECURITY_CTX);
  }
}

/**
  Get value for given property from security context

  @param [in] property    Property to be checked

  @returns value of the property
*/

const char *Security_context_wrapper::get_property(const char *property) {
  MYSQL_LEX_CSTRING value = {.str = nullptr, .length = 0};
  if (!m_valid) {
    return nullptr;
  }
  if (mysql_service_mysql_security_context_options->get(m_sctx, property,
                                                        &value) == 0) {
    return value.str;
  }
  LogComponentErr(
      ERROR_LEVEL,
      ER_CONNECTION_CONTROL_FAILED_TO_GET_ATTRIBUTE_FROM_SECURITY_CTX,
      property);
  return nullptr;
}

/**  Get proxy user information from security context */

const char *Security_context_wrapper::get_proxy_user() {
  return get_property("proxy_user");
}

/** Get priv user information from security context */

const char *Security_context_wrapper::get_priv_user() {
  return get_property("priv_user");
}

/** Get priv host information from security context */

const char *Security_context_wrapper::get_priv_host() {
  return get_property("priv_host");
}

/** Get connected user information from security context */

const char *Security_context_wrapper::get_user() {
  return get_property("user");
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
  return get_property("host");
}

/** Get connected ip information from security context */

const char *Security_context_wrapper::get_ip() { return get_property("ip"); }
}  // namespace connection_control
