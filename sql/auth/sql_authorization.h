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

#ifndef SQL_AUTHORIZATION_INCLUDED
#define SQL_AUTHORIZATION_INCLUDED

#include <functional>
#include <string>
#include <utility>

#include "lex_string.h"
#include "mysql/components/services/mysql_mutex_bits.h"
#include "sql/auth/sql_auth_cache.h"

class Role_id;
class Security_context;
class String;
class THD;

void roles_graphml(THD *thd, String *);
void flatten_role_acls(ACL_USER *user, Security_context *sctx);

bool check_if_granted_role(LEX_CSTRING user, LEX_CSTRING host,
                           LEX_CSTRING role, LEX_CSTRING role_host);
bool find_if_granted_role(Role_vertex_descriptor v,
                          LEX_CSTRING role,
                          LEX_CSTRING role_host,
                          Role_vertex_descriptor *found_vertex=0);
bool has_grant_role_privilege(THD *thd);
bool has_revoke_role_privilege(THD *thd);
bool has_any_table_acl(THD *thd, Security_context *sctx,
                       const LEX_CSTRING &str);
bool has_any_routine_acl(THD *thd, Security_context *sctx,
                         const LEX_CSTRING &db);
std::pair<std::string, std::string >
  get_authid_from_quoted_string(std::string str);
void iterate_comma_separated_quoated_string(std::string str,
                             const std::function<bool (const std::string )> &f);
void get_granted_roles(Role_vertex_descriptor &v,
                       std::function<void(const Role_id &, bool)> f);
/* For for get_mandatory_roles and Sys_mandatory_roles */
extern mysql_mutex_t LOCK_mandatory_roles;
#endif /* SQL_AUTHORIZATION_INCLUDED */
