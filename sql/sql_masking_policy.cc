/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "sql/sql_masking_policy.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "field_types.h"
#include "lex_string.h"
#include "map_helpers.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/my_service.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_string.h"
#include "mysql/components/services/object_policy_service.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/create_field.h"
#include "sql/derror.h"
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/field_common_properties.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"
#include "sql/mysqld_cs.h"
#include "sql/sp.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "sql_string.h"
#include "string_with_len.h"
#include "template_utils.h"

constexpr char kComponentUnavailable[] = "component is unavailable";
constexpr char kStringConversionFailed[] = "string conversion failed";
constexpr char kNoMessage[] = "(no message)";

static LEX_CSTRING make_lex_cstring(MEM_ROOT *root, const String &string) {
  const char *dup = string.dup(root);
  if (dup == nullptr) return NULL_CSTR;
  return {dup, string.length()};
}

/// Returns true if the two names are considered equal when they are used either
/// as masking policy names or as masking policy argument names.
static bool equal_names_for_masking_policy(std::string_view name1,
                                           std::string_view name2) {
  return my_strnncoll(system_charset_info,
                      pointer_cast<const uchar *>(name1.data()), name1.size(),
                      pointer_cast<const uchar *>(name2.data()),
                      name2.size()) == 0;
}

/// Iterate the Table Definition Cache (TDC) to find tables whose TABLE_SHARE
/// has at least one field referencing the given masking policy name and
/// invalidate those tables so they will be reopened with up-to-date metadata.
///
/// Locking and safety:
/// - Acquire LOCK_open while scanning table_def_cache and reading TABLE_SHARE
///   members; skip shares with m_open_in_progress.
/// - Copy db/table names to thd->mem_root while holding LOCK_open; perform
///   invalidation after unlocking to avoid lock ordering issues.
/// - Use tdc_remove_table(..., TDC_RT_MARK_FOR_REOPEN_AND_INVALIDATE_SHARE,
///   has_lock=false) to mark open TABLEs for reopen and invalidate the share.
static bool invalidate_tables_with_masking_policy(
    THD *thd, std::string_view policy_name) {
  struct Table_name {
    const char *db;
    const char *table;
  };
  Mem_root_array<Table_name> to_invalidate(thd->mem_root);

  {
    mysql_mutex_lock(&LOCK_open);
    const auto lock_guard =
        create_scope_guard([]() { mysql_mutex_unlock(&LOCK_open); });
    for (const auto &[_, share] : *table_def_cache) {
      if (share->m_open_in_progress) continue;
      if (!share->has_masking_policy_columns()) continue;

      if (std::any_of(share->field, share->field + share->fields,
                      [&](Field *f) {
                        return equal_names_for_masking_policy(
                            to_string_view(f->masking_policy()), policy_name);
                      })) {
        const char *db_copy =
            strmake_root(thd->mem_root, share->db.str, share->db.length);
        const char *tbl_copy = strmake_root(
            thd->mem_root, share->table_name.str, share->table_name.length);
        if (db_copy == nullptr || tbl_copy == nullptr ||
            to_invalidate.push_back({.db = db_copy, .table = tbl_copy})) {
          return true;
        }
      }
    }
  }

  for (const auto &[db, table] : to_invalidate) {
    tdc_remove_table(thd, TDC_RT_MARK_FOR_REOPEN_AND_INVALIDATE_SHARE, db,
                     table, /*has_lock=*/false);
  }

  return false;
}

bool drop_masking_policy(THD *thd, LEX_CSTRING policy_name, bool if_exists) {
  my_service<SERVICE_TYPE(column_masking_policy_management)> service(
      "column_masking_policy_management", srv_registry);

  if (!service.is_valid()) {
    my_error(ER_MASKING_POLICY_COMPONENT_ERROR, MYF(0), kComponentUnavailable);
    return true;
  }

  String policy_name_buffer{policy_name.str, policy_name.length,
                            system_charset_info};
  String message_buffer;
  if (service->drop(thd, pointer_cast<my_h_string>(&policy_name_buffer),
                    pointer_cast<my_h_string>(&message_buffer)) != 0) {
    const char *message =
        message_buffer.is_empty() ? kNoMessage : message_buffer.c_ptr_safe();
    if (if_exists) {
      // We assume it failed because the policy did not exist, which should only
      // cause a note to be printed for DROP IF EXISTS, and no error is raised.
      // This is an assumption. It is the most likely reason why it fails here,
      // but we can't tell for sure.
      push_warning_printf(
          thd, Sql_condition::SL_NOTE, ER_MASKING_POLICY_COMPONENT_ERROR,
          ER_THD(thd, ER_MASKING_POLICY_COMPONENT_ERROR), message);
      return false;
    }
    my_error(ER_MASKING_POLICY_COMPONENT_ERROR, MYF(0), message);
    return true;
  }

  // Invalidate cached tables so that prepared statements using the dropped
  // masking policy are reprepared in the next execution.
  return invalidate_tables_with_masking_policy(thd,
                                               to_string_view(policy_name));
}

std::optional<Sql_masking_policy_spec> get_masking_policy_spec(
    THD *thd, LEX_CSTRING policy_name, std::string *reason) {
  my_service<SERVICE_TYPE(column_masking_policy_retrieval)> service(
      "column_masking_policy_retrieval", srv_registry);

  if (!service.is_valid()) {
    *reason = kComponentUnavailable;
    return {};
  }

  String policy_name_buffer{policy_name.str, policy_name.length,
                            system_charset_info};
  String expression;
  String argument_name;
  String extra_information;
  String message_buffer;
  if (service->get(thd, pointer_cast<my_h_string>(&policy_name_buffer),
                   pointer_cast<my_h_string>(&expression),
                   pointer_cast<my_h_string>(&argument_name),
                   pointer_cast<my_h_string>(&extra_information),
                   pointer_cast<my_h_string>(&message_buffer)) != 0) {
    *reason =
        message_buffer.is_empty() ? kNoMessage : to_string(message_buffer);
    return {};
  }

  Sql_masking_policy_spec spec{
      .policy_name = make_lex_cstring(thd->mem_root, policy_name_buffer),
      .masking_expression = make_lex_cstring(thd->mem_root, expression),
      .argument_name = make_lex_cstring(thd->mem_root, argument_name),
  };

  if (spec.policy_name.str == nullptr ||
      spec.masking_expression.str == nullptr ||
      spec.argument_name.str == nullptr) {
    *reason = kStringConversionFailed;
    return {};
  }

  return spec;
}

bool check_masking_policy_manage_privilege(THD *thd) {
  // Masking policy DDL requires the MANAGE_DATA_MASKING_POLICY privilege.
  if (!thd->security_context()
           ->has_global_grant(STRING_WITH_LEN("MANAGE_DATA_MASKING_POLICY"))
           .first) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
             "MANAGE_DATA_MASKING_POLICY");
    return true;
  }
  return false;
}

bool create_masking_policy(THD *thd, bool if_not_exists,
                           const Sql_masking_policy_spec &spec) {
  if (std::string reason;
      if_not_exists &&
      get_masking_policy_spec(thd, spec.policy_name, &reason).has_value()) {
    push_warning_printf(
        thd, Sql_condition::SL_NOTE, ER_MASKING_POLICY_ALREADY_EXISTS,
        ER_THD(thd, ER_MASKING_POLICY_ALREADY_EXISTS), spec.policy_name.str);
    return false;
  }

  my_service<SERVICE_TYPE(column_masking_policy_management)> service(
      "column_masking_policy_management", srv_registry);
  if (!service.is_valid()) {
    my_error(ER_MASKING_POLICY_COMPONENT_ERROR, MYF(0), kComponentUnavailable);
    return true;
  }

  String policy_name_buffer{spec.policy_name.str, spec.policy_name.length,
                            system_charset_info};
  String expression{spec.masking_expression.str, spec.masking_expression.length,
                    &my_charset_utf8mb4_bin};
  String argument_name{spec.argument_name.str, spec.argument_name.length,
                       system_charset_info};
  String extra_information{"[]", 2, &my_charset_utf8mb4_bin};
  String message_buffer;
  if (service->create(thd, pointer_cast<my_h_string>(&policy_name_buffer),
                      /*replace=*/false, pointer_cast<my_h_string>(&expression),
                      pointer_cast<my_h_string>(&argument_name),
                      pointer_cast<my_h_string>(&extra_information),
                      pointer_cast<my_h_string>(&message_buffer)) != 0) {
    const char *message =
        message_buffer.is_empty() ? kNoMessage : message_buffer.c_ptr_safe();
    my_error(ER_MASKING_POLICY_COMPONENT_ERROR, MYF(0), message);
    return true;
  }

  return false;
}

/**
  Validates constraints for masking policy assignment on a column.
  Central checker for column-level eligibility: enforces the constraints listed
  under "Checked here" and enumerates related constraints validated elsewhere
  (with pointers), so this block is the canonical index of eligibility rules.

  Constraints checked here:
  - The column must not be a generated column.
  - For existing columns (create_field.field != nullptr), the column must not
    have a histogram.

  Constraints validated elsewhere:
  - The column must not be indexed. Checked in prepare_key_column().
  - The column must not be referenced by generated columns, functional indexes,
    DEFAULT value expressions or CHECK constraints. Checked by
    Item_field::check_function_as_value_generator().
  - The column must not be used by the table partitioning/subpartitioning
    function. This is enforced during partition function fixing in
    sql_partition.cc (fix_partition_func/create_partition_field_array), which
    raises ER_MASKING_POLICY_INCOMPATIBLE_COLUMN_FEATURE when a partition key
    references a masked column.

  @param create_field Create_field object for the column to validate
  @retval true  Validation failed, error was reported
  @retval false Validation succeeded
*/
static bool validate_masking_policy_column_constraints(
    const Create_field &create_field) {
  if (create_field.is_gcol()) {
    my_error(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN, MYF(0),
             "MASKING POLICY");
    return true;
  }

  // For an existing column (create_field.field != nullptr), also check that it
  // has no histogram.
  if (const Field *const field = create_field.field; field != nullptr) {
    if (field->table->find_histogram(field->field_index()) != nullptr) {
      my_error(ER_MASKING_POLICY_INCOMPATIBLE_COLUMN_FEATURE, MYF(0),
               field->field_name, "have a histogram");
      return true;
    }
  }

  return false;
}

bool check_masking_policy_name(LEX_CSTRING name) {
  // Use the same rules for policy names and their arguments as for stored
  // procedure names and their parameters.
  return sp_check_name(name);
}

static bool validate_masking_policy_gatekeeper(Item *gatekeeper_expr) {
  if (!is_function_of_type(gatekeeper_expr, Item_func::CURRENT_USER_IN_FUNC) &&
      !is_function_of_type(gatekeeper_expr, Item_func::CURRENT_ROLE_IN_FUNC)) {
    my_error(ER_MASKING_POLICY_INVALID_GATEKEEPER, MYF(0),
             "MASKING POLICY gatekeeper must be CURRENT_USER_IN or "
             "CURRENT_ROLE_IN.");
    return true;
  }

  Item_func *item_func = down_cast<Item_func *>(gatekeeper_expr);
  for (Item *arg : std::span{item_func->arguments(), item_func->arg_count}) {
    if (!arg->basic_const_item() || arg->type() != Item::STRING_ITEM) {
      my_error(ER_MASKING_POLICY_NON_LITERAL_GATEKEEPER_ARG, MYF(0));
      return true;
    }
  }

  return false;
}

/// The only column references allowed in a masking function are unqualified
/// names (no schema or table) that are equal to the policy's argument name.
static bool validate_policy_argument_reference(Item_field *field,
                                               LEX_CSTRING argument_name) {
  if (field->table_name != nullptr ||
      !equal_names_for_masking_policy(field->field_name,
                                      to_string_view(argument_name))) {
    my_error(ER_MASKING_POLICY_INVALID_COLUMN_REFERENCE, MYF(0),
             argument_name.str, field->full_name());
    return true;
  }
  return false;
}

/// Perform pre-resolving checks for the validity of the masking function:
///
/// - Must not reference other columns than the argument to the masking policy.
/// - Must return exactly one value.
/// - Must not use functions that are not allowed in a generated column
///   (exception: UDFs are allowed).
static bool validate_masking_function_syntax(THD *thd, Item *masking_func,
                                             LEX_CSTRING argument_name) {
  // Disallow all column references that do not reference the policy's argument.
  // The reference must be unqualified, since we have no table at this stage.
  if (WalkItem(masking_func, enum_walk::POSTFIX, [&](Item *item) {
        if (item->type() != Item::FIELD_ITEM) return false;
        return validate_policy_argument_reference(down_cast<Item_field *>(item),
                                                  argument_name);
      })) {
    return true;
  }

  if (masking_func->check_cols(1)) {
    return true;
  }

  /// Perform the generated column pre-resolving check, with exception for UDFs.
  Check_function_as_value_generator_parameters param{
      ER_MASKING_POLICY_DISALLOWED_CONSTRUCT, VGS_GENERATED_COLUMN};
  Item *disallowed_item = nullptr;
  WalkItem(masking_func, enum_walk::POSTFIX, [&](Item *item) {
    if (!is_function_of_type(item, Item_func::UDF_FUNC) &&
        item->check_function_as_value_generator(
            pointer_cast<uchar *>(&param))) {
      disallowed_item = item;
      return true;
    }
    return false;
  });
  if (disallowed_item != nullptr) {
    String name;
    if (param.banned_function_name != nullptr) {
      name = String{param.banned_function_name, system_charset_info};
    } else {
      disallowed_item->print(thd, &name, QT_ORDINARY);
    }
    my_error(ER_MASKING_POLICY_DISALLOWED_CONSTRUCT, MYF(0), name.c_ptr_safe());
    return true;
  }

  return false;
}

// Validate high-level structural restrictions for a masking policy expression.
// Returns true (with error) if validation fails.
bool validate_masking_policy_syntax(THD *thd, LEX_CSTRING argument_name,
                                    Item *expr) {
  if (!is_function_of_type(expr, Item_func::CASE_FUNC)) {
    my_error(ER_MASKING_POLICY_EXPECTS_CASE, MYF(0));
    return true;
  }

  Item_func_case *case_expr = down_cast<Item_func_case *>(expr);
  if (case_expr->get_first_expr_num() != -1) {
    my_error(ER_MASKING_POLICY_EXPECTS_CASE_WHEN, MYF(0));
    return true;
  }

  if (case_expr->get_else_expr_num() == -1) {
    my_error(ER_MASKING_POLICY_MISSING_ELSE, MYF(0));
    return true;
  }

  if (case_expr->arg_count != 3) {
    my_error(ER_MASKING_POLICY_ONE_WHEN_REQUIRED, MYF(0));
    return true;
  }

  if (validate_masking_policy_gatekeeper(case_expr->get_arg(0))) {
    return true;
  }

  Item *const then_expr = case_expr->get_arg(1);
  if (validate_masking_function_syntax(thd, then_expr, argument_name)) {
    return true;
  }

  Item *const else_expr = case_expr->get_arg(case_expr->get_else_expr_num());
  if (validate_masking_function_syntax(thd, else_expr, argument_name)) {
    return true;
  }

  // Either the THEN clause or the ELSE clause must be a reference to the
  // argument.
  if (then_expr->type() != Item::FIELD_ITEM &&
      else_expr->type() != Item::FIELD_ITEM) {
    my_error(ER_MASKING_POLICY_MUST_RETURN_ARGUMENT, MYF(0));
    return true;
  }

  return false;
}

/// Check if both items have compatible (simple-string) types and collation.
/// Returns true if compatible, false otherwise.
static bool compatible_string_types(const Item_field *col, const Item *expr) {
  assert(col->result_type() == STRING_RESULT);
  assert(expr->result_type() == STRING_RESULT);

  if (col->collation.collation != expr->collation.collation) {
    return false;
  }

  const enum_field_types col_type = col->data_type();
  const enum_field_types expr_type = expr->data_type();

  if (col_type == expr_type) {
    return true;
  }

  return is_simple_string_type(col_type) && is_simple_string_type(expr_type);
}

/// Returns true if type is a plain integer or YEAR.
static bool is_simple_integer_or_year_type(enum_field_types type) {
  return is_integer_type(type) || type == MYSQL_TYPE_YEAR;
}

/// Returns true if the types of the two items are identical, or if both are
/// "simple" integers (including YEAR). "Simple" excludes types with special
/// semantics such as BIT.
static bool compatible_int_types(const Item_field *col, const Item *expr) {
  assert(col->result_type() == INT_RESULT);
  assert(expr->result_type() == INT_RESULT);
  const enum_field_types type1 = col->data_type();
  const enum_field_types type2 = expr->data_type();
  return type1 == type2 || (is_simple_integer_or_year_type(type1) &&
                            is_simple_integer_or_year_type(type2));
}

/// Returns true if both items have the same temporal type (DATETIME and
/// TIMESTAMP are considered the same type) with the same fractional seconds
/// precision (FSP).
static bool compatible_temporal_types(const Item_field *col, const Item *expr) {
  return col->is_temporal_with_date() == expr->is_temporal_with_date() &&
         col->is_temporal_with_time() == expr->is_temporal_with_time() &&
         col->decimals == expr->decimals;
}

/// Checks whether the column and resolved masking expression are
/// type-compatible for masking. Assumes both Items are resolved. Policy:
///  - Result kinds (Item_result) must match.
///  - REAL_RESULT: identical precision; data_type() must match
///    (both FLOAT or both DOUBLE).
///  - INT_RESULT: identical types or both simple integers (including YEAR).
///  - DECIMAL_RESULT: identical scale (decimals). Precision may differ.
///  - STRING_RESULT: handled as either:
///      * temporal types: same temporal family (DATE, TIME,
///        DATETIME/TIMESTAMP)
///        and identical FSP (Item::decimals),
///      * simple string types: identical collation; types may differ if both
///        are simple string types (CHAR/VARCHAR/BLOB/TEXT variants), or
///      * other string-result types: require identical data_type() to avoid
///        semantic shifts (e.g., JSON/geometry).
///    This guarantees identical comparison and ordering semantics.
/// Other result kinds are not expected here. Returns true if compatible.
static bool compatible_types(const Item_field *col, const Item *expr) {
  const Item_result cr = col->result_type();
  const Item_result er = expr->result_type();

  if (cr != er) {
    return false;
  }

  switch (cr) {
    case REAL_RESULT:
      return col->data_type() == expr->data_type();
    case INT_RESULT:
      return compatible_int_types(col, expr);
    case DECIMAL_RESULT:
      return col->decimals == expr->decimals;
    case STRING_RESULT:
      // Temporal columns (DATE/TIME/DATETIME/TIMESTAMP) must be handled
      // separately. They are string-result items, but they are not compatible
      // under the generic string rules.
      if (col->is_temporal()) return compatible_temporal_types(col, expr);
      return compatible_string_types(col, expr);
    case INVALID_RESULT:
    case ROW_RESULT:
      break;
  }

  assert(false);
  return false;
}

/// Validates that the resolved masking expression is appropriate for the
/// specified column: checks type compatibility and that the expression is
/// deterministic. Reports errors for incompatible/unsafe masking policies.
/// Rationale: users permitted to see the unmasked value must observe behavior
/// identical to a column without a policy: both presentation and semantics must
/// match. In particular, comparison and ordering must not change (same
/// collation/charset), and numeric precision/scale must be preserved;
/// the expression must be deterministic.
/// Assumes: mask_expr is fully resolved (types, collation/charset,
/// nullability).
static bool validate_masking_policy_for_column(Item_field *item_field,
                                               Item *mask_expr) {
  if (!compatible_types(item_field, mask_expr)) {
    my_error(ER_MASKING_POLICY_INCOMPATIBLE_TYPES, MYF(0));
    return true;
  }

  if (mask_expr->is_non_deterministic()) {
    my_error(ER_MASKING_POLICY_NON_DETERMINISTIC_FUNC, MYF(0));
    return true;
  }

  return false;
}

/*
  The masking policy gatekeeper is constant within a query, so it should be
  evaluated once per execution. In WHERE, the optimizer already wraps such
  expressions in Item_cache via cache_const_expr_analyzer/transformer. That
  mechanism does not apply to other clauses such as the SELECT list, so we
  explicitly wrap the gatekeeper in Item_cache here (regardless of clause) to
  ensure single evaluation per query.
*/
static Item *wrap_gatekeeper_in_item_cache(Item *mask_expr) {
  return TransformItem(mask_expr, [](Item *item) -> Item * {
    if (item->type() != Item::FUNC_ITEM) return item;
    if (!item->const_for_execution()) return item;
    switch (down_cast<Item_func *>(item)->functype()) {
      case Item_func::CURRENT_USER_IN_FUNC:
      case Item_func::CURRENT_ROLE_IN_FUNC: {
        Item_cache *cache = Item_cache::get_cache(item);
        if (cache == nullptr || cache->setup(item)) {
          return nullptr;
        }
        return cache;
      }
      default:
        return item;
    }
  });
}

// Parse and resolve the masking expression under the column’s security
// context. Returns nullptr on failure (error is reported).
Item *resolve_masking_expression(THD *thd, Item_field *item_field,
                                 const Sql_masking_policy_spec &spec) {
  Masking_policy_expr_parser_state parser_state;
  if (parser_state.init(thd, spec.masking_expression.str,
                        spec.masking_expression.length)) {
    return nullptr;
  }

  // The masking expression must have the same security context as the column
  // reference in order to determine if masking should be performed or not.
  // Apart from that, the name resolution context should be empty, as the
  // masking expression should not be able to reference other columns than the
  // masked column.
  auto *const context = new (thd->mem_root) Name_resolution_context{
      .security_ctx = item_field->context->security_ctx};
  if (context == nullptr || thd->lex->push_context(context)) {
    return nullptr;
  }

  if (parse_sql(thd, &parser_state, nullptr)) {
    return nullptr;
  }

  if (validate_masking_policy_syntax(thd, spec.argument_name,
                                     parser_state.result())) {
    return nullptr;
  }

  Item *mask_expr =
      TransformItem(parser_state.result(), [&](Item *item) -> Item * {
        if (item->type() == Item::FIELD_ITEM) {
          if (validate_policy_argument_reference(down_cast<Item_field *>(item),
                                                 spec.argument_name)) {
            return nullptr;
          }
          return item_field;
        }
        return item;
      });

  if (mask_expr->fix_fields(thd, &mask_expr)) {
    return nullptr;
  }

  thd->lex->pop_context();

  mask_expr = wrap_gatekeeper_in_item_cache(mask_expr);
  if (mask_expr == nullptr) return nullptr;

  // After resolving the masking expression, enforce additional constraints that
  // cannot be checked before parsing (type, determinism).
  if (validate_masking_policy_for_column(item_field, mask_expr)) {
    return nullptr;
  }

  return mask_expr;
}

// Validates masking policies for CREATE/ALTER TABLE.
// Performs column-eligibility, masking-function, and type-compatibility checks.
// See sql_masking_policy.h for details.
bool validate_masking_policy_for_create_alter_table(THD *thd, uchar *buf,
                                                    TABLE *table,
                                                    const Create_field &field) {
  if (field.m_masking_policy_name.length == 0) return false;

  if (validate_masking_policy_column_constraints(field)) {
    return true;
  }

  // Do not fail if the policy is not defined at DDL time; skip validation.
  std::string reason;
  std::optional<Sql_masking_policy_spec> spec_opt =
      get_masking_policy_spec(thd, field.m_masking_policy_name, &reason);
  if (!spec_opt.has_value()) {
    return false;
  }

  // Create a fake field with a real data buffer in which to store the value.
  Field *regfield = make_field(field, table->s, buf + 1, buf, /*null_bit=*/0);
  if (regfield == nullptr) {
    return true;
  }
  regfield->init(table);
  regfield->set_masking_policy(field.m_masking_policy_name);
  if ((field.flags & NOT_NULL_FLAG) == 0) {
    regfield->set_null();
  }

  Item_field *item_field = new (thd->mem_root) Item_field{regfield};
  if (item_field == nullptr) return true;
  item_field->context = new (thd->mem_root) Name_resolution_context;
  if (item_field->context == nullptr) return true;

  // resolve_masking_expression() performs all required validation and
  // error reporting; only successful return means policy is valid.
  return resolve_masking_expression(thd, item_field, *spec_opt) == nullptr;
}
