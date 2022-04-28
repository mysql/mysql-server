/* Copyright (c) 2011, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef OPT_EXPLAIN_FORMAT_JSON_INCLUDED
#define OPT_EXPLAIN_FORMAT_JSON_INCLUDED

#include "sql/opt_explain_format.h"
#include "sql/parse_tree_node_base.h"

class Query_result;
class Query_expression;

namespace opt_explain_json_namespace {
class context;
}

/**
  Formatter class for EXPLAIN FORMAT=JSON output
*/

class Explain_format_JSON : public Explain_format {
 private:
  opt_explain_json_namespace::context *current_context;  ///< current tree node

 public:
  Explain_format_JSON() : current_context(nullptr) {}

  bool is_hierarchical() const override { return true; }
  bool send_headers(Query_result *result) override;
  bool begin_context(enum_parsing_context context, Query_expression *subquery,
                     const Explain_format_flags *flags) override;
  bool end_context(enum_parsing_context context) override;
  bool flush_entry() override { return false; }
  qep_row *entry() override;
};

#endif  // OPT_EXPLAIN_FORMAT_JSON_INCLUDED
