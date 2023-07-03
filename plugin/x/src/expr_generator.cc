/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/src/expr_generator.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/helper/to_string.h"
#include "plugin/x/src/json_utils.h"
#include "plugin/x/src/mysql_function_names.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_regex.h"

namespace xpl {
using Placeholder_type = Placeholder_info::Type;

Expression_generator::Error::Error(int error_code, const std::string &message)
    : std::invalid_argument(message), m_error(error_code) {}

void Expression_generator::generate(const Mysqlx::Expr::Expr &arg) const {
  switch (arg.type()) {
    case Mysqlx::Expr::Expr::IDENT:
      generate(arg.identifier());
      break;

    case Mysqlx::Expr::Expr::LITERAL:
      generate(arg.literal());
      break;

    case Mysqlx::Expr::Expr::VARIABLE:
      // m_qb->put("@").quote_identifier(arg.variable());
      // break;
      throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                  "Mysqlx::Expr::Expr::VARIABLE is not supported yet");

    case Mysqlx::Expr::Expr::FUNC_CALL:
      generate(arg.function_call());
      break;

    case Mysqlx::Expr::Expr::OPERATOR:
      generate(arg.operator_());
      break;

    case Mysqlx::Expr::Expr::PLACEHOLDER:
      generate(arg.position());
      break;

    case Mysqlx::Expr::Expr::OBJECT:
      generate(arg.object());
      break;

    case Mysqlx::Expr::Expr::ARRAY:
      generate(arg.array());
      break;

    default:
      throw Error(
          ER_X_EXPR_BAD_TYPE_VALUE,
          "Invalid value for Mysqlx::Expr::Expr_Type " + to_string(arg.type()));
  }
}

void Expression_generator::generate(const Mysqlx::Expr::Identifier &arg,
                                    bool is_function) const {
  if (!m_default_schema.empty() &&
      (!arg.has_schema_name() || arg.schema_name().empty())) {
    // automatically prefix with the schema name
    if (!is_function || !is_native_mysql_function(arg.name()))
      m_qb->quote_identifier_if_needed(m_default_schema).dot();
  }

  if (arg.has_schema_name() && !arg.schema_name().empty())
    m_qb->quote_identifier(arg.schema_name()).dot();

  m_qb->quote_identifier_if_needed(arg.name());
}

void Expression_generator::generate(
    const Mysqlx::Expr::ColumnIdentifier &arg) const {
  bool has_schema_name = arg.has_schema_name() && !arg.schema_name().empty();

  if (has_schema_name && !arg.has_table_name())
    throw Error(ER_X_EXPR_MISSING_ARG,
                "Table name is required if schema name is specified in "
                "ColumnIdentifier.");

  const bool has_docpath = arg.document_path_size() > 0;

  if (arg.has_table_name() && !arg.has_name() &&
      (m_is_relational || !has_docpath))
    throw Error(ER_X_EXPR_MISSING_ARG,
                "Column name is required if table name is specified in "
                "ColumnIdentifier.");

  if (!has_docpath && !arg.has_name() && !arg.has_table_name() &&
      !arg.has_schema_name()) {
    if (m_is_relational) {
      throw Error(ER_X_EXPR_MISSING_ARG,
                  "Column name is required in ColumnIdentifier.");
    } else {
      m_qb->put("JSON_EXTRACT(doc,'$')");
      return;
    }
  }

  if (has_docpath) m_qb->put("JSON_EXTRACT(");

  if (has_schema_name) m_qb->quote_identifier(arg.schema_name()).dot();

  if (arg.has_table_name()) m_qb->quote_identifier(arg.table_name()).dot();

  if (arg.has_name()) m_qb->quote_identifier(arg.name());

  if (has_docpath) {
    if (!arg.has_name()) m_qb->put("doc");

    m_qb->put(",");
    generate(arg.document_path());
    m_qb->put(")");
  }
}

void Expression_generator::generate(const Document_path &arg) const {
  using ::Mysqlx::Expr::DocumentPathItem;

  if (arg.size() == 1 && arg.Get(0).type() == DocumentPathItem::MEMBER &&
      arg.Get(0).value().empty()) {
    m_qb->quote_string("$");
    return;
  }

  m_qb->bquote().put("$");
  for (Document_path::const_iterator item = arg.begin(); item != arg.end();
       ++item) {
    switch (item->type()) {
      case DocumentPathItem::MEMBER:
        if (item->value().empty())
          throw Error(
              ER_X_EXPR_BAD_VALUE,
              "Invalid empty value for Mysqlx::Expr::DocumentPathItem::MEMBER");
        m_qb->dot().put(quote_json_if_needed(item->value()));
        break;
      case DocumentPathItem::MEMBER_ASTERISK:
        m_qb->put(".*");
        break;
      case DocumentPathItem::ARRAY_INDEX:
        m_qb->put("[").put(item->index()).put("]");
        break;
      case DocumentPathItem::ARRAY_INDEX_ASTERISK:
        m_qb->put("[*]");
        break;
      case DocumentPathItem::DOUBLE_ASTERISK:
        m_qb->put("**");
        break;
      default:
        throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                    "Invalid value for Mysqlx::Expr::DocumentPathItem::Type " +
                        to_string(item->type()));
    }
  }
  m_qb->equote();
}

void Expression_generator::generate(
    const Mysqlx::Expr::FunctionCall &arg) const {
  generate(arg.name(), true);
  m_qb->put("(");
  if (is_native_mysql_json_function(arg.name().name()))
    generate_for_each(arg.param(), &Expression_generator::generate);
  else
    generate_for_each(arg.param(),
                      &Expression_generator::generate_unquote_param);
  m_qb->put(")");
}

void Expression_generator::generate(const Mysqlx::Datatypes::Any &arg) const {
  switch (arg.type()) {
    case Mysqlx::Datatypes::Any::SCALAR:
      generate(arg.scalar());
      break;

    case Mysqlx::Datatypes::Any::ARRAY:
      generate(arg.array());
      break;

    case Mysqlx::Datatypes::Any::OBJECT:
      generate(arg.obj());
      break;

    default:
      throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                  "Invalid value for Mysqlx::Datatypes::Any::Type " +
                      to_string(arg.type()));
  }
}

void Expression_generator::generate(
    const Mysqlx::Datatypes::Scalar &arg) const {
  switch (arg.type()) {
    case Mysqlx::Datatypes::Scalar::V_UINT:
      m_qb->put(arg.v_unsigned_int());
      break;

    case Mysqlx::Datatypes::Scalar::V_SINT:
      m_qb->put(arg.v_signed_int());
      break;

    case Mysqlx::Datatypes::Scalar::V_NULL:
      m_qb->put("NULL");
      break;

    case Mysqlx::Datatypes::Scalar::V_OCTETS:
      generate(arg.v_octets());
      break;

    case Mysqlx::Datatypes::Scalar::V_STRING:
      if (arg.v_string().has_collation()) {
        // TODO(bob) handle _utf8'string' type charset specification... but 1st
        // validate charset for alnum_
        // m_qb->put("_").put(arg.v_string().charset());
      }
      handle_string_scalar(arg);
      break;

    case Mysqlx::Datatypes::Scalar::V_DOUBLE:
      m_qb->put(arg.v_double());
      break;

    case Mysqlx::Datatypes::Scalar::V_FLOAT:
      m_qb->put(arg.v_float());
      break;

    case Mysqlx::Datatypes::Scalar::V_BOOL:
      handle_bool_scalar(arg);
      break;

    default:
      throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                  "Invalid value for Mysqlx::Datatypes::Scalar::Type " +
                      to_string(arg.type()));
  }
}

void Expression_generator::generate(
    const Mysqlx::Datatypes::Scalar::Octets &arg) const {
  switch (arg.content_type()) {
    case CT_PLAIN:
      m_qb->quote_string(arg.value());
      break;

    case CT_GEOMETRY:
      m_qb->put("ST_GEOMETRYFROMWKB(").quote_string(arg.value()).put(")");
      break;

    case CT_JSON:
      m_qb->put("CAST(").quote_string(arg.value()).put(" AS JSON)");
      break;

    case CT_XML:
      m_qb->quote_string(arg.value());
      break;

    default:
      throw Error(
          ER_X_EXPR_BAD_TYPE_VALUE,
          "Invalid content type for Mysqlx::Datatypes::Scalar::Octets " +
              to_string(arg.content_type()));
  }
}

void Expression_generator::generate(const Placeholder &arg) const {
  if (arg < static_cast<Placeholder>(m_args.size())) {
    generate(m_args.Get(arg));
    return;
  }
  if (!is_prep_stmt_mode())
    throw Error(ER_X_EXPR_BAD_VALUE, "Invalid value of placeholder");
  m_placeholders->emplace_back(arg - static_cast<Placeholder>(m_args.size()),
                               Placeholder_type::k_raw);
  m_qb->put("?");
}

void Expression_generator::generate(const Mysqlx::Expr::Object &arg) const {
  m_qb->put("JSON_OBJECT(");
  generate_for_each(arg.fld(), &Expression_generator::generate);
  m_qb->put(")");
}

void Expression_generator::generate(
    const Mysqlx::Expr::Object::ObjectField &arg) const {
  if (!arg.has_key() || arg.key().empty())
    throw Error(ER_X_EXPR_BAD_VALUE, "Invalid key for Mysqlx::Expr::Object");
  if (!arg.has_value())
    throw Error(
        ER_X_EXPR_BAD_VALUE,
        "Invalid value for Mysqlx::Expr::Object on key '" + arg.key() + "'");
  m_qb->quote_string(arg.key()).put(",");
  generate(arg.value());
}

void Expression_generator::generate(const Mysqlx::Expr::Array &arg) const {
  m_qb->put("JSON_ARRAY(");
  generate_for_each(arg.value(), &Expression_generator::generate);
  m_qb->put(")");
}

void Expression_generator::generate(
    const Mysqlx::Datatypes::Object &arg) const {
  m_qb->put("JSON_OBJECT(");
  generate_for_each(arg.fld(), &Expression_generator::generate);
  m_qb->put(")");
}

void Expression_generator::generate(
    const Mysqlx::Datatypes::Object::ObjectField &arg) const {
  if (!arg.has_key() || arg.key().empty())
    throw Error(ER_X_EXPR_BAD_VALUE,
                "Invalid key for Mysqlx::Datatypes::Object");
  if (!arg.has_value())
    throw Error(ER_X_EXPR_BAD_VALUE,
                "Invalid value for Mysqlx::Datatypes::Object on key '" +
                    arg.key() + "'");
  handle_object_field(arg);
}

void Expression_generator::generate(const Mysqlx::Datatypes::Array &arg) const {
  m_qb->put("JSON_ARRAY(");
  generate_for_each(arg.value(), &Expression_generator::generate);
  m_qb->put(")");
}

void Expression_generator::generate_unquote_param(
    const Mysqlx::Expr::Expr &arg) const {
  if (arg.type() == Mysqlx::Expr::Expr::IDENT &&
      arg.identifier().document_path_size() > 0) {
    m_qb->put("JSON_UNQUOTE(");
    generate(arg);
    m_qb->put(")");
  } else {
    generate(arg);
  }
}

/**
 * check if argument is a doc-path refering to '_id'.
 */
static bool is_id_docpath(const Mysqlx::Expr::Expr &arg) {
  if (arg.type() == Mysqlx::Expr::Expr::IDENT &&
      arg.identifier().document_path_size() == 1) {
    const auto &p = arg.identifier().document_path(0);

    return (p.has_type() && p.has_value() &&
            p.type() == Mysqlx::Expr::DocumentPathItem_Type_MEMBER &&
            p.value() == "_id");
  }
  return false;
}

static bool binary_operator_is_compare(std::string_view op) {
  const std::array<std::string_view, 6> compare_ops{{
      "==",
      "!=",
      "<",
      "<=",
      ">",
      ">=",
  }};

  return std::find(compare_ops.begin(), compare_ops.end(), op) !=
         compare_ops.end();
}

void Expression_generator::binary_operator(const Mysqlx::Expr::Operator &arg,
                                           const char *str) const {
  if (arg.param_size() != 2) {
    throw Error(
        ER_X_EXPR_BAD_NUM_ARGS,
        "Binary operations require exactly two operands in expression.");
  }

  /* if the argument is a doc-path{"_id"} and the operator is a comparison,
   * unquote the extracted doc-path to force a string-comparison that's resolved
   * by the index on the '_id' field.
   *
   * index is:
   *
   *   _id VARBINARY(32) GENERATED ALWAYS AS
   *        (json_unquote(json_extract(doc, _utf8mb4'$._id'))) STORED NOT NULL,
   */
  auto generate_unquoted_id_path = [this](const Mysqlx::Expr::Expr &param,
                                          bool operator_is_compare) {
    if (operator_is_compare && is_id_docpath(param)) {
      generate_unquote_param(param);
    } else {
      generate(param);
    }
  };

  const bool operator_is_compare = binary_operator_is_compare(arg.name());

  m_qb->put("(");
  generate_unquoted_id_path(arg.param(0), operator_is_compare);
  m_qb->put(str);
  generate_unquoted_id_path(arg.param(1), operator_is_compare);
  m_qb->put(")");
}

void Expression_generator::unary_operator(const Mysqlx::Expr::Operator &arg,
                                          const char *str) const {
  if (arg.param_size() != 1) {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "Unary operations require exactly one operand in expression.");
  }

  m_qb->put("(").put(str);
  generate(arg.param(0));
  m_qb->put(")");
}

namespace {

inline bool is_array(const Mysqlx::Expr::Expr &arg) {
  return arg.type() == Mysqlx::Expr::Expr::ARRAY;
}

inline bool is_literal(const Mysqlx::Expr::Expr &arg) {
  return arg.type() == Mysqlx::Expr::Expr::LITERAL;
}

inline bool is_octets(const Mysqlx::Expr::Expr &arg) {
  return is_literal(arg) &&
         arg.literal().type() == Mysqlx::Datatypes::Scalar::V_OCTETS &&
         arg.literal().has_v_octets();
}

inline bool is_octets(const Mysqlx::Expr::Expr &arg,
                      const Expression_generator::Octets_content_type type) {
  return is_octets(arg) && arg.literal().v_octets().content_type() == type;
}

inline bool is_cast_to_json(const Mysqlx::Expr::Operator &arg) {
  return to_upper(arg.name()) == "CAST" && arg.param_size() > 0 &&
         is_octets(arg.param(1), Expression_generator::CT_PLAIN) &&
         to_upper(arg.param(1).literal().v_octets().value()) == "JSON";
}

inline bool is_json_function_call(const Mysqlx::Expr::FunctionCall &arg) {
  return arg.has_name() && arg.name().has_name() &&
         does_return_json_mysql_function(arg.name().name());
}

}  // namespace

void Expression_generator::in_expression(const Mysqlx::Expr::Operator &arg,
                                         const char *str) const {
  switch (arg.param_size()) {
    case 0:
    case 1:
      throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                  "IN expression requires at least two parameters.");

    case 2:
      if (is_array(arg.param(1))) {
        m_qb->put(str).put("JSON_CONTAINS(");
        generate(arg.param(1));
        m_qb->put(",");
        if (is_literal(arg.param(0)))
          generate_json_literal_param(arg.param(0).literal());
        else
          generate(arg.param(0));
        m_qb->put(")");
        break;
      }
      [[fallthrough]];

    default:
      m_qb->put("(");
      generate_unquote_param(arg.param(0));
      m_qb->put(" ").put(str).put("IN (");
      generate_for_each(arg.param(),
                        &Expression_generator::generate_unquote_param, 1);
      m_qb->put("))");
  }
}

void Expression_generator::generate_json_literal_param(
    const Mysqlx::Datatypes::Scalar &arg) const {
  switch (arg.type()) {
    case Mysqlx::Datatypes::Scalar::V_STRING:
      m_qb->put("JSON_QUOTE(");
      generate(arg);
      m_qb->put(")");
      break;

    case Mysqlx::Datatypes::Scalar::V_OCTETS:
      if (arg.v_octets().content_type() == Expression_generator::CT_JSON) {
        generate(arg);
      } else {
        m_qb->put("JSON_QUOTE(");
        generate(arg);
        m_qb->put(")");
      }
      break;

    case Mysqlx::Datatypes::Scalar::V_NULL:
      m_qb->put("CAST('null' AS JSON)");
      break;

    default:
      m_qb->put("CAST(");
      generate(arg);
      m_qb->put(" AS JSON)");
  }
}

void Expression_generator::generate_json_only_param(
    const Mysqlx::Expr::Expr &arg, const std::string &expr_name) const {
  switch (arg.type()) {
    case Mysqlx::Expr::Expr::LITERAL:
      generate_json_literal_param(arg.literal());
      break;

    case Mysqlx::Expr::Expr::FUNC_CALL:
      if (!is_json_function_call(arg.function_call()))
        throw Error(ER_X_EXPR_BAD_VALUE, expr_name +
                                             " expression requires function"
                                             " that produce a JSON value.");
      generate(arg);
      break;

    case Mysqlx::Expr::Expr::OPERATOR:
      if (!is_cast_to_json(arg.operator_()))
        throw Error(ER_X_EXPR_BAD_VALUE, expr_name +
                                             " expression requires operator"
                                             " that produce a JSON value.");
      generate(arg);
      break;

    case Mysqlx::Expr::Expr::PLACEHOLDER:
      if (arg.position() < static_cast<Placeholder>(m_args.size())) {
        generate_json_literal_param(m_args.Get(arg.position()));
        break;
      }
      if (!is_prep_stmt_mode())
        throw Error(ER_X_EXPR_BAD_VALUE, "Invalid value of placeholder");
      m_placeholders->emplace_back(
          arg.position() - static_cast<Placeholder>(m_args.size()),
          Placeholder_type::k_json);
      m_qb->put("CAST(? AS JSON)");
      break;

    default:
      generate(arg);
  }
}

void Expression_generator::cont_in_expression(const Mysqlx::Expr::Operator &arg,
                                              const char *str) const {
  if (arg.param_size() != 2)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "CONT_IN expression requires two parameters.");

  m_qb->put(str).put("JSON_CONTAINS(");
  generate_json_only_param(arg.param(1), "CONT_IN");
  m_qb->put(",");
  generate_json_only_param(arg.param(0), "CONT_IN");
  m_qb->put(")");
}

void Expression_generator::like_expression(const Mysqlx::Expr::Operator &arg,
                                           const char *str) const {
  int paramSize = arg.param_size();

  if (paramSize != 2 && paramSize != 3) {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "LIKE expression requires exactly two or three parameters.");
  }

  m_qb->put("(");
  generate_unquote_param(arg.param(0));
  m_qb->put(str);
  generate_unquote_param(arg.param(1));
  if (paramSize == 3) {
    m_qb->put(" ESCAPE ");
    generate_unquote_param(arg.param(2));
  }
  m_qb->put(")");
}

void Expression_generator::between_expression(const Mysqlx::Expr::Operator &arg,
                                              const char *str) const {
  if (arg.param_size() != 3) {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "BETWEEN expression requires exactly three parameters.");
  }

  m_qb->put("(");
  generate_unquote_param(arg.param(0));
  m_qb->put(str);
  generate_unquote_param(arg.param(1));
  m_qb->put(" AND ");
  generate_unquote_param(arg.param(2));
  m_qb->put(")");
}

namespace {

struct Interval_unit_validator {
  explicit Interval_unit_validator(const char *const error_msg)
      : m_error_msg(error_msg) {}

  bool operator()(const char *source) const {
    // keep patterns in asc order
    static const char *const patterns[] = {"DAY",
                                           "DAY_HOUR",
                                           "DAY_MICROSECOND",
                                           "DAY_MINUTE",
                                           "DAY_SECOND",
                                           "HOUR",
                                           "HOUR_MICROSECOND",
                                           "HOUR_MINUTE",
                                           "HOUR_SECOND",
                                           "MICROSECOND",
                                           "MINUTE",
                                           "MINUTE_MICROSECOND",
                                           "MINUTE_SECOND",
                                           "MONTH",
                                           "QUARTER",
                                           "SECOND",
                                           "SECOND_MICROSECOND",
                                           "WEEK",
                                           "YEAR",
                                           "YEAR_MONTH"};

    return std::binary_search(std::begin(patterns), std::end(patterns), source,
                              Is_less());
  }

  const char *const m_error_msg;
};

struct Cast_type_validator {
  explicit Cast_type_validator(const char *const error_msg)
      : m_error_msg(error_msg) {}

  bool operator()(const char *str) const {
    static const xpl::Regex re(
        "BINARY(?:\\([[:digit:]]+\\))?|"
        "DATE|DATETIME|TIME|JSON|"
        "CHAR(?:\\([[:digit:]]+\\))?|"
        "DECIMAL(?:\\([[:digit:]]+(?:,[[:digit:]]+)?\\))?|"
        "SIGNED(?: INTEGER)?|UNSIGNED(?: INTEGER)?");
    return re.match(str);
  }

  const char *const m_error_msg;
};

template <typename V>
const std::string &get_valid_string(const Expression_generator::Expr &expr,
                                    const V &is_valid) {
  if (!is_octets(expr, Expression_generator::CT_PLAIN) ||
      !is_valid(expr.literal().v_octets().value().c_str()))
    throw Expression_generator::Error(ER_X_EXPR_BAD_VALUE,
                                      is_valid.m_error_msg);

  return expr.literal().v_octets().value();
}

}  // namespace

void Expression_generator::date_expression(const Mysqlx::Expr::Operator &arg,
                                           const char *str) const {
  if (arg.param_size() != 3)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "DATE expression requires exactly three parameters.");

  m_qb->put(str).put("(");
  generate_unquote_param(arg.param(0));
  m_qb->put(", INTERVAL ");
  generate_unquote_param(arg.param(1));
  m_qb->put(" ");
  m_qb->put(get_valid_string(
      arg.param(2), Interval_unit_validator("DATE interval unit invalid.")));
  m_qb->put(")");
}

void Expression_generator::cast_expression(
    const Mysqlx::Expr::Operator &arg) const {
  if (arg.param_size() != 2)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "CAST expression requires exactly two parameters.");

  std::string as_type =
      get_valid_string(arg.param(1), Cast_type_validator("CAST type invalid."));

  m_qb->put("CAST(");
  if (is_prep_stmt_mode() && as_type == "JSON" &&
      arg.param(0).type() == Mysqlx::Expr::Expr::PLACEHOLDER &&
      arg.param(0).position() >= static_cast<Placeholder>(m_args.size())) {
    m_placeholders->emplace_back(
        arg.param(0).position() - static_cast<Placeholder>(m_args.size()),
        Placeholder_type::k_json);
    m_qb->put("?");
  } else {
    generate_unquote_param(arg.param(0));
  }
  m_qb->put(" AS ");
  m_qb->put(as_type);
  m_qb->put(")");
}

void Expression_generator::binary_expression(const Mysqlx::Expr::Operator &arg,
                                             const char *str) const {
  if (arg.param_size() != 2) {
    throw Error(
        ER_X_EXPR_BAD_NUM_ARGS,
        "Binary operations require exactly two operands in expression.");
  }

  m_qb->put("(");
  generate_unquote_param(arg.param(0));
  m_qb->put(str);
  generate_unquote_param(arg.param(1));
  m_qb->put(")");
}

namespace {
using Operator_ptr = std::function<void(const Expression_generator *,
                                        const Mysqlx::Expr::Operator &)>;
using Operator_bind = std::pair<const char *const, Operator_ptr>;

struct Is_operator_less {
  bool operator()(const Operator_bind &pattern,
                  const std::string &value) const {
    return std::strcmp(pattern.first, value.c_str()) < 0;
  }
};

}  // namespace

void Expression_generator::generate(const Mysqlx::Expr::Operator &arg) const {
  using std::placeholders::_1;
  using std::placeholders::_2;
  using Gen = Expression_generator;

  // keep binding in asc order
  static const Operator_bind operators[] = {
      {"!", std::bind(&Gen::unary_operator, _1, _2, "!")},
      {"!=", std::bind(&Gen::binary_operator, _1, _2, " != ")},
      {"%", std::bind(&Gen::binary_operator, _1, _2, " % ")},
      {"&", std::bind(&Gen::binary_operator, _1, _2, " & ")},
      {"&&", std::bind(&Gen::binary_operator, _1, _2, " AND ")},
      {"*", std::bind(&Gen::asterisk_operator, _1, _2)},
      {"+", std::bind(&Gen::binary_operator, _1, _2, " + ")},
      {"-", std::bind(&Gen::binary_operator, _1, _2, " - ")},
      {"/", std::bind(&Gen::binary_operator, _1, _2, " / ")},
      {"<", std::bind(&Gen::binary_operator, _1, _2, " < ")},
      {"<<", std::bind(&Gen::binary_operator, _1, _2, " << ")},
      {"<=", std::bind(&Gen::binary_operator, _1, _2, " <= ")},
      {"==", std::bind(&Gen::binary_operator, _1, _2, " = ")},
      {">", std::bind(&Gen::binary_operator, _1, _2, " > ")},
      {">=", std::bind(&Gen::binary_operator, _1, _2, " >= ")},
      {">>", std::bind(&Gen::binary_operator, _1, _2, " >> ")},
      {"^", std::bind(&Gen::binary_operator, _1, _2, " ^ ")},
      {"between", std::bind(&Gen::between_expression, _1, _2, " BETWEEN ")},
      {"cast", std::bind(&Gen::cast_expression, _1, _2)},
      {"cont_in", std::bind(&Gen::cont_in_expression, _1, _2, "")},
      {"date_add", std::bind(&Gen::date_expression, _1, _2, "DATE_ADD")},
      {"date_sub", std::bind(&Gen::date_expression, _1, _2, "DATE_SUB")},
      {"default", std::bind(&Gen::nullary_operator, _1, _2, "DEFAULT")},
      {"div", std::bind(&Gen::binary_operator, _1, _2, " DIV ")},
      {"in", std::bind(&Gen::in_expression, _1, _2, "")},
      {"is", std::bind(&Gen::binary_operator, _1, _2, " IS ")},
      {"is_not", std::bind(&Gen::binary_operator, _1, _2, " IS NOT ")},
      {"like", std::bind(&Gen::like_expression, _1, _2, " LIKE ")},
      {"not", std::bind(&Gen::unary_operator, _1, _2, "NOT ")},
      {"not_between",
       std::bind(&Gen::between_expression, _1, _2, " NOT BETWEEN ")},
      {"not_cont_in", std::bind(&Gen::cont_in_expression, _1, _2, "NOT ")},
      {"not_in", std::bind(&Gen::in_expression, _1, _2, "NOT ")},
      {"not_like", std::bind(&Gen::like_expression, _1, _2, " NOT LIKE ")},
      {"not_overlaps", std::bind(&Gen::overlaps_expression, _1, _2, "NOT ")},
      {"not_regexp",
       std::bind(&Gen::binary_expression, _1, _2, " NOT REGEXP ")},
      {"overlaps", std::bind(&Gen::overlaps_expression, _1, _2, "")},
      {"regexp", std::bind(&Gen::binary_expression, _1, _2, " REGEXP ")},
      {"sign_minus", std::bind(&Gen::unary_operator, _1, _2, "-")},
      {"sign_plus", std::bind(&Gen::unary_operator, _1, _2, "+")},
      {"xor", std::bind(&Gen::binary_operator, _1, _2, " XOR ")},
      {"|", std::bind(&Gen::binary_operator, _1, _2, " | ")},
      {"||", std::bind(&Gen::binary_operator, _1, _2, " OR ")},
      {"~", std::bind(&Gen::unary_operator, _1, _2, "~")}};

  const Operator_bind *op =
      std::lower_bound(std::begin(operators), std::end(operators), arg.name(),
                       Is_operator_less());

  if (op == std::end(operators) ||
      std::strcmp(arg.name().c_str(), op->first) != 0)
    throw Error(ER_X_EXPR_BAD_OPERATOR, "Invalid operator " + arg.name());

  op->second(this, arg);
}

void Expression_generator::asterisk_operator(
    const Mysqlx::Expr::Operator &arg) const {
  switch (arg.param_size()) {
    case 0:
      m_qb->put("*");
      break;

    case 2:
      m_qb->put("(");
      generate_unquote_param(arg.param(0));
      m_qb->put(" * ");
      generate_unquote_param(arg.param(1));
      m_qb->put(")");
      break;

    default:
      throw Error(
          ER_X_EXPR_BAD_NUM_ARGS,
          "Asterisk operator require zero or two operands in expression");
  }
}

void Expression_generator::nullary_operator(const Mysqlx::Expr::Operator &arg,
                                            const char *str) const {
  if (arg.param_size() != 0)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "Nullary operator require no operands in expression");

  m_qb->put(str);
}

Expression_generator Expression_generator::clone(
    Query_string_builder *qb) const {
  return Expression_generator(qb, m_args, m_default_schema, m_is_relational);
}

void Expression_generator::overlaps_expression(
    const Mysqlx::Expr::Operator &arg, const char *str) const {
  if (arg.param_size() != 2)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "OVERLAPS expression requires two parameters.");

  m_qb->put(str).put("JSON_OVERLAPS(");
  generate_json_only_param(arg.param(0), "OVERLAPS");
  m_qb->put(",");
  generate_json_only_param(arg.param(1), "OVERLAPS");
  m_qb->put(")");
}

void Expression_generator::handle_object_field(
    const Mysqlx::Datatypes::Object::ObjectField &arg) const {
  m_qb->quote_string(arg.key()).put(",");
  generate(arg.value());
}

void Expression_generator::handle_string_scalar(
    const Mysqlx::Datatypes::Scalar &string_scalar) const {
  m_qb->quote_string(string_scalar.v_string().value());
}

void Expression_generator::handle_bool_scalar(
    const Mysqlx::Datatypes::Scalar &bool_scalar) const {
  m_qb->put((bool_scalar.v_bool() ? "TRUE" : "FALSE"));
}

}  // namespace xpl
