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

#ifndef _EXPR_PARSER_PROJ_H_
#define _EXPR_PARSER_PROJ_H_

#define _EXPR_PARSER_HAS_PROJECTION_KEYWORDS_ 1

#include <boost/format.hpp>
#include "expr_parser.h"
#include "ngs_common/protocol_protobuf.h"
#include "compilerutils.h"

#include <memory>

namespace mysqlx
{
class Proj_parser : public Expr_parser
{
public:
  Proj_parser(const std::string& expr_str, bool document_mode = false, bool allow_alias = true);

  template<typename Container>
  void parse(Container &result)
  {
    Mysqlx::Crud::Projection *colid = result.Add();
    source_expression(*colid);

    if (_tokenizer.tokens_available())
      {
        const mysqlx::Token& tok = _tokenizer.peek_token();
        throw Parser_error((boost::format("Projection parser: Expression '%s' has unexpected token '%s' at position %d") % _tokenizer.get_input() % tok.get_text() %
          tok.get_pos()).str());
      }
  }

  const std::string& id();
  void source_expression(Mysqlx::Crud::Projection &column);
};
};
#endif
