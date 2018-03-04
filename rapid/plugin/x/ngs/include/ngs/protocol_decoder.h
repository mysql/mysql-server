// Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef _NGS_PROTOCOL_DECODER_H_
#define _NGS_PROTOCOL_DECODER_H_

#include <sys/types.h>

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"

namespace ngs
{
  /* X Protocol Message decoder

  Caches instances of protobuf messages, so that they don't need to be reallocated
  every time.
  */
  class Message_decoder
  {
  public:
    Error_code parse(Request &request);

  protected:
    Message *alloc_message(int8_t type, Error_code &ret_error, bool &ret_shared);

    Mysqlx::Sql::StmtExecute m_stmt_execute;
    Mysqlx::Crud::Find m_crud_find;
    Mysqlx::Crud::Insert m_crud_insert;
    Mysqlx::Crud::Update m_crud_update;
    Mysqlx::Crud::Delete m_crud_delete;
    Mysqlx::Expect::Open m_expect_open;
    Mysqlx::Expect::Close m_expect_close;
    Mysqlx::Crud::CreateView m_crud_create_view;
    Mysqlx::Crud::ModifyView m_crud_modify_view;
    Mysqlx::Crud::DropView m_crud_drop_view;
  };
}

#endif
