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

#include "mysqlx_error.h"

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/message_decoder.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

Message *Message_decoder::alloc_message(int8 type, Error_code &ret_error,
                                        bool &ret_shared) {
  try {
    Message *msg = nullptr;
    ret_shared = true;
    switch ((Mysqlx::ClientMessages::Type)type) {
      case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
        msg = allocate_object<Mysqlx::Connection::CapabilitiesGet>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
        msg = allocate_object<Mysqlx::Connection::CapabilitiesSet>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::CON_CLOSE:
        msg = allocate_object<Mysqlx::Connection::Close>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_CLOSE:
        msg = allocate_object<Mysqlx::Session::Close>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_RESET:
        msg = allocate_object<Mysqlx::Session::Reset>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
        msg = allocate_object<Mysqlx::Session::AuthenticateStart>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE:
        msg = allocate_object<Mysqlx::Session::AuthenticateContinue>();
        ret_shared = false;
        break;
      case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
        msg = &m_stmt_execute;
        break;
      case Mysqlx::ClientMessages::CRUD_FIND:
        msg = &m_crud_find;
        break;
      case Mysqlx::ClientMessages::CRUD_INSERT:
        msg = &m_crud_insert;
        break;
      case Mysqlx::ClientMessages::CRUD_UPDATE:
        msg = &m_crud_update;
        break;
      case Mysqlx::ClientMessages::CRUD_DELETE:
        msg = &m_crud_delete;
        break;
      case Mysqlx::ClientMessages::EXPECT_OPEN:
        msg = &m_expect_open;
        break;
      case Mysqlx::ClientMessages::EXPECT_CLOSE:
        msg = &m_expect_close;
        break;
      case Mysqlx::ClientMessages::CRUD_CREATE_VIEW:
        msg = &m_crud_create_view;
        break;
      case Mysqlx::ClientMessages::CRUD_MODIFY_VIEW:
        msg = &m_crud_modify_view;
        break;
      case Mysqlx::ClientMessages::CRUD_DROP_VIEW:
        msg = &m_crud_drop_view;
        break;
      case Mysqlx::ClientMessages::CURSOR_OPEN:
        msg = &m_cursor_open;
        break;
      case Mysqlx::ClientMessages::CURSOR_CLOSE:
        msg = &m_cursor_close;
        break;
      case Mysqlx::ClientMessages::CURSOR_FETCH:
        msg = &m_cursor_fetch;
        break;
      case Mysqlx::ClientMessages::PREPARE_PREPARE:
        msg = &m_prepare_prepare;
        break;
      case Mysqlx::ClientMessages::PREPARE_EXECUTE:
        msg = &m_prepare_execute;
        break;
      case Mysqlx::ClientMessages::PREPARE_DEALLOCATE:
        msg = &m_prepare_deallocate;
        break;

      default:
        log_debug("Cannot decode message of unknown type %i", type);
        ret_error = Error_code(ER_X_BAD_MESSAGE, "Invalid message type");
        break;
    }
    return msg;
  } catch (std::bad_alloc &) {
    ret_error = Error_code(ER_OUTOFMEMORY, "Out of memory");
  }
  return nullptr;
}

Error_code Message_decoder::parse(const uint8 msg_type, const int msg_size,
                                  Zero_copy_input_stream *zero_copy_stream,
                                  Message_request *out_msg) {
  const int max_recursion_limit = 100;
  bool msg_is_shared;
  Error_code ret_error;
  Message *message = alloc_message(msg_type, ret_error, msg_is_shared);

  // Allocation error is ignored
  // if (ret_error) return ret_error;

  if (message) {
    bool parse_result = true;

    if (msg_size > 0) {
      // feed the data to the command (up to the specified boundary)
      google::protobuf::io::CodedInputStream stream(zero_copy_stream);
      // variable 'mysqlx_max_allowed_packet' has been checked when buffer was
      // filling by data
      stream.SetTotalBytesLimit(static_cast<int>(msg_size), -1 /*no warnings*/);
      // Protobuf limits the number of nested objects when decoding messages
      // lets set the value in explicit way (to ensure that is set accordingly
      // with out stack size)
      //
      // Protobuf doesn't print a readable error after reaching the limit
      // thus in case of failure we try to validate the limit by decrementing
      // and incrementing the value & checking result for failure
      stream.SetRecursionLimit(max_recursion_limit);

      if (!message->ParseFromCodedStream(&stream)) {
        parse_result = false;

        if (!msg_is_shared) free_object(message);

        message = nullptr;

        // Workaround
        stream.DecrementRecursionDepth();
        if (!stream.IncrementRecursionDepth()) {
          return Error(ER_X_BAD_MESSAGE,
                       "X Protocol message recursion limit (%i) exceeded",
                       max_recursion_limit);
        }
      }
    } else {
      // ParseFromCodedStream calls "Clear", thus in this case we also need
      // to clear cached data
      message->Clear();
    }

    if (!parse_result || !message->IsInitialized()) {
      if (!msg_is_shared) free_object(message);

      message = nullptr;

      // In case of protobuf-lite, the  call to InitializationErrorString
      // doesn't give any valuable information, thus X Plugin sends back
      // a generic error message.
      return Error_code(ER_X_BAD_MESSAGE,
                        "Parse error unserializing protobuf message");
    }

    out_msg->reset(message, msg_type, msg_is_shared);
  }

  return Success();
}

}  // namespace ngs
