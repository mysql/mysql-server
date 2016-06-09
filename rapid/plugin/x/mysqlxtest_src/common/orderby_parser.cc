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

#include "orderby_parser.h"
#include "ngs_common/protocol_protobuf.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

using namespace mysqlx;

Orderby_parser::Orderby_parser(const std::string& expr_str, bool document_mode)
: Expr_parser(expr_str, document_mode)
{
}

/*
* document_mode = false:
*   column_identifier ::= expr ( ASC | DESC )?
*/
void Orderby_parser::column_identifier(Mysqlx::Crud::Order &orderby_expr)
{
  orderby_expr.set_allocated_expr(my_expr());

  if (_tokenizer.cur_token_type_is(Token::ASC))
  {
    orderby_expr.set_direction(Mysqlx::Crud::Order_Direction_ASC);
    _tokenizer.consume_token(Token::ASC);
  }
  else if (_tokenizer.cur_token_type_is(Token::DESC))
  {
    orderby_expr.set_direction(Mysqlx::Crud::Order_Direction_DESC);
    _tokenizer.consume_token(Token::DESC);
  }
}
