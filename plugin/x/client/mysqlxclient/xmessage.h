/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XMESSAGE_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XMESSAGE_H_

#ifdef WIN32
#pragma warning(push, 0)
#undef DELETE
#endif  // WIN32

#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/wire_format_lite.h>

#ifdef USE_MYSQLX_FULL_PROTO

#include "protobuf/mysqlx.pb.h"
#include "protobuf/mysqlx_connection.pb.h"
#include "protobuf/mysqlx_crud.pb.h"
#include "protobuf/mysqlx_cursor.pb.h"
#include "protobuf/mysqlx_expect.pb.h"
#include "protobuf/mysqlx_notice.pb.h"
#include "protobuf/mysqlx_prepare.pb.h"
#include "protobuf/mysqlx_resultset.pb.h"
#include "protobuf/mysqlx_session.pb.h"
#include "protobuf/mysqlx_sql.pb.h"

#else  // USE_MYSQLX_FULL_PROTO

#include "protobuf_lite/mysqlx.pb.h"
#include "protobuf_lite/mysqlx_connection.pb.h"
#include "protobuf_lite/mysqlx_crud.pb.h"
#include "protobuf_lite/mysqlx_cursor.pb.h"
#include "protobuf_lite/mysqlx_expect.pb.h"
#include "protobuf_lite/mysqlx_notice.pb.h"
#include "protobuf_lite/mysqlx_prepare.pb.h"
#include "protobuf_lite/mysqlx_resultset.pb.h"
#include "protobuf_lite/mysqlx_session.pb.h"
#include "protobuf_lite/mysqlx_sql.pb.h"

#endif  // USE_MYSQLX_FULL_PROTO

#ifdef WIN32
#pragma warning(pop)
#endif  // WIN32

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XMESSAGE_H_
