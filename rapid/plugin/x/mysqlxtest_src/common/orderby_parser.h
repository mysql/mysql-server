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

#ifndef _ORDERBY_PARSER_H_
#define _ORDERBY_PARSER_H_

#include <boost/format.hpp>
#include "expr_parser.h"
#include "ngs_common/protocol_protobuf.h"
#include "compilerutils.h"

#include <memory>

namespace mysqlx
{
  class Orderby_parser : public Expr_parser
  {
  public:
    Orderby_parser(const std::string& expr_str, bool document_mode = false);

    template<typename Container>
    void parse(Container &result)
    {
      Mysqlx::Crud::Order *colid = result.Add();
      column_identifier(*colid);
      
      if (_tokenizer.tokens_available())
      {
        const mysqlx::Token& tok = _tokenizer.peek_token();
        throw Parser_error((boost::format("Orderby parser: Expected EOF, instead stopped at token '%s' at position %d") % tok.get_text()
          % tok.get_pos()).str());
      }
    }

    //const std::string& id();
    void column_identifier(Mysqlx::Crud::Order &orderby_expr);    

    std::vector<Token>::const_iterator begin() const { return _tokenizer.begin(); }
    std::vector<Token>::const_iterator end() const { return _tokenizer.end(); }
  };
};
#endif
