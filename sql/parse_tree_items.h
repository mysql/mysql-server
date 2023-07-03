<<<<<<< HEAD
/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2013, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PARSE_TREE_ITEMS_INCLUDED
#define PARSE_TREE_ITEMS_INCLUDED

<<<<<<< HEAD
#include "field_types.h"  // enum_field_types
=======
#include <stddef.h>
#include <sys/types.h>

<<<<<<< HEAD
#include "binary_log_types.h"
>>>>>>> pr/231
#include "lex_string.h"
#include "m_ctype.h"
#include "my_inttypes.h"  // TODO: replace with cstdint
#include "sql/comp_creator.h"
#include "sql/field.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"       // Item_sum_count
#include "sql/item_timefunc.h"  // Item_func_now_local
#include "sql/parse_location.h"
#include "sql/parse_tree_helpers.h"  // Parse_tree_item
#include "sql/parse_tree_node_base.h"
#include "sql/set_var.h"

class PT_subquery;
class PT_window;
struct udf_func;

<<<<<<< HEAD
class PTI_truth_transform : public Parse_tree_item {
=======
class PTI_table_wild : public Parse_tree_item {
  typedef Parse_tree_item super;

  const char *schema;
  const char *table;

 public:
  explicit PTI_table_wild(const POS &pos, const char *schema_arg,
                          const char *table_arg)
      : super(pos), schema(schema_arg), table(table_arg) {}

  virtual bool itemize(Parse_context *pc, Item **item);
};

class PTI_negate_expression : public Parse_tree_item {
=======
class PTI_negate_expression : public Parse_tree_item
{
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  typedef Parse_tree_item super;

  Item *expr;
  Bool_test truth_test;

 public:
  PTI_truth_transform(const POS &pos, Item *expr_arg, Bool_test truth_test)
      : super(pos), expr(expr_arg), truth_test(truth_test) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_comp_op : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *left;
  chooser_compare_func_creator boolfunc2creator;
  Item *right;

 public:
  PTI_comp_op(const POS &pos, Item *left_arg,
              chooser_compare_func_creator boolfunc2creator_arg,
              Item *right_arg)
      : super(pos),
        left(left_arg),
        boolfunc2creator(boolfunc2creator_arg),
        right(right_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_comp_op_all : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *left;
  chooser_compare_func_creator comp_op;
  bool is_all;
  PT_subquery *subselect;

 public:
  PTI_comp_op_all(const POS &pos, Item *left_arg,
                  chooser_compare_func_creator comp_op_arg, bool is_all_arg,
                  PT_subquery *subselect_arg)
      : super(pos),
        left(left_arg),
        comp_op(comp_op_arg),
        is_all(is_all_arg),
        subselect(subselect_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_simple_ident_ident : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_CSTRING ident;
  Symbol_location raw;

 public:
  PTI_simple_ident_ident(const POS &pos, const LEX_CSTRING &ident_arg)
      : super(pos), ident(ident_arg), raw(pos.raw) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

/**
  Parse tree Item wrapper for 3-dimentional simple_ident-s
*/
class PTI_simple_ident_q_3d : public Parse_tree_item {
  typedef Parse_tree_item super;

 protected:
  const char *db;
  const char *table;
  const char *field;

 public:
  PTI_simple_ident_q_3d(const POS &pos, const char *db_arg,
                        const char *table_arg, const char *field_arg)
      : super(pos), db(db_arg), table(table_arg), field(field_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

/**
  Parse tree Item wrapper for 3-dimentional simple_ident-s
*/
class PTI_simple_ident_q_2d : public PTI_simple_ident_q_3d {
  typedef PTI_simple_ident_q_3d super;

 public:
  PTI_simple_ident_q_2d(const POS &pos, const char *table_arg,
                        const char *field_arg)
      : super(pos, nullptr, table_arg, field_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_simple_ident_nospvar_ident : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;

 public:
  PTI_simple_ident_nospvar_ident(const POS &pos, const LEX_STRING &ident_arg)
      : super(pos), ident(ident_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_function_call_nonkeyword_now final : public Item_func_now_local {
  typedef Item_func_now_local super;

 public:
  PTI_function_call_nonkeyword_now(const POS &pos, uint8 dec_arg)
      : super(pos, dec_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_function_call_nonkeyword_sysdate : public Parse_tree_item {
  typedef Parse_tree_item super;

  uint8 dec;

 public:
  explicit PTI_function_call_nonkeyword_sysdate(const POS &pos, uint8 dec_arg)
      : super(pos), dec(dec_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_udf_expr : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *expr;
  LEX_STRING select_alias;
  Symbol_location expr_loc;

 public:
  PTI_udf_expr(const POS &pos, Item *expr_arg,
               const LEX_STRING &select_alias_arg,
               const Symbol_location &expr_loc_arg)
      : super(pos),
        expr(expr_arg),
        select_alias(select_alias_arg),
        expr_loc(expr_loc_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_function_call_generic_ident_sys : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;
  PT_item_list *opt_udf_expr_list;

  udf_func *udf;

 public:
  PTI_function_call_generic_ident_sys(const POS &pos,
                                      const LEX_STRING &ident_arg,
                                      PT_item_list *opt_udf_expr_list_arg)
      : super(pos),
        ident(ident_arg),
        opt_udf_expr_list(opt_udf_expr_list_arg) {}

<<<<<<< HEAD
  bool itemize(Parse_context *pc, Item **res) override;
=======
<<<<<<< HEAD
  virtual bool itemize(Parse_context *pc, Item **res);
=======
  virtual bool itemize(Parse_context *pc, Item **res)
  {
    if (super::itemize(pc, res))
      return true;

    THD *thd= pc->thd;
#ifdef HAVE_DLOPEN
    udf= 0;
    if (using_udf_functions &&
        (udf= find_udf(ident.str, ident.length)) &&
        udf->type == UDFTYPE_AGGREGATE)
    {
      pc->select->in_sum_expr++;
    }
#endif

    if (sp_check_name(&ident))
      return true;

    /*
      Implementation note:
      names are resolved with the following order:
      - MySQL native functions,
      - User Defined Functions,
      - Stored Functions (assuming the current <use> database)

      This will be revised with WL#2128 (SQL PATH)
    */
    Create_func *builder= find_native_function_builder(thd, ident);
    if (builder)
      *res= builder->create_func(thd, ident, opt_udf_expr_list);
    else
    {
#ifdef HAVE_DLOPEN
      if (udf)
      {
        if (udf->type == UDFTYPE_AGGREGATE)
        {
          pc->select->in_sum_expr--;
        }

        *res= Create_udf_func::s_singleton.create(thd, udf, opt_udf_expr_list);
      }
      else
#endif
      {
        builder= find_qualified_function_builder(thd);
        assert(builder);
        *res= builder->create_func(thd, ident, opt_udf_expr_list);
      }
    }
    return *res == NULL || (*res)->itemize(pc, res);
  }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
};

/**
  Parse tree Item wrapper for 2-dimentional functional names (ex.: db.func_name)
*/
class PTI_function_call_generic_2d : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING db;
  LEX_STRING func;
  PT_item_list *opt_expr_list;

 public:
  PTI_function_call_generic_2d(const POS &pos, const LEX_STRING &db_arg,
                               const LEX_STRING &func_arg,
                               PT_item_list *opt_expr_list_arg)
      : super(pos),
        db(db_arg),
        func(func_arg),
        opt_expr_list(opt_expr_list_arg) {}

<<<<<<< HEAD
  bool itemize(Parse_context *pc, Item **res) override;
=======
<<<<<<< HEAD
  virtual bool itemize(Parse_context *pc, Item **res);
=======
  
  virtual bool itemize(Parse_context *pc, Item **res)
  {
    if (super::itemize(pc, res))
      return true;

    /*
      The following in practice calls:
      <code>Create_sp_func::create()</code>
      and builds a stored function.

      However, it's important to maintain the interface between the
      parser and the implementation in item_create.cc clean,
      since this will change with WL#2128 (SQL PATH):
      - INFORMATION_SCHEMA.version() is the SQL 99 syntax for the native
      function version(),
      - MySQL.version() is the SQL 2003 syntax for the native function
      version() (a vendor can specify any schema).
    */

    if (!db.str ||
        (check_and_convert_db_name(&db, FALSE) != IDENT_NAME_OK))
      return true;
    if (sp_check_name(&func))
      return true;

    Create_qfunc *builder= find_qualified_function_builder(pc->thd);
    assert(builder);
    *res= builder->create(pc->thd, db, func, true, opt_expr_list);
    return *res == NULL || (*res)->itemize(pc, res);
  }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
};

class PTI_text_literal : public Item_string {
  typedef Item_string super;

 protected:
  bool is_7bit;
  LEX_STRING literal;

  PTI_text_literal(const POS &pos, bool is_7bit_arg,
                   const LEX_STRING &literal_arg)
      : super(pos), is_7bit(is_7bit_arg), literal(literal_arg) {}
};

class PTI_text_literal_text_string : public PTI_text_literal {
  typedef PTI_text_literal super;

 public:
  PTI_text_literal_text_string(const POS &pos, bool is_7bit_arg,
                               const LEX_STRING &literal_arg)
      : super(pos, is_7bit_arg, literal_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_text_literal_nchar_string : public PTI_text_literal {
  typedef PTI_text_literal super;

 public:
  PTI_text_literal_nchar_string(const POS &pos, bool is_7bit_arg,
                                const LEX_STRING &literal_arg)
      : super(pos, is_7bit_arg, literal_arg) {}

<<<<<<< HEAD
  bool itemize(Parse_context *pc, Item **res) override;
=======
<<<<<<< HEAD
  virtual bool itemize(Parse_context *pc, Item **res);
=======
  virtual bool itemize(Parse_context *pc, Item **res)
  {
    if (super::itemize(pc, res))
      return true;

    uint repertoire= is_7bit ? MY_REPERTOIRE_ASCII : MY_REPERTOIRE_UNICODE30;
    assert(my_charset_is_ascii_based(national_charset_info));
    init(literal.str, literal.length, national_charset_info,
         DERIVATION_COERCIBLE, repertoire);
    return false;
  }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
};

class PTI_text_literal_underscore_charset : public PTI_text_literal {
  typedef PTI_text_literal super;

  const CHARSET_INFO *cs;

 public:
  PTI_text_literal_underscore_charset(const POS &pos, bool is_7bit_arg,
                                      const CHARSET_INFO *cs_arg,
                                      const LEX_STRING &literal_arg)
      : super(pos, is_7bit_arg, literal_arg), cs(cs_arg) {}

  bool itemize(Parse_context *pc, Item **res) override {
    if (super::itemize(pc, res)) return true;

    init(literal.str, literal.length, cs, DERIVATION_COERCIBLE,
         MY_REPERTOIRE_UNICODE30);
    set_repertoire_from_value();
    set_cs_specified(true);
    return false;
  }
};

class PTI_text_literal_concat : public PTI_text_literal {
  typedef PTI_text_literal super;

  PTI_text_literal *head;

 public:
  PTI_text_literal_concat(const POS &pos, bool is_7bit_arg,
                          PTI_text_literal *head_arg, const LEX_STRING &tail)
      : super(pos, is_7bit_arg, tail), head(head_arg) {}

<<<<<<< HEAD
  bool itemize(Parse_context *pc, Item **res) override;
=======
  virtual bool itemize(Parse_context *pc, Item **res) {
    Item *tmp_head;
    if (super::itemize(pc, res) || head->itemize(pc, &tmp_head)) return true;

<<<<<<< HEAD
    DBUG_ASSERT(tmp_head->type() == STRING_ITEM);
    Item_string *head_str = static_cast<Item_string *>(tmp_head);
=======
    assert(tmp_head->type() == STRING_ITEM);
    Item_string *head_str= static_cast<Item_string *>(tmp_head);
>>>>>>> upstream/cluster-7.6

    head_str->append(literal.str, literal.length);
    if (!(head_str->collation.repertoire & MY_REPERTOIRE_EXTENDED)) {
      /*
         If the string has been pure ASCII so far,
         check the new part.
      */
      const CHARSET_INFO *cs = pc->thd->variables.collation_connection;
      head_str->collation.repertoire |=
          my_string_repertoire(cs, literal.str, literal.length);
    }
    *res = head_str;
    return false;
  }
>>>>>>> pr/231
};

class PTI_temporal_literal : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING literal;
  enum_field_types field_type;
  const CHARSET_INFO *cs;

 public:
  PTI_temporal_literal(const POS &pos, const LEX_STRING &literal_arg,
                       enum_field_types field_type_arg,
                       const CHARSET_INFO *cs_arg)
      : super(pos),
        literal(literal_arg),
        field_type(field_type_arg),
        cs(cs_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_literal_underscore_charset_hex_num : public Item_string {
  typedef Item_string super;

 public:
  PTI_literal_underscore_charset_hex_num(const POS &pos,
                                         const CHARSET_INFO *charset,
                                         const LEX_STRING &literal)
      : super(pos, null_name_string,
              Item_hex_string::make_hex_str(literal.str, literal.length),
              charset) {}

  bool itemize(Parse_context *pc, Item **res) override {
    if (super::itemize(pc, res)) return true;

    set_repertoire_from_value();
    set_cs_specified(true);
    return check_well_formed_result(&str_value, true, true) == nullptr;
  }
};

class PTI_literal_underscore_charset_bin_num : public Item_string {
  typedef Item_string super;

 public:
  PTI_literal_underscore_charset_bin_num(const POS &pos,
                                         const CHARSET_INFO *charset,
                                         const LEX_STRING &literal)
      : super(pos, null_name_string,
              Item_bin_string::make_bin_str(literal.str, literal.length),
              charset) {}

  bool itemize(Parse_context *pc, Item **res) override {
    if (super::itemize(pc, res)) return true;

    set_cs_specified(true);
    return check_well_formed_result(&str_value, true, true) == nullptr;
  }
};

class PTI_variable_aux_set_var final : public Item_func_set_user_var {
  typedef Item_func_set_user_var super;

 public:
  PTI_variable_aux_set_var(const POS &pos, const LEX_STRING &var, Item *expr)
      : super(pos, var, expr) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_user_variable final : public Item_func_get_user_var {
  typedef Item_func_get_user_var super;

 public:
  PTI_user_variable(const POS &pos, const LEX_STRING &var) : super(pos, var) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

/**
  Parse tree Item wrapper for 3-dimentional variable names

  Example: \@global.default.x
*/
class PTI_get_system_variable : public Parse_tree_item {
  typedef Parse_tree_item super;

 public:
  PTI_get_system_variable(const POS &pos, enum_var_type scope,
                          const POS &name_pos, const LEX_CSTRING &opt_prefix,
                          const LEX_CSTRING &name)
      : super{pos},
        m_scope{scope},
        m_name_pos{name_pos},
        m_opt_prefix{opt_prefix},
        m_name{name} {}

  bool itemize(Parse_context *pc, Item **res) override;

<<<<<<< HEAD
 private:
  const enum_var_type m_scope;
  const POS m_name_pos;
  const LEX_CSTRING m_opt_prefix;
  const LEX_CSTRING m_name;
=======
    LEX *lex = pc->thd->lex;
    if (!lex->parsing_options.allows_variable) {
      my_error(ER_VIEW_SELECT_VARIABLE, MYF(0));
      return true;
    }

    /* disallow "SELECT @@global.global.variable" */
    if (ident1.str && ident2.str && check_reserved_words(&ident1)) {
      error(pc, ident1_pos);
      return true;
    }
<<<<<<< HEAD

    LEX_STRING *domain;
    LEX_STRING *variable;
    if (ident2.str && !is_key_cache_variable_suffix(ident2.str)) {
      LEX_STRING component_var;
      domain = &ident1;
      variable = &ident2;
      String tmp_name;
      if (tmp_name.reserve(domain->length + 1 + variable->length + 1) ||
          tmp_name.append(domain->str) || tmp_name.append(".") ||
          tmp_name.append(variable->str))
        return true;  // OOM
      component_var.str = tmp_name.c_ptr();
      component_var.length = tmp_name.length();
      variable->str = NullS;
      variable->length = 0;
      *res = get_system_var(pc, var_type, component_var, *variable);
    } else
      *res = get_system_var(pc, var_type, ident1, ident2);
    if (!(*res)) return true;
    if (is_identifier(ident1, "warning_count") ||
        is_identifier(ident1, "error_count")) {
=======
    if (!(*res= get_system_var(pc, var_type, var, component, true)))
      return true;
    if (!my_strcasecmp(system_charset_info, var.str, "warning_count") ||
        !my_strcasecmp(system_charset_info, var.str, "error_count"))
    {
>>>>>>> upstream/cluster-7.6
      /*
        "Diagnostics variable" used in a non-diagnostics statement.
        Save the information we need for the former, but clear the
        rest of the diagnostics area on account of the latter.
        See reset_condition_info().
      */
      lex->keep_diagnostics = DA_KEEP_COUNTS;
    }
<<<<<<< HEAD
    if (!((Item_func_get_system_var *)*res)->is_written_to_binlog())
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_VARIABLE);

=======
>>>>>>> upstream/cluster-7.6
    return false;
  }
>>>>>>> pr/231
};

class PTI_count_sym : public Item_sum_count {
  typedef Item_sum_count super;

 public:
  PTI_count_sym(const POS &pos, PT_window *w)
      : super(pos, (Item *)nullptr, w) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_in_sum_expr : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *expr;

 public:
  PTI_in_sum_expr(const POS &pos, Item *expr_arg)
      : super(pos), expr(expr_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_singlerow_subselect : public Parse_tree_item {
  typedef Parse_tree_item super;

  PT_subquery *subselect;

 public:
  PTI_singlerow_subselect(const POS &pos, PT_subquery *subselect_arg)
      : super(pos), subselect(subselect_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_exists_subselect : public Parse_tree_item {
  typedef Parse_tree_item super;

  PT_subquery *subselect;

 public:
  PTI_exists_subselect(const POS &pos, PT_subquery *subselect_arg)
      : super(pos), subselect(subselect_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_odbc_date : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;
  Item *expr;

 public:
  PTI_odbc_date(const POS &pos, const LEX_STRING &ident_arg, Item *expr_arg)
      : super(pos), ident(ident_arg), expr(expr_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_handle_sql2003_note184_exception : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *left;
  bool is_negation;
  Item *right;

 public:
  PTI_handle_sql2003_note184_exception(const POS &pos, Item *left_arg,
                                       bool is_negation_arg, Item *right_arg)
      : super(pos),
        left(left_arg),
        is_negation(is_negation_arg),
        right(right_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_expr_with_alias : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *expr;
  Symbol_location expr_loc;
  LEX_CSTRING alias;

 public:
  PTI_expr_with_alias(const POS &pos, Item *expr_arg,
                      const Symbol_location &expr_loc_arg,
                      const LEX_CSTRING &alias_arg)
      : super(pos), expr(expr_arg), expr_loc(expr_loc_arg), alias(alias_arg) {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_int_splocal : public Parse_tree_item {
  using super = Parse_tree_item;

 public:
  PTI_int_splocal(const POS &pos, const LEX_CSTRING &name)
      : super(pos), m_location{pos}, m_name{name} {}

  bool itemize(Parse_context *pc, Item **res) override;

 private:
  /// Location of the variable name.
  const POS m_location;

  /// Same data as in PTI_in_sum_expr#m_location but 0-terminated "for free".
  const LEX_CSTRING m_name;
};

class PTI_limit_option_ident : public PTI_int_splocal {
  using super = PTI_int_splocal;

 public:
  PTI_limit_option_ident(const POS &pos, const LEX_CSTRING &name)
      : super{pos, name} {}

  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_limit_option_param_marker : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item_param *param_marker;

 public:
  explicit PTI_limit_option_param_marker(const POS &pos,
                                         Item_param *param_marker_arg)
      : super(pos), param_marker(param_marker_arg) {}

<<<<<<< HEAD
  bool itemize(Parse_context *pc, Item **res) override;
=======
  virtual bool itemize(Parse_context *pc, Item **res) {
    Item *tmp_param;
    if (super::itemize(pc, res) || param_marker->itemize(pc, &tmp_param))
      return true;
    /*
      The Item_param::type() function may return various values, so we can't
      simply compare tmp_param->type() with some constant, cast tmp_param
      to Item_param* and assign the result back to param_marker.
      OTOH we ensure that Item_param::itemize() always substitute the output
      parameter with "this" pointer of Item_param object, so we can skip
      the check and the assignment.
    */
    assert(tmp_param == param_marker);

    param_marker->limit_clause_param = true;
    *res = param_marker;
    return false;
  }
>>>>>>> pr/231
};

class PTI_context : public Parse_tree_item {
  typedef Parse_tree_item super;
  Item *expr;
  const enum_parsing_context m_parsing_place;

 protected:
  PTI_context(const POS &pos, Item *expr_arg, enum_parsing_context place)
      : super(pos), expr(expr_arg), m_parsing_place(place) {}

 public:
  bool itemize(Parse_context *pc, Item **res) override;
};

class PTI_where final : public PTI_context {
 public:
  PTI_where(const POS &pos, Item *expr_arg)
      : PTI_context(pos, expr_arg, CTX_WHERE) {}
};

<<<<<<< HEAD
class PTI_having final : public PTI_context {
 public:
  PTI_having(const POS &pos, Item *expr_arg)
      : PTI_context(pos, expr_arg, CTX_HAVING) {}
=======
    pc->select->parsing_place = Context;

    if (expr->itemize(pc, &expr)) return true;

    // Ensure we're resetting parsing place of the right select
<<<<<<< HEAD
    DBUG_ASSERT(pc->select->parsing_place == Context);
    pc->select->parsing_place = CTX_NONE;
    DBUG_ASSERT(expr != NULL);
=======
    assert(pc->select->parsing_place == Context);
    pc->select->parsing_place= CTX_NONE;
    assert(expr != NULL);
>>>>>>> upstream/cluster-7.6
    expr->top_level_item();

    *res = expr;
    return false;
  }
>>>>>>> pr/231
};

#endif /* PARSE_TREE_ITEMS_INCLUDED */
