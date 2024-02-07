/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "storing_auto_thd.h"

/**
  Wrapper of create_internal_thd() function, creating a temporary THD
  with the following differences compared to original code:
   - created THD does not have system_user flag set
   - instead, THD has minimal privileges needed to set system variables
   - a user name assigned, so the variables can be persisted with such THD

  @param ctx Security context smart pointer to be assigned.
  @retval THD object created
*/
THD *create_internal_thd_ctx(Sctx_ptr<Security_context> &ctx) {
  THD *thd = create_internal_thd();

  // undo skip_grants
  thd->set_system_user(false);
  thd->set_connection_admin(false);

  const std::vector<std::string> priv_list = {
      "ENCRYPTION_KEY_ADMIN",   "ROLE_ADMIN",
      "SYSTEM_VARIABLES_ADMIN", "AUDIT_ADMIN",
      "TELEMETRY_LOG_ADMIN",    "PERSIST_RO_VARIABLES_ADMIN"};
  const ulong static_priv_list = (SUPER_ACL | FILE_ACL);

  lex_start(thd);
  /* create security context for internal THD */
  Security_context_factory default_factory(
      thd, "sys_session", "localhost", Default_local_authid(thd),
      Grant_temporary_dynamic_privileges(thd, priv_list),
      Grant_temporary_static_privileges(thd, static_priv_list),
      Drop_temporary_dynamic_privileges(priv_list));
  ctx = default_factory.create();
  /* attach this ctx to current security_context */
  thd->set_security_context(ctx.get());
  thd->real_id = my_thread_self();
#ifndef NDEBUG
  thd->for_debug_only_is_set_persist_options = true;
#endif

  return thd;
}

/**
  Wrapper of destroy_internal_thd() function, safely destroying
  a temporary THD and its associated security context smart pointer.

  @param thd Pointer to THD object to be destroyed.
  @param ctx Security context smart pointer to be cleared.
*/
void destroy_internal_thd_ctx(THD *thd, Sctx_ptr<Security_context> &ctx) {
  thd->free_items();
  lex_end(thd->lex);
  ctx.reset(nullptr);
  destroy_internal_thd(thd);
}
