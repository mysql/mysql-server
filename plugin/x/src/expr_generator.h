/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_EXPR_GENERATOR_H_
#define PLUGIN_X_SRC_EXPR_GENERATOR_H_

#include <stdexcept>
#include <string>

#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/prepare_param_handler.h"
#include "plugin/x/src/query_string_builder.h"

namespace xpl {

class Expression_generator {
 public:
  template <typename T>
  using Repeated_field_list = ::google::protobuf::RepeatedPtrField<T>;
  using Expr = ::Mysqlx::Expr::Expr;
  using Arg_list = Repeated_field_list<::Mysqlx::Datatypes::Scalar>;
  using Document_path = Repeated_field_list<::Mysqlx::Expr::DocumentPathItem>;
  using Prep_stmt_placeholder_list = Prepare_param_handler::Placeholder_list;

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

  Expression_generator(Query_string_builder *qb, const Arg_list &args,
                       const std::string &default_schema,
                       const bool &is_relational)
      : m_qb(qb),
        m_args(args),
        m_default_schema(default_schema),
        m_is_relational(is_relational) {}

  Expression_generator(const Expression_generator &) = default;
  Expression_generator(Expression_generator &&) = default;
  virtual ~Expression_generator() = default;

  template <typename T>
  inline void feed(const T &expr) const {
    generate(expr);
  }

  Expression_generator clone(Query_string_builder *qb) const;
  Query_string_builder &query_string_builder() const { return *m_qb; }
  const Arg_list &args() const { return m_args; }
  void set_prep_stmt_placeholder_list(Prep_stmt_placeholder_list *ids) {
    m_placeholders = ids;
  }
  bool is_prep_stmt_mode() const { return m_placeholders != nullptr; }

 protected:
  using Placeholder = ::google::protobuf::uint32;

  virtual void generate(const Mysqlx::Expr::Expr &arg) const;
  virtual void generate(const Mysqlx::Expr::Identifier &arg,
                        const bool is_function = false) const;
  virtual void generate(const Mysqlx::Expr::ColumnIdentifier &arg) const;
  virtual void generate(const Mysqlx::Expr::FunctionCall &arg) const;
  virtual void generate(const Mysqlx::Expr::Operator &arg) const;
  virtual void generate(const Document_path &arg) const;
  virtual void generate(const Placeholder &arg) const;
  virtual void generate(const Mysqlx::Expr::Object &arg) const;
  virtual void generate(const Mysqlx::Expr::Object::ObjectField &arg) const;
  virtual void generate(const Mysqlx::Expr::Array &arg) const;
  virtual void generate(const Mysqlx::Datatypes::Any &arg) const;
  virtual void generate(const Mysqlx::Datatypes::Scalar &arg) const;
  virtual void generate(const Mysqlx::Datatypes::Scalar::Octets &arg) const;
  virtual void generate(const Mysqlx::Datatypes::Array &arg) const;
  virtual void generate(const Mysqlx::Datatypes::Object &arg) const;
  virtual void generate(const Mysqlx::Datatypes::Object_ObjectField &arg) const;

  template <typename T>
  void generate_for_each(
      const Repeated_field_list<T> &list,
      void (Expression_generator::*generate_fun)(const T &) const,
      const typename Repeated_field_list<T>::size_type offset = 0) const;
  void generate_unquote_param(const Mysqlx::Expr::Expr &arg) const;
  void generate_json_literal_param(const Mysqlx::Datatypes::Scalar &arg) const;
  void generate_json_only_param(const Mysqlx::Expr::Expr &arg,
                                const std::string &expr_name) const;
  void generate_placeholder(const Placeholder &arg,
                            void (Expression_generator::*generate_fun)(
                                const Mysqlx::Datatypes::Scalar &) const) const;

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
  void overlaps_expression(const Mysqlx::Expr::Operator &arg,
                           const char *str) const;

  virtual void handle_object_field(
      const Mysqlx::Datatypes::Object::ObjectField &arg) const;
  virtual void handle_string_scalar(
      const Mysqlx::Datatypes::Scalar &string_scalar) const;
  virtual void handle_bool_scalar(
      const Mysqlx::Datatypes::Scalar &bool_scalar) const;

  Query_string_builder *m_qb;
  const Arg_list &m_args;
  const std::string &m_default_schema;
  const bool &m_is_relational;
  Prep_stmt_placeholder_list *m_placeholders{nullptr};
};

template <typename T>
void Expression_generator::generate_for_each(
    const Repeated_field_list<T> &list,
    void (Expression_generator::*generate_fun)(const T &) const,
    const typename Repeated_field_list<T>::size_type offset) const {
  if (list.size() == 0) return;
  using It = typename Repeated_field_list<T>::const_iterator;
  It end = list.end() - 1;
  for (It i = list.begin() + offset; i != end; ++i) {
    (this->*generate_fun)(*i);
    m_qb->put(",");
  }
  (this->*generate_fun)(*end);
}

template <typename T>
void generate_expression(
    Query_string_builder *qb, const T &expr,
    const Expression_generator::Arg_list &args,
    const std::string &default_schema, const bool is_relational,
    Expression_generator::Prep_stmt_placeholder_list *ids = nullptr) {
  Expression_generator gen(qb, args, default_schema, is_relational);
  gen.set_prep_stmt_placeholder_list(ids);
  gen.feed(expr);
}

template <typename T>
ngs::PFS_string generate_expression(
    const T &expr, const Expression_generator::Arg_list &args,
    const std::string &default_schema, const bool is_relational,
    Expression_generator::Prep_stmt_placeholder_list *ids = nullptr) {
  Query_string_builder qb;
  generate_expression(&qb, expr, args, default_schema, is_relational, ids);
  return qb.get();
}

template <typename T>
void generate_expression(Query_string_builder *qb, const T &expr,
                         const std::string &default_schema,
                         const bool is_relational) {
  generate_expression(qb, expr, Expression_generator::Arg_list(),
                      default_schema, is_relational);
}

template <typename T>
ngs::PFS_string generate_expression(
    const T &expr, const std::string &default_schema, const bool is_relational,
    Expression_generator::Prep_stmt_placeholder_list *ids = nullptr) {
  return generate_expression(expr, Expression_generator::Arg_list(),
                             default_schema, is_relational, ids);
}

template <typename T>
inline bool is_table_data_model(const T &msg) {
  return msg.data_model() == ::Mysqlx::Crud::TABLE;
}
}  // namespace xpl

#endif  // PLUGIN_X_SRC_EXPR_GENERATOR_H_
