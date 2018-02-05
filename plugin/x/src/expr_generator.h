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

#ifndef _XPL_EXPR_GENERATOR_H_
#define _XPL_EXPR_GENERATOR_H_

#include <stdexcept>
#include <string>

#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/query_string_builder.h"

namespace xpl {

class Expression_generator {
 public:
  using Expr = ::Mysqlx::Expr::Expr;
  using Args =
      ::google::protobuf::RepeatedPtrField<::Mysqlx::Datatypes::Scalar>;
  using Document_path =
      ::google::protobuf::RepeatedPtrField<::Mysqlx::Expr::DocumentPathItem>;

  class Error : public std::invalid_argument {
    int m_error;

   public:
    Error(int error_code, const std::string &message);

    int error() const { return m_error; }
  };

  // source: ``Mysqlx.Resultset.ColumnMetadata`` for list of known values
  enum Octets_content_type {
    CT_PLAIN = 0x0000,  //   default value; general use of octets
    CT_GEOMETRY = Mysqlx::Resultset::GEOMETRY,
    CT_JSON = Mysqlx::Resultset::JSON,
    CT_XML = Mysqlx::Resultset::XML
  };

  Expression_generator(Query_string_builder *qb, const Args &args,
                       const std::string &default_schema,
                       const bool &is_relational)
      : m_qb(qb),
        m_args(args),
        m_default_schema(default_schema),
        m_is_relational(is_relational) {}

  template <typename T>
  inline void feed(const T &expr) const {
    generate(expr);
  }

  Expression_generator clone(Query_string_builder *qb) const;
  Query_string_builder &query_string_builder() const { return *m_qb; }
  const Args &args() const { return m_args; }

 private:
  using Placeholder = ::google::protobuf::uint32;

  void generate(const Mysqlx::Expr::Expr &arg) const;
  void generate(const Mysqlx::Expr::Identifier &arg,
                const bool is_function = false) const;
  void generate(const Mysqlx::Expr::ColumnIdentifier &arg) const;
  void generate(const Mysqlx::Expr::FunctionCall &arg) const;
  void generate(const Mysqlx::Expr::Operator &arg) const;
  void generate(const Mysqlx::Datatypes::Any &arg) const;
  void generate(const Mysqlx::Datatypes::Scalar &arg) const;
  void generate(const Mysqlx::Datatypes::Scalar::Octets &arg) const;
  void generate(const Document_path &arg) const;
  void generate(const Placeholder &arg) const;
  void generate(const Mysqlx::Expr::Object &arg) const;
  void generate(const Mysqlx::Expr::Object::ObjectField &arg) const;
  void generate(const Mysqlx::Expr::Array &arg) const;

  template <typename T>
  void generate_for_each(
      const ::google::protobuf::RepeatedPtrField<T> &list,
      void (Expression_generator::*generate_fun)(const T &) const,
      const typename ::google::protobuf::RepeatedPtrField<T>::size_type offset =
          0) const;
  void generate_unquote_param(const Mysqlx::Expr::Expr &arg) const;
  void generate_json_literal_param(const Mysqlx::Datatypes::Scalar &arg) const;
  void generate_cont_in_param(const Mysqlx::Expr::Expr &arg) const;

  void binary_operator(const Mysqlx::Expr::Operator &arg,
                       const char *str) const;
  void unary_operator(const Mysqlx::Expr::Operator &arg, const char *str) const;
  void nullary_operator(const Mysqlx::Expr::Operator &arg,
                        const char *str) const;
  void in_expression(const Mysqlx::Expr::Operator &arg, const char *str) const;
  void cont_in_expression(const Mysqlx::Expr::Operator &arg,
                          const char *str) const;
  void like_expression(const Mysqlx::Expr::Operator &arg,
                       const char *str) const;
  void between_expression(const Mysqlx::Expr::Operator &arg,
                          const char *str) const;
  void date_expression(const Mysqlx::Expr::Operator &arg,
                       const char *str) const;
  void cast_expression(const Mysqlx::Expr::Operator &arg) const;
  void binary_expression(const Mysqlx::Expr::Operator &arg,
                         const char *str) const;
  void asterisk_operator(const Mysqlx::Expr::Operator &arg) const;

  void validate_placeholder(const Placeholder &arg) const;

  Query_string_builder *m_qb;
  const Args &m_args;
  const std::string &m_default_schema;
  const bool &m_is_relational;
};

template <typename T>
void generate_expression(Query_string_builder *qb, const T &expr,
                         const Expression_generator::Args &args,
                         const std::string &default_schema,
                         bool is_relational) {
  Expression_generator gen(qb, args, default_schema, is_relational);
  gen.feed(expr);
}

template <typename T>
ngs::PFS_string generate_expression(const T &expr,
                                    const Expression_generator::Args &args,
                                    const std::string &default_schema,
                                    bool is_relational) {
  Query_string_builder qb;
  generate_expression(&qb, expr, args, default_schema, is_relational);
  return qb.get();
}

template <typename T>
void generate_expression(Query_string_builder *qb, const T &expr,
                         const std::string &default_schema,
                         bool is_relational) {
  generate_expression(qb, expr, Expression_generator::Args(), default_schema,
                      is_relational);
}

template <typename T>
ngs::PFS_string generate_expression(const T &expr,
                                    const std::string &default_schema,
                                    bool is_relational) {
  return generate_expression(expr, Expression_generator::Args(), default_schema,
                             is_relational);
}

}  // namespace xpl

#endif  // _XPL_EXPR_GENERATOR_H_
