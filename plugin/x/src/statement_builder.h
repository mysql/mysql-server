/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_STATEMENT_BUILDER_H_
#define PLUGIN_X_SRC_STATEMENT_BUILDER_H_

#include <algorithm>
#include <functional>
#include <string>

#include "plugin/x/generated/mysqlx_error.h"
#include "plugin/x/src/expr_generator.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/ngs/protocol_fwd.h"

namespace xpl {

class Statement_builder {
 public:
  template <typename T>
  using Repeated_field_list = Expression_generator::Repeated_field_list<T>;

  explicit Statement_builder(const Expression_generator &gen)
      : m_builder(gen) {}

 protected:
  using Collection = ::Mysqlx::Crud::Collection;

  void add_collection(const Collection &table) const;
  bool is_prep_stmt_mode() const { return m_builder.m_gen.is_prep_stmt_mode(); }

  template <typename T>
  void add_alias(const T &item) const {
    if (item.has_alias()) m_builder.put(" AS ").put_identifier(item.alias());
  }

  class Generator {
   public:
    explicit Generator(const Expression_generator &gen)
        : m_gen(gen), m_qb(gen.query_string_builder()) {}

    template <typename T>
    const Generator &put_expr(const T &expr) const {
      m_gen.feed(expr);
      return *this;
    }

    template <typename I, typename Op>
    const Generator &put_each(I begin, I end, Op generate) const {
      std::for_each(begin, end, generate);
      return *this;
    }

    template <typename L, typename Op>
    const Generator &put_each(const L &list, Op generate) const {
      return put_each(list.begin(), list.end(), generate);
    }

    template <typename I, typename Op>
    const Generator &put_list(I begin, I end, Op generate,
                              const std::string &separator = ",") const {
      if (std::distance(begin, end) == 0) return *this;

      generate(*begin);
      for (++begin; begin != end; ++begin) {
        m_qb.put(separator);
        generate(*begin);
      }
      return *this;
    }

    template <typename L, typename Op>
    const Generator &put_list(const L &list, Op generate,
                              const std::string &separator = ",") const {
      return put_list(list.begin(), list.end(), generate, separator);
    }

    template <typename T>
    const Generator &put_list(const Repeated_field_list<T> &list,
                              const Generator &(Generator::*put_fun)(const T &)
                                  const,
                              const std::string &separator = ",") const {
      return put_list(list.begin(), list.end(),
                      std::bind(put_fun, this, std::placeholders::_1),
                      separator);
    }

    template <typename T>
    const Generator &put(const T &str) const {
      m_qb.put(str);
      return *this;
    }

    const Generator &put(const Query_string_builder &str) const {
      m_qb.put(str.get());
      return *this;
    }

    const Generator &put_identifier(const std::string &str) const {
      m_qb.quote_identifier(str);
      return *this;
    }

    const Generator &put_quote(const std::string &str) const {
      m_qb.quote_string(str);
      return *this;
    }

    const Generator &dot() const {
      m_qb.dot();
      return *this;
    }

    Expression_generator::Arg_list args() const { return m_gen.args(); }

    const Expression_generator &m_gen;
    Query_string_builder &m_qb;
  } m_builder;
};

class Crud_statement_builder : public Statement_builder {
 public:
  explicit Crud_statement_builder(const Expression_generator &gen)
      : Statement_builder(gen) {}

 protected:
  using Filter = ::Mysqlx::Expr::Expr;
  using Limit = ::Mysqlx::Crud::Limit;
  using LimitExpr = ::Mysqlx::Crud::LimitExpr;
  using Order_item = ::Mysqlx::Crud::Order;
  using Order_list = Repeated_field_list<Order_item>;

  void add_filter(const Filter &filter) const;
  void add_order(const Order_list &order) const;
  void add_limit_field(const Limit &limit, const bool disallow_offset) const;
  void add_limit_expr_field(const LimitExpr &limit,
                            const bool disallow_offset) const;
  void add_order_item(const Order_item &item) const;

  template <typename Message>
  void add_limit(const Message &msg, const bool disallow_offset) const;
};

template <typename Message>
void Crud_statement_builder::add_limit(const Message &msg,
                                       const bool disallow_offset) const {
  if (msg.has_limit() && msg.has_limit_expr()) {
    throw ngs::Error_code(ER_X_BAD_MESSAGE,
                          "Invalid message, one of 'limit' and 'limit_expr' "
                          "fields is allowed. Received both");
  }

  if (msg.has_limit()) add_limit_field(msg.limit(), disallow_offset);

  if (msg.has_limit_expr())
    add_limit_expr_field(msg.limit_expr(), disallow_offset);
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_STATEMENT_BUILDER_H_
