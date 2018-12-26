/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _PROTOCOL_FWD_H_
#define _PROTOCOL_FWD_H_

namespace google
{
namespace protobuf
{
#ifdef USE_MYSQLX_FULL_PROTO
class Message;
#else
class MessageLite;
#endif
}
}

namespace Mysqlx
{

namespace Sql
{
class StmtExecute;
}

namespace Resultset
{
class Row;
}

namespace Datatypes
{
class Any;
class Scalar;
}

namespace Crud
{
class Projection;
class Column;
class Limit;
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
}

namespace Expr
{
class Expr;
}

} // Mysqlx

#endif // _PROTOCOL_FWD_H_
