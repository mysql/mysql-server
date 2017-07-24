/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef _NGS_COMMON_PROTOCOL_PROTOBUF_
#define _NGS_COMMON_PROTOCOL_PROTOBUF_


#ifdef WIN32
#pragma warning(push, 0)
#endif // WIN32

#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/dynamic_message.h>

#include "mysqlx_connection.pb.h"
#include "mysqlx_crud.pb.h"
#include "mysqlx_expect.pb.h"
#include "mysqlx_notice.pb.h"
#include "mysqlx_resultset.pb.h"
#include "mysqlx_session.pb.h"
#include "mysqlx_sql.pb.h"
#include "mysqlx.pb.h"


#ifdef WIN32
#pragma warning(pop)
#endif // WIN32


#endif // _NGS_COMMON_PROTOCOL_PROTOBUF_
