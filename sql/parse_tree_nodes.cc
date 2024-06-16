/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#include "sql/parse_tree_nodes.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "field_types.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/create_field.h"
#include "sql/dd/info_schema/show.h"      // build_show_...
#include "sql/dd/types/abstract_table.h"  // dd::enum_table_type::BASE_TABLE
#include "sql/dd/types/column.h"
#include "sql/derror.h"  // ER_THD
#include "sql/field.h"
#include "sql/gis/srid.h"
#include "sql/intrusive_list_iterator.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_timefunc.h"
#include "sql/key_spec.h"
#include "sql/lex.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"                   // global_system_variables
#include "sql/opt_explain_json.h"         // Explain_format_JSON
#include "sql/opt_explain_traditional.h"  // Explain_format_traditional
#include "sql/parse_location.h"
#include "sql/parse_tree_column_attrs.h"  // PT_field_def_base
#include "sql/parse_tree_hints.h"
#include "sql/parse_tree_items.h"
#include "sql/parse_tree_partitions.h"  // PT_partition
#include "sql/parse_tree_window.h"      // PT_window
#include "sql/parser_yystype.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/query_term.h"
#include "sql/sp.h"  // sp_add_used_routine
#include "sql/sp_head.h"
#include "sql/sp_instr.h"  // sp_instr_set
#include "sql/sp_pcontext.h"
#include "sql/sql_base.h"  // find_temporary_table
#include "sql/sql_call.h"  // Sql_cmd_call...
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_cmd_ddl_table.h"
#include "sql/sql_component.h"  // Sql_cmd_component
#include "sql/sql_const.h"
#include "sql/sql_data_change.h"
#include "sql/sql_db.h"
#include "sql/sql_delete.h"  // Sql_cmd_delete...
#include "sql/sql_do.h"      // Sql_cmd_do...
#include "sql/sql_error.h"
#include "sql/sql_insert.h"  // Sql_cmd_insert...
#include "sql/sql_parse.h"
#include "sql/sql_select.h"  // Sql_cmd_select...
#include "sql/sql_show.h"    // Sql_cmd_show...
#include "sql/sql_show_processlist.h"
#include "sql/sql_show_status.h"  // build_show_session_status, ...
#include "sql/sql_update.h"       // Sql_cmd_update...
#include "sql/strfunc.h"
#include "sql/system_variables.h"
#include "sql/table_function.h"
#include "sql/thr_malloc.h"
#include "sql/trigger_def.h"
#include "sql/window.h"  // Window
#include "sql_string.h"
#include "string_with_len.h"
#include "strxmov.h"
#include "template_utils.h"

static constexpr const size_t MAX_SYS_VAR_LENGTH{32};

namespace {

template <typename Context, typename Node>
bool contextualize_safe(Context *pc, Node node) {
  if (node == nullptr) return false;
  return node->contextualize(pc);
}

template <typename Context>
bool contextualize_safe(Context *pc, mem_root_deque<Item *> *list) {
  if (list == nullptr) return false;
  for (Item *&item : *list) {
    if (item->itemize(pc, &item)) return true;
  }
  return false;
}

/**
  Convenience function that calls Parse_tree_node::do_contextualize() on each of
  the nodes that are non-NULL, stopping when a call returns true.
*/
template <typename Context, typename Node, typename... Nodes>
bool contextualize_safe(Context *pc, Node node, Nodes... nodes) {
  return contextualize_safe(pc, node) || contextualize_safe(pc, nodes...);
}

static void print_table_ident(const THD *thd, const Table_ident *ident,
                              String *s) {
  if (ident->db.length > 0) {
    append_identifier(thd, s, ident->db.str, ident->db.length);
    s->append('.');
  }
  append_identifier(thd, s, ident->table.str, ident->table.length);
}

/**
  Convenience function that calls Item::itemize() on the item if it's
  non-NULL.
*/
bool itemize_safe(Parse_context *pc, Item **item) {
  if (*item == nullptr) return false;
  return (*item)->itemize(pc, item);
}

}  // namespace

Table_ddl_parse_context::Table_ddl_parse_context(THD *thd_arg,
                                                 Query_block *select_arg,
                                                 Alter_info *alter_info)
    : Parse_context(thd_arg, select_arg),
      create_info(thd_arg->lex->create_info),
      alter_info(alter_info),
      key_create_info(&thd_arg->lex->key_create_info) {}

PT_joined_table *PT_table_reference::add_cross_join(PT_cross_join *cj) {
  cj->add_rhs(this);
  return cj;
}

bool PT_joined_table::contextualize_tabs(Parse_context *pc) {
  if (m_left_table_ref != nullptr) return false;  // already done

  bool was_right_join = m_type & JTT_RIGHT;
  // rewrite to LEFT JOIN
  if (was_right_join) {
    m_type =
        static_cast<PT_joined_table_type>((m_type & ~JTT_RIGHT) | JTT_LEFT);
    std::swap(m_left_pt_table, m_right_pt_table);
  }

  if (m_left_pt_table->contextualize(pc) || m_right_pt_table->contextualize(pc))
    return true;

  m_left_table_ref = m_left_pt_table->m_table_ref;
  m_right_table_ref = m_right_pt_table->m_table_ref;

  if (m_left_table_ref == nullptr || m_right_table_ref == nullptr) {
    error(pc, m_join_pos);
    return true;
  }

  if (m_type & JTT_LEFT) {
    m_right_table_ref->outer_join = true;
    if (was_right_join) {
      m_right_table_ref->join_order_swapped = true;
      m_right_table_ref->query_block->set_right_joins();
    }
  }

  return false;
}

bool PT_option_value_no_option_type_charset::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  int flags = opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT;
  const CHARSET_INFO *cs2;
  cs2 =
      opt_charset ? opt_charset : global_system_variables.character_set_client;
  set_var_collation_client *var;
  var = new (thd->mem_root) set_var_collation_client(
      flags, cs2, thd->variables.collation_database, cs2);
  if (var == nullptr) return true;
  lex->var_list.push_back(var);
  return false;
}

bool PT_option_value_no_option_type_names::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  LEX_CSTRING names = {STRING_WITH_LEN("names")};

  if (pctx && pctx->find_variable(names.str, names.length, false))
    my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), names.str);
  else
    error(pc, m_error_pos);

  return true;  // always fails with an error
}

bool PT_set_names::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  const CHARSET_INFO *cs2;
  const CHARSET_INFO *cs3;
  int flags = set_var_collation_client::SET_CS_NAMES |
              (opt_charset ? 0 : set_var_collation_client::SET_CS_DEFAULT) |
              (opt_collation ? set_var_collation_client::SET_CS_COLLATE : 0);
  cs2 =
      opt_charset ? opt_charset : global_system_variables.character_set_client;
  if (opt_collation != nullptr) {
    if (!my_charset_same(cs2, opt_collation)) {
      my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
               opt_collation->m_coll_name, cs2->csname);
      return true;
    }
    cs3 = opt_collation;
  } else {
    if (cs2 == &my_charset_utf8mb4_0900_ai_ci &&
        cs2 != thd->variables.default_collation_for_utf8mb4)
      cs3 = thd->variables.default_collation_for_utf8mb4;
    else
      cs3 = cs2;
  }
  set_var_collation_client *var;
  var = new (thd->mem_root) set_var_collation_client(flags, cs3, cs3, cs3);
  if (var == nullptr) return true;
  lex->var_list.push_back(var);
  return false;
}

bool PT_group::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  Query_block *select = pc->select;
  select->parsing_place = CTX_GROUP_BY;

  if (group_list->contextualize(pc)) return true;
  assert(select == pc->select);

  select->group_list = group_list->value;

  // group by does not have to provide ordering
  ORDER *group = select->group_list.first;
  for (; group; group = group->next) group->direction = ORDER_NOT_RELEVANT;

  // Ensure we're resetting parsing place of the right select
  assert(select->parsing_place == CTX_GROUP_BY);
  select->parsing_place = CTX_NONE;

  switch (olap) {
    case UNSPECIFIED_OLAP_TYPE:
      break;
    case ROLLUP_TYPE:
      if (select->linkage == GLOBAL_OPTIONS_TYPE) {
        my_error(ER_WRONG_USAGE, MYF(0), "WITH ROLLUP",
                 "global union parameters");
        return true;
      }
      select->olap = ROLLUP_TYPE;
      break;
    case CUBE_TYPE:
      if (select->linkage == GLOBAL_OPTIONS_TYPE) {
        my_error(ER_WRONG_USAGE, MYF(0), "CUBE", "global union parameters");
        return true;
      }
      select->olap = CUBE_TYPE;
      pc->thd->lex->set_execute_only_in_secondary_engine(
          /*execute_only_in_secondary_engine_param=*/true, CUBE);
      break;
    default:
      assert(!"unexpected OLAP type!");
  }
  return false;
}

bool PT_order::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  pc->select->parsing_place = CTX_ORDER_BY;
  pc->thd->where = "global ORDER clause";

  if (order_list->contextualize(pc)) return true;
  pc->select->order_list = order_list->value;

  // Reset parsing place only for ORDER BY
  if (pc->select->parsing_place == CTX_ORDER_BY)
    pc->select->parsing_place = CTX_NONE;

  pc->thd->where = THD::DEFAULT_WHERE;
  return false;
}

bool PT_order_expr::do_contextualize(Parse_context *pc) {
  return super::do_contextualize(pc) ||
         item_initial->itemize(pc, &item_initial);
}

/**
  Recognize the transition variable syntax: { OLD | NEW } "." ...

  @param pc             Parse context
  @param opt_prefix     Optional prefix to examine or empty LEX_CSTRING
  @param prefix         "OLD" or "NEW" to compare with

  @returns true if the OLD transition variable syntax, otherwise false
*/
static bool is_transition_variable_prefix(const Parse_context &pc,
                                          const LEX_CSTRING &opt_prefix,
                                          const char *prefix) {
  assert(strcmp(prefix, "OLD") == 0 || strcmp(prefix, "NEW") == 0);
  sp_head *const sp = pc.thd->lex->sphead;
  return opt_prefix.str != nullptr && sp != nullptr &&
         sp->m_type == enum_sp_type::TRIGGER &&
         my_strcasecmp(system_charset_info, opt_prefix.str, prefix) == 0;
}

static bool is_old_transition_variable_prefix(const Parse_context &pc,
                                              const LEX_CSTRING &opt_prefix) {
  return is_transition_variable_prefix(pc, opt_prefix, "OLD");
}

static bool is_new_transition_variable_prefix(const Parse_context &pc,
                                              const LEX_CSTRING &opt_prefix) {
  return is_transition_variable_prefix(pc, opt_prefix, "NEW");
}

/**
  Recognize any of transition variable syntax: { NEW | OLD } "." ...

  @param pc     Parse context
  @param opt_prefix Optional prefix or empty LEX_CSTRING

  @returns true if the transition variable syntax, otherwise false
*/
static bool is_any_transition_variable_prefix(const Parse_context &pc,
                                              const LEX_CSTRING &opt_prefix) {
  return is_old_transition_variable_prefix(pc, opt_prefix) ||
         is_new_transition_variable_prefix(pc, opt_prefix);
}

/**
  Try to resolve a name as SP formal parameter or local variable

  @note Doesn't output error messages on failure.

  @param pc     Parse context
  @param name   Name of probable SP variable

  @returns pointer to SP formal parameter/local variable object on success,
           otherwise nullptr
*/
static sp_variable *find_sp_variable(const Parse_context &pc,
                                     const LEX_CSTRING &name) {
  const sp_pcontext *pctx = pc.thd->lex->get_sp_current_parsing_ctx();
  return pctx ? pctx->find_variable(name.str, name.length, false) : nullptr;
}

/**
  Helper action for a SET statement.
  Used to SET a field of NEW row.

  @param pc                 the parse context
  @param trigger_field_name the NEW-row field name
  @param expr_item          the value expression to be assigned
  @param expr_pos           the value expression query

  @return error status (true if error, false otherwise).
*/

static bool set_trigger_new_row(Parse_context *pc,
                                LEX_CSTRING trigger_field_name, Item *expr_item,
                                const POS &expr_pos) {
  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_head *sp = lex->sphead;

  assert(expr_item != nullptr);

  if (sp->m_trg_chistics.event == TRG_EVENT_DELETE) {
    my_error(ER_TRG_NO_SUCH_ROW_IN_TRG, MYF(0), "NEW", "on DELETE");
    return true;
  }
  if (sp->m_trg_chistics.action_time == TRG_ACTION_AFTER) {
    my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "NEW", "after ");
    return true;
  }

  LEX_CSTRING expr_query{};
  if (lex->is_metadata_used()) {
    expr_query = make_string(thd, expr_pos.raw.start, expr_pos.raw.end);
    if (expr_query.str == nullptr) return true;  // OOM
  }

  assert(sp->m_trg_chistics.action_time == TRG_ACTION_BEFORE &&
         (sp->m_trg_chistics.event == TRG_EVENT_INSERT ||
          sp->m_trg_chistics.event == TRG_EVENT_UPDATE));

  Item_trigger_field *trg_fld = new (pc->mem_root) Item_trigger_field(
      POS(), TRG_NEW_ROW, trigger_field_name.str, UPDATE_ACL, false);

  if (trg_fld == nullptr || trg_fld->itemize(pc, (Item **)&trg_fld))
    return true;
  assert(trg_fld->type() == Item::TRIGGER_FIELD_ITEM);

  auto *sp_instr = new (pc->mem_root)
      sp_instr_set_trigger_field(sp->instructions(), lex, trigger_field_name,
                                 trg_fld, expr_item, expr_query);

  if (sp_instr == nullptr) return true;  // OM

  /*
    Add this item to list of all Item_trigger_field objects in trigger.
  */
  sp->m_cur_instr_trig_field_items.link_in_list(trg_fld,
                                                &trg_fld->next_trg_field);

  return sp->add_instr(thd, sp_instr);
}

/**
  Helper action for a SET statement.
  Used to push a system variable into the assignment list.
  In stored routines detects "SET AUTOCOMMIT" and reject "SET GTID..." stuff.

  @param thd            the current thread
  @param prefix         left hand side of a qualified name (if any)
                        or nullptr string
  @param suffix         the right hand side of a qualified name (if any)
                        or a whole unqualified name
  @param var_type       the scope of the variable
  @param val            the value being assigned to the variable

  @return true if error, false otherwise.
*/
static bool add_system_variable_assignment(THD *thd, LEX_CSTRING prefix,
                                           LEX_CSTRING suffix,
                                           enum enum_var_type var_type,
                                           Item *val) {
  System_variable_tracker var_tracker = System_variable_tracker::make_tracker(
      to_string_view(prefix), to_string_view(suffix));
  if (var_tracker.access_system_variable(thd)) {
    return true;
  }

  LEX *lex = thd->lex;
  sp_head *sp = lex->sphead;
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();

  /* No AUTOCOMMIT from a stored function or trigger. */
  if (pctx != nullptr && var_tracker.eq_static_sys_var(Sys_autocommit_ptr))
    sp->m_flags |= sp_head::HAS_SET_AUTOCOMMIT_STMT;

  if (lex->uses_stored_routines() &&
      (var_tracker.eq_static_sys_var(Sys_gtid_next_ptr) ||
#ifdef HAVE_GTID_NEXT_LIST
       var_tracker.eq_static_sys_var(Sys_gtid_next_list_ptr) ||
#endif
       var_tracker.eq_static_sys_var(Sys_gtid_purged_ptr))) {
    my_error(ER_SET_STATEMENT_CANNOT_INVOKE_FUNCTION, MYF(0),
             var_tracker.get_var_name());
    return true;
  }

  /*
    If the set value is a field, change it to a string to allow things like
    SET table_type=MYISAM;
  */
  if (val && val->type() == Item::FIELD_ITEM) {
    Item_field *item_field = down_cast<Item_field *>(val);
    if (item_field->table_name != nullptr) {
      // Reject a dot-separated identifier as the RHS of:
      //    SET <variable_name> = <table_name>.<field_name>
      my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var_tracker.get_var_name());
      return true;
    }
    assert(item_field->field_name != nullptr);
    val =
        new Item_string(item_field->field_name, strlen(item_field->field_name),
                        system_charset_info);  // names are utf8
    if (val == nullptr) return true;           // OOM
  }

  set_var *var = new (thd->mem_root) set_var(var_type, var_tracker, val);
  if (var == nullptr) return true;

  return lex->var_list.push_back(var);
}

bool PT_set_variable::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || itemize_safe(pc, &m_opt_expr)) {
    return true;
  }
  const bool is_1d_name = m_opt_prefix.str == nullptr;

  /*
    1. Reject `SET OLD.<name>` in triggers:
  */
  if (is_old_transition_variable_prefix(*pc, m_opt_prefix)) {
    my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "OLD", "");
    return true;
  }

  /*
    2. Process NEW.<name> in triggers:
  */
  if (is_new_transition_variable_prefix(*pc, m_opt_prefix)) {
    if (m_opt_expr == nullptr) {
      error(pc, m_expr_pos);  // Reject the syntax: SET NEW.<column> = DEFAULT
      return true;
    }
    if (set_trigger_new_row(pc, m_name, m_opt_expr, m_expr_pos)) return true;
    return false;
  }

  /*
    3. Process SP local variable assignment:
  */

  THD *const thd = pc->thd;
  LEX *const lex = thd->lex;
  if (is_1d_name) {
    // Remove deprecation warning when FULL is used as keyword.
    if (thd->get_stmt_da()->has_sql_condition(ER_WARN_DEPRECATED_IDENT)) {
      thd->get_stmt_da()->reset_condition_info(thd);
    }

    sp_variable *spv = find_sp_variable(*pc, m_name);
    if (spv != nullptr) {
      if (m_opt_expr == nullptr) {
        error(pc,
              m_expr_pos);  // Reject the syntax: SET <sp var name> = DEFAULT
        return true;
      }

      LEX_CSTRING expr_query{};
      if (lex->is_metadata_used()) {
        expr_query = make_string(thd, m_expr_pos.raw.start, m_expr_pos.raw.end);
        if (expr_query.str == nullptr) return true;  // OOM
      }

      /*
        NOTE: every SET-expression has its own LEX-object, even if it is
        a multiple SET-statement, like:

          SET spv1 = expr1, spv2 = expr2, ...

        Every SET-expression has its own sp_instr_set. Thus, the
        instruction owns the LEX-object, i.e. the instruction is
        responsible for destruction of the LEX-object.
      */

      sp_head *const sp = lex->sphead;
      auto *sp_instr = new (thd->mem_root) sp_instr_set(
          sp->instructions(), lex, spv->offset, m_opt_expr, expr_query,
          true);  // The instruction owns its lex.

      return sp_instr == nullptr || sp->add_instr(thd, sp_instr);
    }
  }

  /*
    4. Process assignment of a system variable, including:
       * Multiple Key Cache variables
       * plugin variables
       * component variables
  */
  return add_system_variable_assignment(thd, m_opt_prefix, m_name,
                                        lex->option_type, m_opt_expr);
}

bool PT_option_value_no_option_type_password_for::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  set_var_password *var;
  lex->contains_plaintext_password = true;

  /*
    In case of anonymous user, user->user is set to empty string with
    length 0. But there might be case when user->user.str could be NULL.
    For Ex: "set password for current_user() = password('xyz');".
    In this case, set user information as of the current user.
  */
  if (!user->user.str) {
    LEX_CSTRING sctx_priv_user = thd->security_context()->priv_user();
    assert(sctx_priv_user.str);
    user->user.str = sctx_priv_user.str;
    user->user.length = sctx_priv_user.length;
  }
  if (!user->host.str) {
    LEX_CSTRING sctx_priv_host = thd->security_context()->priv_host();
    assert(sctx_priv_host.str);
    user->host.str = sctx_priv_host.str;
    user->host.length = sctx_priv_host.length;
  }

  // Current password is specified through the REPLACE clause hence set the flag
  if (current_password != nullptr) user->uses_replace_clause = true;

  if (random_password_generator) password = nullptr;

  var = new (thd->mem_root) set_var_password(
      user, const_cast<char *>(password), const_cast<char *>(current_password),
      retain_current_password, random_password_generator);

  if (var == nullptr || lex->var_list.push_back(var)) {
    return true;  // Out of memory
  }
  lex->sql_command = SQLCOM_SET_PASSWORD;
  if (lex->sphead) lex->sphead->m_flags |= sp_head::HAS_SET_AUTOCOMMIT_STMT;
  if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end)) return true;
  return false;
}

bool PT_option_value_no_option_type_password::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  sp_head *sp = lex->sphead;
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  LEX_CSTRING pw = {STRING_WITH_LEN("password")};
  lex->contains_plaintext_password = true;

  if (pctx && pctx->find_variable(pw.str, pw.length, false)) {
    my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), pw.str);
    return true;
  }

  LEX_CSTRING sctx_user = thd->security_context()->user();
  LEX_CSTRING sctx_priv_host = thd->security_context()->priv_host();
  assert(sctx_priv_host.str);

  LEX_USER *user = LEX_USER::alloc(thd, (LEX_STRING *)&sctx_user,
                                   (LEX_STRING *)&sctx_priv_host);
  if (!user) return true;

  if (random_password_generator) password = nullptr;

  set_var_password *var = new (thd->mem_root) set_var_password(
      user, const_cast<char *>(password), const_cast<char *>(current_password),
      retain_current_password, random_password_generator);

  if (var == nullptr || lex->var_list.push_back(var)) {
    return true;  // Out of Memory
  }
  lex->sql_command = SQLCOM_SET_PASSWORD;

  if (sp) sp->m_flags |= sp_head::HAS_SET_AUTOCOMMIT_STMT;

  if (sp_create_assignment_instr(pc->thd, expr_pos.raw.end)) return true;

  return false;
}

PT_key_part_specification::PT_key_part_specification(const POS &pos,
                                                     Item *expression,
                                                     enum_order order)
    : super(pos), m_expression(expression), m_order(order) {}

PT_key_part_specification::PT_key_part_specification(
    const POS &pos, const LEX_CSTRING &column_name, enum_order order,
    int prefix_length)
    : super(pos),
      m_expression(nullptr),
      m_order(order),
      m_column_name(column_name),
      m_prefix_length(prefix_length) {}

bool PT_key_part_specification::do_contextualize(Parse_context *pc) {
  return super::do_contextualize(pc) || itemize_safe(pc, &m_expression);
}

bool PT_select_sp_var::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *lex = pc->thd->lex;
#ifndef NDEBUG
  sp = lex->sphead;
#endif
  sp_pcontext *pctx = lex->get_sp_current_parsing_ctx();
  sp_variable *spv;

  if (!pctx || !(spv = pctx->find_variable(name.str, name.length, false))) {
    my_error(ER_SP_UNDECLARED_VAR, MYF(0), name.str);
    return true;
  }

  offset = spv->offset;

  return false;
}

std::string PT_select_stmt::get_printable_parse_tree(THD *thd) {
  Parse_context pc(thd, thd->lex->current_query_block(),
                   /*show_parse_tree=*/true);

  pc.m_show_parse_tree->push_level(m_pos, typeid(*this).name());

  thd->lex->sql_command = m_sql_command;

  if (m_qe->contextualize(&pc)) {
    return "";
  }

  const bool has_into_clause_inside_query_block = thd->lex->result != nullptr;

  if (has_into_clause_inside_query_block && m_into != nullptr) {
    my_error(ER_MULTIPLE_INTO_CLAUSES, MYF(0));
    return "";
  }
  if (contextualize_safe(&pc, m_into)) {
    return "";
  }

  if (pc.finalize_query_expression()) return "";

  pc.m_show_parse_tree->pop_level();

  return pc.m_show_parse_tree->get_parse_tree();
}

Sql_cmd *PT_select_stmt::make_cmd(THD *thd) {
  Parse_context pc(thd, thd->lex->current_query_block());

  thd->lex->sql_command = m_sql_command;

  if (m_qe->contextualize(&pc)) {
    return nullptr;
  }

  const bool has_into_clause_inside_query_block = thd->lex->result != nullptr;

  if (has_into_clause_inside_query_block && m_into != nullptr) {
    my_error(ER_MULTIPLE_INTO_CLAUSES, MYF(0));
    return nullptr;
  }
  if (contextualize_safe(&pc, m_into)) {
    return nullptr;
  }

  if (pc.finalize_query_expression()) return nullptr;

  // Ensure that first query block is the current one
  assert(pc.select->select_number == 1);

  if (m_into != nullptr && m_has_trailing_locking_clauses) {
    // Example: ... INTO ... FOR UPDATE;
    push_warning(thd, ER_WARN_DEPRECATED_INNER_INTO);
  } else if (has_into_clause_inside_query_block &&
             thd->lex->unit->is_set_operation()) {
    // Example: ... UNION ... INTO ...;
    if (!m_qe->has_trailing_into_clause()) {
      // Example: ... UNION SELECT * INTO OUTFILE 'foo' FROM ...;
      push_warning(thd, ER_WARN_DEPRECATED_INNER_INTO);
    } else if (m_has_trailing_locking_clauses) {
      // Example: ... UNION SELECT ... FROM ... INTO OUTFILE 'foo' FOR UPDATE;
      push_warning(thd, ER_WARN_DEPRECATED_INNER_INTO);
    }
  }

  DBUG_EXECUTE_IF("ast", Query_term *qn =
                             pc.select->master_query_expression()->query_term();
                  std::ostringstream buf; qn->debugPrint(0, buf);
                  DBUG_PRINT("ast", ("\n%s", buf.str().c_str())););

  if (thd->lex->sql_command == SQLCOM_SELECT)
    return new (thd->mem_root) Sql_cmd_select(thd->lex->result);
  else  // (thd->lex->sql_command == SQLCOM_DO)
    return new (thd->mem_root) Sql_cmd_do(nullptr);
}

/*
  Given a table in the source list, find a correspondent table in the
  list of table references.

  @param tbl    Source table to match.
  @param tables Table references list.

  @remark The source table list (tables listed before the FROM clause
  or tables listed in the FROM clause before the USING clause) may
  contain table names or aliases that must match unambiguously one,
  and only one, table in the target table list (table references list,
  after FROM/USING clause).

  @return Matching table, NULL if error.
*/

static Table_ref *multi_delete_table_match(Table_ref *tbl, Table_ref *tables) {
  Table_ref *match = nullptr;
  DBUG_TRACE;

  for (Table_ref *elem = tables; elem; elem = elem->next_local) {
    int cmp;

    if (tbl->is_fqtn && elem->is_alias) continue; /* no match */
    if (tbl->is_fqtn && elem->is_fqtn)
      cmp = my_strcasecmp(table_alias_charset, tbl->table_name,
                          elem->table_name) ||
            strcmp(tbl->db, elem->db);
    else if (elem->is_alias)
      cmp = my_strcasecmp(table_alias_charset, tbl->alias, elem->alias);
    else
      cmp = my_strcasecmp(table_alias_charset, tbl->table_name,
                          elem->table_name) ||
            strcmp(tbl->db, elem->db);

    if (cmp) continue;

    if (match) {
      my_error(ER_NONUNIQ_TABLE, MYF(0), elem->alias);
      return nullptr;
    }

    match = elem;
  }

  if (!match)
    my_error(ER_UNKNOWN_TABLE, MYF(0), tbl->table_name, "MULTI DELETE");

  return match;
}

/**
  Link tables in auxiliary table list of multi-delete with corresponding
  elements in main table list, and set proper locks for them.

  @param pc   Parse context
  @param delete_tables  List of tables to delete from

  @returns false if success, true if error
*/

static bool multi_delete_link_tables(Parse_context *pc,
                                     SQL_I_List<Table_ref> *delete_tables) {
  DBUG_TRACE;

  Table_ref *tables = pc->select->get_table_list();

  for (Table_ref *target_tbl = delete_tables->first; target_tbl;
       target_tbl = target_tbl->next_local) {
    /* All tables in aux_tables must be found in FROM PART */
    Table_ref *walk = multi_delete_table_match(target_tbl, tables);
    if (!walk) return true;
    if (!walk->is_derived()) {
      target_tbl->table_name = walk->table_name;
      target_tbl->table_name_length = walk->table_name_length;
    }
    walk->updating = target_tbl->updating;
    walk->set_lock(target_tbl->lock_descriptor());
    /* We can assume that tables to be deleted from are locked for write. */
    assert(walk->lock_descriptor().type >= TL_WRITE_ALLOW_WRITE);
    walk->mdl_request.set_type(mdl_type_for_dml(walk->lock_descriptor().type));
    target_tbl->correspondent_table = walk;  // Remember corresponding table
  }
  return false;
}

bool PT_delete::add_table(Parse_context *pc, Table_ident *table) {
  const ulong table_opts = is_multitable()
                               ? TL_OPTION_UPDATING | TL_OPTION_ALIAS
                               : TL_OPTION_UPDATING;
  const thr_lock_type lock_type = (opt_delete_options & DELETE_LOW_PRIORITY)
                                      ? TL_WRITE_LOW_PRIORITY
                                      : TL_WRITE_DEFAULT;
  const enum_mdl_type mdl_type = (opt_delete_options & DELETE_LOW_PRIORITY)
                                     ? MDL_SHARED_WRITE_LOW_PRIO
                                     : MDL_SHARED_WRITE;
  return !pc->select->add_table_to_list(
      pc->thd, table, opt_table_alias, table_opts, lock_type, mdl_type, nullptr,
      opt_use_partition, nullptr, pc);
}

Sql_cmd *PT_delete::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  Parse_context pc(thd, select);

  assert(lex->query_block == select);
  lex->sql_command = is_multitable() ? SQLCOM_DELETE_MULTI : SQLCOM_DELETE;
  lex->set_ignore(opt_delete_options & DELETE_IGNORE);
  select->init_order();
  if (opt_delete_options & DELETE_QUICK) select->add_base_options(OPTION_QUICK);

  if (contextualize_safe(&pc, m_with_clause))
    return nullptr; /* purecov: inspected */

  if (is_multitable()) {
    for (Table_ident **i = table_list.begin(); i != table_list.end(); ++i) {
      if (add_table(&pc, *i)) return nullptr;
    }
  } else if (add_table(&pc, table_ident))
    return nullptr;

  if (is_multitable()) {
    select->m_table_list.save_and_clear(&delete_tables);
    lex->query_tables = nullptr;
    lex->query_tables_last = &lex->query_tables;
  } else {
    select->m_table_nest.push_back(select->get_table_list());
  }
  Yacc_state *const yyps = &pc.thd->m_parser_state->m_yacc;
  yyps->m_lock_type = TL_READ_DEFAULT;
  yyps->m_mdl_type = MDL_SHARED_READ;

  if (is_multitable()) {
    if (contextualize_array(&pc, &join_table_list)) return nullptr;
    pc.select->context.table_list =
        pc.select->context.first_name_resolution_table =
            pc.select->get_table_list();
  }

  if (opt_where_clause != nullptr &&
      opt_where_clause->itemize(&pc, &opt_where_clause))
    return nullptr;
  select->set_where_cond(opt_where_clause);

  if (opt_order_clause != nullptr && opt_order_clause->contextualize(&pc))
    return nullptr;

  assert(select->select_limit == nullptr);
  if (opt_delete_limit_clause != nullptr) {
    if (opt_delete_limit_clause->itemize(&pc, &opt_delete_limit_clause))
      return nullptr;
    select->select_limit = opt_delete_limit_clause;
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
  }

  if (is_multitable() && multi_delete_link_tables(&pc, &delete_tables))
    return nullptr;

  if (opt_hints != nullptr && opt_hints->contextualize(&pc)) return nullptr;

  return new (thd->mem_root) Sql_cmd_delete(is_multitable(), &delete_tables);
}

Sql_cmd *PT_update::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  Parse_context pc(thd, select);

  lex->duplicates = DUP_ERROR;

  lex->set_ignore(opt_ignore);

  if (contextualize_safe(&pc, m_with_clause))
    return nullptr; /* purecov: inspected */

  if (contextualize_array(&pc, &join_table_list)) return nullptr;
  select->parsing_place = CTX_UPDATE_VALUE;

  if (column_list->contextualize(&pc) || value_list->contextualize(&pc)) {
    return nullptr;
  }
  select->fields = column_list->value;

  // Ensure we're resetting parsing context of the right select
  assert(select->parsing_place == CTX_UPDATE_VALUE);
  select->parsing_place = CTX_NONE;
  const bool is_multitable = select->m_table_list.elements > 1;
  lex->sql_command = is_multitable ? SQLCOM_UPDATE_MULTI : SQLCOM_UPDATE;

  /*
    In case of multi-update setting write lock for all tables may
    be too pessimistic. We will decrease lock level if possible in
    Sql_cmd_update::prepare_inner().
  */
  select->set_lock_for_tables(opt_low_priority);

  if (opt_where_clause != nullptr &&
      opt_where_clause->itemize(&pc, &opt_where_clause)) {
    return nullptr;
  }
  select->set_where_cond(opt_where_clause);

  if (opt_order_clause != nullptr && opt_order_clause->contextualize(&pc))
    return nullptr;

  assert(select->select_limit == nullptr);
  if (opt_limit_clause != nullptr) {
    if (opt_limit_clause->itemize(&pc, &opt_limit_clause)) return nullptr;
    select->select_limit = opt_limit_clause;
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
  }

  if (opt_hints != nullptr && opt_hints->contextualize(&pc)) return nullptr;

  return new (thd->mem_root) Sql_cmd_update(is_multitable, &value_list->value);
}

bool PT_insert_values_list::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  for (List_item *item_list : many_values) {
    for (auto it = item_list->begin(); it != item_list->end(); ++it) {
      if ((*it)->itemize(pc, &*it)) return true;
    }
  }

  return false;
}

Sql_cmd *PT_insert::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  Parse_context pc(thd, lex->current_query_block());

  // Currently there are two syntaxes (old and new, respectively) for INSERT
  // .. VALUES statements:
  //
  //  - INSERT .. VALUES (), () ..
  //  - INSERT .. VALUES ROW(), ROW() ..
  //
  // The latter is a table value constructor, i.e. it has an subquery
  // expression, while the former is the standard VALUES syntax. When the
  // non-standard VALUES() function (primarily used in ON DUPLICATE KEY UPDATE
  // update expressions) is deprecated in the future, the old syntax can be used
  // as a table value constructor as well.
  //
  // However, until such a change is made, we convert INSERT statements with
  // table value constructors into PT_insert objects that are equal to the old
  // syntax, as to enforce consistency by making sure they both follow the same
  // execution path.
  //
  // Note that this removes the constness of both row_value_list and
  // insert_query_expression, which should both be restored when deprecating
  // VALUES as mentioned above.
  if (has_query_block() &&
      insert_query_expression->is_table_value_constructor()) {
    row_value_list = insert_query_expression->get_row_value_list();
    assert(row_value_list != nullptr);

    insert_query_expression = nullptr;
  }

  if (is_replace) {
    lex->sql_command =
        has_query_block() ? SQLCOM_REPLACE_SELECT : SQLCOM_REPLACE;
    lex->duplicates = DUP_REPLACE;
  } else {
    lex->sql_command = has_query_block() ? SQLCOM_INSERT_SELECT : SQLCOM_INSERT;
    lex->duplicates = DUP_ERROR;
    lex->set_ignore(ignore);
  }

  Yacc_state *yyps = &pc.thd->m_parser_state->m_yacc;
  if (!pc.select->add_table_to_list(
          thd, table_ident, nullptr, TL_OPTION_UPDATING, yyps->m_lock_type,
          yyps->m_mdl_type, nullptr, opt_use_partition)) {
    return nullptr;
  }
  pc.select->set_lock_for_tables(lock_option);

  assert(lex->current_query_block() == lex->query_block);

  if (column_list->contextualize(&pc)) return nullptr;

  if (has_query_block()) {
    /*
      In INSERT/REPLACE INTO t ... SELECT the table_list initially contains
      here a table entry for the destination table `t'.
      Backup it and clean the table list for the processing of
      the query expression and push `t' back to the beginning of the
      table_list finally.

      @todo: Don't save the INSERT/REPLACE destination table in
             Query_block::table_list and remove this backup & restore.

      The following work only with the local list, the global list
      is created correctly in this case
    */
    SQL_I_List<Table_ref> save_list;
    pc.select->m_table_list.save_and_clear(&save_list);

    if (insert_query_expression->contextualize(&pc)) return nullptr;

    if (pc.finalize_query_expression()) return nullptr;

    /*
      The following work only with the local list, the global list
      is created correctly in this case
    */
    pc.select->m_table_list.push_front(&save_list);

    lex->bulk_insert_row_cnt = 0;
  } else {
    pc.select->parsing_place = CTX_INSERT_VALUES;
    if (row_value_list->contextualize(&pc)) return nullptr;
    // Ensure we're resetting parsing context of the right select
    assert(pc.select->parsing_place == CTX_INSERT_VALUES);
    pc.select->parsing_place = CTX_NONE;

    lex->bulk_insert_row_cnt = row_value_list->get_many_values().size();
  }

  // Ensure that further expressions are resolved against first query block
  assert(pc.select->select_number == 1);

  // Create a derived table to use as a table reference to the VALUES rows,
  // which can be referred to from ON DUPLICATE KEY UPDATE. Naming the derived
  // table columns is deferred to Sql_cmd_insert_base::prepare_inner, as this
  // requires the insert table to be resolved.
  Table_ref *values_table{nullptr};
  if (opt_values_table_alias != nullptr && opt_values_column_list != nullptr) {
    if (!strcmp(opt_values_table_alias, table_ident->table.str)) {
      my_error(ER_NONUNIQ_TABLE, MYF(0), opt_values_table_alias);
      return nullptr;
    }

    Table_ident *ti = new (pc.thd->mem_root)
        Table_ident(lex->query_block->master_query_expression());
    if (ti == nullptr) return nullptr;

    values_table = pc.select->add_table_to_list(
        pc.thd, ti, opt_values_table_alias, 0, TL_READ, MDL_SHARED_READ);
    if (values_table == nullptr) return nullptr;
  }

  if (opt_on_duplicate_column_list != nullptr) {
    assert(!is_replace);
    assert(opt_on_duplicate_value_list != nullptr &&
           opt_on_duplicate_value_list->elements() ==
               opt_on_duplicate_column_list->elements());

    lex->duplicates = DUP_UPDATE;
    Table_ref *first_table = lex->query_block->get_table_list();
    /* Fix lock for ON DUPLICATE KEY UPDATE */
    if (first_table->lock_descriptor().type == TL_WRITE_CONCURRENT_DEFAULT)
      first_table->set_lock({TL_WRITE_DEFAULT, THR_DEFAULT});

    pc.select->parsing_place = CTX_INSERT_UPDATE;

    if (opt_on_duplicate_column_list->contextualize(&pc) ||
        opt_on_duplicate_value_list->contextualize(&pc))
      return nullptr;

    // Ensure we're resetting parsing context of the right select
    assert(pc.select->parsing_place == CTX_INSERT_UPDATE);
    pc.select->parsing_place = CTX_NONE;
  }

  if (opt_hints != nullptr && opt_hints->contextualize(&pc)) return nullptr;

  Sql_cmd_insert_base *sql_cmd;
  if (has_query_block())
    sql_cmd =
        new (thd->mem_root) Sql_cmd_insert_select(is_replace, lex->duplicates);
  else
    sql_cmd =
        new (thd->mem_root) Sql_cmd_insert_values(is_replace, lex->duplicates);
  if (sql_cmd == nullptr) return nullptr;

  if (!has_query_block()) {
    sql_cmd->insert_many_values = row_value_list->get_many_values();
    sql_cmd->values_table = values_table;
    sql_cmd->values_column_list = opt_values_column_list;
  }

  sql_cmd->insert_field_list = column_list->value;
  if (opt_on_duplicate_column_list != nullptr) {
    assert(!is_replace);
    sql_cmd->update_field_list = opt_on_duplicate_column_list->value;
    sql_cmd->update_value_list = opt_on_duplicate_value_list->value;
  }

  return sql_cmd;
}

Sql_cmd *PT_call::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  Parse_context pc(thd, lex->current_query_block());

  if (opt_expr_list != nullptr && opt_expr_list->contextualize(&pc))
    return nullptr; /* purecov: inspected */

  lex->sql_command = SQLCOM_CALL;

  sp_add_own_used_routine(lex, thd, Sroutine_hash_entry::PROCEDURE, proc_name);

  mem_root_deque<Item *> *proc_args = nullptr;
  if (opt_expr_list != nullptr) proc_args = &opt_expr_list->value;

  return new (thd->mem_root) Sql_cmd_call(proc_name, proc_args);
}

bool PT_query_specification::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->m_stack.push_back(QueryLevel(pc->mem_root, SC_QUERY_SPECIFICATION));
  pc->select->parsing_place = CTX_SELECT_LIST;

  if (options.query_spec_options & SELECT_HIGH_PRIORITY) {
    Yacc_state *yyps = &pc->thd->m_parser_state->m_yacc;
    yyps->m_lock_type = TL_READ_HIGH_PRIORITY;
    yyps->m_mdl_type = MDL_SHARED_READ;
  }
  if (options.save_to(pc)) return true;

  if (item_list->contextualize(pc)) return true;

  // Ensure we're resetting parsing place of the right select
  assert(pc->select->parsing_place == CTX_SELECT_LIST);
  pc->select->parsing_place = CTX_NONE;

  if (contextualize_safe(pc, opt_into1)) return true;

  if (!from_clause.empty()) {
    if (contextualize_array(pc, &from_clause)) return true;
    pc->select->context.table_list =
        pc->select->context.first_name_resolution_table =
            pc->select->get_table_list();
  }

  if (itemize_safe(pc, &opt_where_clause) ||
      contextualize_safe(pc, opt_group_clause) ||
      itemize_safe(pc, &opt_having_clause))
    return true;

  pc->select->set_where_cond(opt_where_clause);
  pc->select->set_having_cond(opt_having_clause);

  /*
    Window clause is resolved under CTX_SELECT_LIST and not
    under CTX_WINDOW. Reasons being:
    1. Window functions are part of select list and the
    resolution of window definition happens along with
    window functions.
    2. It is tricky to resolve window definition under CTX_WINDOW
    and window functions under CTX_SELECT_LIST.
    3. Unnamed window definitions are anyways naturally placed in
    select list.
    4. Named window definition are not placed in select list of
    the query. But if this window definition is
    used by any window functions, then we resolve under CTX_SELECT_LIST.
    5. Because of all of the above, unused window definitions are
    resolved under CTX_SELECT_LIST. (These unused window definitions
    are removed after syntactic and semantic checks are done).
  */

  pc->select->parsing_place = CTX_SELECT_LIST;
  if (contextualize_safe(pc, opt_window_clause)) return true;

  pc->select->parsing_place = CTX_QUALIFY;
  if (itemize_safe(pc, &opt_qualify_clause)) return true;
  pc->select->set_qualify_cond(opt_qualify_clause);

  if (opt_qualify_clause != nullptr) {
    pc->thd->lex->set_execute_only_in_hypergraph_optimizer(
        /*execute_in_hypergraph_optimizer_param=*/true, QUALIFY_CLAUSE);
  }

  pc->select->parsing_place = CTX_NONE;

  QueryLevel ql = pc->m_stack.back();
  pc->m_stack.pop_back();
  pc->m_stack.back().m_elts.push_back(pc->select);
  return (opt_hints != nullptr ? opt_hints->contextualize(pc) : false);
}

void PT_query_specification::add_json_info(Json_object *obj) {
  std::string select_options;

  get_select_options_str(options.query_spec_options, &select_options);
  if (!select_options.empty())
    obj->add_alias("query_spec_options",
                   create_dom_ptr<Json_string>(select_options));
}

bool PT_table_value_constructor::do_contextualize(Parse_context *pc) {
  pc->m_stack.push_back(QueryLevel(pc->mem_root, SC_TABLE_VALUE_CONSTRUCTOR));

  if (row_value_list->contextualize(pc)) return true;

  pc->select->is_table_value_constructor = true;
  pc->select->row_value_list = &row_value_list->get_many_values();

  // Some queries, such as CREATE TABLE with SELECT, require item_list to
  // contain items to call Query_block::prepare.
  for (Item *item : *pc->select->row_value_list->front()) {
    pc->select->fields.push_back(item);
  }

  QueryLevel ql = pc->m_stack.back();
  pc->m_stack.pop_back();
  pc->m_stack.back().m_elts.push_back(pc->select);

  return false;
}

bool PT_query_expression::contextualize_order_and_limit(Parse_context *pc) {
  /*
    Quick reject test. We don't need to do anything if there are no limit
    or order by clauses.
  */
  if (m_order == nullptr && m_limit == nullptr) return false;

  if (m_body->can_absorb_order_and_limit(m_order != nullptr,
                                         m_limit != nullptr)) {
    if (contextualize_safe(pc, m_order, m_limit)) return true;
  } else {
    LEX *lex = pc->thd->lex;
    Query_block *orig_query_block = pc->select;
    pc->select = nullptr;
    lex->push_context(&pc->select->context);
    assert(pc->select->parsing_place == CTX_NONE);

    bool res = contextualize_safe(pc, m_order, m_limit);

    lex->pop_context();
    pc->select = orig_query_block;

    if (res) return true;
  }
  return false;
}

bool PT_table_factor_function::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || m_expr->itemize(pc, &m_expr)) return true;

  if (m_path->itemize(pc, &m_path)) return true;

  auto nested_columns = new (pc->mem_root) List<Json_table_column>;
  if (nested_columns == nullptr) return true;  // OOM

  for (auto col : *m_nested_columns) {
    if (col->contextualize(pc) || nested_columns->push_back(col->get_column()))
      return true;
  }

  auto root_el = new (pc->mem_root) Json_table_column(m_path, nested_columns);
  auto *root_list = new (pc->mem_root) List<Json_table_column>;
  if (root_el == nullptr || root_list == nullptr ||
      root_list->push_front(root_el))
    return true;  // OOM

  auto jtf = new (pc->mem_root)
      Table_function_json(m_table_alias.str, m_expr, root_list);
  if (jtf == nullptr) return true;  // OOM

  LEX_CSTRING alias;
  alias.length = strlen(jtf->func_name());
  alias.str = sql_strmake(jtf->func_name(), alias.length);
  if (alias.str == nullptr) return true;  // OOM

  auto ti = new (pc->mem_root) Table_ident(alias, jtf);
  if (ti == nullptr) return true;

  m_table_ref = pc->select->add_table_to_list(pc->thd, ti, m_table_alias.str, 0,
                                              TL_READ, MDL_SHARED_READ);
  if (m_table_ref == nullptr || pc->select->add_joined_table(m_table_ref))
    return true;

  return false;
}

PT_derived_table::PT_derived_table(const POS &pos, bool lateral,
                                   PT_subquery *subquery,
                                   const LEX_CSTRING &table_alias,
                                   Create_col_name_list *column_names)
    : super(pos),
      m_lateral(lateral),
      m_subquery(subquery),
      m_table_alias(table_alias.str),
      column_names(*column_names) {
  m_subquery->m_is_derived_table = true;
}

bool PT_derived_table::do_contextualize(Parse_context *pc) {
  Query_block *outer_query_block = pc->select;

  outer_query_block->parsing_place = CTX_DERIVED;
  assert(outer_query_block->linkage != GLOBAL_OPTIONS_TYPE);

  /*
    Determine the immediate outer context for the derived table:
    - if lateral: context of query which owns the FROM i.e. outer_query_block
    - if not lateral: context of query outer to query which owns the FROM.
    For a lateral derived table. this is just a preliminary decision.
    Name resolution {Item_field,Item_ref}::fix_fields() may use or ignore this
    outer context depending on where the derived table is placed in it.
  */
  if (!m_lateral) {
    pc->thd->lex->push_context(outer_query_block->context.outer_context);
  }

  if (m_subquery->contextualize(pc)) return true;

  if (!m_lateral) pc->thd->lex->pop_context();

  outer_query_block->parsing_place = CTX_NONE;

  assert(pc->select->next_query_block() == nullptr);

  Query_expression *unit = pc->select->first_inner_query_expression();
  pc->select = outer_query_block;
  Table_ident *ti = new (pc->mem_root) Table_ident(unit);
  if (ti == nullptr) return true;

  m_table_ref = pc->select->add_table_to_list(pc->thd, ti, m_table_alias, 0,
                                              TL_READ, MDL_SHARED_READ);
  if (m_table_ref == nullptr) return true;
  if (column_names.size()) m_table_ref->set_derived_column_names(&column_names);
  if (m_lateral) {
    // Mark the unit as LATERAL, by turning on one bit in the map:
    m_table_ref->derived_query_expression()->m_lateral_deps =
        OUTER_REF_TABLE_BIT;
  }
  if (pc->select->add_joined_table(m_table_ref)) return true;

  return false;
}

void PT_derived_table::add_json_info(Json_object *obj) {
  if (m_table_alias != nullptr) {
    obj->add_alias("table_alias", create_dom_ptr<Json_string>(m_table_alias));
  }
  if (column_names.size() != 0) {
    String column_names_str;
    print_derived_column_names(nullptr, &column_names_str, &column_names);
    obj->add_alias("table_columns",
                   create_dom_ptr<Json_string>(column_names_str.ptr(),
                                               column_names_str.length()));
  }
  obj->add_alias("lateral", create_dom_ptr<Json_boolean>(m_lateral));
}

bool PT_table_factor_joined_table::do_contextualize(Parse_context *pc) {
  if (Parse_tree_node::do_contextualize(pc)) return true;

  Query_block *outer_query_block = pc->select;
  if (outer_query_block->init_nested_join(pc->thd)) return true;

  if (m_joined_table->contextualize(pc)) return true;
  m_table_ref = m_joined_table->m_table_ref;

  if (outer_query_block->end_nested_join() == nullptr) return true;

  return false;
}

static Surrounding_context qt2sc(Query_term_type qtt) {
  switch (qtt) {
    case QT_UNION:
      return SC_UNION_ALL;
    case QT_EXCEPT:
      return SC_EXCEPT_ALL;
    case QT_INTERSECT:
      return SC_INTERSECT_ALL;
    default:
      assert(false);
  }
  return SC_TOP;
}

/**
  Possibly merge lower syntactic levels of set operations (UNION, INTERSECT and
  EXCEPT) into setop, and set new last DISTINCT index for setop. We only ever
  merge set operations of the same kind, but even that, not always: we prefer
  streaming of UNIONs when possible, and merging UNIONs with a mix of DISTINCT
  and ALL at the top level may lead to inefficient evaluation. Streaming only
  makes sense when the result set is sent from the server, i.e. not further
  materialized, e.g. for ORDER BY or windowing.

  For example, the query:

           EXPLAIN FORMAT=tree
           SELECT * FROM t1 UNION DISTINCT
           SELECT * FROM t2 UNION ALL
           SELECT * FROM t3;

    will yield the plan:

        -> Append
           -> Stream results
               -> Table scan on \<union temporary\>
                   -> Union materialize with deduplication
                       -> Table scan on t1
                       -> Table scan on t2
           -> Stream results
               -> Table scan on t3

    but
           EXPLAIN FORMAT=tree
           SELECT * FROM t1 UNION DISTINCT
           SELECT * FROM t2 UNION DISTINCT
           SELECT * FROM t3;

    will yield the following plan, i.e. we merge the two syntactic levels. The
    former case could also be merged, but the we'd need to write the rows of t3
    to the temporary table, which gives a performance penalty, so we don't
    merge.

        -> Table scan on \<union temporary\>
            -> Union materialize with deduplication
                -> Table scan on t1
                -> Table scan on t2
                -> Table scan on t3

    If have an outer ORDER BY, we will see how this would look (with merge):

           EXPLAIN FORMAT=tree
           SELECT * FROM t1 UNION DISTINCT
           SELECT * FROM t2 UNION ALL
           SELECT * FROM t3
           ORDER BY a;

   will yield

       -> Sort: a
           -> Table scan on \<union temporary\>
               -> Union materialize with deduplication
                   -> Table scan on t1
                   -> Table scan on t2
                   -> Disable deduplication
                       -> Table scan on t3

    since in this case, merging is advantageous, in that we need only one
    temporary file. Another interesting case is when the upper level is
    DISTINCT:

           EXPLAIN FORMAT=tree
           SELECT * FROM t1 UNION DISTINCT
           ( SELECT * FROM t2 UNION ALL
             SELECT * FROM t3 );

    will merge and remove the lower ALL level:

        -> Table scan on <union temporary>
            -> Union materialize with deduplication
                -> Table scan on t1
                -> Table scan on t2
                -> Table scan on t3

  For INTERSECT, the presence of one DISTINCT operator effectively
  makes any INTERSECT ALL equivalent to DISTINCT, so we only retain ALL if
  all operators are ALL, i.e. for a N-ary INTERSECT with at least one DISTINCT,
  has_mixed_distinct_operators always returns false.
  We aggressively merge up all sub-nests for INTERSECT.

  INTERSECT ALL is only binary due to current implementation method, no merge
  up.

  EXCEPT [ALL] is not right associative, so be careful when merging: we only
  merge up a left-most nested EXCEPT into an outer level EXCEPT, since we
  evaluate from left to right.  Note that for an N-ary EXCEPT operation, a mix
  of ALL and DISTINCT is meaningful and supported. After the first operator
  with DISTINCT, further ALL operators are moot since no duplicates are
  left. Example explain:

      SELECT * FROM r EXCEPT ALL SELECT * FROM s EXCEPT SELECT * FROM t

  -> Table scan on \<except temporary\>  (cost=..)
    -> Except materialize with deduplication  (cost=..)
        -> Table scan on r  (cost=..)
        -> Disable deduplication
            -> Table scan on s  (cost=..)
        -> Table scan on t  (cost=..)

  We can see that the first two operand tables are compared with ALL ("disable
  de-duplication"), whereas the final one, t, is DISTINCT.

  @param      pc   the parse context
  @param      setop the set operation query term to be filled in with children
  @param      ql   parsing query level
*/
void PT_set_operation::merge_descendants(Parse_context *pc,
                                         Query_term_set_op *setop,
                                         QueryLevel &ql) {
  // If a child is a Query_block, include it in this set op list as is.  If a
  // child is itself a set operation, include its members in this set
  // operation's descendant list if allowed.

  int count = 0;  /// computes number for members if we collapse

  // last_distinct is used by UNION and INTERSECT. For N-ary intersect, if
  // DISTINCT is present at all, its final value will be N-1, since ALL is
  // irrelevant once we have one DISTINCT for INTERSECT. Note that the
  // computation of last_distinct may be wrong for EXCEPT, but it doesn't rely
  // on it so we don't care.
  int64_t last_distinct = 0;

  // first_distinct is used by EXCEPT only, so its computation for UNION and
  // DISTINCT may be wrong/misleading.
  int64_t first_distinct = std::numeric_limits<int64_t>::max();

  const Query_term_type op = setop->term_type();
  Surrounding_context sc = qt2sc(op);

  if (m_is_distinct) {
    for (Query_term *elt : ql.m_elts) {
      // We only merge same kind, and only if no LIMIT on it
      if (elt->term_type() == QT_QUERY_BLOCK || /* 1 */
          elt->term_type() != op ||             /* 2 */
          (op == QT_EXCEPT && count > 0)) {     /* 3 */
        // (1) Nothing nested in this operand position, or (2) it's another
        // kind of set operation or (3) EXCEPT in non-left position (EXCEPT is
        // not right associative), hence no merge.
        count++;
        if (first_distinct == std::numeric_limits<int64_t>::max())
          first_distinct = 1;
        last_distinct = count - 1;
        setop->m_children.push_back(elt);
      } else if (Query_term_set_op *lower = down_cast<Query_term_set_op *>(elt);
                 elt->query_block()->select_limit == nullptr /* 4 */) {
        // (4) we have no LIMIT in the nest, so we can merge, proceed.
        if (lower->m_first_distinct < std::numeric_limits<int64_t>::max() &&
            lower->m_first_distinct + count < first_distinct) {
          assert(op != QT_EXCEPT || count == 0);
          first_distinct = count + lower->m_first_distinct;
        } else if (first_distinct == std::numeric_limits<int64_t>::max()) {
          first_distinct = lower->m_children.size();
        }
        // upper DISTINCT trumps lower ALL or DISTINCT, so ignore nest's last
        // distinct
        last_distinct = count + lower->m_children.size() - 1;
        count = count + lower->m_children.size();
        // fold in children
        for (auto child : lower->m_children) setop->m_children.push_back(child);
      } else {
        // similar kind of set operation, but contains limit, so do not merge
        count++;
        last_distinct = count - 1;
        setop->m_children.push_back(elt);
      }
    }
  } else {
    for (Query_term *elt : ql.m_elts) {
      // We only collapse same kind, and only if no LIMIT on it. Also, we do
      // not collapse INTERSECT ALL due to difficulty in computing it if we
      // do, cf. logic in MaterializeIterator<Profiler>::MaterializeOperand.
      if (elt->term_type() == QT_QUERY_BLOCK || /* 1 */
          elt->term_type() != op ||             /* 2 */
          (op == QT_EXCEPT && count > 0) ||     /* 3 */
          elt->query_block()->select_limit != nullptr) {
        // (1) Nothing nested in this operand position, or (2) it's another kind
        // of set operation, or non-left EXCEPT (3), hence no merge
        count++;
        setop->m_children.push_back(elt);
      } else if (Query_term_set_op *lower = down_cast<Query_term_set_op *>(elt);
                 (count == 0 ||  // first operand can always be merged
                                 // upper, lower are ALL, so ok:
                  lower->m_last_distinct == 0 ||
                  op == QT_INTERSECT /* maybe merge */)) {
        if (lower->m_last_distinct > 0) {
          if (pc->is_top_level_union_all(sc)) {
            // If we merge we lose the chance to stream the right table,
            // so don't.
            setop->m_children.push_back(elt);
            count++;
          } else {
            if (lower->m_first_distinct < std::numeric_limits<int64_t>::max() &&
                first_distinct == std::numeric_limits<int64_t>::max()) {
              first_distinct = count + lower->m_first_distinct;
            }
            last_distinct = count + lower->m_last_distinct;
            count = count + lower->m_children.size();
            for (auto child : lower->m_children)
              setop->m_children.push_back(child);
          }
        } else {
          // upper and lower level are both ALL, so ok to merge, unless we have
          // INTERSECT
          if (op != QT_INTERSECT) {
            if (lower->m_first_distinct < std::numeric_limits<int64_t>::max() &&
                first_distinct == std::numeric_limits<int64_t>::max()) {
              first_distinct = count + lower->m_first_distinct;
            }
            count = count + lower->m_children.size();
            for (auto child : lower->m_children)
              setop->m_children.push_back(child);
          } else {
            // do not merge INTERSECT ALL: the execution time logic can only
            // handle binary INTERSECT ALL.
            count++;
            setop->m_children.push_back(elt);
          }
        }
      } else if (!pc->is_top_level_union_all(sc) &&
                 lower->m_last_distinct == 0) {
        // We have a higher and lower level setop ALL, so ok to merge
        // unless we have a top level UNION ALL in which case we prefer
        // streaming.
        if (count + lower->m_first_distinct <
                std::numeric_limits<int64_t>::max() &&
            first_distinct == std::numeric_limits<int64_t>::max()) {
          first_distinct = count + lower->m_first_distinct;
        }
        count = count + lower->m_children.size();
        for (auto child : lower->m_children) setop->m_children.push_back(child);
      } else {
        // do not merge
        count++;
        setop->m_children.push_back(elt);
      }
    }
  }
  if (last_distinct > 0 && op == QT_INTERSECT)  // distinct always wins
    last_distinct = setop->m_children.size() - 1;
  setop->m_last_distinct = last_distinct;
  setop->m_first_distinct = first_distinct;
}

bool PT_set_operation::contextualize_setop(Parse_context *pc,
                                           Query_term_type setop_type,
                                           Surrounding_context context) {
  pc->m_stack.push_back(QueryLevel(pc->mem_root, context));
  if (super::do_contextualize(pc)) return true;

  if (m_lhs->contextualize(pc)) return true;

  pc->select = pc->thd->lex->new_set_operation_query(pc->select);

  if (pc->select == nullptr || m_rhs->contextualize(pc)) return true;

  pc->thd->lex->pop_context();

  QueryLevel ql = pc->m_stack.back();
  pc->m_stack.pop_back();

  Query_term_set_op *setop = nullptr;
  switch (setop_type) {
    case QT_UNION:
      setop = new (pc->mem_root) Query_term_union(pc->mem_root);
      break;
    case QT_EXCEPT:
      setop = new (pc->mem_root) Query_term_except(pc->mem_root);
      break;
    case QT_INTERSECT:
      setop = new (pc->mem_root) Query_term_intersect(pc->mem_root);
      break;
    default:
      assert(false);
  }
  if (setop == nullptr) return true;

  merge_descendants(pc, setop, ql);
  setop->label_children();

  Query_expression *qe = pc->select->master_query_expression();
  if (setop->set_block(qe->create_post_processing_block(setop))) return true;
  pc->m_stack.back().m_elts.push_back(setop);
  return false;
}

bool PT_union::do_contextualize(Parse_context *pc) {
  return contextualize_setop(pc, QT_UNION,
                             m_is_distinct ? SC_UNION_DISTINCT : SC_UNION_ALL);
}

bool PT_except::do_contextualize(Parse_context *pc [[maybe_unused]]) {
  return contextualize_setop(
      pc, QT_EXCEPT, m_is_distinct ? SC_EXCEPT_DISTINCT : SC_EXCEPT_ALL);
}

bool PT_intersect::do_contextualize(Parse_context *pc [[maybe_unused]]) {
  return contextualize_setop(
      pc, QT_INTERSECT,
      m_is_distinct ? SC_INTERSECT_DISTINCT : SC_INTERSECT_ALL);
}

static bool setup_index(keytype key_type, const LEX_STRING name,
                        PT_base_index_option *type,
                        List<PT_key_part_specification> *columns,
                        Index_options options, Table_ddl_parse_context *pc) {
  *pc->key_create_info = default_key_create_info;

  if (type != nullptr && type->contextualize(pc)) return true;

  if (contextualize_nodes(options, pc)) return true;

  List_iterator<PT_key_part_specification> li(*columns);

  if ((key_type == KEYTYPE_FULLTEXT || key_type == KEYTYPE_SPATIAL ||
       pc->key_create_info->algorithm == HA_KEY_ALG_HASH)) {
    PT_key_part_specification *kp;
    while ((kp = li++)) {
      if (kp->is_explicit()) {
        my_error(ER_WRONG_USAGE, MYF(0), "spatial/fulltext/hash index",
                 "explicit index order");
        return true;
      }
    }
  }

  List<Key_part_spec> cols;
  for (PT_key_part_specification &kp : *columns) {
    if (kp.contextualize(pc)) return true;

    Key_part_spec *spec;
    if (kp.has_expression()) {
      spec =
          new (pc->mem_root) Key_part_spec(kp.get_expression(), kp.get_order());
    } else {
      spec = new (pc->mem_root) Key_part_spec(
          kp.get_column_name(), kp.get_prefix_length(), kp.get_order());
    }
    if (spec == nullptr || cols.push_back(spec)) {
      return true; /* purecov: deadcode */
    }
  }

  Key_spec *key =
      new (pc->mem_root) Key_spec(pc->mem_root, key_type, to_lex_cstring(name),
                                  pc->key_create_info, false, true, cols);
  if (key == nullptr || pc->alter_info->key_list.push_back(key)) return true;

  return false;
}

Sql_cmd *PT_create_index_stmt::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  Query_block *query_block = lex->current_query_block();

  thd->lex->sql_command = SQLCOM_CREATE_INDEX;

  if (query_block->add_table_to_list(thd, m_table_ident, nullptr,
                                     TL_OPTION_UPDATING, TL_READ_NO_INSERT,
                                     MDL_SHARED_UPGRADABLE) == nullptr)
    return nullptr;

  Table_ddl_parse_context pc(thd, query_block, &m_alter_info);

  m_alter_info.flags = Alter_info::ALTER_ADD_INDEX;

  if (setup_index(m_keytype, m_name, m_type, m_columns, m_options, &pc))
    return nullptr;

  m_alter_info.requested_algorithm = m_algo;
  m_alter_info.requested_lock = m_lock;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_create_index(&m_alter_info);
}

bool PT_inline_index_definition::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  if (setup_index(m_keytype, m_name, m_type, m_columns, m_options, pc))
    return true;

  if (m_keytype == KEYTYPE_PRIMARY && !pc->key_create_info->is_visible)
    my_error(ER_PK_INDEX_CANT_BE_INVISIBLE, MYF(0));

  return false;
}

bool PT_foreign_key_definition::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *const thd = pc->thd;
  LEX *const lex = thd->lex;

  LEX_CSTRING db;
  LEX_CSTRING orig_db;

  if (m_referenced_table->db.str) {
    orig_db = m_referenced_table->db;

    if (check_db_name(orig_db.str, orig_db.length) != Ident_name_check::OK)
      return true;

    if (lower_case_table_names) {
      char *db_str = thd->strmake(orig_db.str, orig_db.length);
      if (db_str == nullptr) return true;  // OOM
      db.length = my_casedn_str(files_charset_info, db_str);
      db.str = db_str;
    } else
      db = orig_db;
  } else {
    /*
      Before 8.0 foreign key metadata was handled by SEs and they
      assumed that parent table belongs to the same database as
      child table unless FQTN was used (and connection's current
      database was ignored). We keep behavior compatible even
      though this is inconsistent with interpretation of non-FQTN
      table names in other contexts.

      If this is ALTER TABLE with RENAME TO <db_name.table_name>
      clause we need to use name of the target database.
    */
    if (pc->alter_info->new_db_name.str) {
      db = orig_db = pc->alter_info->new_db_name;
    } else {
      Table_ref *child_table = lex->query_block->get_table_list();
      db = orig_db = LEX_CSTRING{child_table->db, child_table->db_length};
    }
  }

  Ident_name_check ident_check_status = check_table_name(
      m_referenced_table->table.str, m_referenced_table->table.length);
  if (ident_check_status != Ident_name_check::OK) {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), m_referenced_table->table.str);
    return true;
  }

  LEX_CSTRING table_name;

  if (lower_case_table_names) {
    char *table_name_str = thd->strmake(m_referenced_table->table.str,
                                        m_referenced_table->table.length);
    if (table_name_str == nullptr) return true;  // OOM
    table_name.length = my_casedn_str(files_charset_info, table_name_str);
    table_name.str = table_name_str;
  } else
    table_name = m_referenced_table->table;

  lex->key_create_info = default_key_create_info;

  /*
    If present name from the CONSTRAINT clause is used as name of generated
    supporting index (which is created in cases when there is no explicitly
    created supporting index). Otherwise, the FOREIGN KEY index_name value
    is used. If both are missing name of generated supporting index is
    automatically produced.
  */
  const LEX_CSTRING key_name =
      to_lex_cstring(m_constraint_name.str ? m_constraint_name
                     : m_key_name.str      ? m_key_name
                                           : NULL_STR);

  if (key_name.str && check_string_char_length(key_name, "", NAME_CHAR_LEN,
                                               system_charset_info, true)) {
    my_error(ER_TOO_LONG_IDENT, MYF(0), key_name.str);
    return true;
  }

  List<Key_part_spec> cols;
  if (m_columns != nullptr) {
    // Foreign key specified at the table level.
    for (PT_key_part_specification &kp : *m_columns) {
      if (kp.contextualize(pc)) return true;

      Key_part_spec *spec = new (pc->mem_root) Key_part_spec(
          kp.get_column_name(), kp.get_prefix_length(), kp.get_order());
      if (spec == nullptr || cols.push_back(spec)) {
        return true; /* purecov: deadcode */
      }
    }
  } else {
    // Foreign key specified at the column level.
    assert(m_column_name.str != nullptr);
    Key_part_spec *spec = new (pc->mem_root)
        Key_part_spec(to_lex_cstring(m_column_name), 0, ORDER_ASC);
    if (spec == nullptr || cols.push_back(spec)) {
      return true; /* purecov: deadcode */
    }
  }

  /*
    We always use value from CONSTRAINT clause as a foreign key name.
    If it is not present we use generated name as a foreign key name
    (i.e. we ignore value from FOREIGN KEY index_name part).

    Validity of m_constraint_name has been already checked by the code
    above that handles supporting index name.
  */
  Key_spec *foreign_key = new (pc->mem_root) Foreign_key_spec(
      pc->mem_root, to_lex_cstring(m_constraint_name), cols, db, orig_db,
      table_name, m_referenced_table->table, m_ref_list, m_fk_delete_opt,
      m_fk_update_opt, m_fk_match_option);
  if (foreign_key == nullptr || pc->alter_info->key_list.push_back(foreign_key))
    return true;
  /* Only used for ALTER TABLE. Ignored otherwise. */
  pc->alter_info->flags |= Alter_info::ADD_FOREIGN_KEY;

  Key_spec *key =
      new (pc->mem_root) Key_spec(thd->mem_root, KEYTYPE_MULTIPLE, key_name,
                                  &default_key_create_info, true, true, cols);
  if (key == nullptr || pc->alter_info->key_list.push_back(key)) return true;

  return false;
}

bool PT_with_list::push_back(PT_common_table_expr *el) {
  const LEX_STRING &n = el->name();
  for (auto previous : m_elements) {
    const LEX_STRING &pn = previous->name();
    if (pn.length == n.length && !memcmp(pn.str, n.str, n.length)) {
      my_error(ER_NONUNIQ_TABLE, MYF(0), n.str);
      return true;
    }
  }
  return m_elements.push_back(el);
}

PT_common_table_expr::PT_common_table_expr(
    const POS &pos, const LEX_STRING &name, const LEX_STRING &subq_text,
    uint subq_text_offs, PT_subquery *subq_node,
    const Create_col_name_list *column_names, MEM_ROOT *mem_root)
    : super(pos),
      m_name(name),
      m_subq_text(subq_text),
      m_subq_text_offset(subq_text_offs),
      m_subq_node(subq_node),
      m_column_names(*column_names),
      m_postparse(mem_root) {
  if (lower_case_table_names && m_name.length) {
    // Lowercase name, as in Query_block::add_table_to_list()
    m_name.length = my_casedn_str(files_charset_info, m_name.str);
  }
  m_postparse.name = m_name;
}

bool PT_common_table_expr::do_contextualize(Parse_context *pc) {
  // The subquery node is reparsed and contextualized under the table, not
  // here. But for showing parse tree, we need to show it at it's originally
  // supplied place, i.e. under the WITH clause.
  if (pc->m_show_parse_tree != nullptr && m_subq_node != nullptr)
    return m_subq_node->contextualize(pc);
  return false;
}

void PT_common_table_expr::add_json_info(Json_object *obj) {
  obj->add_alias("cte_name",
                 create_dom_ptr<Json_string>(m_name.str, m_name.length));
  if (m_column_names.size() != 0) {
    String column_names_str;
    print_derived_column_names(nullptr, &column_names_str, &m_column_names);
    obj->add_alias("cte_columns",
                   create_dom_ptr<Json_string>(column_names_str.ptr(),
                                               column_names_str.length()));
  }
}

bool PT_with_clause::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true; /* purecov: inspected */
  // WITH complements a query expression (a unit).
  pc->select->master_query_expression()->m_with_clause = this;

  for (auto *el : m_list->elements())
    if (el->contextualize(pc)) return true;

  return false;
}

void PT_with_clause::print(const THD *thd, String *str,
                           enum_query_type query_type) {
  size_t len1 = str->length();
  str->append("with ");
  if (m_recursive) str->append("recursive ");
  size_t len2 = str->length(), len3 = len2;
  for (auto el : m_list->elements()) {
    if (str->length() != len3) {
      str->append(", ");
      len3 = str->length();
    }
    el->print(thd, str, query_type);
  }
  if (str->length() == len2)
    str->length(len1);  // don't print an empty WITH clause
  else
    str->append(" ");
}

void PT_common_table_expr::print(const THD *thd, String *str,
                                 enum_query_type query_type) {
  size_t len = str->length();
  append_identifier(thd, str, m_name.str, m_name.length);
  if (m_column_names.size())
    print_derived_column_names(thd, str, &m_column_names);
  str->append(" as ");

  /*
    Printing the raw text (this->m_subq_text) would lack:
    - expansion of '||' (which can mean CONCAT or OR, depending on
    sql_mode's PIPES_AS_CONCAT (the effect would be that a view containing
    a CTE containing '||' would change behaviour if sql_mode was
    changed between its creation and its usage).
    - quoting of table identifiers
    - expansion of the default db.
    So, we rather locate one resolved query expression for this CTE; for
    it to be intact this query expression must be non-merged. And we print
    it.
    If query expression has been merged everywhere, its Query_expression is
    gone and printing this CTE can be skipped. Note that when we print the
    view's body to the data dictionary, no merging is done.
  */
  bool found = false;
  for (auto *tl : m_postparse.references) {
    if (!tl->is_merged() &&
        // If 2+ references exist, show the one which is shown in EXPLAIN
        tl->query_block_id_for_explain() == tl->query_block_id()) {
      str->append('(');
      tl->derived_query_expression()->print(thd, str, query_type);
      str->append(')');
      found = true;
      break;
    }
  }
  if (!found) str->length(len);  // don't print a useless CTE definition
}

bool PT_create_table_engine_option::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  pc->create_info->used_fields |= HA_CREATE_USED_ENGINE;
  const bool is_temp_table = pc->create_info->options & HA_LEX_CREATE_TMP_TABLE;
  return resolve_engine(pc->thd, engine, is_temp_table, false,
                        &pc->create_info->db_type);
}

bool PT_create_table_secondary_engine_option::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  pc->create_info->used_fields |= HA_CREATE_USED_SECONDARY_ENGINE;
  pc->create_info->secondary_engine = m_secondary_engine;
  return false;
}

bool PT_create_stats_auto_recalc_option::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  switch (value) {
    case Ternary_option::ON:
      pc->create_info->stats_auto_recalc = HA_STATS_AUTO_RECALC_ON;
      break;
    case Ternary_option::OFF:
      pc->create_info->stats_auto_recalc = HA_STATS_AUTO_RECALC_OFF;
      break;
    case Ternary_option::DEFAULT:
      pc->create_info->stats_auto_recalc = HA_STATS_AUTO_RECALC_DEFAULT;
      break;
    default:
      assert(false);
  }
  pc->create_info->used_fields |= HA_CREATE_USED_STATS_AUTO_RECALC;
  return false;
}

bool PT_create_stats_stable_pages::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  pc->create_info->stats_sample_pages = value;
  pc->create_info->used_fields |= HA_CREATE_USED_STATS_SAMPLE_PAGES;
  return false;
}

bool PT_create_union_option::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *const thd = pc->thd;
  LEX *const lex = thd->lex;
  const Yacc_state *yyps = &thd->m_parser_state->m_yacc;

  Table_ref **exclude_merge_engine_tables = lex->query_tables_last;
  SQL_I_List<Table_ref> save_list;
  lex->query_block->m_table_list.save_and_clear(&save_list);
  if (pc->select->add_tables(thd, tables, TL_OPTION_UPDATING, yyps->m_lock_type,
                             yyps->m_mdl_type))
    return true;
  /*
    Move the union list to the merge_list and exclude its tables
    from the global list.
  */
  pc->create_info->merge_list = lex->query_block->m_table_list;
  lex->query_block->m_table_list = save_list;
  /*
    When excluding union list from the global list we assume that
    elements of the former immediately follow elements which represent
    table being created/altered and parent tables.
  */
  assert(*exclude_merge_engine_tables == pc->create_info->merge_list.first);
  *exclude_merge_engine_tables = nullptr;
  lex->query_tables_last = exclude_merge_engine_tables;

  pc->create_info->used_fields |= HA_CREATE_USED_UNION;
  return false;
}

bool set_default_charset(HA_CREATE_INFO *create_info,
                         const CHARSET_INFO *value) {
  assert(value != nullptr);

  if ((create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
      create_info->default_table_charset &&
      !my_charset_same(create_info->default_table_charset, value)) {
    my_error(ER_CONFLICTING_DECLARATIONS, MYF(0), "CHARACTER SET ",
             create_info->default_table_charset->csname, "CHARACTER SET ",
             value->csname);
    return true;
  }
  if ((create_info->used_fields & HA_CREATE_USED_DEFAULT_COLLATE) == 0)
    create_info->default_table_charset = value;
  create_info->used_fields |= HA_CREATE_USED_DEFAULT_CHARSET;
  return false;
}

bool PT_create_table_default_charset::do_contextualize(
    Table_ddl_parse_context *pc) {
  return (super::do_contextualize(pc) ||
          set_default_charset(pc->create_info, value));
}

bool set_default_collation(HA_CREATE_INFO *create_info,
                           const CHARSET_INFO *collation) {
  assert(collation != nullptr);
  assert((create_info->default_table_charset == nullptr) ==
         ((create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) == 0));

  if (merge_charset_and_collation(create_info->default_table_charset, collation,
                                  &create_info->default_table_charset)) {
    return true;
  }
  create_info->used_fields |= HA_CREATE_USED_DEFAULT_CHARSET;
  create_info->used_fields |= HA_CREATE_USED_DEFAULT_COLLATE;
  return false;
}

bool PT_create_table_default_collation::do_contextualize(
    Table_ddl_parse_context *pc) {
  return (super::do_contextualize(pc) ||
          set_default_collation(pc->create_info, value));
}

bool PT_locking_clause::do_contextualize(Parse_context *pc) {
  LEX *lex = pc->thd->lex;

  if (lex->is_explain()) return false;

  if (m_locked_row_action == Locked_row_action::SKIP)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SKIP_LOCKED);

  if (m_locked_row_action == Locked_row_action::NOWAIT)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_NOWAIT);

  lex->safe_to_cache_query = false;

  return set_lock_for_tables(pc);
}

using Local_tables_iterator =
    IntrusiveListIterator<Table_ref, &Table_ref::next_local>;

/// A list interface over the Table_ref::next_local pointer.
using Local_tables_list = IteratorContainer<Local_tables_iterator>;

bool PT_query_block_locking_clause::set_lock_for_tables(Parse_context *pc) {
  Local_tables_list local_tables(pc->select->get_table_list());
  for (Table_ref *table_list : local_tables)
    if (!table_list->is_derived()) {
      if (table_list->lock_descriptor().type != TL_READ_DEFAULT) {
        my_error(ER_DUPLICATE_TABLE_LOCK, MYF(0), table_list->alias);
        return true;
      }

      pc->select->set_lock_for_table(get_lock_descriptor(), table_list);
    }
  return false;
}

bool PT_column_def::do_contextualize(Table_ddl_parse_context *pc) {
  // Since Alter_info objects are allocated on a mem_root and never
  // destroyed we (move)-assign an empty vector to cf_appliers to
  // ensure any dynamic memory is released. This must be done whenever
  // leaving this scope since appliers may be added in
  // field_def->contextualize(pc).
  auto clr_appliers = create_scope_guard([&]() {
    pc->alter_info->cf_appliers = decltype(pc->alter_info->cf_appliers)();
  });

  if (super::do_contextualize(pc) || field_def->contextualize(pc)) return true;

  if (opt_column_constraint) {
    // FK specification at column level is supported from 9.0 version onwards.
    // If source node version is lesser than 9.0, then ignore FK specification.
    if (!is_rpl_source_older(pc->thd, 90000)) {
      PT_foreign_key_definition *fk_def =
          pointer_cast<PT_foreign_key_definition *>(opt_column_constraint);
      fk_def->set_column_name(field_ident);
      if (contextualize_safe(pc, opt_column_constraint)) return true;
    }
  }

  pc->alter_info->flags |= field_def->alter_info_flags;
  dd::Column::enum_hidden_type field_hidden_type =
      (field_def->type_flags & FIELD_IS_INVISIBLE)
          ? dd::Column::enum_hidden_type::HT_HIDDEN_USER
          : dd::Column::enum_hidden_type::HT_VISIBLE;

  return pc->alter_info->add_field(
      pc->thd, &field_ident, field_def->type, field_def->length, field_def->dec,
      field_def->type_flags, field_def->default_value,
      field_def->on_update_value, &field_def->comment, nullptr,
      field_def->interval_list, field_def->charset,
      field_def->has_explicit_collation, field_def->uint_geom_type,
      field_def->gcol_info, field_def->default_val_info, opt_place,
      field_def->m_srid, field_def->check_const_spec_list, field_hidden_type);
}

Sql_cmd *PT_create_table_stmt::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  lex->sql_command = SQLCOM_CREATE_TABLE;

  Parse_context pc(thd, lex->current_query_block());

  Table_ref *table = pc.select->add_table_to_list(
      thd, table_name, nullptr, TL_OPTION_UPDATING, TL_WRITE, MDL_SHARED);
  if (table == nullptr) return nullptr;

  table->open_strategy = Table_ref::OPEN_FOR_CREATE;

  lex->create_info = &m_create_info;
  Table_ddl_parse_context pc2(thd, pc.select, &m_alter_info);

  pc2.create_info->options = 0;
  if (is_temporary) pc2.create_info->options |= HA_LEX_CREATE_TMP_TABLE;
  if (only_if_not_exists)
    pc2.create_info->options |= HA_LEX_CREATE_IF_NOT_EXISTS;

  pc2.create_info->default_table_charset = nullptr;

  lex->name.str = nullptr;
  lex->name.length = 0;

  Table_ref *qe_tables = nullptr;

  if (opt_like_clause != nullptr) {
    pc2.create_info->options |= HA_LEX_CREATE_TABLE_LIKE;
    Table_ref **like_clause_table = &lex->query_tables->next_global;
    Table_ref *src_table = pc.select->add_table_to_list(
        thd, opt_like_clause, nullptr, 0, TL_READ, MDL_SHARED_READ);
    if (!src_table) return nullptr;
    /* CREATE TABLE ... LIKE is not allowed for views. */
    src_table->required_type = dd::enum_table_type::BASE_TABLE;
    qe_tables = *like_clause_table;
  } else {
    if (opt_table_element_list) {
      for (auto element : *opt_table_element_list) {
        if (element->contextualize(&pc2)) return nullptr;
      }
    }

    if (opt_create_table_options) {
      for (auto option : *opt_create_table_options)
        if (option->contextualize(&pc2)) return nullptr;
    }

    if (opt_partitioning) {
      Table_ref **exclude_part_tables = lex->query_tables_last;
      if (opt_partitioning->contextualize(&pc)) return nullptr;
      /*
        Remove all tables used in PARTITION clause from the global table
        list. Partitioning with subqueries is not allowed anyway.
      */
      *exclude_part_tables = nullptr;
      lex->query_tables_last = exclude_part_tables;

      lex->part_info = &opt_partitioning->part_info;
    }

    switch (on_duplicate) {
      case On_duplicate::IGNORE_DUP:
        lex->set_ignore(true);
        break;
      case On_duplicate::REPLACE_DUP:
        lex->duplicates = DUP_REPLACE;
        break;
      case On_duplicate::ERROR:
        lex->duplicates = DUP_ERROR;
        break;
    }

    if (opt_query_expression) {
      Table_ref **query_expression_tables = &lex->query_tables->next_global;
      /*
        In CREATE TABLE t ... SELECT the table_list initially contains
        here a table entry for the destination table `t'.
        Backup it and clean the table list for the processing of
        the query expression and push `t' back to the beginning of the
        table_list finally.

        @todo: Don't save the CREATE destination table in
               Query_block::table_list and remove this backup & restore.

        The following work only with the local list, the global list
        is created correctly in this case
      */
      SQL_I_List<Table_ref> save_list;
      pc.select->m_table_list.save_and_clear(&save_list);

      if (opt_query_expression->contextualize(&pc)) return nullptr;
      if (pc.finalize_query_expression()) return nullptr;

      // Ensure that first query block is the current one
      assert(pc.select->select_number == 1);
      /*
        The following work only with the local list, the global list
        is created correctly in this case
      */
      pc.select->m_table_list.push_front(&save_list);
      qe_tables = *query_expression_tables;
    }
  }

  lex->set_current_query_block(pc.select);
  if ((pc2.create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      !pc2.create_info->db_type) {
    pc2.create_info->db_type =
        pc2.create_info->options & HA_LEX_CREATE_TMP_TABLE
            ? ha_default_temp_handlerton(thd)
            : ha_default_handlerton(thd);
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_WARN_USING_OTHER_HANDLER,
        ER_THD(thd, ER_WARN_USING_OTHER_HANDLER),
        ha_resolve_storage_engine_name(pc2.create_info->db_type),
        table_name->table.str);
  }
  create_table_set_open_action_and_adjust_tables(lex);

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_create_table(&m_alter_info, qe_tables);
}

bool PT_table_locking_clause::set_lock_for_tables(Parse_context *pc) {
  assert(!m_tables.empty());
  for (Table_ident *table_ident : m_tables) {
    Query_block *select = pc->select;

    Table_ref *table_list = select->find_table_by_name(table_ident);

    THD *thd = pc->thd;

    if (table_list == nullptr)
      return raise_error(thd, table_ident, ER_UNRESOLVED_TABLE_LOCK);

    if (table_list->lock_descriptor().type != TL_READ_DEFAULT)
      return raise_error(thd, table_ident, ER_DUPLICATE_TABLE_LOCK);

    select->set_lock_for_table(get_lock_descriptor(), table_list);
  }

  return false;
}

bool PT_show_table_base::make_table_base_cmd(THD *thd, bool *temporary) {
  LEX *const lex = thd->lex;
  Parse_context pc(thd, lex->current_query_block());

  lex->sql_command = m_sql_command;

  // Create empty query block and add user specified table.
  Table_ref **query_tables_last = lex->query_tables_last;
  Query_block *schema_query_block = lex->new_empty_query_block();
  if (schema_query_block == nullptr) return true;
  Table_ref *tbl = schema_query_block->add_table_to_list(
      thd, m_table_ident, nullptr, 0, TL_READ, MDL_SHARED_READ);
  if (tbl == nullptr) return true;
  lex->query_tables_last = query_tables_last;

  if (m_wild.str && lex->set_wild(m_wild)) return true;  // OOM

  TABLE *show_table = find_temporary_table(thd, tbl);
  *temporary = show_table != nullptr;

  // If its a temporary table then use schema_table implementation,
  // otherwise read I_S system view:
  if (*temporary) {
    Query_block *query_block = lex->current_query_block();

    if (m_where != nullptr) {
      if (m_where->itemize(&pc, &m_where)) return true;
      query_block->set_where_cond(m_where);
    }

    enum enum_schema_tables schema_table = m_sql_command == SQLCOM_SHOW_FIELDS
                                               ? SCH_TMP_TABLE_COLUMNS
                                               : SCH_TMP_TABLE_KEYS;
    if (make_schema_query_block(thd, query_block, schema_table)) return true;

    Table_ref *table_list = query_block->get_table_list();
    table_list->schema_query_block = schema_query_block;
    table_list->schema_table_reformed = true;
  } else {
    Query_block *sel = nullptr;
    switch (m_sql_command) {
      case SQLCOM_SHOW_FIELDS:
        sel = dd::info_schema::build_show_columns_query(
            m_pos, thd, m_table_ident, lex->wild, m_where);
        break;
      case SQLCOM_SHOW_KEYS:
        sel = dd::info_schema::build_show_keys_query(m_pos, thd, m_table_ident,
                                                     m_where);
        break;
      default:
        assert(false);
        sel = nullptr;
        break;
    }

    if (sel == nullptr) return true;

    Table_ref *table_list = sel->get_table_list();
    table_list->schema_query_block = schema_query_block;
  }

  return false;
}

static void setup_lex_show_cmd_type(THD *thd, Show_cmd_type show_cmd_type) {
  thd->lex->verbose = false;
  thd->lex->m_extended_show = false;

  switch (show_cmd_type) {
    case Show_cmd_type::STANDARD:
      break;
    case Show_cmd_type::FULL_SHOW:
      thd->lex->verbose = true;
      break;
    case Show_cmd_type::EXTENDED_SHOW:
      thd->lex->m_extended_show = true;
      break;
    case Show_cmd_type::EXTENDED_FULL_SHOW:
      thd->lex->verbose = true;
      thd->lex->m_extended_show = true;
      break;
  }
}

Sql_cmd *PT_show_binlog_events::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->mi.log_file_name = m_opt_log_file_name.str;

  Parse_context pc(thd, thd->lex->current_query_block());
  if (contextualize_safe(&pc, m_opt_limit_clause)) return nullptr;  // OOM

  return &m_sql_cmd;
}

Sql_cmd *PT_show_binlogs::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_charsets::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_character_set_query(m_pos, thd, lex->wild,
                                                      m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_collations::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_collation_query(m_pos, thd, lex->wild,
                                                  m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_count_base::make_cmd_generic(
    THD *thd, LEX_CSTRING diagnostic_variable_name) {
  LEX *const lex = thd->lex;
  lex->sql_command = SQLCOM_SELECT;

  // SHOW COUNT(*) { ERRORS | WARNINGS } doesn't clear them.
  lex->keep_diagnostics = DA_KEEP_DIAGNOSTICS;

  Parse_context pc(thd, lex->current_query_block());
  Item *var =
      get_system_variable(&pc, OPT_SESSION, diagnostic_variable_name, false);
  if (var == nullptr) {
    assert(false);
    return nullptr;  // should never happen
  }

  constexpr const char session_prefix[] = "@@session.";
  assert(diagnostic_variable_name.length <= MAX_SYS_VAR_LENGTH);
  char buff[sizeof(session_prefix) + MAX_SYS_VAR_LENGTH + 1];
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  char *end =
      strxmov(buff, session_prefix, diagnostic_variable_name.str, nullptr);
  var->item_name.copy(buff, end - buff);

  add_item_to_list(thd, var);

  return new (thd->mem_root) Sql_cmd_select(nullptr);
}

Sql_cmd *PT_show_create_database::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  assert(lex->create_info == nullptr);
  lex->create_info = thd->alloc_typed<HA_CREATE_INFO>();
  if (lex->create_info == nullptr) return nullptr;  // OOM
  lex->create_info->options = m_if_not_exists ? HA_LEX_CREATE_IF_NOT_EXISTS : 0;
  lex->name = m_name;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_event::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->spname = m_spname;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_function::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->spname = m_spname;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_procedure::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->spname = m_spname;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_table::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  assert(lex->create_info == nullptr);
  lex->create_info = thd->alloc_typed<HA_CREATE_INFO>();
  if (lex->create_info == nullptr) return nullptr;  // OOM
  lex->create_info->storage_media = HA_SM_DEFAULT;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_view::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_trigger::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->spname = m_spname;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_create_user::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->grant_user = m_user;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_databases::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_databases_query(m_pos, thd, lex->wild,
                                                  m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_engine_logs::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  assert(lex->create_info == nullptr);
  lex->create_info = thd->alloc_typed<HA_CREATE_INFO>();
  if (lex->create_info == nullptr) return nullptr;  // OOM
  if (!m_all && resolve_engine(thd, to_lex_cstring(m_engine), false, true,
                               &lex->create_info->db_type))
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_engine_mutex::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  assert(lex->create_info == nullptr);
  lex->create_info = thd->alloc_typed<HA_CREATE_INFO>();
  if (lex->create_info == nullptr) return nullptr;  // OOM
  if (!m_all && resolve_engine(thd, to_lex_cstring(m_engine), false, true,
                               &lex->create_info->db_type))
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_engine_status::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  assert(lex->create_info == nullptr);
  lex->create_info = thd->alloc_typed<HA_CREATE_INFO>();
  if (lex->create_info == nullptr) return nullptr;  // OOM
  if (!m_all && resolve_engine(thd, to_lex_cstring(m_engine), false, true,
                               &lex->create_info->db_type))
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_engines::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (prepare_schema_table(thd, lex, nullptr, SCH_ENGINES)) return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_errors::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;
  // SHOW ERRORS will not clear diagnostics
  lex->keep_diagnostics = DA_KEEP_DIAGNOSTICS;

  Parse_context pc(thd, thd->lex->current_query_block());
  if (contextualize_safe(&pc, m_opt_limit_clause)) return nullptr;  // OOM

  return &m_sql_cmd;
}

Sql_cmd *PT_show_fields::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  assert(lex->query_block->db == nullptr);

  setup_lex_show_cmd_type(thd, m_show_cmd_type);
  lex->current_query_block()->parsing_place = CTX_SELECT_LIST;
  if (make_table_base_cmd(thd, &m_sql_cmd.m_temporary)) return nullptr;
  // WL#6599 opt_describe_column is handled during prepare stage in
  // prepare_schema_dd_view instead of execution stage
  lex->current_query_block()->parsing_place = CTX_NONE;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_keys::make_cmd(THD *thd) {
  thd->lex->m_extended_show = m_extended_show;

  if (make_table_base_cmd(thd, &m_sql_cmd.m_temporary)) return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_events::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;
  lex->query_block->db = m_opt_db;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_events_query(m_pos, thd, lex->wild,
                                               m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_binary_log_status::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_open_tables::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  Parse_context pc(thd, lex->query_block);

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM
  if (m_where != nullptr) {
    if (m_where->itemize(&pc, &m_where)) return nullptr;
    lex->query_block->set_where_cond(m_where);
  }
  lex->query_block->db = m_opt_db;

  if (prepare_schema_table(thd, lex, nullptr, SCH_OPEN_TABLES)) return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_plugins::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (prepare_schema_table(thd, lex, nullptr, SCH_PLUGINS)) return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_privileges::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_processlist::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  // Read once, to avoid race conditions.
  bool use_pfs = pfs_processlist_enabled;

  m_sql_cmd.set_use_pfs(use_pfs);
  if (use_pfs) {
    if (build_processlist_query(m_pos, thd, m_sql_cmd.verbose()))
      return nullptr;
  }

  return &m_sql_cmd;
}

Sql_cmd *PT_show_routine_code::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_profile::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->profile_options = m_opt_profile_options;
  lex->show_profile_query_id = m_opt_query_id;

  Parse_context pc(thd, thd->lex->current_query_block());
  if (contextualize_safe(&pc, m_opt_limit_clause)) return nullptr;  // OOM

  if (prepare_schema_table(thd, lex, nullptr, SCH_PROFILES)) return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_profiles::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_relaylog_events::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->mi.log_file_name = m_opt_log_file_name.str;
  if (lex->set_channel_name(m_opt_channel_name)) return nullptr;  // OOM

  Parse_context pc(thd, thd->lex->current_query_block());
  if (contextualize_safe(&pc, m_opt_limit_clause)) return nullptr;  // OOM

  return &m_sql_cmd;
}

Sql_cmd *PT_show_replicas::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_replica_status::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (lex->set_channel_name(m_opt_channel_name)) return nullptr;  // OOM

  return &m_sql_cmd;
}

Sql_cmd *PT_show_status::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (m_var_type == OPT_SESSION) {
    if (build_show_session_status(m_pos, thd, lex->wild, m_where) == nullptr)
      return nullptr;
  } else if (m_var_type == OPT_GLOBAL) {
    if (build_show_global_status(m_pos, thd, lex->wild, m_where) == nullptr)
      return nullptr;
  }
  return &m_sql_cmd;
}

Sql_cmd *PT_show_status_func::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_procedures_query(m_pos, thd, lex->wild,
                                                   m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_status_proc::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_procedures_query(m_pos, thd, lex->wild,
                                                   m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_table_status::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  lex->query_block->db = m_opt_db;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_tables_query(m_pos, thd, lex->wild, m_where,
                                               true) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_tables::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;
  setup_lex_show_cmd_type(thd, m_show_cmd_type);

  lex->query_block->db = m_opt_db;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_tables_query(m_pos, thd, lex->wild, m_where,
                                               false) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_triggers::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;
  lex->verbose = m_full;
  lex->query_block->db = m_opt_db;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (dd::info_schema::build_show_triggers_query(m_pos, thd, lex->wild,
                                                 m_where) == nullptr)
    return nullptr;

  return &m_sql_cmd;
}

Sql_cmd *PT_show_variables::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;

  if (m_wild.str && lex->set_wild(m_wild)) return nullptr;  // OOM

  if (m_var_type == OPT_SESSION) {
    if (build_show_session_variables(m_pos, thd, lex->wild, m_where) == nullptr)
      return nullptr;
  } else if (m_var_type == OPT_GLOBAL) {
    if (build_show_global_variables(m_pos, thd, lex->wild, m_where) == nullptr)
      return nullptr;
  }
  return &m_sql_cmd;
}

Sql_cmd *PT_show_warnings::make_cmd(THD *thd) {
  LEX *lex = thd->lex;
  lex->sql_command = m_sql_command;
  // SHOW WARNINGS will not clear diagnostics
  lex->keep_diagnostics = DA_KEEP_DIAGNOSTICS;

  Parse_context pc(thd, thd->lex->current_query_block());
  if (contextualize_safe(&pc, m_opt_limit_clause)) return nullptr;  // OOM

  return &m_sql_cmd;
}

bool PT_alter_table_change_column::do_contextualize(
    Table_ddl_parse_context *pc) {
  // Since Alter_info objects are allocated on a mem_root and never
  // destroyed we (move)-assign an empty vector to cf_appliers to
  // ensure any dynamic memory is released. This must be done whenever
  // leaving this scope since appliers may be added in
  // m_field_def->contextualize(pc).
  auto clr_appliers = create_scope_guard([&]() {
    pc->alter_info->cf_appliers = decltype(pc->alter_info->cf_appliers)();
  });

  if (super::do_contextualize(pc) || m_field_def->contextualize(pc))
    return true;
  pc->alter_info->flags |= m_field_def->alter_info_flags;
  dd::Column::enum_hidden_type field_hidden_type =
      (m_field_def->type_flags & FIELD_IS_INVISIBLE)
          ? dd::Column::enum_hidden_type::HT_HIDDEN_USER
          : dd::Column::enum_hidden_type::HT_VISIBLE;

  return pc->alter_info->add_field(
      pc->thd, &m_new_name, m_field_def->type, m_field_def->length,
      m_field_def->dec, m_field_def->type_flags, m_field_def->default_value,
      m_field_def->on_update_value, &m_field_def->comment, m_old_name.str,
      m_field_def->interval_list, m_field_def->charset,
      m_field_def->has_explicit_collation, m_field_def->uint_geom_type,
      m_field_def->gcol_info, m_field_def->default_val_info, m_opt_place,
      m_field_def->m_srid, m_field_def->check_const_spec_list,
      field_hidden_type);
}

bool PT_alter_table_rename::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;  // OOM

  if (m_ident->db.str) {
    LEX_STRING db_str = to_lex_string(m_ident->db);
    if (check_and_convert_db_name(&db_str, false) != Ident_name_check::OK)
      return true;
    pc->alter_info->new_db_name = to_lex_cstring(db_str);
  } else if (pc->thd->lex->copy_db_to(&pc->alter_info->new_db_name.str,
                                      &pc->alter_info->new_db_name.length)) {
    return true;
  }
  switch (check_table_name(m_ident->table.str, m_ident->table.length)) {
    case Ident_name_check::WRONG:
      my_error(ER_WRONG_TABLE_NAME, MYF(0), m_ident->table.str);
      return true;
    case Ident_name_check::TOO_LONG:
      my_error(ER_TOO_LONG_IDENT, MYF(0), m_ident->table.str);
      return true;
    case Ident_name_check::OK:
      break;
  }
  pc->alter_info->new_table_name = m_ident->table;
  return false;
}

bool PT_alter_table_convert_to_charset::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;  // OOM

  const CHARSET_INFO *const cs =
      m_charset ? m_charset : pc->thd->variables.collation_database;
  const CHARSET_INFO *const collation = m_collation ? m_collation : cs;

  if (!my_charset_same(cs, collation)) {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0), collation->m_coll_name,
             cs->csname);
    return true;
  }

  if ((pc->create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
      pc->create_info->default_table_charset && collation &&
      !my_charset_same(pc->create_info->default_table_charset, collation)) {
    my_error(ER_CONFLICTING_DECLARATIONS, MYF(0), "CHARACTER SET ",
             pc->create_info->default_table_charset->csname, "CHARACTER SET ",
             collation->csname);
    return true;
  }

  pc->create_info->table_charset = pc->create_info->default_table_charset =
      collation;
  pc->create_info->used_fields |=
      HA_CREATE_USED_CHARSET | HA_CREATE_USED_DEFAULT_CHARSET;
  if (m_collation != nullptr)
    pc->create_info->used_fields |= HA_CREATE_USED_DEFAULT_COLLATE;
  return false;
}

bool PT_alter_table_add_partition_def_list::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  Partition_parse_context part_pc(pc->thd, &m_part_info,
                                  is_add_or_reorganize_partition());
  for (auto *part_def : *m_def_list) {
    if (part_def->contextualize(&part_pc)) return true;
  }
  m_part_info.num_parts = m_part_info.partitions.elements;

  return false;
}

bool PT_alter_table_reorganize_partition_into::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *const lex = pc->thd->lex;
  lex->no_write_to_binlog = m_no_write_to_binlog;

  assert(pc->alter_info->partition_names.is_empty());
  pc->alter_info->partition_names = m_partition_names;

  Partition_parse_context ppc(pc->thd, &m_partition_info,
                              is_add_or_reorganize_partition());

  for (auto *part_def : *m_into)
    if (part_def->contextualize(&ppc)) return true;

  m_partition_info.num_parts = m_partition_info.partitions.elements;
  lex->part_info = &m_partition_info;
  return false;
}

bool PT_alter_table_exchange_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  pc->alter_info->with_validation = m_validation;

  String *s = new (pc->mem_root) String(
      m_partition_name.str, m_partition_name.length, system_charset_info);
  if (s == nullptr || pc->alter_info->partition_names.push_back(s) ||
      !pc->select->add_table_to_list(pc->thd, m_table_name, nullptr,
                                     TL_OPTION_UPDATING, TL_READ_NO_INSERT,
                                     MDL_SHARED_NO_WRITE)) {
    return true;
  }

  return false;
}

/**
  A common initialization part of ALTER TABLE statement variants.

  @param pc             The parse context.
  @param table_name     The name of a table to alter.
  @param algo           The ALGORITHM clause: inplace, copy etc.
  @param lock           The LOCK clause: none, shared, exclusive etc.
  @param validation     The WITH or WITHOUT VALIDATION clause.

  @returns false on success, true on error.

*/
static bool init_alter_table_stmt(Table_ddl_parse_context *pc,
                                  Table_ident *table_name,
                                  Alter_info::enum_alter_table_algorithm algo,
                                  Alter_info::enum_alter_table_lock lock,
                                  Alter_info::enum_with_validation validation) {
  LEX *lex = pc->thd->lex;
  if (!lex->query_block->add_table_to_list(
          pc->thd, table_name, nullptr, TL_OPTION_UPDATING, TL_READ_NO_INSERT,
          MDL_SHARED_UPGRADABLE))
    return true;
  lex->query_block->init_order();
  pc->create_info->db_type = nullptr;
  pc->create_info->default_table_charset = nullptr;
  pc->create_info->row_type = ROW_TYPE_NOT_USED;

  pc->alter_info->new_db_name =
      LEX_CSTRING{lex->query_block->get_table_list()->db,
                  lex->query_block->get_table_list()->db_length};
  lex->no_write_to_binlog = false;
  pc->create_info->storage_media = HA_SM_DEFAULT;

  pc->alter_info->requested_algorithm = algo;
  pc->alter_info->requested_lock = lock;
  pc->alter_info->with_validation = validation;
  return false;
}

Sql_cmd *PT_alter_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ALTER_TABLE;

  thd->lex->create_info = &m_create_info;
  Table_ddl_parse_context pc(thd, thd->lex->current_query_block(),
                             &m_alter_info);

  if (init_alter_table_stmt(&pc, m_table_name, m_algo, m_lock, m_validation))
    return nullptr;

  if (m_opt_actions) {
    /*
      Move RENAME TO <table_name> clauses to the head of array, so they are
      processed before ADD FOREIGN KEY clauses. The latter need to know target
      database name for proper contextualization.

      Use stable sort to preserve order of other clauses which might be
      sensitive to it.
    */
    std::stable_sort(
        m_opt_actions->begin(), m_opt_actions->end(),
        [](const PT_ddl_table_option *lhs, const PT_ddl_table_option *rhs) {
          return lhs->is_rename_table() && !rhs->is_rename_table();
        });

    for (auto *action : *m_opt_actions)
      if (action->contextualize(&pc)) return nullptr;
  }

  if ((pc.create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      !pc.create_info->db_type) {
    pc.create_info->used_fields &= ~HA_CREATE_USED_ENGINE;
  }

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_alter_table(&m_alter_info);
}

Sql_cmd *PT_alter_table_standalone_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ALTER_TABLE;

  thd->lex->create_info = &m_create_info;

  Table_ddl_parse_context pc(thd, thd->lex->current_query_block(),
                             &m_alter_info);
  if (init_alter_table_stmt(&pc, m_table_name, m_algo, m_lock, m_validation) ||
      m_action->contextualize(&pc))
    return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return m_action->make_cmd(&pc);
}

Sql_cmd *PT_repair_table_stmt::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;

  lex->sql_command = SQLCOM_REPAIR;

  Query_block *const select = lex->current_query_block();

  lex->no_write_to_binlog = m_no_write_to_binlog;
  lex->check_opt.flags |= m_flags;
  lex->check_opt.sql_flags |= m_sql_flags;
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return nullptr;

  lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_repair_table(&m_alter_info);
}

Sql_cmd *PT_analyze_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ANALYZE;

  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  lex->no_write_to_binlog = m_no_write_to_binlog;
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return nullptr;

  thd->lex->alter_info = &m_alter_info;
  auto cmd = new (thd->mem_root) Sql_cmd_analyze_table(
      thd, &m_alter_info, m_command, m_num_buckets, m_data, m_auto_update);
  if (cmd == nullptr) return nullptr;
  if (m_command == Sql_cmd_analyze_table::Histogram_command::UPDATE_HISTOGRAM ||
      m_command == Sql_cmd_analyze_table::Histogram_command::DROP_HISTOGRAM) {
    if (cmd->set_histogram_fields(m_columns)) return nullptr;
  }
  return cmd;
}

Sql_cmd *PT_check_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_CHECK;

  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  if (lex->sphead) {
    my_error(ER_SP_BADSTATEMENT, MYF(0), "CHECK");
    return nullptr;
  }

  lex->check_opt.flags |= m_flags;
  lex->check_opt.sql_flags |= m_sql_flags;
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_check_table(&m_alter_info);
}

Sql_cmd *PT_optimize_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_OPTIMIZE;

  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  lex->no_write_to_binlog = m_no_write_to_binlog;
  if (select->add_tables(thd, m_table_list, TL_OPTION_UPDATING, TL_UNLOCK,
                         MDL_SHARED_READ))
    return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_optimize_table(&m_alter_info);
}

Sql_cmd *PT_drop_index_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_DROP_INDEX;

  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  m_alter_info.flags = Alter_info::ALTER_DROP_INDEX;
  m_alter_info.drop_list.push_back(&m_alter_drop);
  if (!select->add_table_to_list(thd, m_table, nullptr, TL_OPTION_UPDATING,
                                 TL_READ_NO_INSERT, MDL_SHARED_UPGRADABLE))
    return nullptr;

  m_alter_info.requested_algorithm = m_algo;
  m_alter_info.requested_lock = m_lock;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_drop_index(&m_alter_info);
}

Sql_cmd *PT_truncate_table_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_TRUNCATE;

  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  if (!select->add_table_to_list(thd, m_table, nullptr, TL_OPTION_UPDATING,
                                 TL_WRITE, MDL_EXCLUSIVE))
    return nullptr;
  return &m_cmd_truncate_table;
}

bool PT_assign_to_keycache::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  if (!pc->select->add_table_to_list(pc->thd, m_table, nullptr, 0, TL_READ,
                                     MDL_SHARED_READ, m_index_hints))
    return true;
  return false;
}

bool PT_adm_partition::do_contextualize(Table_ddl_parse_context *pc) {
  pc->alter_info->flags |= Alter_info::ALTER_ADMIN_PARTITION;

  assert(pc->alter_info->partition_names.is_empty());
  if (m_opt_partitions == nullptr)
    pc->alter_info->flags |= Alter_info::ALTER_ALL_PARTITION;
  else
    pc->alter_info->partition_names = *m_opt_partitions;
  return false;
}

Sql_cmd *PT_cache_index_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ASSIGN_TO_KEYCACHE;

  Table_ddl_parse_context pc(thd, thd->lex->current_query_block(),
                             &m_alter_info);

  for (auto *tbl_index_list : *m_tbl_index_lists)
    if (tbl_index_list->contextualize(&pc)) return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root)
      Sql_cmd_cache_index(&m_alter_info, m_key_cache_name);
}

Sql_cmd *PT_cache_index_partitions_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ASSIGN_TO_KEYCACHE;

  Query_block *const select = thd->lex->current_query_block();

  Table_ddl_parse_context pc(thd, select, &m_alter_info);

  if (m_partitions->contextualize(&pc)) return nullptr;

  if (!select->add_table_to_list(thd, m_table, nullptr, 0, TL_READ,
                                 MDL_SHARED_READ, m_opt_key_usage_list))
    return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root)
      Sql_cmd_cache_index(&m_alter_info, m_key_cache_name);
}

Sql_cmd *PT_load_index_partitions_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_PRELOAD_KEYS;

  Query_block *const select = thd->lex->current_query_block();

  Table_ddl_parse_context pc(thd, select, &m_alter_info);

  if (m_partitions->contextualize(&pc)) return nullptr;

  if (!select->add_table_to_list(
          thd, m_table, nullptr, m_ignore_leaves ? TL_OPTION_IGNORE_LEAVES : 0,
          TL_READ, MDL_SHARED_READ, m_opt_cache_key_list))
    return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_load_index(&m_alter_info);
}

Sql_cmd *PT_load_index_stmt::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_PRELOAD_KEYS;

  Table_ddl_parse_context pc(thd, thd->lex->current_query_block(),
                             &m_alter_info);

  for (auto *preload_keys : *m_preload_list)
    if (preload_keys->contextualize(&pc)) return nullptr;

  thd->lex->alter_info = &m_alter_info;
  return new (thd->mem_root) Sql_cmd_load_index(&m_alter_info);
}

/* Window functions */

Item *PT_border::build_addop(Item_cache *order_expr, bool prec, bool asc,
                             const Window *window) {
  /*
    Check according to SQL 2014 7.15 <window clause> SR 13.a.iii:
    ORDER BY expression is temporal iff bound is temporal.
  */
  if (order_expr->result_type() == STRING_RESULT && order_expr->is_temporal()) {
    if (!m_date_time) {
      my_error(ER_WINDOW_RANGE_FRAME_TEMPORAL_TYPE, MYF(0),
               window->printable_name());
      return nullptr;
    }
  } else {
    if (m_date_time) {
      my_error(ER_WINDOW_RANGE_FRAME_NUMERIC_TYPE, MYF(0),
               window->printable_name());
      return nullptr;
    }
  }

  Item *addop;
  const bool substract = prec ? asc : !asc;
  if (m_date_time) {
    addop =
        new Item_date_add_interval(order_expr, m_value, m_int_type, substract);
  } else {
    if (substract)
      addop = new Item_func_minus(order_expr, m_value);
    else
      addop = new Item_func_plus(order_expr, m_value);
  }
  return addop;
}

PT_json_table_column_for_ordinality::PT_json_table_column_for_ordinality(
    const POS &pos, LEX_STRING name)
    : super(pos), m_name(name.str) {}

PT_json_table_column_for_ordinality::~PT_json_table_column_for_ordinality() =
    default;

bool PT_json_table_column_for_ordinality::do_contextualize(Parse_context *pc) {
  assert(m_column == nullptr);
  m_column = make_unique_destroy_only<Json_table_column>(
      pc->mem_root, enum_jt_column::JTC_ORDINALITY);
  if (m_column == nullptr) return true;
  m_column->init_for_tmp_table(MYSQL_TYPE_LONGLONG, 10, 0, true, true, 8,
                               m_name);
  return super::do_contextualize(pc);
}

PT_json_table_column_with_path::PT_json_table_column_with_path(
    const POS &pos, unique_ptr_destroy_only<Json_table_column> column,
    LEX_STRING name, PT_type *type, const CHARSET_INFO *collation)
    : super(pos),
      m_column(std::move(column)),
      m_name(name.str),
      m_type(type),
      m_collation(collation) {}

PT_json_table_column_with_path::~PT_json_table_column_with_path() = default;

static bool check_unsupported_json_table_default(const Item *item) {
  if (item == nullptr) return false;

  // JSON_TABLE currently only supports string literals on JSON format in
  // DEFAULT clauses. Other literals used to be rejected by the grammar, but the
  // grammar was extended for JSON_VALUE and now accepts all types of literals.
  // Until JSON_TABLE gets support for non-string defaults, reject them here.
  if (item->data_type() != MYSQL_TYPE_VARCHAR) {
    my_error(
        ER_NOT_SUPPORTED_YET, MYF(0),
        "non-string DEFAULT value for a column in a JSON_TABLE expression");
    return true;
  }

  return false;
}

bool PT_json_table_column_with_path::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || m_type->contextualize(pc)) return true;

  if (m_column->m_path_string->itemize(pc, &m_column->m_path_string))
    return true;

  if (check_unsupported_json_table_default(m_column->m_default_empty_string) ||
      check_unsupported_json_table_default(m_column->m_default_error_string))
    return true;

  if (itemize_safe(pc, &m_column->m_default_empty_string)) return true;
  if (itemize_safe(pc, &m_column->m_default_error_string)) return true;

  const CHARSET_INFO *cs = nullptr;
  if (merge_charset_and_collation(m_type->get_charset(), m_collation, &cs))
    return true;
  if (cs == nullptr) {
    cs = pc->thd->variables.collation_connection;
  }

  m_column->init(pc->thd,
                 m_name,                        // Alias
                 m_type->type,                  // Type
                 m_type->get_length(),          // Length
                 m_type->get_dec(),             // Decimals
                 m_type->get_type_flags(),      // Type modifier
                 nullptr,                       // Default value
                 nullptr,                       // On update value
                 &EMPTY_CSTR,                   // Comment
                 nullptr,                       // Change
                 m_type->get_interval_list(),   // Interval list
                 cs,                            // Charset & collation
                 m_collation != nullptr,        // Has "COLLATE" clause
                 m_type->get_uint_geom_type(),  // Geom type
                 nullptr,                       // Gcol_info
                 nullptr,                       // Default gen expression
                 {},                            // SRID
                 dd::Column::enum_hidden_type::HT_VISIBLE);  // Hidden
  return false;
}

bool PT_json_table_column_with_nested_path::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;  // OOM

  if (m_path->itemize(pc, &m_path)) return true;

  auto nested_columns = new (pc->mem_root) List<Json_table_column>;
  if (nested_columns == nullptr) return true;  // OOM

  for (auto col : *m_nested_columns) {
    if (col->contextualize(pc) || nested_columns->push_back(col->get_column()))
      return true;
  }

  m_column = new (pc->mem_root) Json_table_column(m_path, nested_columns);
  if (m_column == nullptr) return true;  // OOM

  return false;
}

Sql_cmd *PT_explain_for_connection::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_EXPLAIN_OTHER;

  if (thd->lex->sphead) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "non-standalone EXPLAIN FOR CONNECTION");
    return nullptr;
  }
  if (thd->lex->is_explain_analyze) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "EXPLAIN ANALYZE FOR CONNECTION");
    return nullptr;
  }
  if (thd->lex->explain_format->is_explain_into()) {
    my_error(ER_EXPLAIN_INTO_FOR_CONNECTION_NOT_SUPPORTED, MYF(0));
    return nullptr;
  }
  return &m_cmd;
}

Sql_cmd *PT_explain::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  switch (m_format) {
    case Explain_format_type::TRADITIONAL:
    case Explain_format_type::TRADITIONAL_STRICT:
      /*
        With no format specified:
        - With ANALYZE, convert TRADITIONAL[_STRICT] to TREE unconditionally.
        - With hypergraph, convert TRADITIONAL to TREE. Don't convert
          TRADITIONAL_STRICT, because it's purpose is to prevent exactly this
          silent conversion with hypergraph. TRADITIONAL_STRICT will throw an
          error, later.
      */
      if (!m_explicit_format &&
          (m_analyze ||
           (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER) &&
            m_format == Explain_format_type::TRADITIONAL))) {
        lex->explain_format = new (thd->mem_root) Explain_format_tree;
      } else {
        lex->explain_format = new (thd->mem_root) Explain_format_traditional;
      }
      break;
    case Explain_format_type::JSON: {
      lex->explain_format =
          new (thd->mem_root) Explain_format_JSON(m_explain_into_variable_name);
      break;
    }
    case Explain_format_type::TREE:
      lex->explain_format = new (thd->mem_root) Explain_format_tree;
      break;
    default:
      assert(false);
      lex->explain_format = new (thd->mem_root) Explain_format_traditional;
  }
  if (lex->explain_format == nullptr) return nullptr;  // OOM
  lex->is_explain_analyze = m_analyze;

  /*
    If this EXPLAIN statement is supposed to be run in another schema change to
    the appropriate schema and save the current schema name.
  */
  lex->explain_format->m_schema_name_for_explain = m_schema_name_for_explain;
  char saved_schema_name_buf[NAME_LEN + 1];
  LEX_STRING saved_schema_name{saved_schema_name_buf,
                               sizeof(saved_schema_name_buf)};
  bool cur_db_changed = false;
  if (m_schema_name_for_explain.length != 0 &&
      mysql_opt_change_db(thd, m_schema_name_for_explain, &saved_schema_name,
                          false, &cur_db_changed)) {
    return nullptr; /* purecov: inspected */
  }

  Sql_cmd *ret = m_explainable_stmt->make_cmd(thd);

  /*
    Change back to original schema to make sure current schema stays consistent
    when preparing a statement or SP with EXPLAIN FOR SCHEMA.
  */
  if (cur_db_changed &&
      mysql_change_db(thd, to_lex_cstring(saved_schema_name), true)) {
    ret = nullptr; /* purecov: inspected */
  }

  if (ret == nullptr) return nullptr;  // OOM

  auto code = ret->sql_command_code();
  if (!is_explainable_query(code) && code != SQLCOM_EXPLAIN_OTHER) {
    assert(!"Should not happen!");
    my_error(ER_WRONG_USAGE, MYF(0), "EXPLAIN", "non-explainable query");
    return nullptr;
  }

  return ret;
}

/**
  Build a parsed tree for :
  SELECT '...json tree string...' as show_parse_tree.
  Essentially the SHOW PARSE_TREE statement is converted into the above
  SQL and passed to the executor.
*/
static Query_block *build_query_for_show_parse(
    const POS &pos, THD *thd, const std::string_view &json_tree) {
  // No query options.
  static const Query_options options = {0 /* query_spec_options */};

  /* '<json_tree>' */
  LEX_STRING json_tree_copy;
  if (lex_string_strmake(thd->mem_root, &json_tree_copy, json_tree.data(),
                         json_tree.length()))
    return nullptr;
  PTI_text_literal_text_string *literal_string = new (thd->mem_root)
      PTI_text_literal_text_string(pos, false, json_tree_copy);
  if (literal_string == nullptr) return nullptr;

  /* Literal string with alias ('<json_tree>' AS show_parse_tree ...) */
  PTI_expr_with_alias *expr_name = new (thd->mem_root) PTI_expr_with_alias(
      pos, literal_string, pos.cpp, {STRING_WITH_LEN("Show_parse_tree")});
  if (expr_name == nullptr) return nullptr;

  PT_select_item_list *item_list = new (thd->mem_root) PT_select_item_list(pos);
  if (item_list == nullptr) return nullptr;
  item_list->push_back(expr_name);

  PT_query_primary *query_specification =
      new (thd->mem_root) PT_query_specification(
          pos, options, item_list,
          Mem_root_array<PT_table_reference *>() /* Empty FROM list */,
          nullptr /*where*/);
  if (query_specification == nullptr) return nullptr;

  PT_query_expression *query_expression =
      new (thd->mem_root) PT_query_expression(pos, query_specification);
  if (query_expression == nullptr) return nullptr;

  LEX *lex = thd->lex;
  Query_block *current_query_block = lex->current_query_block();
  Parse_context pc(thd, current_query_block);
  if (thd->is_error()) return nullptr;

  lex->sql_command = SQLCOM_SELECT;
  if (query_expression->contextualize(&pc)) return nullptr;
  if (pc.finalize_query_expression()) return nullptr;
  lex->sql_command = SQLCOM_SHOW_PARSE_TREE;

  return current_query_block;
}

Sql_cmd *PT_load_table::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  Query_block *const select = lex->current_query_block();

  if (lex->sphead) {
    my_error(
        ER_SP_BADSTATEMENT, MYF(0),
        m_cmd.m_exchange.filetype == FILETYPE_CSV ? "LOAD DATA" : "LOAD XML");
    return nullptr;
  }

  lex->sql_command = SQLCOM_LOAD;

  switch (m_cmd.m_on_duplicate) {
    case On_duplicate::ERROR:
      lex->duplicates = DUP_ERROR;
      break;
    case On_duplicate::IGNORE_DUP:
      lex->set_ignore(true);
      break;
    case On_duplicate::REPLACE_DUP:
      lex->duplicates = DUP_REPLACE;
      break;
  }

  /* Fix lock for LOAD DATA CONCURRENT REPLACE */
  thr_lock_type lock_type = m_lock_type;
  enum_mdl_type mdl_type = MDL_SHARED_WRITE;

  if (m_cmd.is_bulk_load()) {
    lock_type = TL_WRITE;
    mdl_type = MDL_EXCLUSIVE;
  } else {
    if (lex->duplicates == DUP_REPLACE &&
        lock_type == TL_WRITE_CONCURRENT_INSERT) {
      lock_type = TL_WRITE_DEFAULT;
    }
    if (lock_type == TL_WRITE_LOW_PRIORITY) {
      mdl_type = MDL_SHARED_WRITE_LOW_PRIO;
    }
  }

  if (!select->add_table_to_list(thd, m_cmd.m_table, nullptr,
                                 TL_OPTION_UPDATING, lock_type, mdl_type,
                                 nullptr, m_cmd.m_opt_partitions))
    return nullptr;

  /* We can't give an error in the middle when using LOCAL files */
  if (m_cmd.m_is_local_file && lex->duplicates == DUP_ERROR)
    lex->set_ignore(true);

  Parse_context pc(thd, select);
  if (contextualize_safe(&pc, &m_cmd.m_opt_fields_or_vars)) {
    return nullptr;
  }
  assert(select->parsing_place == CTX_NONE);
  select->parsing_place = CTX_UPDATE_VALUE;
  if (contextualize_safe(&pc, &m_cmd.m_opt_set_fields) ||
      contextualize_safe(&pc, &m_cmd.m_opt_set_exprs)) {
    return nullptr;
  }
  assert(select->parsing_place == CTX_UPDATE_VALUE);
  select->parsing_place = CTX_NONE;

  return &m_cmd;
}

bool PT_select_item_list::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->select->fields = value;
  return false;
}

bool PT_limit_clause::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  if (limit_options.is_offset_first && limit_options.opt_offset != nullptr &&
      limit_options.opt_offset->itemize(pc, &limit_options.opt_offset))
    return true;

  if (limit_options.limit->itemize(pc, &limit_options.limit)) return true;

  if (!limit_options.is_offset_first && limit_options.opt_offset != nullptr &&
      limit_options.opt_offset->itemize(pc, &limit_options.opt_offset))
    return true;

  pc->select->select_limit = limit_options.limit;
  pc->select->offset_limit = limit_options.opt_offset;

  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_LIMIT);
  return false;
}

bool PT_table_factor_table_ident::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  Yacc_state *yyps = &thd->m_parser_state->m_yacc;

  m_table_ref = pc->select->add_table_to_list(
      thd, table_ident, opt_table_alias, 0, yyps->m_lock_type, yyps->m_mdl_type,
      opt_key_definition, opt_use_partition, nullptr, pc);
  if (m_table_ref == nullptr) return true;

  if (opt_tablesample != nullptr) {
    /* Check if the input sampling percentage is a valid argument*/
    if (opt_tablesample->m_sample_percentage->itemize(
            pc, &opt_tablesample->m_sample_percentage))
      return true;

    thd->lex->set_execute_only_in_secondary_engine(true, TABLESAMPLE);
    m_table_ref->set_tablesample(opt_tablesample->m_sampling_type,
                                 opt_tablesample->m_sample_percentage);
  }

  if (pc->select->add_joined_table(m_table_ref)) return true;

  return false;
}

void PT_table_factor_table_ident::add_json_info(Json_object *obj) {
  String s;

  print_table_ident(nullptr, table_ident, &s);
  obj->add_alias("table_ident",
                 create_dom_ptr<Json_string>(s.ptr(), s.length()));

  if (opt_table_alias != nullptr)
    obj->add_alias("table_alias", create_dom_ptr<Json_string>(opt_table_alias));
}

bool PT_table_reference_list_parens::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || contextualize_array(pc, &table_list))
    return true;

  assert(table_list.size() >= 2);
  m_table_ref = pc->select->nest_last_join(pc->thd, table_list.size());
  return m_table_ref == nullptr;
}

bool PT_joined_table::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || contextualize_tabs(pc)) return true;

  if (m_type & JTT_NATURAL)
    m_left_table_ref->add_join_natural(m_right_table_ref);

  if (m_type & JTT_STRAIGHT) m_right_table_ref->straight = true;

  return false;
}

void PT_joined_table::add_json_info(Json_object *obj) {
  std::string type_string;

  // Join nodes can re-stucture their child trees (see add_cross_join). It's
  // not worth changing the corresponding text of the modified join tree. Users
  // can anyway generate the text using the join type and leaf table
  // references. So just omit the text field.
  obj->remove("text");

  if (m_type & JTT_STRAIGHT)
    type_string += "STRAIGHT_JOIN";
  else {
    if (m_type & JTT_NATURAL) type_string += "NATURAL ";

    // Join type can't be both inner and outer.
    assert(!((m_type & JTT_INNER) &&
             ((m_type & JTT_LEFT) || (m_type & JTT_RIGHT))));

    if (m_type & JTT_INNER)
      type_string += "INNER ";
    else if (m_type & JTT_LEFT)
      type_string += "LEFT OUTER ";
    else if (m_type & JTT_RIGHT)
      type_string += "RIGHT OUTER ";
    else
      assert(0);  // Has to be inner or outer.

    type_string += "JOIN";
  }

  obj->add_alias("join_type", create_dom_ptr<Json_string>(type_string));
}

bool PT_cross_join::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  m_table_ref = pc->select->nest_last_join(pc->thd);
  return m_table_ref == nullptr;
}

bool PT_joined_table_on::do_contextualize(Parse_context *pc) {
  if (this->contextualize_tabs(pc)) return true;

  if (push_new_name_resolution_context(pc, this->m_left_table_ref,
                                       this->m_right_table_ref)) {
    this->error(pc, this->m_join_pos);
    return true;
  }

  Query_block *sel = pc->select;
  sel->parsing_place = CTX_ON;

  if (super::do_contextualize(pc) || on->itemize(pc, &on)) return true;
  if (!on->is_bool_func()) {
    on = make_condition(pc, on);
    if (on == nullptr) return true;
  }
  assert(sel == pc->select);

  add_join_on(this->m_right_table_ref, on);
  pc->thd->lex->pop_context();
  assert(sel->parsing_place == CTX_ON);
  sel->parsing_place = CTX_NONE;
  m_table_ref = pc->select->nest_last_join(pc->thd);

  return m_table_ref == nullptr;
}

bool PT_joined_table_using::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  m_left_table_ref->add_join_natural(m_right_table_ref);
  m_table_ref = pc->select->nest_last_join(pc->thd);
  if (m_table_ref == nullptr) return true;
  m_table_ref->join_using_fields = using_fields;

  return false;
}

void PT_joined_table_using::add_json_info(Json_object *obj) {
  super::add_json_info(obj);

  if (using_fields == nullptr || using_fields->is_empty()) return;

  List_iterator<String> using_fields_it(*using_fields);
  String using_fields_str;

  for (String *curr_str = using_fields_it++;;) {
    append_identifier(&using_fields_str, curr_str->ptr(), curr_str->length());
    if ((curr_str = using_fields_it++) == nullptr) break;
    using_fields_str.append(",", 1, system_charset_info);
  }
  obj->add_alias("using_fields",
                 create_dom_ptr<Json_string>(using_fields_str.ptr(),
                                             using_fields_str.length()));
}

bool PT_table_locking_clause::raise_error(THD *thd, const Table_ident *name,
                                          int error) {
  String s;
  print_table_ident(thd, name, &s);
  my_error(error, MYF(0), s.ptr());
  return true;
}

bool PT_table_locking_clause::raise_error(int error) {
  my_error(error, MYF(0));
  return true;
}

bool PT_set_scoped_system_variable::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || itemize_safe(pc, &m_opt_expr)) {
    return true;
  }

  // Remove deprecation warning when FULL is used as keyword.
  THD *thd = pc->thd;
  if (thd->get_stmt_da()->has_sql_condition(ER_WARN_DEPRECATED_IDENT)) {
    thd->get_stmt_da()->reset_condition_info(thd);
  }

  const bool is_1d_name = m_opt_prefix.str == nullptr;

  /*
    Reject transition variable names in the syntax:

        {"GLOBAL" | "SESSION" | "PERSIST" | ...} {"NEW" | "OLD"} "."  <name>
  */
  if (is_any_transition_variable_prefix(*pc, m_opt_prefix)) {
    error(pc, m_varpos);
    return true;
  }

  /*
    Reject SP variable names in the syntax:

        {"GLOBAL" | "SESSION" | ...} <SP variable name>

    Note: we also reject overload system variable names here too.
    TODO: most likely this reject is fine, since such a use case is confusing,
          OTOH this is also probable that a rejection of overloaded system
          variables is too strict, so a warning may be a better alternative.
  */
  if (is_1d_name && find_sp_variable(*pc, m_name) != nullptr) {
    error(pc, m_varpos);
    return true;
  }

  return add_system_variable_assignment(pc->thd, m_opt_prefix, m_name,
                                        pc->thd->lex->option_type, m_opt_expr);
}

bool PT_option_value_no_option_type_user_var::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc) || expr->itemize(pc, &expr)) return true;

  THD *thd = pc->thd;
  Item_func_set_user_var *item =
      new (pc->mem_root) Item_func_set_user_var(name, expr);
  if (item == nullptr) return true;
  set_var_user *var = new (thd->mem_root) set_var_user(item);
  if (var == nullptr) return true;
  return thd->lex->var_list.push_back(var);
}

bool PT_set_system_variable::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || itemize_safe(pc, &m_opt_expr)) {
    return true;
  }

  // Remove deprecation warning when FULL is used as keyword.
  THD *thd = pc->thd;
  if (thd->get_stmt_da()->has_sql_condition(ER_WARN_DEPRECATED_IDENT)) {
    thd->get_stmt_da()->reset_condition_info(thd);
  }

  /*
    This is `@@[scope.][prefix.]name, so reject @@[scope].{NEW | OLD}.name:
  */
  if (is_any_transition_variable_prefix(*pc, m_opt_prefix)) {
    error(pc, m_name_pos);
    return true;
  }

  return add_system_variable_assignment(thd, m_opt_prefix, m_name, m_scope,
                                        m_opt_expr);
}

bool PT_option_value_type::do_contextualize(Parse_context *pc) {
  pc->thd->lex->option_type = type;
  return super::do_contextualize(pc) || value->contextualize(pc);
}

bool PT_option_value_list_head::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
#ifndef NDEBUG
  LEX *old_lex = thd->lex;
#endif  // NDEBUG

  sp_create_assignment_lex(thd, delimiter_pos.raw.end);
  assert(thd->lex->query_block == thd->lex->current_query_block());
  Parse_context inner_pc(pc->thd, thd->lex->query_block,
                         pc->m_show_parse_tree.get());

  if (value->contextualize(&inner_pc)) return true;

  if (sp_create_assignment_instr(pc->thd, value_pos.raw.end)) return true;
  assert(thd->lex == old_lex && thd->lex->current_query_block() == pc->select);

  return false;
}

bool PT_start_option_value_list_no_type::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || head->contextualize(pc)) return true;

  if (sp_create_assignment_instr(pc->thd, head_pos.raw.end)) return true;
  assert(pc->thd->lex->query_block == pc->thd->lex->current_query_block());
  pc->select = pc->thd->lex->query_block;

  if (tail != nullptr && tail->contextualize(pc)) return true;

  return false;
}

bool PT_transaction_characteristic::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  Item *item = new (pc->mem_root) Item_int(value);
  if (item == nullptr) return true;
  System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(name);
  if (var_tracker.access_system_variable(thd)) {
    assert(false);
    return true;  // should never happen / OOM
  }
  set_var *var =
      new (thd->mem_root) set_var(lex->option_type, var_tracker, item);
  if (var == nullptr) return true;
  return lex->var_list.push_back(var);
}

bool PT_start_option_value_list_transaction::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  thd->lex->option_type = OPT_DEFAULT;
  if (characteristics->contextualize(pc)) return true;

  if (sp_create_assignment_instr(thd, end_pos.raw.end)) return true;
  assert(pc->thd->lex->query_block == pc->thd->lex->current_query_block());
  pc->select = pc->thd->lex->query_block;

  return false;
}

bool PT_start_option_value_list_following_option_type_eq::do_contextualize(
    Parse_context *pc) {
  if (super::do_contextualize(pc) || head->contextualize(pc)) return true;

  if (sp_create_assignment_instr(pc->thd, head_pos.raw.end)) return true;
  assert(pc->thd->lex->query_block == pc->thd->lex->current_query_block());
  pc->select = pc->thd->lex->query_block;

  if (opt_tail != nullptr && opt_tail->contextualize(pc)) return true;

  return false;
}

bool PT_start_option_value_list_following_option_type_transaction::
    do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc) || characteristics->contextualize(pc))
    return true;

  if (sp_create_assignment_instr(pc->thd, characteristics_pos.raw.end))
    return true;
  assert(pc->thd->lex->query_block == pc->thd->lex->current_query_block());
  pc->select = pc->thd->lex->query_block;

  return false;
}

bool PT_start_option_value_list_type::do_contextualize(Parse_context *pc) {
  pc->thd->lex->option_type = type;
  return super::do_contextualize(pc) || list->contextualize(pc);
}

bool PT_set::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  THD *thd = pc->thd;
  LEX *lex = thd->lex;
  lex->sql_command = SQLCOM_SET_OPTION;
  lex->option_type = OPT_SESSION;
  lex->var_list.clear();
  lex->autocommit = false;

  sp_create_assignment_lex(thd, set_pos.raw.end);
  assert(pc->thd->lex->query_block == pc->thd->lex->current_query_block());
  pc->select = pc->thd->lex->query_block;

  return list->contextualize(pc);
}

bool PT_into_destination::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *lex = pc->thd->lex;
  if (!pc->thd->lex->parsing_options.allows_select_into) {
    if (lex->sql_command == SQLCOM_SHOW_CREATE ||
        lex->sql_command == SQLCOM_CREATE_VIEW)
      my_error(ER_VIEW_SELECT_CLAUSE, MYF(0), "INTO");
    else
      error(pc, m_pos);
    return true;
  }
  return false;
}

bool PT_into_destination_outfile::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *lex = pc->thd->lex;
  lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  lex->result = new (pc->mem_root) Query_result_export(&m_exchange);
  return lex->result == nullptr;
}

bool PT_into_destination_dumpfile::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *lex = pc->thd->lex;
  lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  lex->result = new (pc->mem_root) Query_result_dump(&m_exchange);
  return lex->result == nullptr;
}

bool PT_select_var_list::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  List_iterator<PT_select_var> it(value);
  PT_select_var *var;
  while ((var = it++)) {
    if (var->contextualize(pc)) return true;
  }

  LEX *const lex = pc->thd->lex;
  Query_dumpvar *dumpvar = new (pc->mem_root) Query_dumpvar();
  if (dumpvar == nullptr) return true;

  dumpvar->var_list = value;
  lex->result = dumpvar;
  lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);

  return false;
}

bool PT_query_expression::do_contextualize(Parse_context *pc) {
  pc->m_stack.push_back(
      QueryLevel(pc->mem_root, SC_QUERY_EXPRESSION, m_order != nullptr));
  if (contextualize_safe(pc, m_with_clause))
    return true; /* purecov: inspected */

  if (Parse_tree_node::do_contextualize(pc) || m_body->contextualize(pc))
    return true;

  QueryLevel ql = pc->m_stack.back();
  Query_term *expr = ql.m_elts.back();
  pc->m_stack.pop_back();

  switch (expr->term_type()) {
    case QT_UNARY: {
      Query_term_unary *ex = down_cast<Query_term_unary *>(expr);
      Query_expression *qe = pc->select->master_query_expression();

      // The setting of no_table_names_allowed in the created post processing
      // block below to false foreshadows our removing the parentheses in
      // Query_term::pushdown_limit_order_by. We need to duplicate the logic
      // here in order to allow a construction like
      //
      //   ( SELECT a FROM t ) ORDER BY t.a
      //
      // for which we remove the parentheses because the inner query expression
      // has no LIMIT or ORDER BY of its own.
      if (ex->set_block(qe->create_post_processing_block(ex))) return true;

      if (m_order != nullptr)
        ex->query_block()->order_list = m_order->order_list->value;

      if (m_limit != nullptr) {
        ex->query_block()->select_limit = m_limit->limit_options.limit;
        ex->query_block()->offset_limit = m_limit->limit_options.opt_offset;
      }

      Query_block *orig_query_block = pc->select;
      LEX *lex = pc->thd->lex;
      pc->select = ex->query_block();
      lex->push_context(&pc->select->context);
      assert(pc->select->parsing_place == CTX_NONE);
      if (contextualize_safe(pc, m_order, m_limit)) return true;
      lex->pop_context();
      pc->select = orig_query_block;

      if (pc->m_stack.back().m_type == SC_QUERY_EXPRESSION) {  // 3 deep or more
        expr = new (pc->mem_root) Query_term_unary(pc->mem_root, expr);
        if (expr == nullptr) return true;
      }
      QueryLevel upper = pc->m_stack.back();
      pc->m_stack.pop_back();
      ql.m_elts.pop_back();
      if (upper.m_type == SC_UNION_DISTINCT || upper.m_type == SC_UNION_ALL ||
          upper.m_type == SC_EXCEPT_DISTINCT || upper.m_type == SC_EXCEPT_ALL ||
          upper.m_type == SC_INTERSECT_DISTINCT ||
          upper.m_type == SC_INTERSECT_ALL)
        ql = upper;
      ql.m_elts.push_back(expr);
      pc->m_stack.push_back(ql);
    } break;
    case QT_QUERY_BLOCK: {
      if (contextualize_order_and_limit(pc)) return true;
      if (pc->m_stack.back().m_type == SC_QUERY_EXPRESSION) {  // 2 deep or more
        expr = new (pc->mem_root) Query_term_unary(pc->mem_root, expr);
        if (expr == nullptr) return true;
      }
      QueryLevel upper = pc->m_stack.back();
      pc->m_stack.pop_back();
      ql.m_elts.pop_back();
      if (upper.m_type == SC_UNION_DISTINCT || upper.m_type == SC_UNION_ALL ||
          upper.m_type == SC_EXCEPT_DISTINCT || upper.m_type == SC_EXCEPT_ALL ||
          upper.m_type == SC_INTERSECT_DISTINCT ||
          upper.m_type == SC_INTERSECT_ALL)
        ql = upper;
      ql.m_elts.push_back(expr);
      pc->m_stack.push_back(ql);
    } break;
    case QT_UNION:
    case QT_EXCEPT:
    case QT_INTERSECT: {
      auto ex = down_cast<Query_term_set_op *>(expr);
      const bool already_decorated =
          ex->query_block()->order_list.elements > 0 ||
          ex->query_block()->select_limit != nullptr;
      if (already_decorated) {
        ex = new (pc->mem_root) Query_term_unary(pc->mem_root, ex);
        if (ex == nullptr) return true;
        Query_expression *qe = pc->select->master_query_expression();
        if (ex->set_block(qe->create_post_processing_block(ex))) return true;
      }
      if (m_order != nullptr) {
        ex->query_block()->order_list = m_order->order_list->value;
      }
      if (m_limit != nullptr) {
        ex->query_block()->select_limit = m_limit->limit_options.limit;
        ex->query_block()->offset_limit = m_limit->limit_options.opt_offset;
      }

      Query_block *orig_query_block = pc->select;
      LEX *lex = pc->thd->lex;
      pc->select = ex->query_block();
      lex->push_context(&pc->select->context);
      assert(pc->select->parsing_place == CTX_NONE);
      if (contextualize_safe(pc, m_order, m_limit)) return true;
      lex->pop_context();
      pc->select = orig_query_block;

      if (pc->m_stack.back().m_type == SC_QUERY_EXPRESSION) {  // 3 deep or more
        ex = new (pc->mem_root) Query_term_unary(pc->mem_root, ex);
        if (ex == nullptr) return true;
      }

      pc->m_stack.back().m_elts.push_back(ex);
    } break;
  }

  return false;
}

bool PT_subquery::do_contextualize(Parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *lex = pc->thd->lex;
  if (!lex->expr_allows_subquery || lex->sql_command == SQLCOM_PURGE) {
    error(pc, m_pos);
    return true;
  }

  // Create a Query_expression and Query_block for the subquery's query
  // expression.
  Query_block *child = lex->new_query(pc->select);
  if (child == nullptr) return true;

  Parse_context inner_pc(pc->thd, child, pc->m_show_parse_tree.get());
  inner_pc.m_stack.push_back(QueryLevel(pc->mem_root, SC_SUBQUERY));

  if (m_is_derived_table) child->linkage = DERIVED_TABLE_TYPE;

  if (qe->contextualize(&inner_pc)) return true;

  if (qe->has_into_clause()) {
    my_error(ER_MISPLACED_INTO, MYF(0));
    return true;
  }

  query_block = inner_pc.select->master_query_expression()->first_query_block();
  if (inner_pc.finalize_query_expression()) return true;
  inner_pc.m_stack.pop_back();
  assert(inner_pc.m_stack.size() == 0);

  lex->pop_context();
  pc->select->n_child_sum_items += child->n_sum_items;

  /*
    A subquery (and all the subsequent query blocks in a UNION) can add
    columns to an outer query block. Reserve space for them.
  */
  for (Query_block *temp = child; temp != nullptr;
       temp = temp->next_query_block()) {
    pc->select->select_n_where_fields += temp->select_n_where_fields;
    pc->select->select_n_having_items += temp->select_n_having_items;
  }

  return false;
}

Sql_cmd *PT_create_srs::make_cmd(THD *thd) {
  // Note: This function hard-codes the maximum length of various
  // strings. These lengths must match those in
  // sql/dd/impl/tables/spatial_reference_systems.cc.

  thd->lex->sql_command = SQLCOM_CREATE_SRS;

  if (m_srid > std::numeric_limits<gis::srid_t>::max()) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "SRID",
             m_or_replace ? "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM"
                          : "CREATE SPATIAL REFERENCE SYSTEM");
    return nullptr;
  }
  if (m_srid == 0) {
    my_error(ER_CANT_MODIFY_SRID_0, MYF(0));
    return nullptr;
  }

  if (m_attributes.srs_name.str == nullptr) {
    my_error(ER_SRS_MISSING_MANDATORY_ATTRIBUTE, MYF(0), "NAME");
    return nullptr;
  }
  MYSQL_LEX_STRING srs_name_utf8 = {nullptr, 0};
  if (thd->convert_string(&srs_name_utf8, &my_charset_utf8mb3_bin,
                          m_attributes.srs_name.str,
                          m_attributes.srs_name.length, thd->charset())) {
    /* purecov: begin inspected */
    my_error(ER_DA_OOM, MYF(0));
    return nullptr;
    /* purecov: end */
  }
  if (srs_name_utf8.length == 0 || std::isspace(srs_name_utf8.str[0]) ||
      std::isspace(srs_name_utf8.str[srs_name_utf8.length - 1])) {
    my_error(ER_SRS_NAME_CANT_BE_EMPTY_OR_WHITESPACE, MYF(0));
    return nullptr;
  }
  if (contains_control_char(srs_name_utf8.str, srs_name_utf8.length)) {
    my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "NAME");
    return nullptr;
  }
  String srs_name_str(srs_name_utf8.str, srs_name_utf8.length,
                      &my_charset_utf8mb3_bin);
  if (srs_name_str.numchars() > 80) {
    my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "NAME", 80);
    return nullptr;
  }

  if (m_attributes.definition.str == nullptr) {
    my_error(ER_SRS_MISSING_MANDATORY_ATTRIBUTE, MYF(0), "DEFINITION");
    return nullptr;
  }
  MYSQL_LEX_STRING definition_utf8 = {nullptr, 0};
  if (thd->convert_string(&definition_utf8, &my_charset_utf8mb3_bin,
                          m_attributes.definition.str,
                          m_attributes.definition.length, thd->charset())) {
    /* purecov: begin inspected */
    my_error(ER_DA_OOM, MYF(0));
    return nullptr;
    /* purecov: end */
  }
  String definition_str(definition_utf8.str, definition_utf8.length,
                        &my_charset_utf8mb3_bin);
  if (contains_control_char(definition_utf8.str, definition_utf8.length)) {
    my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "DEFINITION");
    return nullptr;
  }
  if (definition_str.numchars() > 4096) {
    my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "DEFINITION", 4096);
    return nullptr;
  }

  MYSQL_LEX_STRING organization_utf8 = {nullptr, 0};
  if (m_attributes.organization.str != nullptr) {
    if (thd->convert_string(&organization_utf8, &my_charset_utf8mb3_bin,
                            m_attributes.organization.str,
                            m_attributes.organization.length, thd->charset())) {
      /* purecov: begin inspected */
      my_error(ER_DA_OOM, MYF(0));
      return nullptr;
      /* purecov: end */
    }
    if (organization_utf8.length == 0 ||
        std::isspace(organization_utf8.str[0]) ||
        std::isspace(organization_utf8.str[organization_utf8.length - 1])) {
      my_error(ER_SRS_ORGANIZATION_CANT_BE_EMPTY_OR_WHITESPACE, MYF(0));
      return nullptr;
    }
    String organization_str(organization_utf8.str, organization_utf8.length,
                            &my_charset_utf8mb3_bin);
    if (contains_control_char(organization_utf8.str,
                              organization_utf8.length)) {
      my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "ORGANIZATION");
      return nullptr;
    }
    if (organization_str.numchars() > 256) {
      my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "ORGANIZATION", 256);
      return nullptr;
    }

    if (m_attributes.organization_coordsys_id >
        std::numeric_limits<gis::srid_t>::max()) {
      my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "IDENTIFIED BY",
               m_or_replace ? "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM"
                            : "CREATE SPATIAL REFERENCE SYSTEM");
      return nullptr;
    }
  }

  MYSQL_LEX_STRING description_utf8 = {nullptr, 0};
  if (m_attributes.description.str != nullptr) {
    if (thd->convert_string(&description_utf8, &my_charset_utf8mb3_bin,
                            m_attributes.description.str,
                            m_attributes.description.length, thd->charset())) {
      /* purecov: begin inspected */
      my_error(ER_DA_OOM, MYF(0));
      return nullptr;
      /* purecov: end */
    }
    String description_str(description_utf8.str, description_utf8.length,
                           &my_charset_utf8mb3_bin);
    if (contains_control_char(description_utf8.str, description_utf8.length)) {
      my_error(ER_SRS_INVALID_CHARACTER_IN_ATTRIBUTE, MYF(0), "DESCRIPTION");
      return nullptr;
    }
    if (description_str.numchars() > 2048) {
      my_error(ER_SRS_ATTRIBUTE_STRING_TOO_LONG, MYF(0), "DESCRIPTION", 2048);
      return nullptr;
    }
  }

  sql_cmd.init(m_or_replace, m_if_not_exists, m_srid, srs_name_utf8,
               definition_utf8, organization_utf8,
               m_attributes.organization_coordsys_id, description_utf8);
  return &sql_cmd;
}

Sql_cmd *PT_drop_srs::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_DROP_SRS;

  if (m_srid > std::numeric_limits<gis::srid_t>::max()) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "SRID",
             "DROP SPATIAL REFERENCE SYSTEM");
    return nullptr;
  }
  if (m_srid == 0) {
    my_error(ER_CANT_MODIFY_SRID_0, MYF(0));
    return nullptr;
  }

  return &sql_cmd;
}

Sql_cmd *PT_alter_instance::make_cmd(THD *thd) {
  thd->lex->no_write_to_binlog = false;
  return &sql_cmd;
}

bool PT_check_constraint::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc) ||
      cc_spec.check_expr->itemize(pc, &cc_spec.check_expr))
    return true;

  if (pc->alter_info->check_constraint_spec_list.push_back(&cc_spec))
    return true;

  pc->alter_info->flags |= Alter_info::ADD_CHECK_CONSTRAINT;
  return false;
}

Sql_cmd *PT_create_role::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_CREATE_ROLE;
  return &sql_cmd;
}

Sql_cmd *PT_drop_role::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_DROP_ROLE;
  return &sql_cmd;
}

Sql_cmd *PT_set_role::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_SET_ROLE;
  return &sql_cmd;
}

LEX_USER *PT_role_or_privilege::get_user(THD *thd) {
  thd->syntax_error_at(m_errpos, "Illegal authorization identifier");
  return nullptr;
}

Privilege *PT_role_or_privilege::get_privilege(THD *thd) {
  thd->syntax_error_at(m_errpos, "Illegal privilege identifier");
  return nullptr;
}

LEX_USER *PT_role_at_host::get_user(THD *thd) {
  return LEX_USER::alloc(thd, &role, &host);
}

LEX_USER *PT_role_or_dynamic_privilege::get_user(THD *thd) {
  return LEX_USER::alloc(thd, &ident, nullptr);
}

Privilege *PT_role_or_dynamic_privilege::get_privilege(THD *thd) {
  return new (thd->mem_root) Dynamic_privilege(ident, nullptr);
}

Privilege *PT_static_privilege::get_privilege(THD *thd) {
  return new (thd->mem_root) Static_privilege(grant, columns);
}

Privilege *PT_dynamic_privilege::get_privilege(THD *thd) {
  return new (thd->mem_root) Dynamic_privilege(ident, nullptr);
}

Sql_cmd *PT_grant_roles::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_GRANT_ROLE;

  List<LEX_USER> *role_objects = new (thd->mem_root) List<LEX_USER>;
  if (role_objects == nullptr) return nullptr;  // OOM
  for (PT_role_or_privilege *r : *roles) {
    LEX_USER *user = r->get_user(thd);
    if (r == nullptr || role_objects->push_back(user)) return nullptr;
  }

  return new (thd->mem_root)
      Sql_cmd_grant_roles(role_objects, users, with_admin_option);
}

Sql_cmd *PT_revoke_roles::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_REVOKE_ROLE;

  List<LEX_USER> *role_objects = new (thd->mem_root) List<LEX_USER>;
  if (role_objects == nullptr) return nullptr;  // OOM
  for (PT_role_or_privilege *r : *roles) {
    LEX_USER *user = r->get_user(thd);
    if (r == nullptr || role_objects->push_back(user)) return nullptr;
  }
  return new (thd->mem_root) Sql_cmd_revoke_roles(role_objects, users);
}

Sql_cmd *PT_alter_user_default_role::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_ALTER_USER_DEFAULT_ROLE;
  return &sql_cmd;
}

Sql_cmd *PT_show_grants::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_SHOW_GRANTS;
  return &sql_cmd;
}

Sql_cmd *PT_show_parse_tree::make_cmd(THD *thd) {
  LEX *lex = thd->lex;

  // Several of the 'simple_statement:' do $$= nullptr;
  if (m_parse_tree_stmt == nullptr) {
    // my_error(ER_NOT_SUPPORTED_YET ....)
    Parse_tree_root::get_printable_parse_tree(thd);
    return nullptr;
  }
  std::string parse_tree_str = m_parse_tree_stmt->get_printable_parse_tree(thd);

  if (parse_tree_str.empty()) return nullptr;

  // get_printable_parse_tree() must have updated lex, and we want to
  // start-over with a new query. So reset lex.
  // 'result' may be non-null for an INTO clause. lex->reset() doesn't like
  // non-null 'result'.
  lex->result = nullptr;
  lex_start(thd);

  lex->sql_command = m_sql_command;

  if (build_query_for_show_parse(m_pos, thd, parse_tree_str) == nullptr)
    return nullptr;

  lex->sql_command = SQLCOM_SHOW_PARSE_TREE;
  return &m_sql_cmd;
}

bool PT_alter_table_action::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->alter_info->flags |= flag;
  return false;
}

bool PT_alter_table_set_default::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc) || itemize_safe(pc, &m_expr)) return true;
  Alter_column *alter_column;
  if (m_expr == nullptr || m_expr->basic_const_item()) {
    Item *actual_expr = m_expr;
    if (m_expr && m_expr->type() == Item::FUNC_ITEM) {
      /*
        Default value should be literal => basic constants =>
        no need fix_fields()
       */
      Item_func *func = down_cast<Item_func *>(m_expr);
      if (func->result_type() != INT_RESULT) {
        my_error(ER_INVALID_DEFAULT, MYF(0), m_name);
        return true;
      }
      assert(dynamic_cast<Item_func_true *>(func) ||
             dynamic_cast<Item_func_false *>(func));
      actual_expr = new Item_int(func->val_int());
    }
    alter_column = new (pc->mem_root) Alter_column(m_name, actual_expr);
  } else {
    auto vg = new (pc->mem_root) Value_generator;
    if (vg == nullptr) return true;  // OOM
    vg->expr_item = m_expr;
    vg->set_field_stored(true);
    alter_column = new (pc->mem_root) Alter_column(m_name, vg);
  }
  if (alter_column == nullptr ||
      pc->alter_info->alter_list.push_back(alter_column)) {
    return true;  // OOM
  }
  return false;
}

bool PT_alter_table_order::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc) || m_order->contextualize(pc)) return true;
  pc->select->order_list = m_order->value;
  return false;
}

bool PT_alter_table_partition_by::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc) || m_partition->contextualize(pc))
    return true;
  pc->thd->lex->part_info = &m_partition->part_info;
  return false;
}

bool PT_alter_table_add_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *const lex = pc->thd->lex;
  lex->no_write_to_binlog = m_no_write_to_binlog;
  assert(lex->part_info == nullptr);
  lex->part_info = &m_part_info;
  return false;
}

bool PT_alter_table_drop_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  assert(pc->alter_info->partition_names.is_empty());
  pc->alter_info->partition_names = m_partitions;
  return false;
}

bool PT_alter_table_rebuild_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
  return false;
}

bool PT_alter_table_optimize_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
  return false;
}

bool PT_alter_table_analyze_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
  return false;
}

bool PT_alter_table_check_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *const lex = pc->thd->lex;
  lex->check_opt.flags |= m_flags;
  lex->check_opt.sql_flags |= m_sql_flags;
  return false;
}

bool PT_alter_table_repair_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  LEX *const lex = pc->thd->lex;
  lex->no_write_to_binlog = m_no_write_to_binlog;

  lex->check_opt.flags |= m_flags;
  lex->check_opt.sql_flags |= m_sql_flags;

  return false;
}

bool PT_alter_table_coalesce_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;

  pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
  pc->alter_info->num_parts = m_num_parts;
  return false;
}

bool PT_alter_table_truncate_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  return false;
}

bool PT_alter_table_reorganize_partition::do_contextualize(
    Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc)) return true;
  pc->thd->lex->part_info = &m_partition_info;
  pc->thd->lex->no_write_to_binlog = m_no_write_to_binlog;
  return false;
}

bool PT_preload_keys::do_contextualize(Table_ddl_parse_context *pc) {
  if (super::do_contextualize(pc) ||
      !pc->select->add_table_to_list(
          pc->thd, m_table, nullptr,
          m_ignore_leaves ? TL_OPTION_IGNORE_LEAVES : 0, TL_READ,
          MDL_SHARED_READ, m_opt_cache_key_list))
    return true;
  return false;
}

Alter_tablespace_parse_context::Alter_tablespace_parse_context(
    THD *thd, bool show_parse_tree)
    : Parse_context_base(show_parse_tree), thd(thd), mem_root(thd->mem_root) {}

bool PT_alter_tablespace_option_nodegroup::do_contextualize(
    Alter_tablespace_parse_context *pc) {
  if (super::do_contextualize(pc)) return true; /* purecov: inspected */  // OOM

  if (pc->nodegroup_id != UNDEF_NODEGROUP) {
    my_error(ER_FILEGROUP_OPTION_ONLY_ONCE, MYF(0), "NODEGROUP");
    return true;
  }
  pc->nodegroup_id = m_nodegroup_id;
  return false;
}

Sql_cmd *PT_create_resource_group::make_cmd(THD *thd) {
  if (check_resource_group_support()) return nullptr;

  if (check_resource_group_name_len(sql_cmd.m_name, Sql_condition::SL_ERROR))
    return nullptr;

  if (has_priority &&
      validate_resource_group_priority(thd, &sql_cmd.m_priority, sql_cmd.m_name,
                                       sql_cmd.m_type))
    return nullptr;

  for (auto &range : *sql_cmd.m_cpu_list) {
    if (validate_vcpu_range(range)) return nullptr;
  }

  thd->lex->sql_command = SQLCOM_CREATE_RESOURCE_GROUP;
  return &sql_cmd;
}

Sql_cmd *PT_alter_resource_group::make_cmd(THD *thd) {
  if (check_resource_group_support()) return nullptr;

  if (check_resource_group_name_len(sql_cmd.m_name, Sql_condition::SL_ERROR))
    return nullptr;

  for (auto &range : *sql_cmd.m_cpu_list) {
    if (validate_vcpu_range(range)) return nullptr;
  }

  thd->lex->sql_command = SQLCOM_ALTER_RESOURCE_GROUP;
  return &sql_cmd;
}

Sql_cmd *PT_drop_resource_group::make_cmd(THD *thd) {
  if (check_resource_group_support()) return nullptr;

  if (check_resource_group_name_len(sql_cmd.m_name, Sql_condition::SL_ERROR))
    return nullptr;

  thd->lex->sql_command = SQLCOM_DROP_RESOURCE_GROUP;
  return &sql_cmd;
}

Sql_cmd *PT_set_resource_group::make_cmd(THD *thd) {
  if (check_resource_group_support()) return nullptr;

  if (check_resource_group_name_len(sql_cmd.m_name, Sql_condition::SL_ERROR))
    return nullptr;

  thd->lex->sql_command = SQLCOM_SET_RESOURCE_GROUP;
  return &sql_cmd;
}

Sql_cmd *PT_restart_server::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_RESTART_SERVER;
  return &sql_cmd;
}

/**
   Generic attribute node that can be used with different base types
   and corresponding parse contexts. CFP (Contextualizer Function
   Pointer) argument implements a suitable contextualize action in the
   given context. Value is typically a decayed captureless lambda.
 */
template <class ATTRIBUTE, class BASE>
class PT_attribute : public BASE {
  ATTRIBUTE m_attr;
  using CFP = bool (*)(ATTRIBUTE, typename BASE::context_t *);
  CFP m_cfp;

 public:
  PT_attribute(ATTRIBUTE a, CFP cfp) : BASE(POS()), m_attr{a}, m_cfp{cfp} {}
  bool do_contextualize(typename BASE::context_t *pc) override {
    return BASE::do_contextualize(pc) || m_cfp(m_attr, pc);
  }
};

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_alter_tablespace_option_base* to PT_attribute object.
 */
PT_alter_tablespace_option_base *make_tablespace_engine_attribute(
    MEM_ROOT *mem_root, LEX_CSTRING attr) {
  return new (mem_root)
      PT_attribute<LEX_CSTRING, PT_alter_tablespace_option_base>(
          attr, +[](LEX_CSTRING a, Alter_tablespace_parse_context *pc) {
            pc->engine_attribute = a;
            return false;
          });
}

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_alter_tablespace_option_base* to PT_attribute object.

 */
PT_create_table_option *make_table_engine_attribute(MEM_ROOT *mem_root,
                                                    LEX_CSTRING attr) {
  return new (mem_root) PT_attribute<LEX_CSTRING, PT_create_table_option>(
      attr, +[](LEX_CSTRING a, Table_ddl_parse_context *pc) {
        pc->create_info->engine_attribute = a;
        pc->create_info->used_fields |= HA_CREATE_USED_ENGINE_ATTRIBUTE;
        pc->alter_info->flags |=
            (Alter_info::ALTER_OPTIONS | Alter_info::ANY_ENGINE_ATTRIBUTE);
        return false;
      });
}

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_create_table_option* to PT_attribute object.

 */
PT_create_table_option *make_table_secondary_engine_attribute(
    MEM_ROOT *mem_root, LEX_CSTRING attr) {
  return new (mem_root) PT_attribute<LEX_CSTRING, PT_create_table_option>(
      attr, +[](LEX_CSTRING a, Table_ddl_parse_context *pc) {
        pc->create_info->secondary_engine_attribute = a;
        pc->create_info->used_fields |=
            HA_CREATE_USED_SECONDARY_ENGINE_ATTRIBUTE;
        pc->alter_info->flags |= Alter_info::ALTER_OPTIONS;
        return false;
      });
}

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_create_table_option* to PT_attribute object.

 */
PT_column_attr_base *make_column_engine_attribute(MEM_ROOT *mem_root,
                                                  LEX_CSTRING attr) {
  return new (mem_root) PT_attribute<LEX_CSTRING, PT_column_attr_base>(
      attr, +[](LEX_CSTRING a, Column_parse_context *pc) {
        // Note that a std::function is created from the lambda and constructed
        // directly in the vector.
        // This means it is necessary to ensure that the elements of the vector
        // are destroyed. This will not happen automatically when the vector is
        // moved to the Alter_info struct which is allocated on the mem_root
        // and not destroyed.
        pc->cf_appliers.emplace_back([=](Create_field *cf, Alter_info *ai) {
          cf->m_engine_attribute = a;
          ai->flags |= Alter_info::ANY_ENGINE_ATTRIBUTE;
          return false;
        });
        return false;
      });
}

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_column_attr_base* to PT_attribute object.

 */
PT_column_attr_base *make_column_secondary_engine_attribute(MEM_ROOT *mem_root,
                                                            LEX_CSTRING attr) {
  return new (mem_root) PT_attribute<LEX_CSTRING, PT_column_attr_base>(
      attr, +[](LEX_CSTRING a, Column_parse_context *pc) {
        // Note that a std::function is created from the lambda and constructed
        // directly in the vector.
        // This means it is necessary to ensure that the elements of the vector
        // are destroyed. This will not happen automatically when the vector is
        // moved to the Alter_info struct which is allocated on the mem_root
        // and not destroyed.
        pc->cf_appliers.emplace_back([=](Create_field *cf, Alter_info *) {
          cf->m_secondary_engine_attribute = a;
          return false;
        });
        return false;
      });
}

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_base_index_option* to PT_attribute object.

 */
PT_base_index_option *make_index_engine_attribute(MEM_ROOT *mem_root,
                                                  LEX_CSTRING attr) {
  return new (mem_root) PT_attribute<LEX_CSTRING, PT_base_index_option>(
      attr, +[](LEX_CSTRING a, Table_ddl_parse_context *pc) {
        pc->key_create_info->m_engine_attribute = a;
        pc->alter_info->flags |= Alter_info::ANY_ENGINE_ATTRIBUTE;
        return false;
      });
}

/**
   Factory function which instantiates PT_attribute with suitable
   parameters, allocates on the provided mem_root, and returns the
   appropriate base pointer.

   @param mem_root Memory arena.
   @param attr     Attribute value from parser.

   @return PT_base_index_option* to PT_attribute object.
 */
PT_base_index_option *make_index_secondary_engine_attribute(MEM_ROOT *mem_root,
                                                            LEX_CSTRING attr) {
  return new (mem_root) PT_attribute<LEX_CSTRING, PT_base_index_option>(
      attr, +[](LEX_CSTRING a, Table_ddl_parse_context *pc) {
        pc->key_create_info->m_secondary_engine_attribute = a;
        return false;
      });
}

PT_install_component::PT_install_component(
    const POS &pos, THD *thd, const Mem_root_array_YY<LEX_STRING> urns,
    List<PT_install_component_set_element> *set_elements)
    : Parse_tree_root(pos), m_urns(urns), m_set_elements(set_elements) {
  const char *prefix = "file://component_";
  const auto prefix_len = sizeof("file://component_") - 1;

  if (m_urns.size() == 1 &&
      !thd->charset()->coll->strnncoll(
          thd->charset(), pointer_cast<const uchar *>(m_urns[0].str),
          m_urns[0].length, pointer_cast<const uchar *>(prefix), prefix_len,
          true)) {
    for (auto &elt : *m_set_elements) {
      if (elt.name.prefix.length == 0) {
        elt.name.prefix.str = m_urns[0].str + prefix_len;
        elt.name.prefix.length = m_urns[0].length - prefix_len;
      }
    }
  }
}

Sql_cmd *PT_install_component::make_cmd(THD *thd) {
  thd->lex->sql_command = SQLCOM_INSTALL_COMPONENT;

  if (!m_set_elements->is_empty()) {
    Parse_context pc(thd, thd->lex->current_query_block());
    for (auto &elt : *m_set_elements) {
      if (elt.expr->itemize(&pc, &elt.expr)) return nullptr;
      /*
        If the SET value is a field, change it to a string to allow things
        SET variable = OFF
      */
      if (elt.expr->type() == Item::FIELD_ITEM) {
        Item_field *item_field = down_cast<Item_field *>(elt.expr);
        if (item_field->table_name != nullptr) {
          // Reject a dot-separated identified at the RHS of:
          //    SET <variable_name> = <table_name>.<field_name>
          my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), elt.name.name.str);
          return nullptr;
        }
        assert(item_field->field_name != nullptr);
        elt.expr = new (thd->mem_root)
            Item_string(item_field->field_name, strlen(item_field->field_name),
                        system_charset_info);     // names are utf8
        if (elt.expr == nullptr) return nullptr;  // OOM
      }
      if (elt.expr->has_subquery() || elt.expr->has_stored_program() ||
          elt.expr->has_aggregation()) {
        my_error(ER_SET_CONSTANTS_ONLY, MYF(0));
        return nullptr;
      }
    }
  }

  return new (thd->mem_root) Sql_cmd_install_component(m_urns, m_set_elements);
}
