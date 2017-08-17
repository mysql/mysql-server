/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _sql_component_h
#define _sql_component_h

#include "lex_string.h"
#include "my_sqlcommand.h"
#include "sql/mem_root_array.h"
#include "sql/sql_cmd.h"

class THD;

/**
   This class implements the INSTALL COMPONENT statement.
*/

class Sql_cmd_install_component : public Sql_cmd
{
public:
  Sql_cmd_install_component(const Mem_root_array_YY<LEX_STRING> &urns)
  : m_urns(urns)
  { }

  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_INSTALL_COMPONENT; }

  /**
    Install a new component by loading it by dynamic loader service.

    @param thd  Thread context

    @returns false if success, true otherwise
  */
  virtual bool execute(THD *thd);

private:
  const Mem_root_array_YY<LEX_STRING> m_urns;
};


/**
   This class implements the UNINSTALL COMPONENT statement.
*/

class Sql_cmd_uninstall_component : public Sql_cmd
{
public:
  Sql_cmd_uninstall_component(const Mem_root_array_YY<LEX_STRING> &urns)
  : m_urns(urns)
  { }

  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_UNINSTALL_COMPONENT; }

  /**
    Uninstall a plugin by unloading it in the dynamic loader service.

    @param thd  Thread context

    @returns false if success, true otherwise
  */
  virtual bool execute(THD *thd);

private:
  const Mem_root_array_YY<LEX_STRING> m_urns;
};

#endif
