/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifndef SQL_PACKAGE_PACKAGE_INTERFACE_INCLUDED
#define SQL_PACKAGE_PACKAGE_INTERFACE_INCLUDED

class sp_name;
class PT_item_list;
class Parse_tree_root;
class THD;

/**
  Interface of native package module.
*/

namespace im {

/* Initialize Package context. */
extern void package_context_init();

/**
  whether exist native proc by schema_name and proc_name

  @retval       true              Exist
  @retval       false             Not exist
*/
bool exist_native_proc(const char *db, const char *name);

/**
  Find the native proc and evoke the parse tree root

  @param[in]    THD               Thread context
  @param[in]    sp_name           Proc name
  @param[in]    pt_expr_list      Parameters

  @retval       parse_tree_root   Parser structure
*/
extern Parse_tree_root *find_native_proc_and_evoke(THD *thd, sp_name *sp_name,
                                                   PT_item_list *pt_expr_list);

} /* namespace im */
#endif
