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

#include <functional>

#include "plugin/x/src/statement_builder.h"

namespace xpl {

namespace {

void validate_limit_expr(const Mysqlx::Expr::Expr &expr) {
  if (Mysqlx::Expr::Expr_Type_LITERAL != expr.type() &&
      Mysqlx::Expr::Expr_Type_PLACEHOLDER != expr.type()) {
    throw ngs::Error(ER_X_INVALID_ARGUMENT,
                     "Invalid expression type used in limit field: %i, "
                     "expected types PLACEHOLDER or LITERAL",
                     static_cast<int>(expr.type()));
  }

  if (Mysqlx::Expr::Expr_Type_LITERAL == expr.type()) {
    switch (expr.literal().type()) {
      case Mysqlx::Datatypes::Scalar_Type_V_SINT: {
        if (expr.literal().v_signed_int() < 0)
          throw ngs::Error(ER_X_INVALID_ARGUMENT,
                           "Invalid value, limit fields "
                           "doesn't allow negative values.");
      } break;

      case Mysqlx::Datatypes::Scalar_Type_V_UINT:
        break;

      default:
        throw ngs::Error(ER_X_INVALID_ARGUMENT,
                         "Invalid scalar type used in limit field: %i, "
                         "expected types UINT, SINT",
                         static_cast<int>(expr.literal().type()));
    }
  }
}

}  // namespace

void Statement_builder::add_collection(const Collection &collection) const {
  if (!collection.has_name() || collection.name().empty())
    throw ngs::Error_code(ER_X_BAD_TABLE, "Invalid name of table/collection");

  if (collection.has_schema() && !collection.schema().empty())
    m_builder.put_identifier(collection.schema()).dot();

  m_builder.put_identifier(collection.name());
}

void Crud_statement_builder::add_filter(const Filter &filter) const {
  if (filter.IsInitialized()) m_builder.put(" WHERE ").put_expr(filter);
}

void Crud_statement_builder::add_order_item(const Order_item &item) const {
  m_builder.put_expr(item.expr());
  if (item.direction() == ::Mysqlx::Crud::Order::DESC) m_builder.put(" DESC");
}

void Crud_statement_builder::add_order(const Order_list &order) const {
  if (order.size() == 0) return;

  m_builder.put(" ORDER BY ")
      .put_list(order, std::bind(&Crud_statement_builder::add_order_item, this,
                                 std::placeholders::_1));
}

void Crud_statement_builder::add_limit_expr_field(
    const LimitExpr &limit, const bool disallow_offset) const {
  if (!limit.IsInitialized()) return;

  m_builder.put(" LIMIT ");

  if (limit.has_offset()) validate_limit_expr(limit.offset());
  if (limit.has_row_count()) validate_limit_expr(limit.row_count());

  if (limit.has_offset()) {
    if (disallow_offset) {
      throw ngs::Error_code(
          ER_X_INVALID_ARGUMENT,
          "Invalid parameter: offset value is not allowed for this operation");
    }

    if (!disallow_offset) m_builder.put_expr(limit.offset()).put(", ");
  }
  m_builder.put_expr(limit.row_count());
}

void Crud_statement_builder::add_limit_field(const Limit &limit,
                                             const bool disallow_offset) const {
  if (!limit.IsInitialized()) return;

  m_builder.put(" LIMIT ");
  if (limit.has_offset()) {
    if (disallow_offset && limit.offset() != 0)
      throw ngs::Error_code(ER_X_INVALID_ARGUMENT,
                            "Invalid parameter: non-zero offset "
                            "value is not allowed for this operation");
    if (!disallow_offset) m_builder.put(limit.offset()).put(", ");
  }
  m_builder.put(limit.row_count());
}

}  // namespace xpl
