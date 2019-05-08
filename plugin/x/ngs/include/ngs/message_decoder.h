// Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_MESSAGE_DECODER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_MESSAGE_DECODER_H_

#include <sys/types.h>
#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

/**
 X Protocol Message decoder

 Unserializes the binary data into cached protobuf message,
 so that they don't need to be reallocated every time.
*/
class Message_decoder {
 public:
  using Zero_copy_input_stream = google::protobuf::io::ZeroCopyInputStream;

 public:
  /**
    Parse X Protocol message reading it from input steam.

    All IO errors must be stores on stream object, which must also give user
    the possibility to check it. In case of IO error the return value might
    point a success.

    @param msg_type  message that should be deserialized
    @param stream    object wrapping IO operations
    @param out_msg   returns message that is result of parsing

    @return Error_code is used only to pass logic error.
  */
  Error_code parse(const uint8 msg_type, const int msg_size,
                   Zero_copy_input_stream *stream, Message_request *out_msg);

 private:
  Message *alloc_message(const int8 type, Error_code &ret_error,
                         bool &ret_shared);

  // Messages that are cached
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
  Mysqlx::Cursor::Open m_cursor_open;
  Mysqlx::Cursor::Close m_cursor_close;
  Mysqlx::Cursor::Fetch m_cursor_fetch;
  Mysqlx::Prepare::Prepare m_prepare_prepare;
  Mysqlx::Prepare::Execute m_prepare_execute;
  Mysqlx::Prepare::Deallocate m_prepare_deallocate;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_MESSAGE_DECODER_H_
