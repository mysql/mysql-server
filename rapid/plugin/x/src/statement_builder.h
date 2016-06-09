/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef STATEMENT_BUILDER_H_
#define STATEMENT_BUILDER_H_

#include "ngs/error_code.h"
#include "ngs/protocol_fwd.h"
#include "expr_generator.h"

#include "ngs_common/protocol_protobuf.h"
#include <algorithm>
#include <boost/bind.hpp>

namespace xpl
{

class Statement_builder
{
public:
  Statement_builder(Query_string_builder &qb, const Expression_generator::Args &args,
                    const std::string &schema, bool is_relational)
  : m_builder(qb, args, schema, is_relational),
    m_is_relational(is_relational)
  { }
  virtual ~Statement_builder() {}

  ngs::Error_code build() const;

  class Builder
  {
  public:
    Builder(Query_string_builder &qb, const Expression_generator::Args &args,
            const std::string &default_schema, bool is_relational)
    : m_qb(qb), m_gen(qb, args, default_schema, is_relational)
    { }

    Builder(Query_string_builder &qb, const Expression_generator &gen)
    : m_qb(qb), m_gen(qb, gen.get_args(), gen.get_default_schema(), gen.is_relational())
    { }

    template<typename T>
    const Builder &gen(const T &expr) const { m_gen.feed(expr); return *this; }

    template<typename I, typename Op>
    const Builder &put_each(I begin, I end, Op generate) const
    {
      std::for_each(begin, end, generate);
      return *this;
    }

    template<typename L, typename Op>
    const Builder &put_list(const L &list, Op generate) const
    {
      if (list.size() == 0)
        return *this;

      typename L::const_iterator p = list.begin();
      generate(*p);
      for (++p; p != list.end(); ++p)
      {
        m_qb.put(",");
        generate(*p);
      }
      return *this;
    }

    const Builder &put_list(const ::google::protobuf::RepeatedPtrField<Expression_generator::Expr> &list) const
    {
      return put_list(list, boost::bind(&Expression_generator::feed<Expression_generator::Expr>, m_gen, _1));
    }

    template<typename T>
    const Builder &put(const T &str) const { m_qb.put(str); return *this; }

    const Builder &put(const Query_string_builder &str) const { m_qb.put(str.get()); return *this; }

    const Builder &put_identifier(const std::string &str) const { m_qb.quote_identifier(str); return *this; }

    const Builder &put_quote(const std::string &str) const { m_qb.quote_string(str); return *this; }

    const Builder &dot() const { m_qb.dot(); return *this; }

    const Expression_generator &get_generator() const { return m_gen; }

  private:
    Query_string_builder &m_qb;
    Expression_generator m_gen;
  };

protected:
  typedef ::Mysqlx::Expr::Expr Filter;
  typedef ::Mysqlx::Crud::Limit Limit;
  typedef ::Mysqlx::Crud::Collection Collection;
  typedef ::Mysqlx::Crud::Order Order_item;
  typedef ::google::protobuf::RepeatedPtrField<Order_item> Order_list;

  virtual void add_statement() const = 0;

  void add_table(const Collection &table) const;
  void add_filter(const Filter &filter) const;
  void add_order(const Order_list &order) const;
  void add_limit(const Limit &limit, bool no_offset) const;
  void add_order_item(const Order_item &item) const;

  Builder m_builder;
  bool m_is_relational;
};

} // namespace xpl

#endif // STATEMENT_BUILDER_H_
