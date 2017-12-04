/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "expr_generator.h"

#include <algorithm>
#include "json_utils.h"
#include "ngs_common/bind.h"
#include "ngs_common/to_string.h"
#include "xpl_error.h"
#include "xpl_regex.h"
#include "mysql_function_names.h"

namespace xpl
{

Expression_generator::Error::Error(int error_code, const std::string& message)
: std::invalid_argument(message), m_error(error_code)
{}


void Expression_generator::generate(const Mysqlx::Expr::Expr &arg) const
{
  switch(arg.type())
  {
  case Mysqlx::Expr::Expr::IDENT:
    generate(arg.identifier());
    break;

  case Mysqlx::Expr::Expr::LITERAL:
    generate(arg.literal());
    break;

  case Mysqlx::Expr::Expr::VARIABLE:
    //m_qb.put("@").quote_identifier(arg.variable());
    //break;
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
    throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                "Invalid value for Mysqlx::Expr::Expr_Type "+ngs::to_string(arg.type()));
  }
}


void Expression_generator::generate(const Mysqlx::Expr::Identifier& arg, bool is_function) const
{
  if (!m_default_schema.empty() &&
      (!arg.has_schema_name() || arg.schema_name().empty()))
  {
    // automatically prefix with the schema name
    if (!is_function || !is_native_mysql_function(arg.name()))
      m_qb.quote_identifier_if_needed(m_default_schema).dot();
  }

  if (arg.has_schema_name() && !arg.schema_name().empty())
    m_qb.quote_identifier(arg.schema_name()).dot();

  m_qb.quote_identifier_if_needed(arg.name());
}


void Expression_generator::generate(const Mysqlx::Expr::ColumnIdentifier &arg) const
{
  bool has_schema_name = arg.has_schema_name() && !arg.schema_name().empty();

  if (has_schema_name && arg.has_table_name() == false)
    throw Error(ER_X_EXPR_MISSING_ARG,
                "Table name is required if schema name is specified in ColumnIdentifier.");

  const bool has_docpath = arg.document_path_size() > 0;

  if (arg.has_table_name() && arg.has_name() == false &&
      (m_is_relational || !has_docpath))
    throw Error(ER_X_EXPR_MISSING_ARG,
                "Column name is required if table name is specified in ColumnIdentifier.");

  if (has_docpath)
    m_qb.put("JSON_EXTRACT(");

  if (has_schema_name)
    m_qb.quote_identifier(arg.schema_name()).dot();

  if (arg.has_table_name())
    m_qb.quote_identifier(arg.table_name()).dot();

  if (arg.has_name())
    m_qb.quote_identifier(arg.name());

  if (has_docpath)
  {
    if(arg.has_name() == false)
      m_qb.put("doc");

    m_qb.put(",");
    generate(arg.document_path());
    m_qb.put(")");
  }
}


void Expression_generator::generate(const Document_path &arg) const
{
  using ::Mysqlx::Expr::DocumentPathItem;

  if (arg.size() == 1 &&
      arg.Get(0).type() == DocumentPathItem::MEMBER &&
      arg.Get(0).value().empty())
  {
    m_qb.quote_string("$");
    return;
  }

  m_qb.bquote().put("$");
  for (Document_path::const_iterator item = arg.begin(); item != arg.end(); ++item)
  {
    switch (item->type())
    {
    case DocumentPathItem::MEMBER:
      if (item->value().empty())
        throw Error(ER_X_EXPR_BAD_VALUE,
                    "Invalid empty value for Mysqlx::Expr::DocumentPathItem::MEMBER");
      m_qb.dot().put(quote_json_if_needed(item->value()));
      break;
    case DocumentPathItem::MEMBER_ASTERISK:
      m_qb.put(".*");
      break;
    case DocumentPathItem::ARRAY_INDEX:
      m_qb.put("[").put(item->index()).put("]");
      break;
    case DocumentPathItem::ARRAY_INDEX_ASTERISK:
      m_qb.put("[*]");
      break;
    case DocumentPathItem::DOUBLE_ASTERISK:
      m_qb.put("**");
      break;
    default:
      throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                  "Invalid value for Mysqlx::Expr::DocumentPathItem::Type " +
                  ngs::to_string(item->type()));
    }
  }
  m_qb.equote();
}


void Expression_generator::generate(const Mysqlx::Expr::FunctionCall &arg) const
{
  generate(arg.name(), true);
  m_qb.put("(");
  generate_for_each(arg.param(), &Expression_generator::generate_unquote_param);
  m_qb.put(")");
}


void Expression_generator::generate(const Mysqlx::Datatypes::Any &arg) const
{
  switch(arg.type())
  {
  case Mysqlx::Datatypes::Any::SCALAR:
    generate(arg.scalar());
    break;
  default:
    throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                "Invalid value for Mysqlx::Datatypes::Any::Type " +
                ngs::to_string(arg.type()));
  }
}


void Expression_generator::generate(const Mysqlx::Datatypes::Scalar &arg) const
{
  switch(arg.type())
  {
  case Mysqlx::Datatypes::Scalar::V_UINT:
    m_qb.put(arg.v_unsigned_int());
    break;

  case Mysqlx::Datatypes::Scalar::V_SINT:
    m_qb.put(arg.v_signed_int());
    break;

  case Mysqlx::Datatypes::Scalar::V_NULL:
    m_qb.put("NULL");
    break;

  case Mysqlx::Datatypes::Scalar::V_OCTETS:
    generate(arg.v_octets());
    break;

  case Mysqlx::Datatypes::Scalar::V_STRING:
    if (arg.v_string().has_collation())
    {
      //TODO handle _utf8'string' type charset specification... but 1st validate charset for alnum_
      //m_qb.put("_").put(arg.v_string().charset());
    }
    m_qb.quote_string(arg.v_string().value());
    break;

  case Mysqlx::Datatypes::Scalar::V_DOUBLE:
    m_qb.put(arg.v_double());
    break;

  case Mysqlx::Datatypes::Scalar::V_FLOAT:
    m_qb.put(arg.v_float());
    break;

  case Mysqlx::Datatypes::Scalar::V_BOOL:
    m_qb.put((arg.v_bool() ? "TRUE" : "FALSE"));
    break;

  default:
    throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                "Invalid value for Mysqlx::Datatypes::Scalar::Type " +
                ngs::to_string(arg.type()));
  }
}


void Expression_generator::generate(const Mysqlx::Datatypes::Scalar::Octets &arg) const
{
  switch (arg.content_type())
  {
  case CT_PLAIN:
    m_qb.quote_string(arg.value());
    break;

  case CT_GEOMETRY:
    m_qb.put("ST_GEOMETRYFROMWKB(").quote_string(arg.value()).put(")");
    break;

  case CT_JSON:
    m_qb.put("CAST(").quote_string(arg.value()).put(" AS JSON)");
    break;

  case CT_XML:
    m_qb.quote_string(arg.value());
    break;

  default:
    throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                "Invalid content type for Mysqlx::Datatypes::Scalar::Octets " +
                ngs::to_string(arg.content_type()));
  }
}


void Expression_generator::generate(const Placeholder &arg) const
{
  if (arg < static_cast<Placeholder>(m_args.size()))
    generate(m_args.Get(arg));
  else
    throw Error(ER_X_EXPR_BAD_VALUE, "Invalid value of placeholder");
}


void Expression_generator::generate(const Mysqlx::Expr::Object &arg) const
{
  m_qb.put("JSON_OBJECT(");
  generate_for_each(arg.fld(), &Expression_generator::generate);
  m_qb.put(")");
}


void Expression_generator::generate(const Mysqlx::Expr::Object::ObjectField &arg) const
{
  if (!arg.has_key() || arg.key().empty())
    throw Error(ER_X_EXPR_BAD_VALUE, "Invalid key for Mysqlx::Expr::Object");
  if (!arg.has_value())
    throw Error(ER_X_EXPR_BAD_VALUE, "Invalid value for Mysqlx::Expr::Object on key '" + arg.key() + "'");
  m_qb.quote_string(arg.key()).put(",");
  generate(arg.value());
}


void Expression_generator::generate(const Mysqlx::Expr::Array &arg) const
{
  m_qb.put("JSON_ARRAY(");
  generate_for_each(arg.value(), &Expression_generator::generate);
  m_qb.put(")");
}


template <typename T>
void Expression_generator::generate_for_each(const ::google::protobuf::RepeatedPtrField<T> &list,
                                             void (Expression_generator::*generate_fun)(const T&) const,
                                             const typename ::google::protobuf::RepeatedPtrField<T>::size_type offset) const
{
  if (list.size() == 0)
    return;
  typedef typename ::google::protobuf::RepeatedPtrField<T>::const_iterator It;
  It end = list.end() - 1;
  for (It i = list.begin() + offset; i != end; ++i)
  {
    (this->*generate_fun)(*i);
    m_qb.put(",");
  }
  (this->*generate_fun)(*end);
}


void Expression_generator::generate_unquote_param(const Mysqlx::Expr::Expr &arg) const
{
  if (arg.type() == Mysqlx::Expr::Expr::IDENT && arg.identifier().document_path_size() > 0)
  {
    m_qb.put("JSON_UNQUOTE(");
    generate(arg);
    m_qb.put(")");
  }
  else
    generate(arg);
}


void Expression_generator::binary_operator(const Mysqlx::Expr::Operator &arg, const char* str) const
{
  if(arg.param_size() != 2)
  {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "Binary operations require exactly two operands in expression.");
  }

  m_qb.put("(");
  generate(arg.param(0));
  m_qb.put(str);
  generate(arg.param(1));
  m_qb.put(")");
}


void Expression_generator::unary_operator(const Mysqlx::Expr::Operator &arg, const char* str) const
{
  if(arg.param_size() != 1)
  {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "Unary operations require exactly one operand in expression.");
  }

  m_qb.put("(").put(str);
  generate(arg.param(0));
  m_qb.put(")");
}


namespace
{

inline bool is_array(const Mysqlx::Expr::Expr &arg)
{
  return arg.type() == Mysqlx::Expr::Expr_Type_ARRAY;
}


inline bool is_octets(const Mysqlx::Expr::Expr &arg)
{
  return arg.type() == Mysqlx::Expr::Expr_Type_LITERAL &&
      arg.literal().type() == Mysqlx::Datatypes::Scalar_Type_V_OCTETS &&
      arg.literal().has_v_octets();
}


inline bool is_plain_octets(const Mysqlx::Expr::Expr &arg)
{
  return is_octets(arg) &&
      arg.literal().v_octets().content_type() == Expression_generator::CT_PLAIN;
}

}  // namespace


void Expression_generator::in_expression(const Mysqlx::Expr::Operator &arg, const char *str) const
{
  switch (arg.param_size())
  {
  case 0:
  case 1:
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "IN expression requires at least two parameters.");

  case 2:
    if (is_array(arg.param(1)))
    {
      m_qb.put(str).put("JSON_CONTAINS(");
      generate(arg.param(1));
      m_qb.put(",");
      if (is_octets(arg.param(0)))
      {
        m_qb.put("JSON_QUOTE(");
        generate(arg.param(0));
        m_qb.put("))");
      }
      else
      {
        m_qb.put("CAST(");
        generate(arg.param(0));
        m_qb.put(" AS JSON))");
      }
      break;
    }
    // Fall through.

  default:
    m_qb.put("(");
    generate_unquote_param(arg.param(0));
    m_qb.put(" ").put(str).put("IN (");
    generate_for_each(arg.param(), &Expression_generator::generate_unquote_param, 1);
    m_qb.put("))");
  }
}


void Expression_generator::like_expression(const Mysqlx::Expr::Operator &arg, const char *str) const
{
  int paramSize = arg.param_size();

  if(paramSize != 2 && paramSize != 3)
  {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "LIKE expression requires exactly two or three parameters.");
  }

  m_qb.put("(");
  generate_unquote_param(arg.param(0));
  m_qb.put(str);
  generate_unquote_param(arg.param(1));
  if(paramSize == 3)
  {
    m_qb.put(" ESCAPE ");
    generate_unquote_param(arg.param(2));
  }
  m_qb.put(")");
}


void Expression_generator::between_expression(const Mysqlx::Expr::Operator &arg, const char *str) const
{
  if(arg.param_size() != 3)
  {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "BETWEEN expression requires exactly three parameters.");
  }

  m_qb.put("(");
  generate_unquote_param(arg.param(0));
  m_qb.put(str);
  generate_unquote_param(arg.param(1));
  m_qb.put(" AND ");
  generate_unquote_param(arg.param(2));
  m_qb.put(")");
}


namespace
{

struct Interval_unit_validator
{
  Interval_unit_validator(const char* const error_msg)
  : m_error_msg(error_msg)
  {}

  bool operator() (const char* source) const
  {
    // keep patterns in asc order
    static const char* const patterns[] = {
        "DAY",
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
        "YEAR_MONTH"
    };
    static const char* const *patterns_end = get_array_end(patterns);

    return std::binary_search(patterns, patterns_end, source, Is_less());
  }

  const char* const m_error_msg;
};


struct Cast_type_validator
{
  Cast_type_validator(const char* const error_msg)
  : m_error_msg(error_msg)
  {}

  bool operator() (const char* str) const
  {
    static const xpl::Regex re(
        "^("
        "BINARY([[.left-parenthesis.]][[:digit:]]+[[.right-parenthesis.]])?|"
        "DATE|DATETIME|TIME|JSON|"
        "CHAR([[.left-parenthesis.]][[:digit:]]+[[.right-parenthesis.]])?|"
        "DECIMAL([[.left-parenthesis.]][[:digit:]]+(,[[:digit:]]+)?[[.right-parenthesis.]])?|"
        "SIGNED( INTEGER)?|UNSIGNED( INTEGER)?"
        "){1}$");

    return re.match(str);
  }

  const char* const m_error_msg;
};


template<typename V>
const std::string& get_valid_string(const Expression_generator::Expr &expr, const V& is_valid)
{
  if (!is_plain_octets(expr) || !is_valid(expr.literal().v_octets().value().c_str()))
    throw Expression_generator::Error(ER_X_EXPR_BAD_VALUE, is_valid.m_error_msg);

  return expr.literal().v_octets().value();
}

} // namespace


void Expression_generator::date_expression(const Mysqlx::Expr::Operator &arg, const char* str) const
{
  if(arg.param_size() != 3)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "DATE expression requires exactly three parameters.");

  m_qb.put(str).put("(");
  generate_unquote_param(arg.param(0));
  m_qb.put(", INTERVAL ");
  generate_unquote_param(arg.param(1));
  m_qb.put(" ");
  m_qb.put(get_valid_string(arg.param(2),
                            Interval_unit_validator("DATE interval unit invalid.")));
  m_qb.put(")");
}


void Expression_generator::cast_expression(const Mysqlx::Expr::Operator &arg) const
{
  if(arg.param_size() != 2)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "CAST expression requires exactly two parameters.");

  m_qb.put("CAST(");
  generate_unquote_param(arg.param(0));
  m_qb.put(" AS ");
  m_qb.put(get_valid_string(arg.param(1),
                            Cast_type_validator("CAST type invalid.")));
  m_qb.put(")");
}


void Expression_generator::binary_expression(const Mysqlx::Expr::Operator &arg, const char* str) const
{
  if(arg.param_size() != 2)
  {
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "Binary operations require exactly two operands in expression.");
  }

  m_qb.put("(");
  generate_unquote_param(arg.param(0));
  m_qb.put(str);
  generate_unquote_param(arg.param(1));
  m_qb.put(")");
}


namespace
{
typedef ngs::function<void (const Expression_generator*,
                            const Mysqlx::Expr::Operator&)> Operator_ptr;

typedef std::pair<const char* const, Operator_ptr> Operator_bind;

struct Is_operator_less
{
  bool operator() (const Operator_bind& pattern, const std::string& value) const
  {
    return std::strcmp(pattern.first, value.c_str()) < 0;
  }
};

} // namespace


void Expression_generator::generate(const Mysqlx::Expr::Operator &arg) const
{
  // keep binding in asc order
  static const Operator_bind operators[] = {
      std::make_pair("!", ngs::bind(&Expression_generator::unary_operator, ngs::placeholders::_1, ngs::placeholders::_2,  "!")),
      std::make_pair("!=", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " != ")),
      std::make_pair("%", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " % ")),
      std::make_pair("&", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " & ")),
      std::make_pair("&&", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " AND ")),
      std::make_pair("*", ngs::bind(&Expression_generator::asterisk_operator, ngs::placeholders::_1, ngs::placeholders::_2)),
      std::make_pair("+", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " + ")),
      std::make_pair("-", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " - ")),
      std::make_pair("/", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " / ")),
      std::make_pair("<", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " < ")),
      std::make_pair("<<", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " << ")),
      std::make_pair("<=", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " <= ")),
      std::make_pair("==", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " = ")),
      std::make_pair(">", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " > ")),
      std::make_pair(">=", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " >= ")),
      std::make_pair(">>", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " >> ")),
      std::make_pair("^", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " ^ ")),
      std::make_pair("between", ngs::bind(&Expression_generator::between_expression, ngs::placeholders::_1, ngs::placeholders::_2, " BETWEEN ")),
      std::make_pair("cast", ngs::bind(&Expression_generator::cast_expression, ngs::placeholders::_1, ngs::placeholders::_2)),
      std::make_pair("date_add", ngs::bind(&Expression_generator::date_expression, ngs::placeholders::_1, ngs::placeholders::_2, "DATE_ADD")),
      std::make_pair("date_sub", ngs::bind(&Expression_generator::date_expression, ngs::placeholders::_1, ngs::placeholders::_2, "DATE_SUB")),
      std::make_pair("default", ngs::bind(&Expression_generator::nullary_operator, ngs::placeholders::_1, ngs::placeholders::_2, "DEFAULT")),
      std::make_pair("div", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " DIV ")),
      std::make_pair("in", ngs::bind(&Expression_generator::in_expression, ngs::placeholders::_1, ngs::placeholders::_2, "")),
      std::make_pair("is", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " IS ")),
      std::make_pair("is_not", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " IS NOT ")),
      std::make_pair("like", ngs::bind(&Expression_generator::like_expression, ngs::placeholders::_1, ngs::placeholders::_2, " LIKE ")),
      std::make_pair("not", ngs::bind(&Expression_generator::unary_operator, ngs::placeholders::_1, ngs::placeholders::_2, "NOT ")),
      std::make_pair("not_between", ngs::bind(&Expression_generator::between_expression, ngs::placeholders::_1, ngs::placeholders::_2, " NOT BETWEEN ")),
      std::make_pair("not_in", ngs::bind(&Expression_generator::in_expression, ngs::placeholders::_1, ngs::placeholders::_2, "NOT ")),
      std::make_pair("not_like", ngs::bind(&Expression_generator::like_expression, ngs::placeholders::_1, ngs::placeholders::_2, " NOT LIKE ")),
      std::make_pair("not_regexp", ngs::bind(&Expression_generator::binary_expression, ngs::placeholders::_1, ngs::placeholders::_2, " NOT REGEXP ")),
      std::make_pair("regexp", ngs::bind(&Expression_generator::binary_expression, ngs::placeholders::_1, ngs::placeholders::_2, " REGEXP ")),
      std::make_pair("sign_minus", ngs::bind(&Expression_generator::unary_operator, ngs::placeholders::_1, ngs::placeholders::_2, "-")),
      std::make_pair("sign_plus", ngs::bind(&Expression_generator::unary_operator, ngs::placeholders::_1, ngs::placeholders::_2, "+")),
      std::make_pair("xor",ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " XOR ")),
      std::make_pair("|", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " | ")),
      std::make_pair("||", ngs::bind(&Expression_generator::binary_operator, ngs::placeholders::_1, ngs::placeholders::_2, " OR ")),
      std::make_pair("~", ngs::bind(&Expression_generator::unary_operator, ngs::placeholders::_1, ngs::placeholders::_2, "~"))
  };
  static const Operator_bind *operators_end = get_array_end(operators);

  const Operator_bind *op = std::lower_bound(operators, operators_end,
                                             arg.name(), Is_operator_less());

  if (op == operators_end || std::strcmp(arg.name().c_str(), op->first) != 0)
    throw Error(ER_X_EXPR_BAD_OPERATOR, "Invalid operator " + arg.name());

  op->second(this, arg);
}


void Expression_generator::asterisk_operator(const Mysqlx::Expr::Operator &arg) const
{
   switch (arg.param_size())
   {
   case 0:
     m_qb.put("*");
     break;

   case 2:
     m_qb.put("(");
     generate_unquote_param(arg.param(0));
     m_qb.put(" * ");
     generate_unquote_param(arg.param(1));
     m_qb.put(")");
     break;

   default:
     throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                 "Asterisk operator require zero or two operands in expression");
   }
}

void Expression_generator::nullary_operator(const Mysqlx::Expr::Operator &arg, const char* str) const
{
  if (arg.param_size() != 0)
    throw Error(ER_X_EXPR_BAD_NUM_ARGS,
                "Nullary operator require no operands in expression");

  m_qb.put(str);
}

Expression_generator Expression_generator::clone(Query_string_builder &qb) const
{
  return Expression_generator(qb, m_args, m_default_schema, m_is_relational);
}

} // namespace xpl
