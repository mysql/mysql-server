/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
#include <string>

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/protocol_fwd.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/expr_generator.h"

namespace xpl {

class Statement_builder {
 public:
  explicit Statement_builder(const Expression_generator &gen)
      : m_builder(gen) {}

 protected:
  using Collection = ::Mysqlx::Crud::Collection;

  void add_collection(const Collection &table) const;

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
    const Generator &put_list(
        const ::google::protobuf::RepeatedPtrField<T> &list,
        const Generator &(Generator::*put_fun)(const T &)const,
        const std::string &separator = ",") const {
      return put_list(list.begin(), list.end(),
                      ngs::bind(put_fun, this, ngs::placeholders::_1),
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

    Expression_generator::Args args() const { return m_gen.args(); }

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
  using Order_item = ::Mysqlx::Crud::Order;
  using Order_list = ::google::protobuf::RepeatedPtrField<Order_item>;

  void add_filter(const Filter &filter) const;
  void add_order(const Order_list &order) const;
  void add_limit(const Limit &limit, const bool no_offset) const;
  void add_order_item(const Order_item &item) const;
};

template <typename T>
inline bool is_table_data_model(const T &msg) {
  return msg.data_model() == ::Mysqlx::Crud::TABLE;
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_STATEMENT_BUILDER_H_
