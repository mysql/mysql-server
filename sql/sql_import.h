/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_IMPORT_INCLUDED
#define SQL_IMPORT_INCLUDED

#include "lex_string.h"
#include "my_sqlcommand.h"
#include "sql/mem_root_array.h"
#include "sql/sql_cmd.h"   // Sql_cmd

class THD;

/**
  @file sql/sql_import.h Declaration of command class for the IMPORT TABLES command.
 */

/**
  Command class for the IMPORT command.
 */
class Sql_cmd_import_table : public Sql_cmd
{
  typedef Mem_root_array_YY<LEX_STRING> Sdi_patterns_type;
  const Sdi_patterns_type m_sdi_patterns;

public:
  /**
    Called by sql_yacc.yy.

    @param patterns - Memroot_array_YY of all the sdi file patterns
    provided as arguments.
   */
  Sql_cmd_import_table(const Sdi_patterns_type &patterns);

  /**
    Import tables from SDI files or patterns provided to constructor.
    @param thd - thread handle
    @retval true on error
    @retval false otherwise
 */
  virtual bool execute(THD *thd);

  /**
    Provide access to the command code enum value.
    @return command code enum value
   */
  virtual enum_sql_command sql_command_code() const;
};
#endif /* !SQL_IMPORT_INCLUDED */
