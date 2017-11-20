/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef ROLE_TABLES_H
#define	ROLE_TABLES_H

#include "sql/auth/auth_common.h"

class THD;
struct TABLE;

bool modify_role_edges_in_table(THD *thd, TABLE *table, const Auth_id_ref &from_user,
                                  const Auth_id_ref &to_user, bool with_admin_option,
                                  bool delete_option);
bool modify_default_roles_in_table(THD *thd, TABLE *table, const Auth_id_ref &auth_id,
                                   const Auth_id_ref &role,
                                   bool delete_option);
TABLE *open_role_edges_table(THD *thd);
TABLE *open_default_role_table(THD *thd);
void close_all_role_tables(THD *thd);
#endif	/* ROLE_TABLES_H */
