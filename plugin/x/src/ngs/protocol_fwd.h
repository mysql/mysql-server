/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_FWD_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_FWD_H_

namespace google {
namespace protobuf {
#ifdef USE_MYSQLX_FULL_PROTO
class Message;
#else
class MessageLite;
#endif
}  // namespace protobuf
}  // namespace google

namespace Mysqlx {

namespace Sql {
class StmtExecute;
}

namespace Resultset {
class Row;
}

namespace Datatypes {
class Any;
class Scalar;
}  // namespace Datatypes

namespace Crud {
class Projection;
class Column;
class Limit;
class LimitExpr;
class Order;
class Insert;
class Insert_TypedRow;
class UpdateOperation;
class Update;
class Collection;
class Find;
class Delete;
class DropView;
class CreateView;
class ModifyView;
}  // namespace Crud

namespace Expr {
class Expr;
}  // namespace Expr

namespace Prepare {
class Prepare;
class Execute;
class Deallocate;
}  // namespace Prepare

namespace Connection {
class CapabilitiesSet;
class CapabilitiesGet;
}  // namespace Connection

}  // namespace Mysqlx

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_FWD_H_
