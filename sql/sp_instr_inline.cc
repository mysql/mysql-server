/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "sql/sp_instr_inline.h"

#include "scope_guard.h"
#include "sql/item.h"
#include "sql/mem_root_array.h"
#include "sql/parse_tree_nodes.h"
#include "sql/sp.h"
#include "sql/sp_instr.h"
#include "sql/sp_pcontext.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_derived.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_parse.h"
#include "sql/sql_resolver.h"
#include "sql/window.h"

namespace sp_inl {

/**
  Base class for instructions within stored programs (currently limited to
  stored functions) that are eligible for inlining. The sp_inline_instr class
  defines the interface and provides the foundational implementation for
  inlining stored function instructions.
*/
class sp_inline_instr {
 public:
  sp_inline_instr(sp_instr *sp_instr)
      : m_sp_instr(sp_instr),
        m_is_redundant_instr(false),
        m_is_validated_instr(false) {}

  virtual ~sp_inline_instr() {}

  static void record_instruction_inlining_error(std::string &err_reason,
                                                uint ip) {
    err_reason.append(" Statement at line ")
        .append(std::to_string(ip))
        .append(" is not supported by the secondary engine.");
  }

  /**
    Sets m_is_validated_instr to true, indicating that the instruction has
    successfully passed the checks performed by validate().
  */
  void set_is_validated_instr() { m_is_validated_instr = true; }

  /**
    Gets the value of m_is_validated_instr.
  */
  bool is_validated_instr() { return m_is_validated_instr; }

  /**
    Sets m_is_redundant_instr to true, indicating that the instruction is not
    required for computing the return result of a stored function.
  */
  void set_is_redundant_instr() { m_is_redundant_instr = true; }

  /**
    Gets the value of m_is_redundant_instr.
  */
  bool is_redundant_instr() { return m_is_redundant_instr; }

  /**
    Retrieves the instruction pointer (IP) for this instruction. The IP
    represents the order number of the instruction within the stored function.
  */
  uint get_ip() { return m_sp_instr->get_ip(); }

  /**
    Checks for any errors that may occur during the preparation phase of a
    redundant instruction.

    @param      thd    thread context
    @param[out] err_reason  error reason if it occurs

    @returns false if success, true if error
  */
  virtual bool check_redundant_instr_for_errors(THD *thd [[maybe_unused]],
                                                std::string &err_reason
                                                [[maybe_unused]]) {
    return false;
  }

  /**
    Checks if this stored function instruction can be inlined

    @param[out] err_reason if the function cannot be inlined err_reason returns
    the error reason

    @return "false" if validation successful, "true" otherwise
  */
  virtual bool validate(std::string &err_reason) = 0;

  /**
    Determines whether the current stored function instruction is redundant
    based on the provided set of live variables.
    A live variable is one that is required to compute the final result of the
    stored function. An instruction is considered redundant if it does not
    contribute to this result. If the instruction is determined to be
    non-redundant, this function updates the set of live variables with those
    used by the instruction.
    Additionally, the function collects instances of stored functions referenced
    in this instruction in order to use them later for recursion detection.

    @param[in]      thd                   Thread context.
    @param[in,out]  offsets_live_variables Current set of live variable offsets.
    @param[out]     used_sp_functions     Set of stored function instances used
    in this instruction.
    @param[out]     err_reason            Populated with the error reason if an
    error occurs.

    @return false if the computation is successful; true otherwise.
  */
  virtual bool compute_is_redundant_and_collect_functions(
      THD *thd, std::unordered_set<uint> &offsets_live_variables,
      std::unordered_set<sp_head *> &used_sp_functions,
      std::string &err_reason) = 0;

  /**
    Processes an sp_inline_instr that is validated and non-redundant. This
    method either updates the current mapping of local variables to their values
    or returns the final result through the result argument.

    @param[in]      thd                  Thread context.
    @param[in,out]  map_var_offset_to_value  Current mapping of local variables
    to their values.
    @param[in]      sp_args              Input arguments of the stored function.
    @param[in]      sp_arg_count         Number of arguments for the stored
    function.
    @param[in]      sp_head              Stored function instance.
    @param[in]      sp_name_resolution_ctx Name resolution context for the
    stored function.
    @param[out]     result_item          Result of a processed return
    instruction, or nullptr if not applicable.

    @return false if success, true if error
  */
  virtual bool process(
      THD *thd, std::unordered_map<uint, Item *> &map_var_offset_to_value,
      Item **sp_args, uint sp_arg_count, sp_head *sp_head,
      Name_resolution_context *sp_name_resolution_ctx, Item **result_item) = 0;

 protected:
  /**
    Traverses the item tree and collects variable offsets for any encountered
    Item::ROUTINE_FIELD_ITEM instances. Additionally, collects instances of
    stored functions identified during the traversal.

    @param[in] thd Thread context
    @param[in] item Item to traverse
    @param[out] var_offsets Set of variable offsets found within the item
    @param[out] used_sp_functions  Set of stored function instances found in the
    item
    @param[out] err_reason Populated with the error reason if an error occurs

     @return false if success, true if error
  */
  bool walk_and_collect_functions_and_variables(
      THD *thd, Item *item, std::unordered_set<uint> &var_offsets,
      std::unordered_set<sp_head *> &used_sp_functions,
      std::string &err_reason);

  /**
    Collects variable offsets for all encountered Item::ROUTINE_FIELD_ITEM
    instances across the various components of the query block. Also gathers the
    set of stored functions invoked directly within this query block.

    @param[in] thd Thread context
    @param[in] qb Query block
    @param[out] variable_offsets Set of variable offsets used in the query block
    @param[out] used_sp_functions Set of stored functions called directly within
    this query block
    @param[out] err_reason Populated with the error reason if an error occurs

    @return false if success, true if error
  */
  bool collect_functions_and_variables_from_query_block(
      THD *thd, Query_block *qb, std::unordered_set<uint> &variable_offsets,
      std::unordered_set<sp_head *> &used_sp_functions,
      std::string &err_reason);

  /**
    Creates a deep copy of the input expression and then in-place replaces the
    references to local variables with their values.

    @param[in] thd Thread context
    @param[in] expr_item The original item tree
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function

    @return A copy of expr_item with inlined variable contents
  */
  Item *parse_and_inline_expression(
      THD *thd, Item *expr_item,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count);

  /**
    Transform sp_inline_instr into a subquery with inlined references to local
    variables

    @param[in] thd Thread context
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] sp_head Instance of the stored function
    @param[in] sp_name_resolution_ctx Name resolution context of the stored
    function
  */
  Item_singlerow_subselect *create_and_inline_subquery(
      THD *thd, std::unordered_map<uint, Item *> &map_var_offset_to_value,
      Item **sp_args, uint sp_arg_count, sp_head *sp_head,
      Name_resolution_context *sp_name_resolution_ctx);

  /**
    The original sp_instr which this class is meant to inline.
  */
  sp_instr *m_sp_instr;

 private:
  /**
    Indicates whether this sp_inline_instr instance is required to compute the
    RETURN result of the stored function. Computed during
    compute_is_redundant_and_collect_functions.
  */
  bool m_is_redundant_instr;

  /**
    Indicates whether this sp_inline_instr instance has passed the checks in
    validate.
  */
  bool m_is_validated_instr;

  /**
    Transforms the input item by replacing the references to local variables
    with their values. If the value is a FIELD_ITEM, m_was_sp_local_variable is
    set to true.

    @param[in,out] item Item to transform
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function
  */
  void inline_local_variables(
      Item **item, std::unordered_map<uint, Item *> &map_var_offset_to_value,
      Item **sp_args, uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in the select list of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function
  */
  void inline_select_list(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in the join conditions of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function

    @returns false if success, true if error
  */
  bool inline_join_conditions(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in the where condition of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function
  */
  void inline_where_cond(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in the group by clause of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function
  */
  void inline_group_by(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in window functions of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function
  */
  void inline_window_functions(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in the order by clause of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
    @param[in] context Name resolution context of the stored function
  */
  void inline_order_by(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count, Name_resolution_context *context);

  /**
    Inline local variables used in limit and offset of the query block

    @param[in,out] qb Query block
    @param[in] map_var_offset_to_value Map of variable offsets to their current
    values
    @param[in] sp_args Input arguments of the stored function
    @param[in] sp_arg_count Number of arguments of the stored function
  */
  void inline_limit_offset(
      Query_block *qb,
      std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
      uint sp_arg_count);
};

///////////////////////////////////////////////////////////////////////////

/**
  The sp_inline_instr_set class provides the base implementation for inlining
  SET instructions (sp_instr_set) within stored functions. Currently, inlining
  is supported for SET instructions that assign local variables to an expression
  or a subquery. For additional details about the SET instruction, refer to the
  sp_instr_set class.
*/
class sp_inline_instr_set : public sp_inline_instr {
 public:
  sp_inline_instr_set(sp_instr *sp_instr) : sp_inline_instr(sp_instr) {}

  /////////////////////////////////////////////////////////////////////////
  // sp_inline_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  bool validate(std::string &err_reason) override {
    sp_instr_set *set_instr = down_cast<sp_instr_set *>(m_sp_instr);
    Item *set_item = set_instr->get_value_item();
    if (set_item->type() != Item::SUBQUERY_ITEM && set_item->has_subquery()) {
      err_reason.append(
          "Subqueries in SET statements are not supported as expression "
          "arguments. Consider splitting into multiple statements using local "
          "variables.");
      return true;
    }
    set_is_validated_instr();
    return false;
  }

  bool compute_is_redundant_and_collect_functions(
      THD *thd, std::unordered_set<uint> &offsets_live_variables,
      std::unordered_set<sp_head *> &used_sp_functions,
      std::string &err_reason) override {
    sp_instr_set *set_instr = down_cast<sp_instr_set *>(m_sp_instr);
    uint set_var_offset = set_instr->get_offset();
    if (offsets_live_variables.find(set_var_offset) ==
        offsets_live_variables.end()) {
      set_is_redundant_instr();
      return false;
    }
    offsets_live_variables.erase(set_var_offset);
    Item *set_value = set_instr->get_value_item();

    if (set_value->has_subquery()) {
      const LEX *lex = set_instr->get_lex();
      for (Query_block *qb = lex->all_query_blocks_list; qb != nullptr;
           qb = qb->next_select_in_list()) {
        if (collect_functions_and_variables_from_query_block(
                thd, qb, offsets_live_variables, used_sp_functions,
                err_reason)) {
          return true;
        }
      }
    } else {
      if (walk_and_collect_functions_and_variables(
              thd, set_value, offsets_live_variables, used_sp_functions,
              err_reason)) {
        return true;
      }
    }
    return false;
  }

  bool process(THD *thd,
               std::unordered_map<uint, Item *> &map_var_offset_to_value,
               Item **sp_args, uint sp_arg_count, sp_head *sp_head,
               Name_resolution_context *sp_name_resolution_ctx,
               Item **result_item [[maybe_unused]]) override {
    assert(is_redundant_instr() == false);
    assert(is_validated_instr() == true);
    sp_instr_set *set_instr = down_cast<sp_instr_set *>(m_sp_instr);
    Item *set_item = set_instr->get_value_item();
    if (set_item->type() == Item::SUBQUERY_ITEM) {
      set_item = create_and_inline_subquery(thd, map_var_offset_to_value,
                                            sp_args, sp_arg_count, sp_head,
                                            sp_name_resolution_ctx);
    } else {
      set_item = parse_and_inline_expression(
          thd, set_item, map_var_offset_to_value, sp_args, sp_arg_count);
    }
    if (set_item == nullptr) {
      std::string err_reason{};
      record_instruction_inlining_error(err_reason, get_ip());
      report_stored_function_inlining_error(thd, sp_head->m_qname.str,
                                            err_reason);
      return true;
    }
    map_var_offset_to_value[set_instr->get_offset()] = set_item;

    return false;
  }
};

///////////////////////////////////////////////////////////////////////////

/**
  The sp_inline_instr_stmt class provides the base implementation for inlining
  statement instructions (sp_instr_stmt) within stored functions. Currently,
  inlining is supported for SELECT INTO statements that insert into a single
  local variable. For further details about statement instructions, refer to the
  sp_instr_stmt class.
*/
class sp_inline_instr_stmt : public sp_inline_instr {
 public:
  sp_inline_instr_stmt(sp_instr *sp_instr) : sp_inline_instr(sp_instr) {}

  /////////////////////////////////////////////////////////////////////////
  // sp_inline_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  bool validate(std::string &err_reason) override {
    sp_lex_instr *lex_instr = down_cast<sp_lex_instr *>(m_sp_instr);
    const LEX *lex = lex_instr->get_lex();
    if (lex->sql_command != SQLCOM_SELECT) {
      return true;
    }
    if (lex->result) {
      /* This is a SELECT INTO statement, as expected */
      Query_dumpvar *qd = down_cast<Query_dumpvar *>(lex->result);
      if (qd == nullptr) {
        return true;
      } else if (qd->var_list.size() != 1) {
        err_reason.append(
            "Stored functions in secondary engine support SELECT into "
            "only one variable.");
        return true;
      }
    } else {
      return true;
    }
    set_is_validated_instr();
    return false;
  }

  bool compute_is_redundant_and_collect_functions(
      THD *thd, std::unordered_set<uint> &offsets_live_variables,
      std::unordered_set<sp_head *> &used_sp_functions,
      std::string &err_reason) override {
    uint into_var_offset;
    if (get_stmt_result_variable(&into_var_offset, err_reason)) {
      return true;
    }
    if (offsets_live_variables.find(into_var_offset) ==
        offsets_live_variables.end()) {
      set_is_redundant_instr();
      return false;
    }
    offsets_live_variables.erase(into_var_offset);
    sp_lex_instr *lex_instr = down_cast<sp_lex_instr *>(m_sp_instr);
    for (Query_block *qb = lex_instr->get_lex()->all_query_blocks_list;
         qb != nullptr; qb = qb->next_select_in_list()) {
      if (collect_functions_and_variables_from_query_block(
              thd, qb, offsets_live_variables, used_sp_functions, err_reason)) {
        return true;
      }
    }
    return false;
  }

  bool process(THD *thd,
               std::unordered_map<uint, Item *> &map_var_offset_to_value,
               Item **sp_args, uint sp_arg_count, sp_head *sp_head,
               Name_resolution_context *sp_name_resolution_ctx,
               Item **result_item [[maybe_unused]]) override {
    assert(is_redundant_instr() == false);
    assert(is_validated_instr() == true);
    Item_singlerow_subselect *subquery = create_and_inline_subquery(
        thd, map_var_offset_to_value, sp_args, sp_arg_count, sp_head,
        sp_name_resolution_ctx);
    std::string err_reason{};
    if (subquery == nullptr) {
      record_instruction_inlining_error(err_reason, get_ip());
      report_stored_function_inlining_error(thd, sp_head->m_qname.str,
                                            err_reason);
      return true;
    }
    uint offset;
    if (get_stmt_result_variable(&offset, err_reason)) {
      report_stored_function_inlining_error(thd, sp_head->m_qname.str,
                                            err_reason);
      return true;
    }
    /* Map the INTO variable to the newly created subquery */
    map_var_offset_to_value[offset] = subquery;
    return false;
  }

  bool check_redundant_instr_for_errors(THD *thd,
                                        std::string &err_reason) override {
    assert(is_redundant_instr());

    auto *lex_instr = down_cast<sp_lex_instr *>(m_sp_instr);
    if (const LEX *lex = lex_instr->get_lex(); lex == nullptr) {
      return false;
    }

    String sql_query;
    auto *stmt_instr = down_cast<sp_instr_stmt *>(lex_instr);
    stmt_instr->get_query(&sql_query);

    LEX *orig_lex = thd->lex;
    thd->lex = new (thd->mem_root) st_lex_local;
    lex_start(thd);

    auto guard = create_scope_guard([thd, orig_lex] {
      lex_end(thd->lex);
      thd->lex->set_secondary_engine_execution_context(nullptr);
      std::destroy_at(thd->lex);
      thd->lex = orig_lex;
    });

    Parser_state parser_state;
    if (parser_state.init(thd, sql_query.ptr(), sql_query.length())) {
      return true;
    }

    thd->lex->set_sp_current_parsing_ctx(lex_instr->get_parsing_ctx());
    parse_sql(thd, &parser_state, nullptr);

    if (thd->lex->query_tables != nullptr &&
        open_tables_for_query(thd, thd->lex->query_tables, 0)) {
      if (thd->is_error()) {
        err_reason.append(thd->get_stmt_da()->message_text());
      }
      return true;
    }

    check_table_access(thd, SELECT_ACL, thd->lex->query_tables, false, UINT_MAX,
                       false);

    if (thd->is_error() ||
        thd->lex->unit->first_query_block()->prepare(thd, nullptr)) {
      return true;
    }

    return false;
  }

 private:
  /**
    Retrieves the variable offset for the result variable of the current
    statement (i.e., the variable being selected into).

    @param[out] var_offset Offset of the result variable
    @param[out] err_reason Populated with the error reason if an error occurs

    @return false if success, true if error
   */
  bool get_stmt_result_variable(uint *var_offset, std::string &err_reason) {
    sp_lex_instr *lex_instr = down_cast<sp_lex_instr *>(m_sp_instr);
    const LEX *lex = lex_instr->get_lex();
    Query_dumpvar *qd = down_cast<Query_dumpvar *>(lex->result);
    PT_select_var *v = qd->var_list[0];
    const LEX_STRING var_name = v->name;
    sp_pcontext *pctx = m_sp_instr->get_parsing_ctx();
    sp_variable *spv =
        pctx->find_variable(var_name.str, var_name.length, false);
    if (spv == nullptr) {
      err_reason.append(
          "Only local variables are supported in inlined stored functions. "
          "Variable not found: ");
      err_reason.append(var_name.str);
      return true;
    }
    *var_offset = spv->offset;
    return false;
  }
};

///////////////////////////////////////////////////////////////////////////

/**
  The sp_inline_instr_freturn class provides the base implementation for
  inlining RETURN instructions (sp_instr_freturn) in stored functions.
  Currently, inlining is supported for RETURN instructions that return an
  expression. For further details about return instructions, refer to the
  sp_instr_freturn class.
*/
class sp_inline_instr_freturn : public sp_inline_instr {
 public:
  sp_inline_instr_freturn(sp_instr *sp_instr, sp_head *sp_head)
      : sp_inline_instr(sp_instr),
        m_return_field_def(&sp_head->m_return_field_def) {}

  /////////////////////////////////////////////////////////////////////////
  // sp_inline_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  bool validate(std::string &err_reason) override {
    sp_instr_freturn *return_instr = down_cast<sp_instr_freturn *>(m_sp_instr);
    Item *expr_item = return_instr->get_expr_item();
    if (expr_item->has_subquery()) {
      err_reason.append(
          "Subqueries in RETURN instruction not supported. Consider "
          "splitting into multiple instructions.");
      return true;
    }
    set_is_validated_instr();
    return false;
  }

  bool compute_is_redundant_and_collect_functions(
      THD *thd, std::unordered_set<uint> &offsets_live_variables,
      std::unordered_set<sp_head *> &used_sp_functions,
      std::string &err_reason) override {
    sp_instr_freturn *return_instr = down_cast<sp_instr_freturn *>(m_sp_instr);
    if (walk_and_collect_functions_and_variables(
            thd, return_instr->get_expr_item(), offsets_live_variables,
            used_sp_functions, err_reason))
      return true;
    return false;
  }

  bool process(THD *thd,
               std::unordered_map<uint, Item *> &map_var_offset_to_value,
               Item **sp_args, uint sp_arg_count,
               sp_head *sp_head [[maybe_unused]],
               Name_resolution_context *sp_name_resolution_ctx [[maybe_unused]],
               Item **result_item) override {
    assert(is_redundant_instr() == false);
    assert(is_validated_instr() == true);
    sp_instr_freturn *return_instruction =
        down_cast<sp_instr_freturn *>(m_sp_instr);
    Item *expr_item = return_instruction->get_expr_item();

    Item *ret = parse_and_inline_expression(
        thd, expr_item, map_var_offset_to_value, sp_args, sp_arg_count);
    if (ret == nullptr) {
      std::string err_reason{};
      record_instruction_inlining_error(err_reason, get_ip());
      report_stored_function_inlining_error(thd, sp_head->m_qname.str,
                                            err_reason);
      return true;
    }
    if (ret->has_subquery()) {
      *result_item = ret;
      return false;
    }
    Item *ret_converted = nullptr;

    enum_field_types return_type = m_return_field_def->sql_type;
    if (ret->data_type() != m_return_field_def->sql_type) {
      if (ret->basic_const_item() && ret->type() != Item::FUNC_ITEM) {
        /* Convert return charset to the one required by the return field. This
         * is done only for simple constants. */

        if (return_type == MYSQL_TYPE_VAR_STRING ||
            return_type == MYSQL_TYPE_STRING || return_type == MYSQL_TYPE_SET ||
            return_type == MYSQL_TYPE_ENUM) {
          ret_converted =
              ret->convert_charset(thd, m_return_field_def->charset);
        }
      }
      if (ret_converted == nullptr) {
        switch (return_type) {
          case MYSQL_TYPE_DATETIME:
          case MYSQL_TYPE_DATE:
          case MYSQL_TYPE_TIME:
          case MYSQL_TYPE_TIME2:
          case MYSQL_TYPE_DOUBLE:
            if (wrap_in_cast(&ret, return_type, /*fix_new_item*/ false)) {
              return true;
            }
            break;

          case MYSQL_TYPE_NEWDECIMAL:
          case MYSQL_TYPE_DECIMAL:
            if (wrap_in_decimal_cast(
                    &ret, m_return_field_def->max_display_width_in_codepoints(),
                    m_return_field_def->decimals, /*fix_new_item*/ false)) {
              return true;
            }
            break;

          case MYSQL_TYPE_LONG:
          case MYSQL_TYPE_LONGLONG:
          case MYSQL_TYPE_INT24:
          case MYSQL_TYPE_SHORT:
          case MYSQL_TYPE_TINY:
            if (wrap_in_int_cast(&ret, m_return_field_def->is_unsigned,
                                 /*fix_new_item*/ false)) {
              return true;
            }
            break;

          default:
            // no special handling
            break;
        }
      }
    }

    if (ret_converted != nullptr) {
      *result_item = ret_converted;
    } else {
      *result_item = ret;
    }
    return false;
  }

 private:
  /* Definition of the RETURN-field for the stored function */
  Create_field *m_return_field_def{nullptr};
};

static Item *find_variable_from_offset_inner(
    uint offset, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **args, uint arg_count) {
  if (map_var_offset_to_value.find(offset) != map_var_offset_to_value.end()) {
    return map_var_offset_to_value[offset];
  }
  if (arg_count > 0 && offset < arg_count) {
    return args[offset];
  }
  return nullptr;
}

static Item *find_variable_from_offset(
    Item *var_item, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **args, uint arg_count) {
  assert(var_item->type() == Item::ROUTINE_FIELD_ITEM);
  Item_splocal *local_var = down_cast<Item_splocal *>(var_item);
  return find_variable_from_offset_inner(
      local_var->get_var_idx(), map_var_offset_to_value, args, arg_count);
}

///////////////////////////////////////////////////////////////////////////
// sp_inline_instr implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_inline_instr::walk_and_collect_functions_and_variables(
    THD *thd, Item *item, std::unordered_set<uint> &var_offsets,
    std::unordered_set<sp_head *> &used_sp_functions, std::string &err_reason) {
  bool error_detected = false;
  WalkItem(item, enum_walk::SUBQUERY_PREFIX,
           [&var_offsets, &used_sp_functions, thd, &err_reason,
            &error_detected](Item *sub_item) {
             if (sub_item->type() == Item::FUNC_ITEM) {
               auto *func_item = down_cast<Item_func *>(sub_item);
               if (func_item->functype() == Item_func::FUNC_SP) {
                 auto *sp_func_item = down_cast<Item_func_sp *>(func_item);
                 sp_head *sp = sp_func_item->get_sp();
                 if (sp == nullptr) {
                   sp = sp_find_routine(thd, enum_sp_type::FUNCTION,
                                        sp_func_item->get_name(),
                                        &thd->sp_func_cache, true);
                   if (sp == nullptr) {
                     error_detected = true;
                     err_reason.append(sp_func_item->get_name()->m_qname.str)
                         .append(" does not exist.");
                     return true;
                   }
                 }
                 used_sp_functions.insert(sp);
               }
             } else if (sub_item->type() == Item::ROUTINE_FIELD_ITEM) {
               Item_splocal *local_var = down_cast<Item_splocal *>(sub_item);
               var_offsets.insert(local_var->get_var_idx());
             }
             return false;
           });
  return error_detected;
}

bool sp_inline_instr::collect_functions_and_variables_from_query_block(
    THD *thd, Query_block *qb, std::unordered_set<uint> &variable_offsets,
    std::unordered_set<sp_head *> &used_sp_functions, std::string &err_reason) {
  for (Item *item : qb->fields) {
    if (walk_and_collect_functions_and_variables(thd, item, variable_offsets,
                                                 used_sp_functions, err_reason))
      return true;
  }

  if (qb->where_cond() != nullptr) {
    if (walk_and_collect_functions_and_variables(thd, qb->where_cond(),
                                                 variable_offsets,
                                                 used_sp_functions, err_reason))
      return true;
  }

  Item::Collect_scalar_subquery_info subqueries;
  walk_join_conditions(
      qb->m_table_nest,
      [&](Item **expr_p) mutable -> bool {
        if (*expr_p != nullptr) {
          if (walk_and_collect_functions_and_variables(
                  thd, *expr_p, variable_offsets, used_sp_functions,
                  err_reason))
            return true;
        }
        return false;
      },
      &subqueries);

  if (qb->offset_limit != nullptr) {
    if (walk_and_collect_functions_and_variables(thd, qb->offset_limit,
                                                 variable_offsets,
                                                 used_sp_functions, err_reason))
      return true;
  }

  if (qb->select_limit != nullptr) {
    if (walk_and_collect_functions_and_variables(thd, qb->select_limit,
                                                 variable_offsets,
                                                 used_sp_functions, err_reason))
      return true;
  }

  if (qb->order_list.elements > 0) {
    for (ORDER *order = qb->order_list.first; order; order = order->next) {
      Item **order_item = order->item;
      if (walk_and_collect_functions_and_variables(
              thd, *order_item, variable_offsets, used_sp_functions,
              err_reason))
        return true;
    }
  }

  if (qb->group_list_size() > 0) {
    for (ORDER *grp = qb->group_list.first; grp; grp = grp->next) {
      if (walk_and_collect_functions_and_variables(
              thd, *grp->item, variable_offsets, used_sp_functions, err_reason))
        return true;
    }
  }

  if (qb->has_windows()) {
    uint32_t num_windows = qb->m_windows.elements;
    for (uint32_t idx = 0; idx < num_windows; ++idx) {
      auto *win = qb->m_windows[idx];

      const PT_order_list *order_by = win->effective_order_by();
      if (order_by != nullptr) {
        for (ORDER *o = order_by->value.first; o != nullptr; o = o->next) {
          if (walk_and_collect_functions_and_variables(
                  thd, *o->item, variable_offsets, used_sp_functions,
                  err_reason))
            return true;
        }
      }
      const PT_order_list *partition_by = win->effective_partition_by();
      if (partition_by != nullptr) {
        for (ORDER *p = partition_by->value.first; p != nullptr; p = p->next) {
          if (walk_and_collect_functions_and_variables(
                  thd, *p->item, variable_offsets, used_sp_functions,
                  err_reason))
            return true;
        }
      }
    }
  }
  return false;
}

Item *sp_inline_instr::parse_and_inline_expression(
    THD *thd, Item *expr_item,
    std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
    uint sp_arg_count) {
  assert(m_sp_instr->type() == sp_instr::Instr_type::INSTR_LEX_SET ||
         m_sp_instr->type() == sp_instr::Instr_type::INSTR_LEX_FRETURN);
  sp_lex_instr *lex_instr = down_cast<sp_lex_instr *>(m_sp_instr);
  /* We need a deep copy of the expr_item; the only way to currently do it
   * is to reparse the Item (mechanism introduced in WL#13730). */
  Item *expr_item_copy = parse_expression(
      thd, expr_item, lex_instr->get_lex()->current_query_block(), nullptr);
  if (expr_item_copy == nullptr) {
    return nullptr;
  }
  /* Analyze expressions which can contain local variables to inline */
  if (expr_item->type() == Item::FUNC_ITEM ||
      expr_item->type() == Item::COND_ITEM) {
    expr_item_copy =
        TransformItem(expr_item_copy, [&](Item *sub_item) -> Item * {
          if (sub_item->type() == Item::FIELD_ITEM) {
            /* The parser assumes function arguments and local variables are
             * just fields. This is not possible in a return statement. We need
             * to map field items back to variables. */
            sp_pcontext *pctx = m_sp_instr->get_parsing_ctx();
            sp_variable *spv = pctx->find_variable(
                sub_item->item_name.ptr(), sub_item->item_name.length(), false);
            if (spv != nullptr) {
              Item *var = find_variable_from_offset_inner(
                  spv->offset, map_var_offset_to_value, sp_args, sp_arg_count);
              if (var != nullptr) {
                return var;
              }
            }
          }
          /* Covers error cases as well as skipping non-fields */
          return sub_item;
        });
    return expr_item_copy;
  } else if (expr_item->type() == Item::ROUTINE_FIELD_ITEM) {
    /* e.g. the function just returns one of the input args */
    return find_variable_from_offset(expr_item, map_var_offset_to_value,
                                     sp_args, sp_arg_count);
  }
  return expr_item_copy;
}

void sp_inline_instr::inline_local_variables(
    Item **item, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, Name_resolution_context *context) {
  *item = TransformItem(*item, [&](Item *sub_item) -> Item * {
    if (sub_item->type() == Item::ROUTINE_FIELD_ITEM) {
      Item *replace_item = find_variable_from_offset(
          sub_item, map_var_offset_to_value, sp_args, sp_arg_count);
      if (replace_item == nullptr) {
        /* Error will be reported later in the resolver */
        return sub_item;
      }
      if (replace_item->type() == Item::FIELD_ITEM) {
        Item_field *field_item = down_cast<Item_field *>(replace_item);
        if (context != nullptr) {
          field_item->context = context;
          field_item->set_item_was_sp_local_variable();
        }
        return field_item;
      } else {
        return replace_item;
      }
    }
    return sub_item;
  });
}

void sp_inline_instr::inline_select_list(
    Query_block *qb, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, Name_resolution_context *context) {
  for (uint i = 0; i < qb->fields.size(); i++) {
    inline_local_variables(&qb->fields[i], map_var_offset_to_value, sp_args,
                           sp_arg_count, context);
  }
}

bool sp_inline_instr::inline_join_conditions(
    Query_block *qb, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, Name_resolution_context *context) {
  Item::Collect_scalar_subquery_info subqueries;
  if (walk_join_conditions(
          qb->m_table_nest,
          [&](Item **expr_p) mutable -> bool {
            if (*expr_p != nullptr) {
              inline_local_variables(expr_p, map_var_offset_to_value, sp_args,
                                     sp_arg_count, context);
            }
            return false;
          },
          &subqueries))
    return true;
  return false;
}

void sp_inline_instr::inline_where_cond(
    Query_block *qb, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, Name_resolution_context *context) {
  if (qb->where_cond() != nullptr) {
    inline_local_variables(qb->where_cond_ref(), map_var_offset_to_value,
                           sp_args, sp_arg_count, context);
  }
}

void sp_inline_instr::inline_group_by(
    Query_block *qb, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, Name_resolution_context *context) {
  if (qb->group_list.elements == 0) return;

  ORDER *prev = nullptr;
  ORDER *grp = qb->group_list.first;

  while (grp) {
    ORDER *next = grp->next;
    Item *grp_item = grp->item[0];
    /*
      Skip constants to avoid interpreting them as fields.
      At this stage (prior to expression resolution), const_for_execution()
      generally cannot be used; however, for Item_sp_local items, it is the only
      available method.
    */
    if (grp_item->const_for_execution() &&
        grp_item->type() == Item::ROUTINE_FIELD_ITEM) {
      if (prev) {
        prev->next = next;
      } else {
        qb->group_list.first = next;
      }
      qb->group_list.elements--;
      grp = next;
      continue;
    }
    inline_local_variables(grp->item, map_var_offset_to_value, sp_args,
                           sp_arg_count, context);
    prev = grp;
    grp = next;
  }
}

void sp_inline_instr::inline_window_functions(
    Query_block *query_block,
    std::unordered_map<uint, Item *> &map_var_offset_to_value, Item **sp_args,
    uint sp_arg_count, Name_resolution_context *context) {
  List_iterator<Window> li(query_block->m_windows);
  for (Window *w = li++; w != nullptr; w = li++) {
    for (auto it : {w->first_partition_by(), w->first_order_by()}) {
      if (it != nullptr) {
        for (ORDER *o = it; o != nullptr; o = o->next) {
          inline_local_variables(o->item, map_var_offset_to_value, sp_args,
                                 sp_arg_count, context);
        }
      }
    }
  }
}

void sp_inline_instr::inline_order_by(
    Query_block *qb, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, Name_resolution_context *context) {
  if (qb->order_list.elements == 0) return;

  ORDER *prev = nullptr;
  ORDER *order = qb->order_list.first;

  while (order) {
    ORDER *next = order->next;
    Item *order_item = order->item[0];
    /*
      Skip constants to avoid interpreting them as fields.
      At this stage (prior to expression resolution), const_for_execution()
      generally cannot be used; however, for Item_sp_local items, it is the only
      available method.
    */
    if (order_item->const_for_execution() &&
        order_item->type() == Item::ROUTINE_FIELD_ITEM) {
      if (prev) {
        prev->next = next;
      } else {
        qb->order_list.first = next;
      }
      qb->order_list.elements--;
      order = next;
      continue;
    }
    inline_local_variables(order->item, map_var_offset_to_value, sp_args,
                           sp_arg_count, context);
    prev = order;
    order = next;
  }
}

void sp_inline_instr::inline_limit_offset(
    Query_block *qb, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count) {
  if (qb->offset_limit != nullptr) {
    inline_local_variables(&qb->offset_limit, map_var_offset_to_value, sp_args,
                           sp_arg_count, nullptr);
  }

  if (qb->select_limit != nullptr) {
    inline_local_variables(&qb->select_limit, map_var_offset_to_value, sp_args,
                           sp_arg_count, nullptr);
  }
}

Item_singlerow_subselect *sp_inline_instr::create_and_inline_subquery(
    THD *thd, std::unordered_map<uint, Item *> &map_var_offset_to_value,
    Item **sp_args, uint sp_arg_count, sp_head *sp_head,
    Name_resolution_context *sp_name_resolution_ctx) {
  sp_lex_instr *lex_instr = down_cast<sp_lex_instr *>(m_sp_instr);

  String sql_query;

  switch (lex_instr->type()) {
    case sp_instr::Instr_type::INSTR_LEX_STMT: {
      sp_instr_stmt *stmt_instr = down_cast<sp_instr_stmt *>(lex_instr);
      stmt_instr->get_query(&sql_query);
      break;
    }
    case sp_instr::Instr_type::INSTR_LEX_SET: {
      sp_instr_set *set_instr = down_cast<sp_instr_set *>(lex_instr);
      if (set_instr->get_expr_query().length > 0) {
        if (sql_query.append(to_lex_string(set_instr->get_expr_query()))) {
          return nullptr;
        }
      } else {
        return nullptr;
      }
      break;
    }
    default:
      assert(false);
      return nullptr;
  }

  assert(sql_query.length() > 0);

  Parser_state parser_state;
  if (parser_state.init(thd, sql_query.ptr(), sql_query.length())) {
    return nullptr;
  }
  LEX *lex_orig = thd->lex;

  thd->lex = new (thd->mem_root) st_lex_local;
  lex_start(thd);

  auto guard = create_scope_guard([thd, lex_orig] {
    lex_end(thd->lex);
    thd->lex->set_secondary_engine_execution_context(nullptr);
    std::destroy_at(thd->lex);
    thd->lex = lex_orig;
  });

  /*
    Setting this context ensures that variables are represented as item type
    ROUTINE_FIELD_ITEM rather than FIELD_ITEM.
  */
  thd->lex->set_sp_current_parsing_ctx(lex_instr->get_parsing_ctx());
  /* Required to find local variables */
  thd->lex->sphead = sp_head;
  parse_sql(thd, &parser_state, nullptr);
  thd->lex->set_sp_current_parsing_ctx(nullptr);
  thd->lex->sphead = nullptr;
  Query_block *query_block_new_lex = thd->lex->current_query_block();
  if (query_block_new_lex == nullptr) {
    assert(false);
    return nullptr;
  }

  inline_select_list(query_block_new_lex, map_var_offset_to_value, sp_args,
                     sp_arg_count, &query_block_new_lex->context);
  if (inline_join_conditions(query_block_new_lex, map_var_offset_to_value,
                             sp_args, sp_arg_count,
                             &query_block_new_lex->context)) {
    return nullptr;
  }
  inline_where_cond(query_block_new_lex, map_var_offset_to_value, sp_args,
                    sp_arg_count, &query_block_new_lex->context);
  inline_group_by(query_block_new_lex, map_var_offset_to_value, sp_args,
                  sp_arg_count, &query_block_new_lex->context);
  inline_window_functions(query_block_new_lex, map_var_offset_to_value, sp_args,
                          sp_arg_count, &query_block_new_lex->context);
  inline_order_by(query_block_new_lex, map_var_offset_to_value, sp_args,
                  sp_arg_count, &query_block_new_lex->context);
  inline_limit_offset(query_block_new_lex, map_var_offset_to_value, sp_args,
                      sp_arg_count);

  Query_expression *qe = query_block_new_lex->master_query_expression();
  if (qe == nullptr) {
    return nullptr;
  }
  qe->include_down(lex_orig, lex_orig->current_query_block());
  query_block_new_lex->parent_lex = lex_orig;
  query_block_new_lex->include_in_global(&lex_orig->all_query_blocks_list);
  query_block_new_lex->context.outer_context =
      &lex_orig->current_query_block()->context;

  if (thd->lex->query_tables != nullptr) {
    /*
      Open tables for the new query and resolve privileges:
      If the original is a VIEW and the new query expands to base tables,
      reuse the privileges already checked via the original view.
      In all other cases, explicitly re-check table access.
    */
    open_tables_for_query(thd, thd->lex->query_tables, 0);

    if (lex_orig->query_tables->is_view() &&
        !thd->lex->query_tables->is_view() &&
        sp_name_resolution_ctx->first_name_resolution_table != nullptr) {
      thd->lex->query_tables->grant =
          sp_name_resolution_ctx->first_name_resolution_table->grant;
    } else {
      check_table_access(thd, SELECT_ACL, thd->lex->query_tables, false,
                         UINT_MAX, false);
    }
  }

  if (thd->is_error()) {
    return nullptr;
  }

  Item_singlerow_subselect *subquery =
      new Item_singlerow_subselect(query_block_new_lex);

  if (thd->is_error()) {
    return nullptr;
  }
  return subquery;
}

void report_stored_function_inlining_error(THD *thd, const char *func_name,
                                           std::string &err_reason) {
  std::string err_msg{};
  err_msg = "Stored function not supported for inlining";
  if (func_name != nullptr) {
    err_msg.append(" [");
    err_msg.append(func_name);
    err_msg.append("]");
  }
  err_msg.append(". ");
  err_msg.append(err_reason);
  set_fail_reason_and_raise_error(thd->lex, std::string_view{err_msg});
}

///////////////////////////////////////////////////////////////////////////

/**
  Main functions for stored function inlining
*/

bool needs_stored_function_inlining(THD *thd) {
  return thd->lex->m_sql_cmd != nullptr && thd->lex->has_stored_functions &&
         thd->lex->m_sql_cmd->using_secondary_storage_engine();
}

bool can_inline_stored_function(THD *thd, sp_head *sp, uint sp_arg_count) {
  assert(sp != nullptr);

  std::string err_reason{};

  /* Only SQL stored functions can be inlined */
  if (!sp->is_sql()) {
    err_reason.append(
        "Only SQL stored functions are supported for inlining in "
        "secondary engine.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    SECURITY DEFINER functions run with privileges of the user that created the
    stored function. Inlining such functions could unintentionally bypass
    security mechanisms or privilege boundaries.
  */
  if (sp->m_chistics->suid != SP_IS_NOT_SUID) {
    err_reason.append(
        "SECURITY DEFINER stored functions are not supported for inlining in "
        "secondary engine. Redefine the function as INVOKER.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    Inlining of stored functions is not supported inside prepared statements
    due to lifecycle and context mismatches.
  */
  if (!thd->stmt_arena->is_regular() && !thd->lex->m_sql_cmd->is_part_of_sp()) {
    err_reason.append(
        "Stored functions in prepared statements are not supported in "
        "secondary engine.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    Disallow inlining if the function modifies data (DML). Such functions may
    introduce transactional or consistency challenges in the secondary engine.
  */
  if (sp->modifies_data()) {
    err_reason.append(
        "Stored functions that modify data are not supported in secondary "
        "engine.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    SQL mode differences between function creation and session may change
    semantics. To avoid behavioral inconsistencies, inlining is restricted.
  */
  if (sp->m_sql_mode != thd->variables.sql_mode) {
    err_reason.append(
        "Stored functions that have a different sql_mode than the session are "
        "not supported in secondary engine");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    Character set mismatches between function and session could result in
    interpretation errors, so inlining is not permitted in this case.
  */
  if (sp->get_creation_ctx()->get_client_cs() !=
      thd->variables.character_set_client) {
    err_reason.append(
        "Stored functions that have a different character set than the session "
        "are not supported in secondary engine.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    Connection collation mismatches between function and session can similarly
    cause semantic problems when inlining, so this is disallowed.
  */
  if (sp->get_creation_ctx()->get_connection_cl() !=
      thd->variables.collation_connection) {
    err_reason.append(
        "Stored functions that have a different Connection Collation than the "
        "session are not supported in secondary engine.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    If the function creates or drops temp tables, resource management and
    scoping become complex and it is currently restricted for inlining in
    secondary engine.
  */
  if (sp->has_temp_table_ddl()) {
    err_reason.append(
        "Stored functions that create or drop temporary tables are not "
        "supported in secondary engine.");
    report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
    return false;
  }

  /*
    Inlining functions with ENUM or SET arguments is unsupported; these types
    may not be interpreted consistently by the secondary engine.
  */
  if (sp_arg_count > 0) {
    for (uint i = 0; i < sp_arg_count; i++) {
      sp_pcontext *pctx = sp->get_root_parsing_context();
      sp_variable *var = pctx->find_variable(i);
      if (var != nullptr) {
        if (var->type == MYSQL_TYPE_ENUM || var->type == MYSQL_TYPE_SET) {
          err_reason.append(
              "Stored functions using ENUM or SET arguments are not supported "
              "in secondary engine.");
          report_stored_function_inlining_error(thd, sp->m_qname.str,
                                                err_reason);
          return false;
        }
      }
    }
  }

  return true;
}

Mem_root_array<sp_inline_instr *> *prepare(
    THD *thd, sp_head *sp, std::unordered_set<sp_head *> &used_sp_functions) {
  const Mem_root_array<sp_instr *> &instructions = sp->get_instructions();

  auto *prepared_instructions =
      new (thd->mem_root) Mem_root_array<sp_inline_instr *>(thd->mem_root);

  std::string err_reason{};
/* Create and validate inline instructions */
#ifndef NDEBUG
  int return_instr_ip = -1;
#endif
  for (auto *instr : instructions) {
    sp_inline_instr *inline_instr = nullptr;
    switch (instr->type()) {
      case sp_instr::Instr_type::INSTR_LEX_FRETURN:
        inline_instr = new (thd->mem_root) sp_inline_instr_freturn(instr, sp);
#ifndef NDEBUG
        return_instr_ip = instr->get_ip();
#endif
        break;
      case sp_instr::Instr_type::INSTR_LEX_SET:
        inline_instr = new (thd->mem_root) sp_inline_instr_set(instr);
        break;
      case sp_instr::Instr_type::INSTR_LEX_STMT:
        inline_instr = new (thd->mem_root) sp_inline_instr_stmt(instr);
        break;
      default:
        err_reason.append(
            "Currently supported stored function instructions are RETURN, SET "
            "and SELECT INTO.");
    }

    if (inline_instr != nullptr && !inline_instr->validate(err_reason)) {
      prepared_instructions->push_back(inline_instr);
    } else {
      sp_inline_instr::record_instruction_inlining_error(err_reason,
                                                         instr->get_ip());
      report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
      return nullptr;
    }
  }

  /*
    Redundant instructions are marked by starting from the RETURN instruction
    and tracing backwards to identify all live variables. This process also
    collects the used stored programs (functions) for recursion detection.
  */
  std::unordered_set<uint> live_variables;

  for (int i = prepared_instructions->size() - 1; i >= 0; i--) {
    sp_inline_instr *inline_instr = prepared_instructions->at(i);
    assert(i <= return_instr_ip);
    if (inline_instr->compute_is_redundant_and_collect_functions(
            thd, live_variables, used_sp_functions, err_reason)) {
      report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
      return nullptr;
    }
  }

  for (auto used_sp : used_sp_functions) {
    /* Check for direct recursive function calls or recursive function chains */
    if (used_sp == sp || used_sp->m_recursion_level > 0) {
      err_reason.append("Recursive stored functions are not allowed.");
      report_stored_function_inlining_error(thd, sp->m_qname.str, err_reason);
      return nullptr;
    } else {
      used_sp->m_recursion_level++;
    }
  }

  return prepared_instructions;
}

Item *inline_stored_function(
    THD *thd, Mem_root_array<sp_inline_instr *> *prepared_instructions,
    Item **sp_args, uint sp_arg_count, sp_head *sp_head,
    Name_resolution_context *sp_name_resolution_ctx) {
  std::unordered_map<uint, Item *> map_var_offset_to_value;
  std::string err_reason{};
  for (sp_inline_instr *inline_instr : *prepared_instructions) {
    if (inline_instr->is_redundant_instr()) {
      if (inline_instr->check_redundant_instr_for_errors(thd, err_reason)) {
        report_stored_function_inlining_error(thd, sp_head->m_qname.str,
                                              err_reason);
        return nullptr;
      }
      /* Skip the redundant instruction */
      continue;
    }
    Item *result_item = nullptr;
    if (inline_instr->process(thd, map_var_offset_to_value, sp_args,
                              sp_arg_count, sp_head, sp_name_resolution_ctx,
                              &result_item)) {
      return nullptr;
    }
    if (result_item != nullptr) {
      return result_item;
    }
  }
  assert(false);
  report_stored_function_inlining_error(thd, sp_head->m_qname.str, err_reason);
  return nullptr;
}

}  // namespace sp_inl
