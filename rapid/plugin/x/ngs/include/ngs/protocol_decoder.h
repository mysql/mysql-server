// Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; version 2 of the
// License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
// 02110-1301  USA

#ifndef _NGS_PROTOCOL_DECODER_H_
#define _NGS_PROTOCOL_DECODER_H_

#include "protocol/message.h"
#include "ngs_common/protocol_protobuf.h"
#include "error_code.h"

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
