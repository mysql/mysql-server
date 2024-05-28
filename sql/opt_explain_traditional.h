/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#ifndef OPT_EXPLAIN_FORMAT_TRADITIONAL_INCLUDED
#define OPT_EXPLAIN_FORMAT_TRADITIONAL_INCLUDED

#include <assert.h>
// assert
#include <memory>
#include <string>
#include <vector>

#include "sql/opt_explain_format.h"
#include "sql/parse_tree_node_base.h"

class Item;
class Json_dom;
class Json_object;
class Query_result;
class Query_expression;
template <class T>
class mem_root_deque;

/**
  Formatter for the traditional EXPLAIN output
*/

class Explain_format_traditional : public Explain_format {
  class Item_null *nil;
  qep_row column_buffer;  ///< buffer for the current output row

 public:
  Explain_format_traditional() : nil(nullptr) {}

  bool is_hierarchical() const override { return false; }
  bool send_headers(Query_result *result) override;
  bool begin_context(enum_parsing_context, Query_expression *,
                     const Explain_format_flags *) override {
    return false;
  }
  bool end_context(enum_parsing_context) override { return false; }
  bool flush_entry() override;
  qep_row *entry() override { return &column_buffer; }

 private:
  bool push_select_type(mem_root_deque<Item *> *items);
};

class Explain_format_tree : public Explain_format {
 public:
  Explain_format_tree() = default;

  bool is_hierarchical() const override { return false; }
  bool send_headers(Query_result *) override {
    assert(false);
    return true;
  }
  bool begin_context(enum_parsing_context, Query_expression *,
                     const Explain_format_flags *) override {
    assert(false);
    return true;
  }
  bool end_context(enum_parsing_context) override {
    assert(false);
    return true;
  }
  bool flush_entry() override {
    assert(false);
    return true;
  }
  qep_row *entry() override {
    assert(false);
    return nullptr;
  }
  bool is_iterator_based(THD *explain_thd [[maybe_unused]],
                         const THD *query_thd [[maybe_unused]]) const override {
    return true;
  }

  /* Convert Json object to string */
  std::string ExplainJsonToString(Json_object *json) override;
  void ExplainPrintTreeNode(const Json_dom *json, int level,
                            std::string *explain,
                            std::vector<std::string> *tokens_for_force_subplan);

 private:
  bool push_select_type(mem_root_deque<Item *> *items);

  void AppendChildren(const Json_dom *children, int level, std::string *explain,
                      std::vector<std::string> *tokens_for_force_subplan,
                      std::string *child_token_digest);
  void ExplainPrintCosts(const Json_object *obj, std::string *explain);
};

#endif  // OPT_EXPLAIN_FORMAT_TRADITIONAL_INCLUDED
