/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <sys/types.h>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/field.h"
#include "sql/item.h"
#include "sql/item_create.h"  // Create_func
#include "sql/item_func.h"
#include "sql/item_strfunc.h"
#include "sql/item_subselect.h"
#include "sql/item_sum.h"       // Item_sum_count
#include "sql/item_timefunc.h"  // Item_func_now_local
#include "sql/parse_location.h"
#include "sql/parse_tree_helpers.h"  // Parse_tree_item
#include "sql/parse_tree_node_base.h"
#include "sql/protocol.h"
#include "sql/set_var.h"
#include "sql/sp_head.h"  // sp_head
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"  // negate_expression
#include "sql/system_variables.h"
#include "sql_string.h"

class PT_subquery;
class PT_window;
struct udf_func;

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
  typedef Parse_tree_item super;

  Item *expr;

 public:
  PTI_negate_expression(const POS &pos, Item *expr_arg)
      : super(pos), expr(expr_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res) || expr->itemize(pc, &expr)) return true;

    *res = negate_expression(pc, expr);
    return *res == NULL;
  }
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

  virtual bool itemize(Parse_context *pc, Item **res);
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

  virtual bool itemize(Parse_context *pc, Item **res);
};

class PTI_simple_ident_ident : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;
  Symbol_location raw;

 public:
  PTI_simple_ident_ident(const POS &pos, const LEX_STRING &ident_arg)
      : super(pos), ident(ident_arg), raw(pos.raw) {}

  virtual bool itemize(Parse_context *pc, Item **res);
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

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    THD *thd = pc->thd;
    const char *schema =
        thd->get_protocol()->has_client_capability(CLIENT_NO_SCHEMA) ? NULL
                                                                     : db;
    if (pc->select->no_table_names_allowed) {
      my_error(ER_TABLENAME_NOT_ALLOWED_HERE, MYF(0), table, thd->where);
    }
    if ((pc->select->parsing_place != CTX_HAVING) ||
        (pc->select->get_in_sum_expr() > 0)) {
      *res = new (pc->mem_root) Item_field(POS(), schema, table, field);
    } else {
      *res = new (pc->mem_root) Item_ref(POS(), schema, table, field);
    }
    return *res == NULL || (*res)->itemize(pc, res);
  }
};

/**
  Parse tree Item wrapper for 3-dimentional simple_ident-s
*/
class PTI_simple_ident_q_2d : public PTI_simple_ident_q_3d {
  typedef PTI_simple_ident_q_3d super;

 public:
  PTI_simple_ident_q_2d(const POS &pos, const char *table_arg,
                        const char *field_arg)
      : super(pos, NULL, table_arg, field_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res);
};

class PTI_simple_ident_nospvar_ident : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;

 public:
  PTI_simple_ident_nospvar_ident(const POS &pos, const LEX_STRING &ident_arg)
      : super(pos), ident(ident_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    if ((pc->select->parsing_place != CTX_HAVING) ||
        (pc->select->get_in_sum_expr() > 0)) {
      *res = new (pc->mem_root) Item_field(POS(), NullS, NullS, ident.str);
    } else {
      *res = new (pc->mem_root) Item_ref(POS(), NullS, NullS, ident.str);
    }
    return *res == NULL || (*res)->itemize(pc, res);
  }
};

class PTI_function_call_nonkeyword_now final : public Item_func_now_local {
  typedef Item_func_now_local super;

 public:
  PTI_function_call_nonkeyword_now(const POS &pos, uint8 dec_arg)
      : super(pos, dec_arg) {}

  bool itemize(Parse_context *pc, Item **res) override {
    if (super::itemize(pc, res)) return true;

    pc->thd->lex->safe_to_cache_query = 0;
    return false;
  }
};

class PTI_function_call_nonkeyword_sysdate : public Parse_tree_item {
  typedef Parse_tree_item super;

  uint8 dec;

 public:
  explicit PTI_function_call_nonkeyword_sysdate(const POS &pos, uint8 dec_arg)
      : super(pos), dec(dec_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res);
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

  virtual bool itemize(Parse_context *pc, Item **res);
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

  virtual bool itemize(Parse_context *pc, Item **res);
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

  virtual bool itemize(Parse_context *pc, Item **res);
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
                               const LEX_STRING &literal)
      : super(pos, is_7bit_arg, literal) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    THD *thd = pc->thd;
    LEX_STRING tmp;
    const CHARSET_INFO *cs_con = thd->variables.collation_connection;
    const CHARSET_INFO *cs_cli = thd->variables.character_set_client;
    uint repertoire = is_7bit && my_charset_is_ascii_based(cs_cli)
                          ? MY_REPERTOIRE_ASCII
                          : MY_REPERTOIRE_UNICODE30;
    if (thd->charset_is_collation_connection ||
        (repertoire == MY_REPERTOIRE_ASCII &&
         my_charset_is_ascii_based(cs_con)))
      tmp = literal;
    else {
      if (thd->convert_string(&tmp, cs_con, literal.str, literal.length,
                              cs_cli))
        return true;
    }
    init(tmp.str, tmp.length, cs_con, DERIVATION_COERCIBLE, repertoire);
    return false;
  }
};

class PTI_text_literal_nchar_string : public PTI_text_literal {
  typedef PTI_text_literal super;

 public:
  PTI_text_literal_nchar_string(const POS &pos, bool is_7bit_arg,
                                const LEX_STRING &literal)
      : super(pos, is_7bit_arg, literal) {}

  virtual bool itemize(Parse_context *pc, Item **res);
};

class PTI_text_literal_underscore_charset : public PTI_text_literal {
  typedef PTI_text_literal super;

  const CHARSET_INFO *cs;

 public:
  PTI_text_literal_underscore_charset(const POS &pos, bool is_7bit_arg,
                                      const CHARSET_INFO *cs_arg,
                                      const LEX_STRING &literal)
      : super(pos, is_7bit_arg, literal), cs(cs_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
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

  virtual bool itemize(Parse_context *pc, Item **res) {
    Item *tmp_head;
    if (super::itemize(pc, res) || head->itemize(pc, &tmp_head)) return true;

    DBUG_ASSERT(tmp_head->type() == STRING_ITEM);
    Item_string *head_str = static_cast<Item_string *>(tmp_head);

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

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    *res = create_temporal_literal(pc->thd, literal.str, literal.length, cs,
                                   field_type, true);
    return *res == NULL;
  }
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

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    set_repertoire_from_value();
    set_cs_specified(true);
    return check_well_formed_result(&str_value, true, true) == NULL;
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

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    set_cs_specified(true);
    return check_well_formed_result(&str_value, true, true) == NULL;
  }
};

class PTI_variable_aux_set_var final : public Item_func_set_user_var {
  typedef Item_func_set_user_var super;

 public:
  PTI_variable_aux_set_var(const POS &pos, const LEX_STRING &var, Item *expr)
      : super(pos, var, expr, false) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    LEX *lex = pc->thd->lex;
    if (!lex->parsing_options.allows_variable) {
      my_error(ER_VIEW_SELECT_VARIABLE, MYF(0));
      return true;
    }
    lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
    lex->set_var_list.push_back(this);
    return false;
  }
};

class PTI_variable_aux_ident_or_text final : public Item_func_get_user_var {
  typedef Item_func_get_user_var super;

 public:
  PTI_variable_aux_ident_or_text(const POS &pos, const LEX_STRING &var)
      : super(pos, var) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    LEX *lex = pc->thd->lex;
    if (!lex->parsing_options.allows_variable) {
      my_error(ER_VIEW_SELECT_VARIABLE, MYF(0));
      return true;
    }
    lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
    return false;
  }
};

/**
  Parse tree Item wrapper for 3-dimentional variable names

  Example: \@global.default.x
*/
class PTI_variable_aux_3d : public Parse_tree_item {
  typedef Parse_tree_item super;

  enum_var_type var_type;
  LEX_STRING ident1;
  POS ident1_pos;
  LEX_STRING ident2;

 public:
  PTI_variable_aux_3d(const POS &pos, enum_var_type var_type_arg,
                      const LEX_STRING &ident1_arg, const POS &ident1_pos_arg,
                      const LEX_STRING &ident2_arg)
      : super(pos),
        var_type(var_type_arg),
        ident1(ident1_arg),
        ident1_pos(ident1_pos_arg),
        ident2(ident2_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

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
      /*
        "Diagnostics variable" used in a non-diagnostics statement.
        Save the information we need for the former, but clear the
        rest of the diagnostics area on account of the latter.
        See reset_condition_info().
      */
      lex->keep_diagnostics = DA_KEEP_COUNTS;
    }
    if (!((Item_func_get_system_var *)*res)->is_written_to_binlog())
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_VARIABLE);

    return false;
  }
};

class PTI_count_sym : public Item_sum_count {
  typedef Item_sum_count super;

 public:
  PTI_count_sym(const POS &pos, PT_window *w) : super(pos, (Item *)NULL, w) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    args[0] = new (pc->mem_root) Item_int((int32)0L, 1);
    if (args[0] == NULL) return true;

    return super::itemize(pc, res);
  }
};

class PTI_in_sum_expr : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *expr;

 public:
  PTI_in_sum_expr(const POS &pos, Item *expr_arg)
      : super(pos), expr(expr_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    pc->select->in_sum_expr++;

    if (super::itemize(pc, res) || expr->itemize(pc, &expr)) return true;

    pc->select->in_sum_expr--;

    *res = expr;
    return false;
  }
};

class PTI_singlerow_subselect : public Parse_tree_item {
  typedef Parse_tree_item super;

  PT_subquery *subselect;

 public:
  PTI_singlerow_subselect(const POS &pos, PT_subquery *subselect_arg)
      : super(pos), subselect(subselect_arg) {}

  bool itemize(Parse_context *pc, Item **res);
};

class PTI_exists_subselect : public Parse_tree_item {
  typedef Parse_tree_item super;

  PT_subquery *subselect;

 public:
  PTI_exists_subselect(const POS &pos, PT_subquery *subselect_arg)
      : super(pos), subselect(subselect_arg) {}

  bool itemize(Parse_context *pc, Item **res);
};

class PTI_odbc_date : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;
  Item *expr;

 public:
  PTI_odbc_date(const POS &pos, const LEX_STRING &ident_arg, Item *expr_arg)
      : super(pos), ident(ident_arg), expr(expr_arg) {}

  bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res) || expr->itemize(pc, &expr)) return true;

    *res = NULL;
    /*
      If "expr" is reasonably short pure ASCII string literal,
      try to parse known ODBC style date, time or timestamp literals,
      e.g:
      SELECT {d'2001-01-01'};
      SELECT {t'10:20:30'};
      SELECT {ts'2001-01-01 10:20:30'};
    */
    if (expr->type() == Item::STRING_ITEM &&
        expr->collation.repertoire == MY_REPERTOIRE_ASCII) {
      String buf;
      String *tmp_str = expr->val_str(&buf);
      if (tmp_str->length() < MAX_DATE_STRING_REP_LENGTH * 4) {
        enum_field_types type = MYSQL_TYPE_STRING;
        ErrConvString str(tmp_str);
        LEX_STRING *ls = &ident;
        if (ls->length == 1) {
          if (ls->str[0] == 'd') /* {d'2001-01-01'} */
            type = MYSQL_TYPE_DATE;
          else if (ls->str[0] == 't') /* {t'10:20:30'} */
            type = MYSQL_TYPE_TIME;
        } else if (ls->length == 2) /* {ts'2001-01-01 10:20:30'} */
        {
          if (ls->str[0] == 't' && ls->str[1] == 's')
            type = MYSQL_TYPE_DATETIME;
        }
        if (type != MYSQL_TYPE_STRING)
          *res = create_temporal_literal(pc->thd, str.ptr(), str.length(),
                                         system_charset_info, type, false);
      }
    }
    if (*res == NULL) *res = expr;
    return false;
  }
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

  virtual bool itemize(Parse_context *pc, Item **res);
};

class PTI_expr_with_alias : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *expr;
  Symbol_location expr_loc;
  LEX_STRING alias;

 public:
  PTI_expr_with_alias(const POS &pos, Item *expr_arg,
                      const Symbol_location &expr_loc_arg,
                      const LEX_STRING &alias_arg)
      : super(pos), expr(expr_arg), expr_loc(expr_loc_arg), alias(alias_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res);
};

class PTI_limit_option_ident : public Parse_tree_item {
  typedef Parse_tree_item super;

  LEX_STRING ident;
  Symbol_location ident_loc;

 public:
  explicit PTI_limit_option_ident(const POS &pos, const LEX_STRING &ident_arg,
                                  const Symbol_location &ident_loc_arg)
      : super(pos), ident(ident_arg), ident_loc(ident_loc_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    LEX *lex = pc->thd->lex;
    sp_head *sp = lex->sphead;
    const char *query_start_ptr =
        sp ? sp->m_parser_data.get_current_stmt_start_ptr() : NULL;

    Item_splocal *v = create_item_for_sp_var(
        pc->thd, ident, NULL, query_start_ptr, ident_loc.start, ident_loc.end);
    if (!v) return true;

    lex->safe_to_cache_query = false;

    if (v->type() != Item::INT_ITEM) {
      my_error(ER_WRONG_SPVAR_TYPE_IN_LIMIT, MYF(0));
      return true;
    }

    v->limit_clause_param = true;
    *res = v;
    return false;
  }
};

class PTI_limit_option_param_marker : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item_param *param_marker;

 public:
  explicit PTI_limit_option_param_marker(const POS &pos,
                                         Item_param *param_marker_arg)
      : super(pos), param_marker(param_marker_arg) {}

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
    DBUG_ASSERT(tmp_param == param_marker);

    param_marker->limit_clause_param = true;
    *res = param_marker;
    return false;
  }
};

template <enum_parsing_context Context>
class PTI_context : public Parse_tree_item {
  typedef Parse_tree_item super;

  Item *expr;

 public:
  PTI_context(const POS &pos, Item *expr_arg) : super(pos), expr(expr_arg) {}

  virtual bool itemize(Parse_context *pc, Item **res) {
    if (super::itemize(pc, res)) return true;

    pc->select->parsing_place = Context;

    if (expr->itemize(pc, &expr)) return true;

    // Ensure we're resetting parsing place of the right select
    DBUG_ASSERT(pc->select->parsing_place == Context);
    pc->select->parsing_place = CTX_NONE;
    DBUG_ASSERT(expr != NULL);
    expr->top_level_item();

    *res = expr;
    return false;
  }
};

#endif /* PARSE_TREE_ITEMS_INCLUDED */
