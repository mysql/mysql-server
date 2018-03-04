/*
   Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TRIGGER_CREATION_CTX_H_INCLUDED
#define TRIGGER_CREATION_CTX_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

#include "mysql/psi/mysql_statement.h"
#include "sql/item_create.h"
#include "sql/key.h"
#include "sql/session_tracker.h"
#include "sql/sp_head.h" // Stored_program_creation_ctx
#include "sql/sql_alloc.h"
#include "sql/sql_servers.h"
#include "sql/thr_malloc.h"

class Object_creation_ctx;
class THD;

/**
  Trigger_creation_ctx -- creation context of triggers.
*/

class Trigger_creation_ctx : public Stored_program_creation_ctx,
                             public Sql_alloc
{
public:
  static Trigger_creation_ctx *create(THD *thd,
                                      const LEX_CSTRING &db_name,
                                      const LEX_CSTRING &table_name,
                                      const LEX_CSTRING &client_cs_name,
                                      const LEX_CSTRING &connection_cl_name,
                                      const LEX_CSTRING &db_cl_name);

public:
  virtual Stored_program_creation_ctx *clone(MEM_ROOT *mem_root)
  {
    return new (mem_root) Trigger_creation_ctx(m_client_cs,
                                               m_connection_cl,
                                               m_db_cl);
  }

protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const
  {
    return new (*THR_MALLOC) Trigger_creation_ctx(thd);
  }

private:
  Trigger_creation_ctx(THD *thd)
    :Stored_program_creation_ctx(thd)
  { }

  Trigger_creation_ctx(const CHARSET_INFO *client_cs,
                       const CHARSET_INFO *connection_cl,
                       const CHARSET_INFO *db_cl)
    :Stored_program_creation_ctx(client_cs, connection_cl, db_cl)
  { }
};

///////////////////////////////////////////////////////////////////////////

#endif // TRIGGER_CREATION_CTX_H_INCLUDED
