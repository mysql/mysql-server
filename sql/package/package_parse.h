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

#ifndef SQL_PACKAGE_PACKAGE_PARSE_INCLUDED
#define SQL_PACKAGE_PACKAGE_PARSE_INCLUDED

#include "sql/parse_tree_nodes.h"

namespace im {
class Proc;
} /* namespace im */

/**
  Interface of all native package element parser tree.

  PT_package_base is the uniform interface of all different types of
  package element for parser.

  1) Proc
    PT_package_proc is the uniform parser interface for all native proc objects.

    Proc will PT_revoke() the PT_package_proc instance.

  2) ...
*/
namespace im {

/**
  Base parser root inteface for all package element.
*/
class PT_package_base : public Parse_tree_root {
 public:
  PT_package_base() {}

  virtual ~PT_package_base() {}

  /**
    Interface of make sql command
  */
  virtual Sql_cmd *make_cmd(THD *thd) override = 0;
};

/**
  Implementation of proc parser tree root.
*/
class PT_package_proc final : public PT_package_base {
 public:
  explicit PT_package_proc(PT_item_list *opt_expr_list, const Proc *proc)
      : m_opt_expr_list(opt_expr_list), m_proc(proc) {}

  /**
    Generate the proc execution command.

    @param[in]      THD       Thread context

    @retval         Sql_cmd   The Sql_cmd_proc object
  */
  Sql_cmd *make_cmd(THD *thd) override;

 private:
  PT_item_list *m_opt_expr_list;
  const Proc *m_proc;
};

} /* namespace im */

#endif
