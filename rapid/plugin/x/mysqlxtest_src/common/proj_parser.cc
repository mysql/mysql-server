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

#include "proj_parser.h"
#include "ngs_common/protocol_protobuf.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

using namespace mysqlx;

Proj_parser::Proj_parser(const std::string& expr_str, bool document_mode, bool allow_alias)
  : Expr_parser(expr_str, document_mode, allow_alias)
{
}

/*
 * id ::= IDENT | MUL
 */
const std::string& Proj_parser::id()
{
  if (_tokenizer.cur_token_type_is(Token::IDENT))
    return _tokenizer.consume_token(Token::IDENT);
  else
    return _tokenizer.consume_token(Token::MUL);
}

/*
 * column_identifier ::= ( expr [ [AS] IDENT ] ) | ( DOLLAR [ IDENT ] document_path )
 */
void Proj_parser::source_expression(Mysqlx::Crud::Projection &col)
{
  if ( _document_mode && _tokenizer.cur_token_type_is(Token::DOLLAR))
  {
    _tokenizer.consume_token(Token::DOLLAR);
    Mysqlx::Expr::ColumnIdentifier* colid = col.mutable_source()->mutable_identifier();
    col.mutable_source()->set_type(Mysqlx::Expr::Expr::IDENT);
    if (_tokenizer.cur_token_type_is(Token::IDENT))
    {
      const std::string& ident = _tokenizer.consume_token(Token::IDENT);
      colid->mutable_document_path()->Add()->set_value(ident.c_str(), ident.size());
  }
      document_path(*colid);
    }
    else
      col.set_allocated_source(my_expr());

  // Sets the alias token
  if (_allow_alias)
  {
  if (_tokenizer.cur_token_type_is(Token::AS))
  {
      _tokenizer.consume_token(Token::AS);
      const std::string& alias = _tokenizer.consume_token(Token::IDENT);
      col.set_alias(alias.c_str());
    }
  else if (_tokenizer.cur_token_type_is(Token::IDENT))
  {
    const std::string& alias = _tokenizer.consume_token(Token::IDENT);
    col.set_alias(alias.c_str());
  }
    else if (_document_mode)
      col.set_alias(_tokenizer.get_input());
  }
}