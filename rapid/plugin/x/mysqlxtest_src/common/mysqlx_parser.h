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

#ifndef _MYSQLX_PARSER_H_
#define _MYSQLX_PARSER_H_

#include "expr_parser.h"
#include "proj_parser.h"
#include "orderby_parser.h"

#include <string>

namespace mysqlx
{
  namespace parser
  {
    inline Mysqlx::Expr::Expr* parse_collection_filter(const std::string &source, std::vector<std::string>* placeholders = NULL)
    {
      Expr_parser parser(source, true, false, placeholders);
      return parser.expr();
    }

    inline void parse_document_path(const std::string& source, Mysqlx::Expr::ColumnIdentifier& colid)
    {
      Expr_parser parser(source, true);
      return parser.document_path(colid);
    }

    inline Mysqlx::Expr::Expr* parse_column_identifier(const std::string& source)
    {
      Expr_parser parser(source, true);
      return parser.document_field();
    }

    inline Mysqlx::Expr::Expr* parse_table_filter(const std::string &source, std::vector<std::string>* placeholders = NULL)
    {
      Expr_parser parser(source, false, false, placeholders);
      return parser.expr();
    }

    template<typename Container>
    void parse_collection_sort_column(Container &container, const std::string &source)
    {
      Orderby_parser parser(source, true);
      return parser.parse(container);
    }

    template<typename Container>
    void parse_table_sort_column(Container &container, const std::string &source)
    {
      Orderby_parser parser(source, false);
      return parser.parse(container);
    }

    template<typename Container>
    void parse_collection_column_list(Container &container, const std::string &source)
    {
      Proj_parser parser(source, true, false);
      parser.parse(container);
    }

    template<typename Container>
    void parse_collection_column_list_with_alias(Container &container, const std::string &source)
    {
      Proj_parser parser(source, true, true);
      parser.parse(container);
    }

    template<typename Container>
    void parse_table_column_list(Container &container, const std::string &source)
    {
      Proj_parser parser(source, false, false);
      parser.parse(container);
    }

    template<typename Container>
    void parse_table_column_list_with_alias(Container &container, const std::string &source)
    {
      Proj_parser parser(source, false, true);
      parser.parse(container);
    }
  };
};

#endif
