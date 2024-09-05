/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

/**
  @file

  @brief
  This file defines all numerical Items
*/

#include "sql/item_func.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cfloat>  // DBL_DIG
#include <cmath>   // std::log2
#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <iosfwd>
#include <limits>  // std::numeric_limits
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "integer_digits.h"
#include "m_string.h"
#include "map_helpers.h"
#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_double2ulonglong.h"
#include "my_hostname.h"
#include "my_psi_config.h"
#include "my_rnd.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_systime.h"
#include "my_thread.h"
#include "my_user.h"  // parse_user
#include "mysql/components/services/bits/mysql_cond_bits.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/my_loglevel.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/plugin_audit.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_mysql_password_policy.h"
#include "mysql/service_thd_wait.h"
#include "mysql/status_var.h"
#include "mysql/strings/dtoa.h"
#include "mysql/strings/int2str.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/strings/my_strtoll10.h"
#include "prealloced_array.h"
#include "sql-common/json_dom.h"  // Json_wrapper
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_password_strength
#include "sql/auth/sql_security_ctx.h"
#include "sql/binlog.h"  // mysql_bin_log
#include "sql/check_stack.h"
#include "sql/current_thd.h"                 // current_thd
#include "sql/dd/info_schema/table_stats.h"  // dd::info_schema::Table_stati...
#include "sql/dd/info_schema/tablespace_stats.h"  // dd::info_schema::Tablesp...
#include "sql/dd/object_id.h"
#include "sql/dd/properties.h"  // dd::Properties
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/column.h"
#include "sql/dd/types/index.h"  // Index::enum_index_type
#include "sql/dd_sql_view.h"     // push_view_warning_or_error
#include "sql/dd_table_share.h"  // dd_get_old_field_type
#include "sql/debug_sync.h"      // DEBUG_SYNC
#include "sql/derror.h"          // ER_THD
#include "sql/error_handler.h"   // Internal_error_handler
#include "sql/item.h"            // Item_json
#include "sql/item_cmpfunc.h"    // get_datetime_value
#include "sql/item_json_func.h"  // get_json_wrapper
#include "sql/item_strfunc.h"    // Item_func_concat_ws
#include "sql/item_subselect.h"  // Item_subselect
#include "sql/key.h"
#include "sql/log_event.h"  // server_version
#include "sql/mdl.h"
#include "sql/mysqld.h"                // log_10 stage_user_sleep
#include "sql/parse_tree_helpers.h"    // PT_item_list
#include "sql/parse_tree_node_base.h"  // Parse_context
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"
#include "sql/resourcegroups/resource_group.h"
#include "sql/resourcegroups/resource_group_basic_types.h"
#include "sql/resourcegroups/resource_group_mgr.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_mi.h"       // Master_info
#include "sql/rpl_msr.h"      // channel_map
#include "sql/rpl_rli.h"      // Relay_log_info
#include "sql/sp.h"           // sp_setup_routine
#include "sql/sp_head.h"      // sp_name
#include "sql/sp_pcontext.h"  // sp_variable
#include "sql/sql_array.h"    // just to keep clang happy
#include "sql/sql_audit.h"    // audit_global_variable
#include "sql/sql_base.h"     // Internal_error_handler_holder
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_cmd.h"
#include "sql/sql_derived.h"  // Condition_pushdown
#include "sql/sql_error.h"
#include "sql/sql_exchange.h"  // sql_exchange
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_load.h"       // Sql_cmd_load_table
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_parse.h"      // check_stack_overrun
#include "sql/sql_show.h"       // append_identifier
#include "sql/sql_time.h"       // TIME_from_longlong_packed
#include "sql/strfunc.h"        // find_type
#include "sql/system_variables.h"
#include "sql/thd_raii.h"
#include "sql/val_int_compare.h"  // Integer_value
#include "sql_string.h"
#include "storage/perfschema/terminology_use_previous_enum.h"
#include "string_with_len.h"
#include "template_utils.h"
#include "template_utils.h"  // pointer_cast
#include "thr_mutex.h"
#include "vector-common/vector_constants.h"  // get_dimensions

using std::max;
using std::min;

static void free_user_var(user_var_entry *entry) { entry->destroy(); }

static int get_var_with_binlog(THD *thd, enum_sql_command sql_command,
                               const Name_string &name,
                               user_var_entry **out_entry);

bool check_reserved_words(const char *name) {
  if (!my_strcasecmp(system_charset_info, name, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name, "SESSION"))
    return true;
  return false;
}

void report_conversion_error(const CHARSET_INFO *to_cs, const char *from,
                             size_t from_length, const CHARSET_INFO *from_cs) {
  char printable_buff[32];
  convert_to_printable(printable_buff, sizeof(printable_buff), from,
                       from_length, from_cs, 6);
  const char *from_name = from_cs->csname;
  const char *to_name = to_cs->csname;
  my_error(ER_CANNOT_CONVERT_STRING, MYF(0), printable_buff, from_name,
           to_name);
}

/**
  Simplify the string arguments to a function, if possible.

  Currently used to substitute const values with character strings
  in the desired character set. Only used during resolving.

  @param thd      thread handler
  @param c        Desired character set and collation
  @param args     Pointer to argument array
  @param nargs    Number of arguments to process

  @returns false if success, true if error
*/
bool simplify_string_args(THD *thd, const DTCollation &c, Item **args,
                          uint nargs) {
  // Only used during resolving
  assert(!thd->lex->is_exec_started());

  if (thd->lex->is_view_context_analysis()) return false;

  uint i;
  Item **arg;
  for (i = 0, arg = args; i < nargs; i++, arg++) {
    size_t dummy_offset;
    // Only convert const values.
    if (!(*arg)->const_item()) continue;
    if (!String::needs_conversion(1, (*arg)->collation.collation, c.collation,
                                  &dummy_offset))
      continue;

    StringBuffer<STRING_BUFFER_USUAL_SIZE> original;
    StringBuffer<STRING_BUFFER_USUAL_SIZE> converted;
    String *ostr = (*arg)->val_str(&original);
    if (ostr == nullptr) {
      if (thd->is_error()) return true;
      *arg = new (thd->mem_root) Item_null;
      if (*arg == nullptr) return true;
      continue;
    }
    uint conv_status;
    converted.copy(ostr->ptr(), ostr->length(), ostr->charset(), c.collation,
                   &conv_status);
    if (conv_status != 0) {
      report_conversion_error(c.collation, ostr->ptr(), ostr->length(),
                              ostr->charset());
      return true;
    }
    // If source is a binary string, the string may have to be validated:
    if (c.collation != &my_charset_bin && ostr->charset() == &my_charset_bin &&
        !converted.is_valid_string(c.collation)) {
      report_conversion_error(c.collation, ostr->ptr(), ostr->length(),
                              ostr->charset());
      return true;
    }

    char *ptr = thd->strmake(converted.ptr(), converted.length());
    if (ptr == nullptr) return true;
    Item *conv = new (thd->mem_root)
        Item_string(ptr, converted.length(), converted.charset(), c.derivation);
    if (conv == nullptr) return true;

    *arg = conv;

    assert(conv->fixed);
  }
  return false;
}

/**
  Evaluate an argument string and return it in the desired character set.
  Perform character set conversion if needed.
  Perform character set validation (from a binary string) if needed.

  @param to_cs  The desired character set
  @param arg    Argument to evaluate as a string value
  @param buffer String buffer where argument is evaluated, if necessary

  @returns string pointer if success, NULL if error or NULL value
*/

String *eval_string_arg_noinline(const CHARSET_INFO *to_cs, Item *arg,
                                 String *buffer) {
  size_t offset;
  const bool convert =
      String::needs_conversion(0, arg->collation.collation, to_cs, &offset);

  if (convert) {
    StringBuffer<STRING_BUFFER_USUAL_SIZE> local_string(nullptr, 0, to_cs);
    String *res = arg->val_str(&local_string);
    // Return immediately if argument is a NULL value, or there was an error
    if (res == nullptr) return nullptr;
    /*
      String must be converted from source character set. It has been built
      in the "local_string" buffer and will be copied with conversion into the
      caller provided buffer.
    */
    uint errors = 0;
    buffer->length(0);
    buffer->copy(res->ptr(), res->length(), res->charset(), to_cs, &errors);
    if (errors) {
      report_conversion_error(to_cs, res->ptr(), res->length(), res->charset());
      return nullptr;
    }
    return buffer;
  }
  String *res = arg->val_str(buffer);
  // Return immediately if argument is a NULL value, or there was an error
  if (res == nullptr) return nullptr;

  // If source is a binary string, the string may have to be validated:
  if (to_cs != &my_charset_bin && arg->collation.collation == &my_charset_bin &&
      !res->is_valid_string(to_cs)) {
    report_conversion_error(to_cs, res->ptr(), res->length(), res->charset());
    return nullptr;
  }
  // Adjust target character set to the desired value
  res->set_charset(to_cs);
  return res;
}

/**
  Evaluate a constant condition, represented by an Item tree

  @param      thd   Thread handler
  @param      cond  The constant condition to evaluate
  @param[out] value Returned value, either true or false

  @returns false if evaluation is successful, true otherwise
*/

bool eval_const_cond(THD *thd, Item *cond, bool *value) {
  // Function may be used both during resolving and during optimization:
  assert(cond->may_evaluate_const(thd));
  *value = cond->val_bool();
  return thd->is_error();
}

/**
   Test if the sum of arguments overflows the ulonglong range.
*/
static inline bool test_if_sum_overflows_ull(ulonglong arg1, ulonglong arg2) {
  return ULLONG_MAX - arg1 < arg2;
}

bool Item_func::set_arguments(mem_root_deque<Item *> *list, bool context_free) {
  allowed_arg_cols = 1;
  if (alloc_args(*THR_MALLOC, list->size())) return true;
  std::copy(list->begin(), list->end(), args);
  if (!context_free) {
    for (const Item *arg : make_array(args, arg_count)) {
      add_accum_properties(arg);
    }
  }
  list->clear();  // Fields are used
  return false;
}

Item_func::Item_func(const POS &pos, PT_item_list *opt_list)
    : Item_result_field(pos), allowed_arg_cols(1) {
  if (opt_list == nullptr) {
    args = m_embedded_arguments;
    arg_count = 0;
  } else
    set_arguments(&opt_list->value, true);
}

Item_func::Item_func(THD *thd, const Item_func *item)
    : Item_result_field(thd, item),
      null_on_null(item->null_on_null),
      allowed_arg_cols(item->allowed_arg_cols),
      used_tables_cache(item->used_tables_cache),
      not_null_tables_cache(item->not_null_tables_cache) {
  if (alloc_args(thd->mem_root, item->arg_count)) return;
  std::copy_n(item->args, arg_count, args);
}

bool Item_func::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (Item_result_field::do_itemize(pc, res)) return true;
  const bool no_named_params = !may_have_named_parameters();
  for (size_t i = 0; i < arg_count; i++) {
    if (args[i]->itemize(pc, &args[i])) return true;
    add_accum_properties(args[i]);
    if (no_named_params && !args[i]->item_name.is_autogenerated()) {
      my_error(functype() == FUNC_SP ? ER_WRONG_PARAMETERS_TO_STORED_FCT
                                     : ER_WRONG_PARAMETERS_TO_NATIVE_FCT,
               MYF(0), func_name());
      return true;
    }
  }
  return false;
}

/*
  Resolve references to table column for a function and its argument

  SYNOPSIS:
  fix_fields()
  thd		Thread object

  DESCRIPTION
    Call fix_fields() for all arguments to the function.  The main intention
    is to allow all Item_field() objects to setup pointers to the table fields.

    Sets as a side effect the following class variables:
      maybe_null	Set if any argument may return NULL
      used_tables_cache Set to union of the tables used by arguments

      str_value.charset If this is a string function, set this to the
                        character set for the first argument.
                        If any argument is binary, this is set to binary

   If for any item any of the defaults are wrong, then this can
   be fixed in the resolve_type() function that is called after this one or
   by writing a specialized fix_fields() for the item.

  RETURN VALUES
  false	ok
  true	Got error.  Stored with my_error().
*/

bool Item_func::fix_fields(THD *thd, Item **) {
  assert(!fixed || basic_const_item());

  Item **arg, **arg_end;
  uchar buff[STACK_BUFF_ALLOC];  // Max argument in function

  const Condition_context CCT(thd->lex->current_query_block());

  used_tables_cache = get_initial_pseudo_tables();
  not_null_tables_cache = 0;

  /*
    Use stack limit of STACK_MIN_SIZE * 2 since
    on some platforms a recursive call to fix_fields
    requires more than STACK_MIN_SIZE bytes (e.g. for
    MIPS, it takes about 22kB to make one recursive
    call to Item_func::fix_fields())
  */
  if (check_stack_overrun(thd, STACK_MIN_SIZE * 2, buff))
    return true;    // Fatal error if flag is set!
  if (arg_count) {  // Print purify happy
    for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
      if (fix_func_arg(thd, arg)) return true;
    }
  }

  if (resolve_type(thd) || thd->is_error())  // Some impls still not error-safe
    return true;
  fixed = true;
  return false;
}

bool Item_func::fix_func_arg(THD *thd, Item **arg) {
  if ((!(*arg)->fixed && (*arg)->fix_fields(thd, arg)))
    return true; /* purecov: inspected */
  Item *item = *arg;

  if (allowed_arg_cols) {
    if (item->check_cols(allowed_arg_cols)) return true;
  } else {
    /*  we have to fetch allowed_arg_cols from first argument */
    assert(arg == args);  // it is first argument
    allowed_arg_cols = item->cols();
    assert(allowed_arg_cols);  // Can't be 0 any more
  }

  set_nullable(is_nullable() || item->is_nullable());
  used_tables_cache |= item->used_tables();
  if (null_on_null) not_null_tables_cache |= item->not_null_tables();
  add_accum_properties(item);

  return false;
}

void Item_func::fix_after_pullout(Query_block *parent_query_block,
                                  Query_block *removed_query_block) {
  if (const_item()) {
    /*
      Pulling out a const item changes nothing to it. Moreover, some items may
      have decided that they're const by some other logic than the generic
      one below, and we must preserve that decision.
    */
    return;
  }

  Item **arg, **arg_end;

  used_tables_cache = get_initial_pseudo_tables();
  not_null_tables_cache = 0;

  if (arg_count) {
    for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
      Item *const item = *arg;
      item->fix_after_pullout(parent_query_block, removed_query_block);
      used_tables_cache |= item->used_tables();
      if (null_on_null) not_null_tables_cache |= item->not_null_tables();
    }
  }
}

/**
  Default implementation for all functions:
  Propagate base_item's type into all arguments.

  The functions that have context-aware parameter type detection must
  implement Item::default_data_type() and Item::resolve_type_inner().

  If an SQL function or operator F embeds another SQL function or
  operator G: F::fix_fields() runs and calls G::fix_fields() which calls
  G::resolve_type(); assuming that G is outer-context dependent and has only
  dynamic parameters as arguments, G misses type information and thus does a
  no-op resolve_type(); then F::fix_fields() continues and calls
  F::resolve_type() which sees that G::data_type() == MYSQL_TYPE_INVALID;
  F thus calls G::propagate_type() to send it the necessary type
  information (i.e. provide the outer context); this then assigns the type
  to dynamic parameters of G and finishes the job of G::resolve_type() by
  calling G::resolve_type_inner().
*/
bool Item_func::propagate_type(THD *thd, const Type_properties &type) {
  assert(data_type() == MYSQL_TYPE_INVALID);
  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->data_type() == MYSQL_TYPE_INVALID)
      if (args[i]->propagate_type(thd, type)) return true;
  }
  if (resolve_type_inner(thd)) return true;
  assert(data_type() != MYSQL_TYPE_INVALID);

  return false;
}

/**
   For arguments of this Item_func ("args" array), in range
   [start, start+step, start+2*step,...,end[ : if they're a PS
   parameter with invalid (not known) type, give them default type "def".
   @param thd   thread handler
   @param start range's start (included)
   @param end   range's end (excluded)
   @param step  range's step
   @param def   default type

   @returns false if success, true if error
*/
bool Item_func::param_type_is_default(THD *thd, uint start, uint end, uint step,
                                      enum_field_types def) {
  for (uint i = start; i < end; i += step) {
    if (i >= arg_count) break;
    if (args[i]->propagate_type(thd, def)) return true;
  }
  return false;
}

/**
   For arguments of this Item_func ("args" array), in range [start,end[ :
   sends error if they're a dynamic parameter.
   @param start range's start (included)
   @param end   range's end (excluded)
   @returns true if error.
*/
bool Item_func::param_type_is_rejected(uint start, uint end) {
  for (uint i = start; i < end; i++) {
    if (i >= arg_count) break;
    if (args[i]->data_type() == MYSQL_TYPE_INVALID) {
      my_error(ER_INVALID_PARAMETER_USE, MYF(0), "?");
      return true;
    }
  }
  return false;
}

/**
  For arguments of this Item_func  ("args" array), all of them: find an
  argument that is not a dynamic parameter; if found, all dynamic parameters
  without a valid type get the type of this; if not found, they get type "def".

  @param thd       thread handler
  @param arg_count number of arguments to check
  @param args      array of arguments, size 'arg_count'
  @param def       default type

  @returns false if success, true if error
*/
inline bool param_type_uses_non_param_inner(THD *thd, uint arg_count,
                                            Item **args, enum_field_types def) {
  // Use first non-parameter type as base item
  // @todo If there are multiple non-parameter items, we could use a
  // consolidated type instead of the first one (consider CASE, COALESCE,
  // BETWEEN).
  const uint col_cnt = args[0]->cols();
  if (col_cnt > 1) {
    /*
      Row or subquery object: set parameter type recursively for the ith
      Item in each argument row.
    */
    Item **arguments = new (*THR_MALLOC) Item *[arg_count];
    for (uint i = 0; i < col_cnt; i++) {
      for (uint j = 0; j < arg_count; j++) {
        if (args[j]->cols() != col_cnt)  // Column count not checked yet
          return false;
        if (args[j]->type() == Item::ROW_ITEM)
          arguments[j] = down_cast<Item_row *>(args[j])->element_index(i);
        else if (args[j]->type() == Item::SUBQUERY_ITEM)
          arguments[j] = (*down_cast<Item_subselect *>(args[j])
                               ->query_expr()
                               ->get_unit_column_types())[i];
      }
      if (param_type_uses_non_param_inner(thd, arg_count, arguments, def))
        return true;
    }
    // Resolving for row done, set data type to MYSQL_TYPE_NULL as final action.
    for (uint j = 0; j < arg_count; j++)
      args[j]->set_data_type(MYSQL_TYPE_NULL);
    return false;
  }
  Item *base_item = nullptr;
  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->data_type() != MYSQL_TYPE_INVALID) {
      base_item = args[i];
      break;
    }
  }
  if (base_item == nullptr) {
    if (args[0]->propagate_type(thd, def)) return true;
    base_item = args[0];
  }
  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->data_type() != MYSQL_TYPE_INVALID) continue;
    if (args[i]->propagate_type(thd, Type_properties(*base_item))) return true;
  }
  return false;
}

bool Item_func::param_type_uses_non_param(THD *thd, enum_field_types def) {
  if (arg_count == 0) return false;
  return param_type_uses_non_param_inner(thd, arg_count, args, def);
}

Item *Item_func::replace_func_call(uchar *arg) {
  auto *info = pointer_cast<Item::Item_func_call_replacement *>(arg);
  if (eq(info->m_target)) {
    assert(info->m_curr_block == info->m_trans_block);
    return info->m_item;
  }
  return this;
}

bool Item_func::walk(Item_processor processor, enum_walk walk,
                     uchar *argument) {
  if ((walk & enum_walk::PREFIX) && (this->*processor)(argument)) return true;

  Item **arg, **arg_end;
  for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
    if ((*arg)->walk(processor, walk, argument)) return true;
  }
  return (walk & enum_walk::POSTFIX) && (this->*processor)(argument);
}

void Item_func::traverse_cond(Cond_traverser traverser, void *argument,
                              traverse_order order) {
  if (arg_count) {
    Item **arg, **arg_end;

    switch (order) {
      case (PREFIX):
        (*traverser)(this, argument);
        for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
          (*arg)->traverse_cond(traverser, argument, order);
        }
        break;
      case (POSTFIX):
        for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
          (*arg)->traverse_cond(traverser, argument, order);
        }
        (*traverser)(this, argument);
    }
  } else
    (*traverser)(this, argument);
}

/**
  Transform an Item_func object with a transformer callback function.

    The function recursively applies the transform method to each
    argument of the Item_func node.
    If the call of the method for an argument item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_func object.
*/

Item *Item_func::transform(Item_transformer transformer, uchar *argument) {
  if (arg_count) {
    Item **arg, **arg_end;
    for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
      *arg = (*arg)->transform(transformer, argument);
      if (*arg == nullptr) return nullptr; /* purecov: inspected */
    }
  }
  return (this->*transformer)(argument);
}

/**
  Compile Item_func object with a processor and a transformer
  callback functions.

    First the function applies the analyzer to the root node of
    the Item_func object. Then if the analyzer succeeeds (returns true)
    the function recursively applies the compile method to each argument
    of the Item_func node.
    If the call of the method for an argument item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_func object.
*/

Item *Item_func::compile(Item_analyzer analyzer, uchar **arg_p,
                         Item_transformer transformer, uchar *arg_t) {
  if (!(this->*analyzer)(arg_p)) return this;
  if (arg_count) {
    Item **arg, **arg_end;
    for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
      /*
        The same parameter value of arg_p must be passed
        to analyze any argument of the condition formula.
      */
      uchar *arg_v = *arg_p;
      Item *new_item = (*arg)->compile(analyzer, &arg_v, transformer, arg_t);
      if (new_item == nullptr) return nullptr;
      if (*arg != new_item) current_thd->change_item_tree(arg, new_item);
    }
  }
  return (this->*transformer)(arg_t);
}

/**
  See comments in Item_cmp_func::split_sum_func()
*/

bool Item_func::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                               mem_root_deque<Item *> *fields) {
  Item **arg, **arg_end;
  for (arg = args, arg_end = args + arg_count; arg != arg_end; arg++) {
    if ((*arg)->split_sum_func2(thd, ref_item_array, fields, arg, true)) {
      return true;
    }
  }
  return false;
}

void Item_func::update_used_tables() {
  used_tables_cache = get_initial_pseudo_tables();
  not_null_tables_cache = 0;
  // Reset all flags except Grouping Set dependency
  m_accum_properties &= PROP_HAS_GROUPING_SET_DEP;

  for (uint i = 0; i < arg_count; i++) {
    args[i]->update_used_tables();
    used_tables_cache |= args[i]->used_tables();
    if (null_on_null) not_null_tables_cache |= args[i]->not_null_tables();
    add_accum_properties(args[i]);
  }
}

void Item_func::print(const THD *thd, String *str,
                      enum_query_type query_type) const {
  str->append(func_name());
  str->append('(');
  print_args(thd, str, 0, query_type);
  str->append(')');
}

void Item_func::print_args(const THD *thd, String *str, uint from,
                           enum_query_type query_type) const {
  for (uint i = from; i < arg_count; i++) {
    if (i != from) str->append(',');
    args[i]->print(thd, str, query_type);
  }
}

void Item_func::print_op(const THD *thd, String *str,
                         enum_query_type query_type) const {
  str->append('(');
  for (uint i = 0; i < arg_count - 1; i++) {
    args[i]->print(thd, str, query_type);
    str->append(' ');
    str->append(func_name());
    str->append(' ');
  }
  args[arg_count - 1]->print(thd, str, query_type);
  str->append(')');
}

bool Item_func::eq(const Item *item) const {
  if (this == item) return true;
  if (item->type() != type()) return false;
  const Item_func::Functype func_type = functype();
  const Item_func *func = down_cast<const Item_func *>(item);
  /*
    Note: most function names are in ASCII character set, however stored
          functions and UDFs return names in system character set,
          therefore the comparison is performed using this character set.
  */
  return func_type == func->functype() && arg_count == func->arg_count &&
         !my_strcasecmp(system_charset_info, func_name(), func->func_name()) &&
         (arg_count == 0 || AllItemsAreEqual(args, func->args, arg_count)) &&
         eq_specific(item);
}

Field *Item_func::tmp_table_field(TABLE *table) {
  Field *field = nullptr;

  switch (result_type()) {
    case INT_RESULT:
      if (this->data_type() == MYSQL_TYPE_YEAR)
        field = new (*THR_MALLOC) Field_year(is_nullable(), item_name.ptr());
      else if (max_length > MY_INT32_NUM_DECIMAL_DIGITS)
        field = new (*THR_MALLOC) Field_longlong(
            max_length, is_nullable(), item_name.ptr(), unsigned_flag);
      else
        field = new (*THR_MALLOC) Field_long(max_length, is_nullable(),
                                             item_name.ptr(), unsigned_flag);
      break;
    case REAL_RESULT:
      if (this->data_type() == MYSQL_TYPE_FLOAT) {
        field = new (*THR_MALLOC)
            Field_float(max_char_length(), is_nullable(), item_name.ptr(),
                        decimals, unsigned_flag);
      } else {
        field = new (*THR_MALLOC)
            Field_double(max_char_length(), is_nullable(), item_name.ptr(),
                         decimals, unsigned_flag);
      }
      break;
    case STRING_RESULT:
      return make_string_field(table);
      break;
    case DECIMAL_RESULT:
      field = Field_new_decimal::create_from_item(this);
      break;
    case ROW_RESULT:
    default:
      // This case should never be chosen
      assert(0);
      field = nullptr;
      break;
  }
  if (field) field->init(table);
  return field;
}

my_decimal *Item_func::val_decimal(my_decimal *decimal_value) {
  assert(fixed);
  longlong nr = val_int();
  if (null_value) return nullptr; /* purecov: inspected */
  if (current_thd->is_error()) return error_decimal(decimal_value);
  int2my_decimal(E_DEC_FATAL_ERROR, nr, unsigned_flag, decimal_value);
  return decimal_value;
}

String *Item_real_func::val_str(String *str) {
  assert(fixed);
  double nr = val_real();
  if (null_value) return nullptr; /* purecov: inspected */
  if (current_thd->is_error()) return error_str();
  str->set_real(nr, decimals, collation.collation);
  return str;
}

my_decimal *Item_real_func::val_decimal(my_decimal *decimal_value) {
  assert(fixed);
  double nr = val_real();
  if (null_value) return nullptr; /* purecov: inspected */
  double2my_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return decimal_value;
}

void Item_func::fix_num_length_and_dec() {
  uint fl_length = 0;
  decimals = 0;
  for (uint i = 0; i < arg_count; i++) {
    decimals = max(decimals, args[i]->decimals);
    fl_length = max(fl_length, args[i]->max_length);
  }
  max_length = float_length(decimals);
  if (fl_length > max_length) {
    decimals = DECIMAL_NOT_SPECIFIED;
    max_length = float_length(DECIMAL_NOT_SPECIFIED);
  }
}

void Item_func_numhybrid::fix_num_length_and_dec() {}

void Item_func::signal_divide_by_null() {
  THD *thd = current_thd;
  if (thd->variables.sql_mode & MODE_ERROR_FOR_DIVISION_BY_ZERO)
    push_warning(thd, Sql_condition::SL_WARNING, ER_DIVISION_BY_ZERO,
                 ER_THD(thd, ER_DIVISION_BY_ZERO));
  null_value = true;
}

void Item_func::signal_invalid_argument_for_log() {
  THD *thd = current_thd;
  push_warning(thd, Sql_condition::SL_WARNING,
               ER_INVALID_ARGUMENT_FOR_LOGARITHM,
               ER_THD(thd, ER_INVALID_ARGUMENT_FOR_LOGARITHM));
  null_value = true;
}

Item *Item_func::get_tmp_table_item(THD *thd) {
  DBUG_TRACE;

  /*
    For items with aggregate functions, return the copy
    of the function.
    For constant items, return the same object, as fields
    are not created in temp tables for them.
    For items with windowing functions, return the same
    object (temp table fields are not created for windowing
    functions if they are not evaluated at this stage).
  */
  if (!has_aggregation() && !has_wf() &&
      !(const_for_execution() &&
        evaluate_during_optimization(this, thd->lex->current_query_block()))) {
    Item *result = new Item_field(result_field);
    return result;
  }
  Item *result = copy_or_same(thd);
  return result;
}

const Item_field *Item_func::contributes_to_filter(
    THD *thd, table_map read_tables, table_map filter_for_table,
    const MY_BITMAP *fields_to_ignore) const {
  // We are loth to change existing plans. Therefore we keep the existing
  // behavior for the original optimizer, which is to return nullptr if
  // any of PSEUDO_TABLE_BITS are set in used_tables().
  const table_map remaining_tables = thd->lex->using_hypergraph_optimizer()
                                         ? (~read_tables & ~PSEUDO_TABLE_BITS)
                                         : ~read_tables;

  assert((read_tables & filter_for_table) == 0);
  /*
    Multiple equality (Item_multi_eq) should not call this function
    because it would reject valid comparisons.
  */
  assert(functype() != MULTI_EQ_FUNC);

  /*
    To contribute to filtering effect, the condition must refer to
    exactly one unread table: the table filtering is currently
    calculated for.
  */
  if ((used_tables() & remaining_tables) != filter_for_table) return nullptr;

  /*
    Whether or not this Item_func has an operand that is a field in
    'filter_for_table' that is not in 'fields_to_ignore'.
  */
  Item_field *usable_field = nullptr;

  /*
    Whether or not this Item_func has an operand that can be used as
    available value. arg_count==1 for Items with implicit values like
    "field IS NULL".
  */
  bool found_comparable = (arg_count == 1);

  for (uint i = 0; i < arg_count; i++) {
    const Item::Type arg_type = args[i]->real_item()->type();

    if (arg_type == Item::SUBQUERY_ITEM) {
      if (args[i]->const_for_execution()) {
        // Constant subquery, i.e., not a dependent subquery.
        found_comparable = true;
        continue;
      }

      /*
        This is either "fld OP <dependent_subquery>" or "fld BETWEEN X
        and Y" where either X or Y is a dependent subquery. Filtering
        effect should not be calculated for this item because the cost
        of evaluating the dependent subquery is currently not
        calculated and its accompanying filtering effect is too
        uncertain. See WL#7384.
      */
      return nullptr;
    }  // ... if subquery.

    const table_map used_tabs = args[i]->used_tables();

    if (arg_type == Item::FIELD_ITEM && (used_tabs == filter_for_table)) {
      /*
        The qualifying table of args[i] is filter_for_table. args[i]
        may be a field or a reference to a field, e.g. through a
        view.
      */
      Item_field *fld = static_cast<Item_field *>(args[i]->real_item());

      /*
        Use args[i] as value if
        1) this field shall be ignored, or
        2) a usable field has already been found (meaning that
        this is "filter_for_table.colX OP filter_for_table.colY").
      */
      if (bitmap_is_set(fields_to_ignore, fld->field->field_index()) ||  // 1)
          usable_field)                                                  // 2)
      {
        found_comparable = true;
        continue;
      }

      /*
        This field shall contribute to filtering effect if a
        value is found for it
      */
      usable_field = fld;
    }  // if field.
    else {
      /*
        It's not a subquery. May be a function, a constant, an outer
        reference, a field of another table...

        Already checked that this predicate does not refer to tables
        later in the join sequence. Verify it:
      */
      assert(!(used_tabs & remaining_tables & ~filter_for_table));
      found_comparable = true;
    }
  }
  return (found_comparable ? usable_field : nullptr);
}

bool Item_func::is_valid_for_pushdown(uchar *arg) {
  Condition_pushdown::Derived_table_info *dti =
      pointer_cast<Condition_pushdown::Derived_table_info *>(arg);
  // We cannot push conditions that are not deterministic to a
  // derived table having set operations.
  return (dti->is_set_operation() && is_non_deterministic());
}

bool Item_func::check_column_in_window_functions(uchar *arg [[maybe_unused]]) {
  // Pushing conditions having non-deterministic results must be done with
  // care, or it may result in eliminating rows which would have
  // otherwise contributed to aggregations.
  // For Ex: SELECT * FROM (SELECT a AS x, SUM(b) FROM t1 GROUP BY a) dt
  //         WHERE x>5*RAND() AND x<3.
  // In this case, x<3 is pushed to the WHERE clause of the derived table
  // because there is grouping on "a". If we did the same for the random
  // condition, this condition might reduce the number of rows that get
  // qualified for grouping, resulting in wrong values for SUM. Similarly for
  // window functions. So we refrain from pushing the random condition
  // past the last operation done in the derived table's
  // materialization. Therefore, if there are window functions we cannot push
  // to HAVING, and if there is GROUP BY we cannot push to WHERE.
  // See also Item_field::is_valid_for_pushdown().
  return is_non_deterministic();
}

bool Item_func::check_column_in_group_by(uchar *arg [[maybe_unused]]) {
  return is_non_deterministic();
}

bool is_function_of_type(const Item *item, Item_func::Functype type) {
  return item->type() == Item::FUNC_ITEM &&
         down_cast<const Item_func *>(item)->functype() == type;
}

bool contains_function_of_type(Item *item, Item_func::Functype type) {
  return WalkItem(item, enum_walk::PREFIX, [type](Item *inner_item) {
    return is_function_of_type(inner_item, type);
  });
}

/**
  Return new Item_field if given expression matches GC

  @see substitute_gc()

  @param func           Expression to be replaced
  @param fld            GCs field
  @param type           Result type to match with Field
  @param[out] found     If given, just return found field, without Item_field

  @returns
    item new Item_field for matched GC
    NULL otherwise
*/

Item_field *get_gc_for_expr(const Item *func, Field *fld, Item_result type,
                            Field **found) {
  func = func->real_item();
  Item *expr = fld->gcol_info->expr_item;

  /*
    In the case where the generated column expression returns JSON and
    the predicate compares the values as strings, it is not safe to
    replace the expression with the generated column, since the
    indexed string values will be double-quoted. The generated column
    expression should use the JSON_UNQUOTE function to strip off the
    double-quotes in order to get a usable index for looking up
    strings. See also the comment below.
  */
  if (type == STRING_RESULT && expr->data_type() == MYSQL_TYPE_JSON)
    return nullptr;

  /*
    In order to match expressions against a functional index's expression,
    it's needed to skip CAST(.. AS .. ) and potentially COLLATE from the latter.
    This can't be joined with striping json_unquote below, since we might need
    to skip it too in expression like:
      CAST(JSON_UNQUOTE(<expr>) AS CHAR(X))

    Also skip unquoting function. This is needed to address JSON string
    comparison issue. All JSON_* functions return quoted strings. In
    order to create usable index, GC column expression has to include
    JSON_UNQUOTE function, e.g JSON_UNQUOTE(JSON_EXTRACT(..)).
    Hence, the unquoting function in column expression have to be
    skipped in order to correctly match GC expr to expr in
    WHERE condition.  The exception is if user has explicitly used
    JSON_UNQUOTE in WHERE condition.
  */

  for (Item_func::Functype functype :
       {Item_func::COLLATE_FUNC, Item_func::TYPECAST_FUNC,
        Item_func::JSON_UNQUOTE_FUNC}) {
    if (is_function_of_type(expr, functype) &&
        !is_function_of_type(func, functype)) {
      expr = down_cast<Item_func *>(expr)->get_arg(0);
    }
  }

  if (!expr->can_be_substituted_for_gc(fld->is_array())) {
    return nullptr;
  }

  // JSON implementation always uses binary collation
  if (type == fld->result_type() && func->eq(expr)) {
    if (found) {
      // Temporary mark the field in order to check correct value conversion
      fld->table->mark_column_used(fld, MARK_COLUMNS_TEMP);
      *found = fld;
      return nullptr;
    }
    // Mark field for read
    fld->table->mark_column_used(fld, MARK_COLUMNS_READ);
    Item_field *field = new Item_field(fld);
    return field;
  }
  if (found) *found = nullptr;
  return nullptr;
}

/**
  Attempt to substitute an expression with an equivalent generated
  column in a predicate.

  @param expr      the expression that should be substituted
  @param value     if given, value will be coerced to GC field's type and
                   the result will substitute the original value. Used by
                   multi-valued index.
  @param gc_fields list of indexed generated columns to check for
                   equivalence with the expression
  @param type      the acceptable type of the generated column that
                   replaces the expression
  @param predicate the predicate in which the substitution is done

  @return true on error, false on success
*/
static bool substitute_gc_expression(Item **expr, Item **value,
                                     List<Field> *gc_fields, Item_result type,
                                     Item_func *predicate) {
  List_iterator<Field> li(*gc_fields);
  Item_field *item_field = nullptr;
  while (Field *field = li++) {
    // Check whether the field has usable keys.
    Key_map tkm = field->part_of_key;
    tkm.merge(field->part_of_prefixkey);  // Include prefix keys.
    tkm.intersect(field->table->keys_in_use_for_query);
    /*
      Don't substitute if:
      1) Key is disabled
      2) It's a multi-valued index's field and predicate isn't MEMBER OF
    */
    if (tkm.is_clear_all() ||                           // (1)
        (field->is_array() && predicate->functype() !=  // (2)
                                  Item_func::MEMBER_OF_FUNC))
      continue;
    // If the field is a hidden field used by a functional index, we require
    // that the collation of the field must match the collation of the
    // expression. If not, we might end up with the wrong result when using
    // the index (see bug#27337092). Ideally, this should be done for normal
    // generated columns as well, but that is delayed to a later fix since the
    // impact might be quite large.
    if (!(field->is_field_for_functional_index() &&
          field->match_collation_to_optimize_range() &&
          (*expr)->collation.collation != field->charset())) {
      item_field = get_gc_for_expr(*expr, field, type);
      if (item_field != nullptr) break;
    }
  }

  if (item_field == nullptr) return false;

  // A matching expression is found. Substitute the expression with
  // the matching generated column.
  THD *thd = current_thd;
  if (item_field->returns_array() && value) {
    Json_wrapper wr;
    String str_val, buf;
    Field_typed_array *afld = down_cast<Field_typed_array *>(item_field->field);

    const Functional_index_error_handler functional_index_error_handler(afld,
                                                                        thd);

    if (get_json_atom_wrapper(value, 0, "MEMBER OF", &str_val, &buf, &wr,
                              nullptr, true))
      return true;

    auto to_wr = make_unique_destroy_only<Json_wrapper>(thd->mem_root);
    if (to_wr == nullptr) return true;

    // Don't substitute if value can't be coerced to field's type
    if (afld->coerce_json_value(&wr, /*no_error=*/true, to_wr.get()))
      return false;

    Item_json *jsn =
        new (thd->mem_root) Item_json(std::move(to_wr), predicate->item_name);
    if (jsn == nullptr || jsn->fix_fields(thd, nullptr)) return true;
    thd->change_item_tree(value, jsn);
  }
  thd->change_item_tree(expr, item_field);

  // Adjust the predicate.
  return predicate->resolve_type(thd);
}

/**
  A helper function for Item_func::gc_subst_transformer, that tries to
  substitute the given JSON_CONTAINS or JSON_OVERLAPS function for one of GCs
  from the provided list. The function checks whether there's an index with
  matching expression and whether all scalars for lookup can be coerced to
  index's GC field without errors. If so, index's GC field substitutes the
  given function, args are replaced for array of coerced values in order to
  match GC's type. substitute_gc_expression() can't be used to these functions
  as it's tailored to handle regular single-valued indexes and doesn't ensure
  correct coercion of all values to lookup in multi-valued index.

  @param func     Function to replace
  @param vals     Args to replace
  @param vals_wr  Json_wrapper containing array of values for index lookup
  @param gc_fields List of generated fields to look the function's substitute in
*/

static void gc_subst_overlaps_contains(Item **func, Item **vals,
                                       Json_wrapper &vals_wr,
                                       List<Field> *gc_fields) {
  // Field to substitute function for. NULL when no matching index was found.
  Field *found = nullptr;
  assert(vals_wr.type() != enum_json_type::J_OBJECT &&
         vals_wr.type() != enum_json_type::J_ERROR);
  THD *thd = current_thd;
  // Vector of coerced keys
  Json_array_ptr coerced_keys = create_dom_ptr<Json_array>();

  // Find a field that matches the expression
  for (Field &fld : *gc_fields) {
    bool can_use_index = true;
    // Check whether field has usable keys
    Key_map tkm = fld.part_of_key;
    tkm.intersect(fld.table->keys_in_use_for_query);

    if (tkm.is_clear_all() || !fld.is_array()) continue;
    Functional_index_error_handler func_idx_err_hndl(&fld, thd);
    found = nullptr;

    get_gc_for_expr(*func, &fld, fld.result_type(), &found);
    if (!found || !found->is_array()) continue;
    Field_typed_array *afld = down_cast<Field_typed_array *>(found);
    // Check that array's values can be coerced to found field's type
    uint len;
    if (vals_wr.type() == enum_json_type::J_ARRAY)
      len = vals_wr.length();
    else
      len = 1;
    coerced_keys->clear();
    for (uint i = 0; i < len; i++) {
      Json_wrapper elt = vals_wr[i];
      Json_wrapper res;
      if (afld->coerce_json_value(&elt, true, &res)) {
        can_use_index = false;
        found = nullptr;
        break;
      }
      coerced_keys->append_clone(res.to_dom());
    }
    if (can_use_index) break;
  }
  if (!found) return;
  TABLE *table = found->table;
  Item_field *subs_item = new Item_field(found);
  if (!subs_item) return;
  auto res = make_unique_destroy_only<Json_wrapper>(thd->mem_root,
                                                    coerced_keys.release());
  if (res == nullptr) return;
  Item_json *array_arg =
      new (thd->mem_root) Item_json(std::move(res), (*func)->item_name);
  if (!array_arg || array_arg->fix_fields(thd, nullptr)) return;
  table->mark_column_used(found, MARK_COLUMNS_READ);
  thd->change_item_tree(func, subs_item);
  thd->change_item_tree(vals, array_arg);
}

/**
  Transformer function for GC substitution.

  @param arg  List of indexed GC field

  @return this item on successful execution, nullptr on error

  @details This function transforms a search condition. It doesn't change
  'this' item but rather changes its arguments. It takes list of GC fields
  and checks whether arguments of 'this' item matches them and index over
  the GC field isn't disabled with hints. If so, it replaces
  the argument with newly created Item_field which uses the matched GC field.
  The following predicates' arguments could be transformed:
  - EQ_FUNC, LT_FUNC, LE_FUNC, GE_FUNC, GT_FUNC, JSON_OVERLAPS
    - Left _or_ right argument if the opposite argument is a constant.
  - IN_FUNC, BETWEEN
    - Left argument if all other arguments are constant and of the same type.
  - MEMBER OF
    - Right argument if left argument is constant.
  - JSON_CONTAINS
    - First argument if the second argument is constant.

  After transformation comparators are updated to take into account the new
  field.

  Note: Range optimizer is used with multi-value indexes and it prefers
  constants. Outer references are not considered as constants in JSON functions.
  However, range optimizer supports dynamic ranges, where ranges are
  re-optimized for each row. But the range optimizer is currently not able to
  handle multi-valued indexes with dynamic ranges, hence we use only constants
  in these cases.

*/

Item *Item_func::gc_subst_transformer(uchar *arg) {
  List<Field> *gc_fields = pointer_cast<List<Field> *>(arg);

  auto is_const_or_outer_reference = [](const Item *item) {
    return ((item->used_tables() & ~(OUTER_REF_TABLE_BIT | INNER_TABLE_BIT)) ==
            0);
  };

  switch (functype()) {
    case EQ_FUNC:
    case LT_FUNC:
    case LE_FUNC:
    case GE_FUNC:
    case GT_FUNC: {
      Item **func = nullptr;
      Item *val = nullptr;

      // Check if we can substitute a function with a GC. The
      // predicate must be on the form <expr> OP <constant> or
      // <constant> OP <expr>.
      if (args[0]->can_be_substituted_for_gc() &&
          is_const_or_outer_reference(args[1])) {
        func = args;
        val = args[1];
      } else if (args[1]->can_be_substituted_for_gc() &&
                 is_const_or_outer_reference(args[0])) {
        func = args + 1;
        val = args[0];
      } else {
        break;
      }

      if (substitute_gc_expression(func, nullptr, gc_fields, val->result_type(),
                                   this))
        return nullptr; /* purecov: inspected */
      break;
    }
    case BETWEEN:
    case IN_FUNC: {
      if (!args[0]->can_be_substituted_for_gc()) break;

      // Can only substitute if all the operands on the right-hand
      // side are constants of the same type.
      const Item_result type = args[1]->result_type();
      if (!std::all_of(
              args + 1, args + arg_count,
              [type, is_const_or_outer_reference](const Item *item_arg) {
                return is_const_or_outer_reference(item_arg) &&
                       item_arg->result_type() == type;
              })) {
        break;
      }
      if (substitute_gc_expression(args, nullptr, gc_fields, type, this))
        return nullptr;
      break;
    }
    case MEMBER_OF_FUNC: {
      const Item_result type = args[0]->result_type();
      /*
        Check whether MEMBER OF is applicable for optimization:
        1) 1st arg is constant for execution
        2) .. and it isn't NULL, as MEMBER OF can't be used to lookup NULLs
        3) 2nd arg can be substituted for a GC
      */
      if (args[0]->const_for_execution() &&                      // 1
          !args[0]->is_null() &&                                 // 2
          args[1]->can_be_substituted_for_gc(/*array=*/true)) {  // 3
        if (substitute_gc_expression(args + 1, args, gc_fields, type, this))
          return nullptr;
      }
      break;
    }
    case JSON_CONTAINS: {
      Json_wrapper vals_wr;
      String str;
      /*
        Check whether JSON_CONTAINS is applicable for optimization:
        1) 1st arg can be substituted with a generated column
        2) value to lookup is constant for execution
        3) value to lookup is a proper JSON doc
        4) value to lookup is an array or scalar
      */
      if (!args[0]->can_be_substituted_for_gc(/*array=*/true) ||  // 1
          !args[1]->const_for_execution())                        // 2
        break;
      if (get_json_wrapper(args, 1, &str, func_name(), &vals_wr)) {  // 3
        return nullptr;
      }
      if (args[1]->null_value ||
          vals_wr.type() == enum_json_type::J_OBJECT)  // 4
        break;
      gc_subst_overlaps_contains(args, args + 1, vals_wr, gc_fields);
      break;
    }
    case JSON_OVERLAPS: {
      Item **func = nullptr;
      int vals = -1;

      /*
        Check whether JSON_OVERLAPS is applicable for optimization:
        1) One argument is constant for execution
        2) The other argument can be substituted with a generated column
        3) value to lookup is a proper JSON doc
        4) value to lookup is an array or scalar
      */
      if (args[0]->can_be_substituted_for_gc(/*array=*/true) &&  // 2
          args[1]->const_for_execution()) {                      // 1
        func = args;
        vals = 1;
      } else if (args[1]->can_be_substituted_for_gc(/*array=*/true) &&  // 2
                 args[0]->const_for_execution()) {                      // 1
        func = args + 1;
        vals = 0;
      } else {
        break;
      }

      Json_wrapper vals_wr;
      String str;
      if (get_json_wrapper(args, vals, &str, func_name(), &vals_wr)) {  // 3
        return nullptr;
      }
      if (args[vals]->null_value ||
          vals_wr.type() == enum_json_type::J_OBJECT)  // 4
        break;
      gc_subst_overlaps_contains(func, args + vals, vals_wr, gc_fields);
      break;
    }
    default:
      break;
  }
  return this;
}

double Item_int_func::val_real() {
  assert(fixed);

  return unsigned_flag ? (double)((ulonglong)val_int()) : (double)val_int();
}

String *Item_int_func::val_str(String *str) {
  assert(fixed);
  longlong nr = val_int();
  if (null_value) return nullptr;
  str->set_int(nr, unsigned_flag, collation.collation);
  return str;
}

bool Item_func_connection_id::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

bool Item_func_connection_id::resolve_type(THD *thd) {
  if (Item_int_func::resolve_type(thd)) return true;
  unsigned_flag = true;
  return false;
}

bool Item_func_connection_id::fix_fields(THD *thd, Item **ref) {
  if (Item_int_func::fix_fields(thd, ref)) return true;
  thd->thread_specific_used = true;
  return false;
}

longlong Item_func_connection_id::val_int() {
  assert(fixed);
  return current_thd->variables.pseudo_thread_id;
}

/**
  Check arguments to determine the data type for a numeric
  function of two arguments.
*/

void Item_num_op::set_numeric_type(void) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("name %s", func_name()));
  assert(arg_count == 2);
  const Item_result r0 = args[0]->numeric_context_result_type();
  const Item_result r1 = args[1]->numeric_context_result_type();

  assert(r0 != STRING_RESULT && r1 != STRING_RESULT);

  if (r0 == REAL_RESULT || r1 == REAL_RESULT) {
    /*
      Since DATE/TIME/DATETIME data types return INT_RESULT/DECIMAL_RESULT
      type codes, we should never get to here when both fields are temporal.
    */
    assert(!args[0]->is_temporal() || !args[1]->is_temporal());
    aggregate_float_properties(MYSQL_TYPE_DOUBLE, args, arg_count);
    hybrid_type = REAL_RESULT;
  } else if (r0 == DECIMAL_RESULT || r1 == DECIMAL_RESULT) {
    set_data_type(MYSQL_TYPE_NEWDECIMAL);
    hybrid_type = DECIMAL_RESULT;
    result_precision();
  } else {
    assert(r0 == INT_RESULT && r1 == INT_RESULT);
    set_data_type(MYSQL_TYPE_LONGLONG);
    decimals = 0;
    hybrid_type = INT_RESULT;
    result_precision();
  }
  DBUG_PRINT("info",
             ("Type: %s", (hybrid_type == REAL_RESULT      ? "REAL_RESULT"
                           : hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT"
                           : hybrid_type == INT_RESULT     ? "INT_RESULT"
                                                       : "--ILLEGAL!!!--")));
}

/**
  Set data type for a numeric function with one argument
  (can be also used by a numeric function with many arguments, if the result
  type depends only on the first argument)
*/

void Item_func_num1::set_numeric_type() {
  DBUG_TRACE;
  DBUG_PRINT("info", ("name %s", func_name()));
  switch (hybrid_type = args[0]->result_type()) {
    case INT_RESULT:
      set_data_type(MYSQL_TYPE_LONGLONG);
      unsigned_flag = args[0]->unsigned_flag;
      break;
    case STRING_RESULT:
    case REAL_RESULT:
      set_data_type(MYSQL_TYPE_DOUBLE);
      hybrid_type = REAL_RESULT;
      max_length = float_length(decimals);
      break;
    case DECIMAL_RESULT:
      set_data_type(MYSQL_TYPE_NEWDECIMAL);
      unsigned_flag = args[0]->unsigned_flag;
      break;
    default:
      assert(0);
  }
  DBUG_PRINT("info",
             ("Type: %s", (hybrid_type == REAL_RESULT      ? "REAL_RESULT"
                           : hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT"
                           : hybrid_type == INT_RESULT     ? "INT_RESULT"
                                                       : "--ILLEGAL!!!--")));
}

void Item_func_num1::fix_num_length_and_dec() {
  decimals = args[0]->decimals;
  max_length = args[0]->max_length;
}

uint Item_func::num_vector_args() {
  uint num_vectors = 0;
  for (uint i = 0; i < arg_count; i++) {
    /* VECTOR type fields should not participate as function arguments. */
    if (args[i]->data_type() == MYSQL_TYPE_VECTOR) {
      num_vectors++;
    }
  }
  return num_vectors;
}

/*
  Reject unsupported VECTOR type arguments.
 */
bool Item_func::reject_vector_args() {
  if (num_vector_args() > 0) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    return true;
  }
  return false;
}

/*
  Reject geometry arguments, should be called in resolve_type() for
  SQL functions/operators where geometries are not suitable as operands.
 */
bool Item_func::reject_geometry_args() {
  /*
    We want to make sure the operands are not GEOMETRY strings because
    it's meaningless for them to participate in arithmetic and/or numerical
    calculations.

    When a variable holds a MySQL Geometry byte string, it is regarded as a
    string rather than a MYSQL_TYPE_GEOMETRY, so here we can't catch an illegal
    variable argument which was assigned with a geometry.

    Item::data_type() requires the item not be of ROW_RESULT, since a row
    isn't a field.
  */
  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->result_type() != ROW_RESULT &&
        args[i]->data_type() == MYSQL_TYPE_GEOMETRY) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return true;
    }
  }

  return false;
}

/**
  Go through the arguments of a function and check if any of them are
  JSON. If a JSON argument is found, raise a warning saying that this
  operation is not supported yet. This function is used to notify
  users that they are comparing JSON values using a mechanism that has
  not yet been updated to use the JSON comparator. JSON values are
  typically handled as strings in that case.

  @param arg_count  the number of arguments
  @param args       the arguments to go through looking for JSON values
  @param msg        the message that explains what is not supported
*/
void unsupported_json_comparison(size_t arg_count, Item **args,
                                 const char *msg) {
  for (size_t i = 0; i < arg_count; ++i) {
    if (args[i]->result_type() == STRING_RESULT &&
        args[i]->data_type() == MYSQL_TYPE_JSON) {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_NOT_SUPPORTED_YET,
                          ER_THD(current_thd, ER_NOT_SUPPORTED_YET), msg);
      break;
    }
  }
}

bool Item_func_numhybrid::resolve_type(THD *thd) {
  assert(arg_count == 1 || arg_count == 2);
  /*
    If no arguments have type information, return and trust
    propagate_type() to assign data types later.
    If some argument has type information, propagate the same type to
    the other argument.
  */
  if (arg_count == 1) {
    if (args[0]->data_type() == MYSQL_TYPE_INVALID) return false;
  } else {
    if (args[0]->data_type() == MYSQL_TYPE_INVALID &&
        args[1]->data_type() == MYSQL_TYPE_INVALID)
      return false;

    if (args[0]->data_type() == MYSQL_TYPE_INVALID) {
      if (args[0]->propagate_type(thd, Type_properties(*args[1]))) return true;
    } else if (args[1]->data_type() == MYSQL_TYPE_INVALID) {
      if (args[1]->propagate_type(thd, Type_properties(*args[0]))) return true;
    }
  }
  if (resolve_type_inner(thd)) return true;
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;

  return false;
}

bool Item_func_numhybrid::resolve_type_inner(THD *) {
  assert(args[0]->data_type() != MYSQL_TYPE_INVALID);
  assert(arg_count == 1 || args[1]->data_type() != MYSQL_TYPE_INVALID);
  fix_num_length_and_dec();
  set_numeric_type();
  return false;
}

String *Item_func_numhybrid::val_str(String *str) {
  assert(fixed);
  switch (hybrid_type) {
    case DECIMAL_RESULT: {
      my_decimal decimal_value, *val;
      if (!(val = decimal_op(&decimal_value))) return nullptr;  // null is set
      my_decimal_round(E_DEC_FATAL_ERROR, val, decimals, false, val);
      str->set_charset(collation.collation);
      my_decimal2string(E_DEC_FATAL_ERROR, val, str);
      break;
    }
    case INT_RESULT: {
      const longlong nr = int_op();
      if (null_value) return nullptr; /* purecov: inspected */
      str->set_int(nr, unsigned_flag, collation.collation);
      break;
    }
    case REAL_RESULT: {
      const double nr = real_op();
      if (null_value) return nullptr; /* purecov: inspected */
      str->set_real(nr, decimals, collation.collation);
      break;
    }
    case STRING_RESULT:
      switch (data_type()) {
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
          return val_string_from_datetime(str);
        case MYSQL_TYPE_DATE:
          return val_string_from_date(str);
        case MYSQL_TYPE_TIME:
          return val_string_from_time(str);
        default:
          break;
      }
      return str_op(&str_value);
    default:
      assert(0);
  }
  return str;
}

double Item_func_numhybrid::val_real() {
  assert(fixed);
  switch (hybrid_type) {
    case DECIMAL_RESULT: {
      my_decimal decimal_value, *val;
      double result;
      if (!(val = decimal_op(&decimal_value))) return 0.0;  // null is set
      my_decimal2double(E_DEC_FATAL_ERROR, val, &result);
      return result;
    }
    case INT_RESULT: {
      const longlong result = int_op();
      return unsigned_flag ? (double)((ulonglong)result) : (double)result;
    }
    case REAL_RESULT:
      return real_op();
    case STRING_RESULT: {
      switch (data_type()) {
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
          return val_real_from_decimal();
        default:
          break;
      }
      const char *end_not_used;
      int err_not_used;
      String *res = str_op(&str_value);
      return (res ? my_strntod(res->charset(), res->ptr(), res->length(),
                               &end_not_used, &err_not_used)
                  : 0.0);
    }
    default:
      assert(0);
  }
  return 0.0;
}

longlong Item_func_numhybrid::val_int() {
  assert(fixed);
  switch (hybrid_type) {
    case DECIMAL_RESULT: {
      my_decimal decimal_value, *val;
      if (!(val = decimal_op(&decimal_value))) return 0;  // null is set
      longlong result;
      my_decimal2int(E_DEC_FATAL_ERROR, val, unsigned_flag, &result);
      return result;
    }
    case INT_RESULT:
      return int_op();
    case REAL_RESULT: {
      return llrint_with_overflow_check(real_op());
    }
    case STRING_RESULT: {
      switch (data_type()) {
        case MYSQL_TYPE_DATE:
          return val_int_from_date();
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
          return val_int_from_datetime();
        case MYSQL_TYPE_TIME:
          return val_int_from_time();
        default:
          break;
      }
      int err_not_used;
      String *res;
      if (!(res = str_op(&str_value))) return 0;

      const char *end = res->ptr() + res->length();
      const CHARSET_INFO *cs = res->charset();
      return (*(cs->cset->strtoll10))(cs, res->ptr(), &end, &err_not_used);
    }
    default:
      assert(0);
  }
  return 0;
}

my_decimal *Item_func_numhybrid::val_decimal(my_decimal *decimal_value) {
  my_decimal *val = decimal_value;
  assert(fixed);
  switch (hybrid_type) {
    case DECIMAL_RESULT:
      val = decimal_op(decimal_value);
      break;
    case INT_RESULT: {
      const longlong result = int_op();
      int2my_decimal(E_DEC_FATAL_ERROR, result, unsigned_flag, decimal_value);
      break;
    }
    case REAL_RESULT: {
      const double result = real_op();
      double2my_decimal(E_DEC_FATAL_ERROR, result, decimal_value);
      break;
    }
    case STRING_RESULT: {
      switch (data_type()) {
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
          return val_decimal_from_date(decimal_value);
        case MYSQL_TYPE_TIME:
          return val_decimal_from_time(decimal_value);
        default:
          break;
      }
      String *res;
      if (!(res = str_op(&str_value))) return nullptr;

      str2my_decimal(E_DEC_FATAL_ERROR, res->ptr(), res->length(),
                     res->charset(), decimal_value);
      break;
    }
    case ROW_RESULT:
    default:
      assert(0);
  }
  return val;
}

bool Item_func_numhybrid::get_date(MYSQL_TIME *ltime,
                                   my_time_flags_t fuzzydate) {
  assert(fixed);
  switch (data_type()) {
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return date_op(ltime, fuzzydate);
    case MYSQL_TYPE_TIME:
      return get_date_from_time(ltime);
    case MYSQL_TYPE_YEAR:
      return get_date_from_int(ltime, fuzzydate);
    default:
      return Item::get_date_from_non_temporal(ltime, fuzzydate);
  }
}

bool Item_func_numhybrid::get_time(MYSQL_TIME *ltime) {
  assert(fixed);
  switch (data_type()) {
    case MYSQL_TYPE_TIME:
      return time_op(ltime);
    case MYSQL_TYPE_DATE:
      return get_time_from_date(ltime);
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return get_time_from_datetime(ltime);
    case MYSQL_TYPE_YEAR:
      return get_time_from_int(ltime);
    default:
      return Item::get_time_from_non_temporal(ltime);
  }
}

void Item_typecast_signed::print(const THD *thd, String *str,
                                 enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" as signed)"));
}

bool Item_typecast_signed::resolve_type(THD *thd) {
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return args[0]->propagate_type(thd, MYSQL_TYPE_LONGLONG, false, true);
}

static longlong val_int_from_str(Item *item, bool unsigned_flag,
                                 bool *null_value) {
  /*
    For a string result, we must first get the string and then convert it
    to a longlong
  */
  StringBuffer<MAX_FIELD_WIDTH> tmp;
  const String *res = item->val_str(&tmp);
  *null_value = item->null_value;
  if (*null_value) return 0;

  const size_t length = res->length();
  const char *start = res->ptr();
  const char *end = start + length;
  return longlong_from_string_with_check(res->charset(), start, end,
                                         unsigned_flag);
}

longlong Item_typecast_signed::val_int() {
  longlong value;

  if (args[0]->cast_to_int_type() != STRING_RESULT || args[0]->is_temporal()) {
    value = args[0]->val_int();
    null_value = args[0]->null_value;
  } else {
    value = val_int_from_str(args[0], unsigned_flag, &null_value);
  }

#ifndef NDEBUG
  if (null_value) {
    assert(is_nullable());
  } else if (value >= 0) {
    const int digits = count_digits(static_cast<ulonglong>(value));
    assert(digits <= decimal_int_part());
    assert(static_cast<unsigned>(digits) <= max_length);
  } else {
    const int digits =
        count_digits(ulonglong{0} - static_cast<ulonglong>(value));
    assert(digits <= decimal_int_part());
    assert(static_cast<unsigned>(digits) + 1 <= max_length);
  }
#endif

  return value;
}

void Item_typecast_unsigned::print(const THD *thd, String *str,
                                   enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" as unsigned)"));
}

bool Item_typecast_unsigned::resolve_type(THD *thd) {
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return args[0]->propagate_type(thd, MYSQL_TYPE_LONGLONG, false, true);
}

longlong Item_typecast_unsigned::val_int() {
  longlong value = 0;

  if (args[0]->cast_to_int_type() == DECIMAL_RESULT) {
    my_decimal tmp, *dec = args[0]->val_decimal(&tmp);
    null_value = args[0]->null_value;
    if (!null_value) {
      my_decimal2int(E_DEC_FATAL_ERROR, dec, !dec->sign(), &value);
    }
  } else if (args[0]->cast_to_int_type() != STRING_RESULT ||
             args[0]->is_temporal()) {
    value = args[0]->val_int();
    null_value = args[0]->null_value;
  } else {
    value = val_int_from_str(args[0], unsigned_flag, &null_value);
  }

  assert(!null_value || is_nullable());
  assert(count_digits(static_cast<ulonglong>(value)) <= decimal_int_part());

  return value;
}

String *Item_typecast_decimal::val_str(String *str) {
  my_decimal tmp_buf, *tmp = val_decimal(&tmp_buf);
  if (null_value) return nullptr;
  my_decimal2string(E_DEC_FATAL_ERROR, tmp, str);
  return str;
}

double Item_typecast_decimal::val_real() {
  my_decimal tmp_buf, *tmp = val_decimal(&tmp_buf);
  double res;
  if (null_value) return 0.0;
  my_decimal2double(E_DEC_FATAL_ERROR, tmp, &res);
  return res;
}

longlong Item_typecast_decimal::val_int() {
  my_decimal tmp_buf, *tmp = val_decimal(&tmp_buf);
  longlong res;
  if (null_value) return 0;
  my_decimal2int(E_DEC_FATAL_ERROR, tmp, unsigned_flag, &res);
  return res;
}

my_decimal *Item_typecast_decimal::val_decimal(my_decimal *dec) {
  my_decimal tmp_buf, *tmp = args[0]->val_decimal(&tmp_buf);
  bool sign;
  uint precision;

  if ((null_value = args[0]->null_value)) return nullptr;
  my_decimal_round(E_DEC_FATAL_ERROR, tmp, decimals, false, dec);
  sign = dec->sign();
  if (unsigned_flag) {
    if (sign) {
      my_decimal_set_zero(dec);
      goto err;
    }
  }
  precision =
      my_decimal_length_to_precision(max_length, decimals, unsigned_flag);
  if (precision - decimals < (uint)my_decimal_intg(dec)) {
    max_my_decimal(dec, precision, decimals);
    dec->sign(sign);
    goto err;
  }
  return dec;

err:
  push_warning_printf(
      current_thd, Sql_condition::SL_WARNING, ER_WARN_DATA_OUT_OF_RANGE,
      ER_THD(current_thd, ER_WARN_DATA_OUT_OF_RANGE), item_name.ptr(), 1L);
  return dec;
}

void Item_typecast_decimal::print(const THD *thd, String *str,
                                  enum_query_type query_type) const {
  const uint precision =
      my_decimal_length_to_precision(max_length, decimals, unsigned_flag);
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" as decimal("));
  str->append_ulonglong(precision);
  str->append(',');
  str->append_ulonglong(decimals);
  str->append(')');
  str->append(')');
}

void Item_typecast_decimal::add_json_info(Json_object *obj) {
  const uint precision =
      my_decimal_length_to_precision(max_length, decimals, unsigned_flag);
  obj->add_alias("precision", create_dom_ptr<Json_uint>(precision));
  obj->add_alias("scale", create_dom_ptr<Json_uint>(decimals));
}

String *Item_typecast_real::val_str(String *str) {
  return val_string_from_real(str);
}

double Item_typecast_real::val_real() {
  double res = args[0]->val_real();
  null_value = args[0]->null_value;
  if (null_value) return 0.0;
  if (data_type() == MYSQL_TYPE_FLOAT) {
    if (res > std::numeric_limits<float>::max() ||
        res < std::numeric_limits<float>::lowest()) {
      return raise_float_overflow();
    }
    res = static_cast<float>(res);
  }
  return check_float_overflow(res);
}

longlong Item_func::val_int_from_real() {
  double res = val_real();
  if (null_value) return 0;

  if (unsigned_flag) {
    if (res < 0 || res >= ULLONG_MAX_DOUBLE) {
      return raise_integer_overflow();
    } else
      return static_cast<longlong>(double2ulonglong(res));
  } else {
    if (res <= LLONG_MIN || res > LLONG_MAX_DOUBLE) {
      return raise_integer_overflow();
    } else
      return static_cast<longlong>(rint(res));
  }
}

bool Item_typecast_real::get_date(MYSQL_TIME *ltime,
                                  my_time_flags_t fuzzydate) {
  return my_double_to_datetime_with_warn(val_real(), ltime, fuzzydate);
}

bool Item_typecast_real::get_time(MYSQL_TIME *ltime) {
  return my_double_to_time_with_warn(val_real(), ltime);
}

my_decimal *Item_typecast_real::val_decimal(my_decimal *decimal_value) {
  const double result = val_real();
  if (null_value) return nullptr;
  double2my_decimal(E_DEC_FATAL_ERROR, result, decimal_value);

  return decimal_value;
}

void Item_typecast_real::print(const THD *thd, String *str,
                               enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append((data_type() == MYSQL_TYPE_FLOAT) ? "float)" : "double)");
}

double Item_func_plus::real_op() {
  const double val1 = args[0]->val_real();
  if (current_thd->is_error()) return error_real();
  const double val2 = args[1]->val_real();
  if (current_thd->is_error()) return error_real();

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0.0;
  const double value = val1 + val2;
  return check_float_overflow(value);
}

longlong Item_func_plus::int_op() {
  const longlong val0 = args[0]->val_int();
  if (current_thd->is_error()) return error_int();
  const longlong val1 = args[1]->val_int();
  if (current_thd->is_error()) return error_int();
  const longlong res = static_cast<unsigned long long>(val0) +
                       static_cast<unsigned long long>(val1);
  bool res_unsigned = false;

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0;

  /*
    First check whether the result can be represented as a
    (bool unsigned_flag, longlong value) pair, then check if it is compatible
    with this Item's unsigned_flag by calling check_integer_overflow().
  */
  if (args[0]->unsigned_flag) {
    if (args[1]->unsigned_flag || val1 >= 0) {
      if (test_if_sum_overflows_ull((ulonglong)val0, (ulonglong)val1)) goto err;
      res_unsigned = true;
    } else {
      /* val1 is negative */
      if ((ulonglong)val0 > (ulonglong)LLONG_MAX) res_unsigned = true;
    }
  } else {
    if (args[1]->unsigned_flag) {
      if (val0 >= 0) {
        if (test_if_sum_overflows_ull((ulonglong)val0, (ulonglong)val1))
          goto err;
        res_unsigned = true;
      } else {
        if ((ulonglong)val1 > (ulonglong)LLONG_MAX) res_unsigned = true;
      }
    } else {
      if (val0 >= 0 && val1 >= 0)
        res_unsigned = true;
      else if (val0 < 0 && val1 < 0 && res >= 0)
        goto err;
    }
  }
  return check_integer_overflow(res, res_unsigned);

err:
  return raise_integer_overflow();
}

/**
  Calculate plus of two decimals.

  @param decimal_value	Buffer that can be used to store result

  @return Value of operation as a decimal
  @retval
    0  Value was NULL;  In this case null_value is set
*/

my_decimal *Item_func_plus::decimal_op(my_decimal *decimal_value) {
  my_decimal value1, *val1;
  my_decimal value2, *val2;
  val1 = args[0]->val_decimal(&value1);
  if ((null_value = args[0]->null_value)) return nullptr;
  val2 = args[1]->val_decimal(&value2);
  if ((null_value = args[1]->null_value)) return nullptr;

  if (check_decimal_overflow(my_decimal_add(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                                            decimal_value, val1, val2)) > 3) {
    return error_decimal(decimal_value);
  }
  return decimal_value;
}

/**
  Set precision of results for additive operations (+ and -)
*/
void Item_func_additive_op::result_precision() {
  decimals = max(args[0]->decimals, args[1]->decimals);
  const int arg1_int = args[0]->decimal_precision() - args[0]->decimals;
  const int arg2_int = args[1]->decimal_precision() - args[1]->decimals;
  const int precision = max(arg1_int, arg2_int) + 1 + decimals;

  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag = args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag = args[0]->unsigned_flag & args[1]->unsigned_flag;
  max_length = my_decimal_precision_to_length_no_truncation(precision, decimals,
                                                            unsigned_flag);
}

/**
  The following function is here to allow the user to force
  subtraction of UNSIGNED BIGINT/DECIMAL to return negative values.
*/

bool Item_func_minus::resolve_type(THD *thd) {
  if (Item_num_op::resolve_type(thd)) return true;
  if (unsigned_flag && (thd->variables.sql_mode & MODE_NO_UNSIGNED_SUBTRACTION))
    unsigned_flag = false;
  return false;
}

double Item_func_minus::real_op() {
  const double val1 = args[0]->val_real();
  if (current_thd->is_error()) return error_real();
  const double val2 = args[1]->val_real();
  if (current_thd->is_error()) return error_real();

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0.0;
  const double value = val1 - val2;
  return check_float_overflow(value);
}

longlong Item_func_minus::int_op() {
  const longlong val0 = args[0]->val_int();
  if (current_thd->is_error()) return error_int();
  const longlong val1 = args[1]->val_int();
  if (current_thd->is_error()) return error_int();
  const longlong res = static_cast<unsigned long long>(val0) -
                       static_cast<unsigned long long>(val1);
  bool res_unsigned = false;

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0;

  /*
    First check whether the result can be represented as a
    (bool unsigned_flag, longlong value) pair, then check if it is compatible
    with this Item's unsigned_flag by calling check_integer_overflow().
  */
  if (args[0]->unsigned_flag) {
    if (args[1]->unsigned_flag) {
      if ((ulonglong)val0 < (ulonglong)val1) {
        if (res >= 0) goto err;
      } else
        res_unsigned = true;
    } else {
      if (val1 >= 0) {
        if ((ulonglong)val0 > (ulonglong)val1) res_unsigned = true;
      } else {
        if (test_if_sum_overflows_ull((ulonglong)val0, (ulonglong)-val1))
          goto err;
        res_unsigned = true;
      }
    }
  } else {
    if (args[1]->unsigned_flag) {
      if ((ulonglong)(val0 - LLONG_MIN) < (ulonglong)val1) goto err;
    } else {
      if (val0 >= 0 && val1 < 0)
        res_unsigned = true;
      else if (val0 < 0 && val1 > 0 && res >= 0)
        goto err;
    }
  }
  return check_integer_overflow(res, res_unsigned);

err:
  return raise_integer_overflow();
}

/**
  See Item_func_plus::decimal_op for comments.
*/

my_decimal *Item_func_minus::decimal_op(my_decimal *decimal_value) {
  my_decimal value1, *val1;
  my_decimal value2, *val2;

  val1 = args[0]->val_decimal(&value1);
  if ((null_value = args[0]->null_value)) return nullptr;

  val2 = args[1]->val_decimal(&value2);
  if ((null_value = args[1]->null_value)) return nullptr;

  if (check_decimal_overflow(my_decimal_sub(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                                            decimal_value, val1, val2)) > 3) {
    return error_decimal(decimal_value);
  }
  /*
   Allow sign mismatch only if sql_mode includes MODE_NO_UNSIGNED_SUBTRACTION
   See Item_func_minus::resolve_type().
  */
  if (unsigned_flag && decimal_value->sign()) {
    raise_decimal_overflow();
    return error_decimal(decimal_value);
  }
  return decimal_value;
}

double Item_func_mul::real_op() {
  assert(fixed);
  const double val1 = args[0]->val_real();
  if (current_thd->is_error()) return error_real();
  const double val2 = args[1]->val_real();
  if (current_thd->is_error()) return error_real();

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0.0;
  const double value = val1 * val2;
  return check_float_overflow(value);
}

longlong Item_func_mul::int_op() {
  assert(fixed);
  longlong a = args[0]->val_int();
  if (current_thd->is_error()) return error_int();
  longlong b = args[1]->val_int();
  if (current_thd->is_error()) return error_int();
  longlong res;
  ulonglong res0, res1;

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0;

  if (a == 0 || b == 0) return 0;

  /*
    First check whether the result can be represented as a
    (bool unsigned_flag, longlong value) pair, then check if it is compatible
    with this Item's unsigned_flag by calling check_integer_overflow().

    Let a = a1 * 2^32 + a0 and b = b1 * 2^32 + b0. Then
    a * b = (a1 * 2^32 + a0) * (b1 * 2^32 + b0) = a1 * b1 * 2^64 +
            + (a1 * b0 + a0 * b1) * 2^32 + a0 * b0;
    We can determine if the above sum overflows the ulonglong range by
    sequentially checking the following conditions:
    1. If both a1 and b1 are non-zero.
    2. Otherwise, if (a1 * b0 + a0 * b1) is greater than ULONG_MAX.
    3. Otherwise, if (a1 * b0 + a0 * b1) * 2^32 + a0 * b0 is greater than
    ULLONG_MAX.

    Since we also have to take the unsigned_flag for a and b into account,
    it is easier to first work with absolute values and set the
    correct sign later.

    We handle INT_MIN64 == -9223372036854775808 specially first,
    to avoid UBSAN warnings.
  */
  const bool a_negative = (!args[0]->unsigned_flag && a < 0);
  const bool b_negative = (!args[1]->unsigned_flag && b < 0);

  const bool res_unsigned = (a_negative == b_negative);

  if (a_negative && a == INT_MIN64) {
    if (b == 1) return check_integer_overflow(a, res_unsigned);
    return raise_integer_overflow();
  }

  if (b_negative && b == INT_MIN64) {
    if (a == 1) return check_integer_overflow(b, res_unsigned);
    return raise_integer_overflow();
  }

  if (a_negative) {
    a = -a;
  }
  if (b_negative) {
    b = -b;
  }

  const ulong a0 = 0xFFFFFFFFUL & a;
  const ulong a1 = ((ulonglong)a) >> 32;
  const ulong b0 = 0xFFFFFFFFUL & b;
  const ulong b1 = ((ulonglong)b) >> 32;

  if (a1 && b1) goto err;

  res1 = (ulonglong)a1 * b0 + (ulonglong)a0 * b1;
  if (res1 > 0xFFFFFFFFUL) goto err;

  res1 = res1 << 32;
  res0 = (ulonglong)a0 * b0;

  if (test_if_sum_overflows_ull(res1, res0)) goto err;
  res = res1 + res0;

  if (a_negative != b_negative) {
    if ((ulonglong)res > (ulonglong)LLONG_MAX) goto err;
    res = -res;
  }

  return check_integer_overflow(res, res_unsigned);

err:
  return raise_integer_overflow();
}

/** See Item_func_plus::decimal_op for comments. */

my_decimal *Item_func_mul::decimal_op(my_decimal *decimal_value) {
  my_decimal value1, *val1;
  my_decimal value2, *val2;
  val1 = args[0]->val_decimal(&value1);
  if ((null_value = args[0]->null_value)) return nullptr;
  val2 = args[1]->val_decimal(&value2);
  if ((null_value = args[1]->null_value)) return nullptr;

  if (check_decimal_overflow(my_decimal_mul(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                                            decimal_value, val1, val2)) > 3) {
    return error_decimal(decimal_value);
  }
  return decimal_value;
}

void Item_func_mul::result_precision() {
  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag = args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag = args[0]->unsigned_flag & args[1]->unsigned_flag;
  decimals = min(args[0]->decimals + args[1]->decimals, DECIMAL_MAX_SCALE);
  const uint est_prec =
      args[0]->decimal_precision() + args[1]->decimal_precision();
  uint precision = min<uint>(est_prec, DECIMAL_MAX_PRECISION);
  max_length = my_decimal_precision_to_length_no_truncation(precision, decimals,
                                                            unsigned_flag);
}

double Item_func_div_base::real_op() {
  assert(fixed);
  const double val1 = args[0]->val_real();
  if (current_thd->is_error()) return error_real();
  const double val2 = args[1]->val_real();
  if (current_thd->is_error()) return error_real();

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0.0;
  if (val2 == 0.0) {
    signal_divide_by_null();
    return 0.0;
  }
  return check_float_overflow(val1 / val2);
}

my_decimal *Item_func_div_base::decimal_op(my_decimal *decimal_value) {
  my_decimal value1, *val1;
  my_decimal value2, *val2;
  int err;

  val1 = args[0]->val_decimal(&value1);
  if ((null_value = args[0]->null_value)) return nullptr;
  val2 = args[1]->val_decimal(&value2);
  if ((null_value = args[1]->null_value)) return nullptr;

  if ((err = check_decimal_overflow(
           my_decimal_div(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW & ~E_DEC_DIV_ZERO,
                          decimal_value, val1, val2, m_prec_increment))) > 3) {
    if (err == E_DEC_DIV_ZERO) signal_divide_by_null();
    return error_decimal(decimal_value);
  }
  return decimal_value;
}

void Item_func_div::result_precision() {
  uint precision = min<uint>(
      args[0]->decimal_precision() + args[1]->decimals + m_prec_increment,
      DECIMAL_MAX_PRECISION);

  if (result_type() == DECIMAL_RESULT) assert(precision > 0);

  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag = args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag = args[0]->unsigned_flag & args[1]->unsigned_flag;
  decimals = min<uint>(args[0]->decimals + m_prec_increment, DECIMAL_MAX_SCALE);
  max_length = my_decimal_precision_to_length_no_truncation(precision, decimals,
                                                            unsigned_flag);
}

void Item_func_div_int::result_precision() {
  assert(result_type() == INT_RESULT);

  // Integer operations keep unsigned_flag if one of arguments is unsigned
  unsigned_flag = args[0]->unsigned_flag | args[1]->unsigned_flag;

  uint arg0_decimals = args[0]->decimals;
  if (arg0_decimals == DECIMAL_NOT_SPECIFIED) arg0_decimals = 0;
  uint arg1_decimals = args[1]->decimals;
  if (arg1_decimals == DECIMAL_NOT_SPECIFIED)
    arg1_decimals = args[1]->decimal_precision();

  uint precision =
      min<uint>(args[0]->decimal_precision() - arg0_decimals + arg1_decimals,
                MY_INT64_NUM_DECIMAL_DIGITS);

  max_length =
      my_decimal_precision_to_length_no_truncation(precision, 0, unsigned_flag);
}

bool Item_func_div::resolve_type(THD *thd) {
  DBUG_TRACE;
  m_prec_increment = thd->variables.div_precincrement;
  if (Item_num_op::resolve_type(thd)) return true;

  switch (hybrid_type) {
    case REAL_RESULT: {
      decimals = max(args[0]->decimals, args[1]->decimals) + m_prec_increment;
      decimals = min(decimals, uint8(DECIMAL_NOT_SPECIFIED));
      uint tmp = float_length(decimals);
      if (decimals == DECIMAL_NOT_SPECIFIED)
        max_length = tmp;
      else {
        max_length = args[0]->max_length - args[0]->decimals + decimals;
        max_length = min(max_length, tmp);
      }
      break;
    }
    case INT_RESULT:
      set_data_type(MYSQL_TYPE_NEWDECIMAL);
      hybrid_type = DECIMAL_RESULT;
      DBUG_PRINT("info", ("Type changed: DECIMAL_RESULT"));
      result_precision();
      break;
    case DECIMAL_RESULT:
      result_precision();
      break;
    default:
      assert(0);
  }
  set_nullable(true);  // division by zero
  return false;
}

longlong Item_func_div_base::int_op() {
  assert(fixed);

  /*
    Perform division using DECIMAL math if either of the operands has a
    non-integer type
  */
  if (args[0]->result_type() != INT_RESULT ||
      args[1]->result_type() != INT_RESULT) {
    my_decimal tmp;
    my_decimal *val0p = args[0]->val_decimal(&tmp);
    if ((null_value = args[0]->null_value)) return 0;
    if (current_thd->is_error()) return error_int();
    const my_decimal val0 = *val0p;

    my_decimal *val1p = args[1]->val_decimal(&tmp);
    if ((null_value = args[1]->null_value)) return 0;
    if (current_thd->is_error()) return error_int();
    const my_decimal val1 = *val1p;

    int err;
    if ((err = my_decimal_div(E_DEC_FATAL_ERROR & ~E_DEC_DIV_ZERO, &tmp, &val0,
                              &val1, 0)) > 3) {
      if (err == E_DEC_DIV_ZERO) signal_divide_by_null();
      return 0;
    }

    my_decimal truncated;
    const bool do_truncate = true;
    if (my_decimal_round(E_DEC_FATAL_ERROR, &tmp, 0, do_truncate, &truncated))
      assert(false);

    longlong res;
    if (my_decimal2int(E_DEC_FATAL_ERROR, &truncated, unsigned_flag, &res) &
        E_DEC_OVERFLOW)
      raise_integer_overflow();
    return res;
  }

  const longlong val0 = args[0]->val_int();
  const longlong val1 = args[1]->val_int();
  bool val0_negative, val1_negative, res_negative;
  ulonglong uval0, uval1, res;
  if ((null_value = (args[0]->null_value || args[1]->null_value))) return 0;
  if (val1 == 0) {
    signal_divide_by_null();
    return 0;
  }

  val0_negative = !args[0]->unsigned_flag && val0 < 0;
  val1_negative = !args[1]->unsigned_flag && val1 < 0;
  res_negative = val0_negative != val1_negative;
  uval0 = (ulonglong)(val0_negative && val0 != LLONG_MIN ? -val0 : val0);
  uval1 = (ulonglong)(val1_negative && val1 != LLONG_MIN ? -val1 : val1);
  res = uval0 / uval1;
  if (res_negative) {
    if (res > (ulonglong)LLONG_MAX) return raise_integer_overflow();
    res = (ulonglong)(-(longlong)res);
  }
  return check_integer_overflow(res, !res_negative);
}

bool Item_func_div_int::resolve_type(THD *thd) {
  // Integer division forces result to be integer, so force arguments
  // that are parameters to be integer as well.
  if (param_type_uses_non_param(thd, MYSQL_TYPE_LONGLONG)) return true;

  if (Item_func_div_base::resolve_type(thd)) return true;
  set_nullable(true);  // division by zero

  return false;
}

void Item_func_div_int::set_numeric_type() {
  set_data_type_longlong();
  hybrid_type = INT_RESULT;
  result_precision();
}

longlong Item_func_mod::int_op() {
  assert(fixed);
  const longlong val0 = args[0]->val_int();
  if (current_thd->is_error()) return error_int();
  const longlong val1 = args[1]->val_int();
  if (current_thd->is_error()) return error_int();
  bool val0_negative, val1_negative;
  ulonglong uval0, uval1;
  ulonglong res;

  if ((null_value = args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  if (val1 == 0) {
    signal_divide_by_null();
    return 0;
  }

  /*
    '%' is calculated by integer division internally. Since dividing
    LLONG_MIN by -1 generates SIGFPE, we calculate using unsigned values and
    then adjust the sign appropriately.
  */
  val0_negative = !args[0]->unsigned_flag && val0 < 0;
  val1_negative = !args[1]->unsigned_flag && val1 < 0;
  uval0 = (ulonglong)(val0_negative && val0 != LLONG_MIN ? -val0 : val0);
  uval1 = (ulonglong)(val1_negative && val1 != LLONG_MIN ? -val1 : val1);
  res = uval0 % uval1;
  MY_COMPILER_DIAGNOSTIC_PUSH()
  // Suppress warning C4146 unary minus operator applied to unsigned type,
  // result still unsigned
  MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4146)
  return check_integer_overflow(val0_negative ? -res : res, !val0_negative);
  MY_COMPILER_DIAGNOSTIC_POP()
}

double Item_func_mod::real_op() {
  assert(fixed);
  const double val1 = args[0]->val_real();
  if (current_thd->is_error()) return error_real();
  const double val2 = args[1]->val_real();
  if (current_thd->is_error()) return error_real();

  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0.0;
  if (val2 == 0.0) {
    signal_divide_by_null();
    return 0.0;
  }
  return fmod(val1, val2);
}

my_decimal *Item_func_mod::decimal_op(my_decimal *decimal_value) {
  my_decimal value1, *val1;
  my_decimal value2, *val2;

  val1 = args[0]->val_decimal(&value1);
  if ((null_value = args[0]->null_value)) return nullptr;
  val2 = args[1]->val_decimal(&value2);
  if ((null_value = args[1]->null_value)) return nullptr;
  switch (my_decimal_mod(E_DEC_FATAL_ERROR & ~E_DEC_DIV_ZERO, decimal_value,
                         val1, val2)) {
    case E_DEC_TRUNCATED:
    case E_DEC_OK:
      return decimal_value;
    case E_DEC_DIV_ZERO:
      signal_divide_by_null();
      [[fallthrough]];
    default:
      null_value = true;
      return nullptr;
  }
}

void Item_func_mod::result_precision() {
  decimals = max(args[0]->decimals, args[1]->decimals);
  const uint precision =
      max(args[0]->decimal_precision(), args[1]->decimal_precision());

  max_length = my_decimal_precision_to_length_no_truncation(precision, decimals,
                                                            unsigned_flag);

  // Increase max_length if we have: signed % unsigned(precision == scale)
  if (!args[0]->unsigned_flag && args[1]->unsigned_flag &&
      args[0]->max_length <= args[1]->max_length &&
      args[1]->decimals == args[1]->decimal_precision()) {
    max_length += 1;
  }
}

bool Item_func_mod::resolve_type(THD *thd) {
  if (Item_num_op::resolve_type(thd)) return true;
  set_nullable(true);
  unsigned_flag = args[0]->unsigned_flag;
  return false;
}

double Item_func_neg::real_op() {
  const double value = args[0]->val_real();
  null_value = args[0]->null_value;
  return -value;
}

longlong Item_func_neg::int_op() {
  const longlong value = args[0]->val_int();
  if ((null_value = args[0]->null_value)) return 0;
  if (args[0]->unsigned_flag && (ulonglong)value > (ulonglong)LLONG_MAX + 1ULL)
    return raise_integer_overflow();
  // For some platforms we need special handling of LLONG_MIN to
  // guarantee overflow.
  if (value == LLONG_MIN && !args[0]->unsigned_flag && !unsigned_flag)
    return raise_integer_overflow();
  // Avoid doing '-value' below, it is undefined.
  if (value == LLONG_MIN && args[0]->unsigned_flag && !unsigned_flag)
    return LLONG_MIN;
  return check_integer_overflow(-value, !args[0]->unsigned_flag && value < 0);
}

my_decimal *Item_func_neg::decimal_op(my_decimal *decimal_value) {
  my_decimal val, *value = args[0]->val_decimal(&val);
  if (!(null_value = args[0]->null_value)) {
    my_decimal2decimal(value, decimal_value);
    my_decimal_neg(decimal_value);
    return decimal_value;
  }
  return nullptr;
}

void Item_func_neg::fix_num_length_and_dec() {
  decimals = args[0]->decimals;
  max_length = args[0]->max_length + (args[0]->unsigned_flag ? 1 : 0);
  // Booleans have max_length = 1, but need to add the minus sign
  if (max_length == 1) max_length++;
}

bool Item_func_neg::resolve_type(THD *thd) {
  DBUG_TRACE;
  if (Item_func_num1::resolve_type(thd)) return true;
  /*
    If this is in integer context keep the context as integer if possible
    (This is how multiplication and other integer functions works)
    Use val() to get value as arg_type doesn't mean that item is
    Item_int or Item_real due to existence of Item_param.
  */
  if (hybrid_type == INT_RESULT && args[0]->const_item() &&
      args[0]->may_eval_const_item(thd)) {
    const longlong val = args[0]->val_int();
    if ((ulonglong)val >= (ulonglong)LLONG_MIN &&
        ((ulonglong)val != (ulonglong)LLONG_MIN ||
         args[0]->type() != INT_ITEM)) {
      /*
        Ensure that result is converted to DECIMAL, as longlong can't hold
        the negated number
      */
      unsigned_flag = false;
      set_data_type_decimal(
          min<uint>(args[0]->decimal_precision(), DECIMAL_MAX_PRECISION), 0);
      hybrid_type = DECIMAL_RESULT;
      DBUG_PRINT("info", ("Type changed: DECIMAL_RESULT"));
    }
  }
  unsigned_flag = false;
  return false;
}

double Item_func_abs::real_op() {
  const double value = args[0]->val_real();
  null_value = args[0]->null_value;
  return fabs(value);
}

longlong Item_func_abs::int_op() {
  const longlong value = args[0]->val_int();
  if ((null_value = args[0]->null_value)) return 0;
  if (unsigned_flag) return value;
  /* -LLONG_MIN = LLONG_MAX + 1 => outside of signed longlong range */
  if (value == LLONG_MIN) return raise_integer_overflow();
  return (value >= 0) ? value : -value;
}

my_decimal *Item_func_abs::decimal_op(my_decimal *decimal_value) {
  my_decimal val, *value = args[0]->val_decimal(&val);
  if (!(null_value = args[0]->null_value)) {
    my_decimal2decimal(value, decimal_value);
    if (decimal_value->sign()) my_decimal_neg(decimal_value);
    return decimal_value;
  }
  return nullptr;
}

bool Item_func_abs::resolve_type(THD *thd) {
  if (Item_func_num1::resolve_type(thd)) return true;
  unsigned_flag = args[0]->unsigned_flag;
  return false;
}

bool Item_dec_func::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_DOUBLE)) return true;
  decimals = DECIMAL_NOT_SPECIFIED;
  max_length = float_length(decimals);
  set_nullable(true);
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return false;
}

/** Gateway to natural LOG function. */
double Item_func_ln::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  if (value <= 0.0) {
    signal_invalid_argument_for_log();
    return 0.0;
  }
  return log(value);
}

/**
  Extended but so slower LOG function.

  We have to check if all values are > zero and first one is not one
  as these are the cases then result is not a number.
*/
double Item_func_log::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  if (value <= 0.0) {
    signal_invalid_argument_for_log();
    return 0.0;
  }
  if (arg_count == 2) {
    const double value2 = args[1]->val_real();
    if ((null_value = args[1]->null_value)) return 0.0;
    if (value2 <= 0.0 || value == 1.0) {
      signal_invalid_argument_for_log();
      return 0.0;
    }
    return log(value2) / log(value);
  }
  return log(value);
}

double Item_func_log2::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();

  if ((null_value = args[0]->null_value)) return 0.0;
  if (value <= 0.0) {
    signal_invalid_argument_for_log();
    return 0.0;
  }
  return std::log2(value);
}

double Item_func_log10::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  if (value <= 0.0) {
    signal_invalid_argument_for_log();
    return 0.0;
  }
  return log10(value);
}

double Item_func_exp::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0; /* purecov: inspected */
  return check_float_overflow(exp(value));
}

double Item_func_sqrt::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = (args[0]->null_value || value < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(value);
}

double Item_func_pow::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  const double val2 = args[1]->val_real();
  if ((null_value = (args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  const double pow_result = pow(value, val2);
  return check_float_overflow(pow_result);
}

// Trigonometric functions

double Item_func_acos::val_real() {
  assert(fixed);
  /* One can use this to defer SELECT processing. */
  DEBUG_SYNC(current_thd, "before_acos_function");
  const double value = args[0]->val_real();
  if ((null_value = (args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return acos(value);
}

double Item_func_asin::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = (args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return asin(value);
}

double Item_func_atan::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  if (arg_count == 2) {
    const double val2 = args[1]->val_real();
    if ((null_value = args[1]->null_value)) return 0.0;
    return check_float_overflow(atan2(value, val2));
  }
  return atan(value);
}

double Item_func_cos::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  return cos(value);
}

double Item_func_sin::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  return sin(value);
}

double Item_func_tan::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  return check_float_overflow(tan(value));
}

double Item_func_cot::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  const double val2 = tan(value);
  if (val2 == 0.0) {
    return raise_float_overflow();
  }
  return check_float_overflow(1.0 / val2);
}

// Bitwise functions

bool Item_func_bit::resolve_type(THD *thd) {
  const bool second_arg = binary_result_requires_binary_second_arg();
  /*
    In ?&?, we assume varbinary; if integer is provided we'll re-prepare.
    To force var*binary*, we temporarily change the charset:
  */
  const CHARSET_INFO *save_cs = thd->variables.collation_connection;
  thd->variables.collation_connection = &my_charset_bin;
  if (second_arg) {
    if (param_type_uses_non_param(thd)) return true;
  } else {
    if (param_type_is_default(thd, 0, 1)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONGLONG)) return true;
  }
  thd->variables.collation_connection = save_cs;
  if (bit_func_returns_binary(args[0], second_arg ? args[1] : nullptr)) {
    hybrid_type = STRING_RESULT;
    set_data_type_string(
        max<uint32>(args[0]->max_length, second_arg ? args[1]->max_length : 0U),
        &my_charset_bin);
  } else {
    hybrid_type = INT_RESULT;
    set_data_type_longlong();
    unsigned_flag = true;
  }
  if (reject_vector_args()) return true;
  return reject_geometry_args();
}

longlong Item_func_bit::val_int() {
  assert(fixed);
  if (hybrid_type == INT_RESULT)
    return int_op();
  else {
    String *res;
    if (!(res = str_op(&str_value))) return 0;

    int ovf_error;
    const char *from = res->ptr();
    const size_t len = res->length();
    const char *end = from + len;
    return my_strtoll10(from, &end, &ovf_error);
  }
}

double Item_func_bit::val_real() {
  assert(fixed);
  if (hybrid_type == INT_RESULT)
    return static_cast<ulonglong>(int_op());
  else {
    String *res;
    if (!(res = str_op(&str_value))) return 0.0;

    int ovf_error;
    const char *from = res->ptr();
    const size_t len = res->length();
    const char *end = from + len;
    return my_strtod(from, &end, &ovf_error);
  }
}

my_decimal *Item_func_bit::val_decimal(my_decimal *decimal_value) {
  assert(fixed);
  if (hybrid_type == INT_RESULT)
    return val_decimal_from_int(decimal_value);
  else
    return val_decimal_from_string(decimal_value);
}

String *Item_func_bit::val_str(String *str) {
  assert(fixed);
  if (hybrid_type == INT_RESULT) {
    const longlong nr = int_op();
    if (null_value) return nullptr;
    str->set_int(nr, unsigned_flag, collation.collation);
    return str;
  } else
    return str_op(str);
}

// Shift-functions, same as << and >> in C/C++

/**
  Template function that evaluates the bitwise shift operation over integer
  arguments.
  @tparam to_left True if left-shift, false if right-shift
*/
template <bool to_left>
longlong Item_func_shift::eval_int_op() {
  assert(fixed);
  ulonglong res = args[0]->val_uint();
  if (current_thd->is_error()) return error_int();
  if (args[0]->null_value) return error_int();

  ulonglong shift = args[1]->val_uint();
  if (current_thd->is_error()) return error_int();
  if (args[1]->null_value) return error_int();

  null_value = false;
  if (shift < sizeof(longlong) * 8)
    return to_left ? (res << shift) : (res >> shift);
  return 0;
}

longlong Item_func_shift_left::int_op() { return eval_int_op<true>(); }
longlong Item_func_shift_right::int_op() { return eval_int_op<false>(); }

/**
  Template function that evaluates the bitwise shift operation over binary
  string arguments.
  @tparam to_left True if left-shift, false if right-shift
*/
template <bool to_left>
String *Item_func_shift::eval_str_op(String *) {
  assert(fixed);

  String tmp_str;
  String *arg = args[0]->val_str(&tmp_str);
  if (current_thd->is_error()) return error_str();
  if (args[0]->null_value) return error_str();

  ssize_t arg_length = arg->length();
  size_t shift =
      min(args[1]->val_uint(), static_cast<ulonglong>(arg_length) * 8);
  if (current_thd->is_error()) return error_str();
  if (args[1]->null_value) return error_str();

  if (tmp_value.alloc(arg->length())) return error_str();

  tmp_value.length(arg_length);
  tmp_value.set_charset(&my_charset_bin);
  /*
    Example with left-shift-by-21-bits:
    |........|........|........|........|
      byte i  byte i+1 byte i+2 byte i+3
    First (leftmost) bit has number 1.
    21 = 2*8 + 5.
    Bits of number 1-3 of byte 'i' receive bits 22-24 i.e. the last 3 bits of
    byte 'i+2'. So, take byte 'i+2', shift it left by 5 bits, that puts the
    last 3 bits of byte 'i+2' in bits 1-3, and 0s elsewhere.
    Bits of number 4-8 of byte 'i' receive bits 25-39 i.e. the first 5 bits of
    byte 'i+3'. So, take byte 'i+3', shift it right by 3 bits, that puts the
    first 5 bits of byte 'i+3' in bits 4-8, and 0s elsewhere.
    In total, do OR of both results.
  */
  size_t mod = shift % 8;
  size_t mod_complement = 8 - mod;
  ssize_t entire_bytes = shift / 8;

  const unsigned char *from_c = pointer_cast<const unsigned char *>(arg->ptr());
  unsigned char *to_c = pointer_cast<unsigned char *>(tmp_value.c_ptr_quick());

  if (to_left) {
    // Bytes of lower index are overwritten by bytes of higher index
    for (ssize_t i = 0; i < arg_length; i++)
      if (i + entire_bytes + 1 < arg_length)
        to_c[i] = (from_c[i + entire_bytes] << mod) |
                  (from_c[i + entire_bytes + 1] >> mod_complement);
      else if (i + entire_bytes + 1 == arg_length)
        to_c[i] = from_c[i + entire_bytes] << mod;
      else
        to_c[i] = 0;
  } else {
    // Bytes of higher index are overwritten by bytes of lower index
    for (ssize_t i = arg_length - 1; i >= 0; i--)
      if (i > entire_bytes)
        to_c[i] = (from_c[i - entire_bytes] >> mod) |
                  (from_c[i - entire_bytes - 1] << mod_complement);
      else if (i == entire_bytes)
        to_c[i] = from_c[i - entire_bytes] >> mod;
      else
        to_c[i] = 0;
  }

  null_value = false;
  return &tmp_value;
}

String *Item_func_shift_left::str_op(String *str) {
  return eval_str_op<true>(str);
}

String *Item_func_shift_right::str_op(String *str) {
  return eval_str_op<false>(str);
}

// Bit negation ('~')

longlong Item_func_bit_neg::int_op() {
  assert(fixed);
  const ulonglong res = (ulonglong)args[0]->val_int();
  if (args[0]->null_value) return error_int();
  null_value = false;
  return ~res;
}

String *Item_func_bit_neg::str_op(String *str) {
  assert(fixed);
  String *res = args[0]->val_str(str);
  if (args[0]->null_value || !res) return error_str();

  if (tmp_value.alloc(res->length())) return error_str();

  const size_t arg_length = res->length();
  tmp_value.length(arg_length);
  tmp_value.set_charset(&my_charset_bin);
  const unsigned char *from_c = pointer_cast<const unsigned char *>(res->ptr());
  unsigned char *to_c = pointer_cast<unsigned char *>(tmp_value.c_ptr_quick());
  size_t i = 0;
  while (i + sizeof(longlong) <= arg_length) {
    int8store(&to_c[i], ~(uint8korr(&from_c[i])));
    i += sizeof(longlong);
  }
  while (i < arg_length) {
    to_c[i] = ~from_c[i];
    i++;
  }

  null_value = false;
  return &tmp_value;
}

/**
  Template function used to evaluate the bitwise operation over int arguments.

  @param int_func  The bitwise function.
*/
template <class Int_func>
longlong Item_func_bit_two_param::eval_int_op(Int_func int_func) {
  assert(fixed);
  ulonglong arg0 = args[0]->val_uint();
  if (args[0]->null_value) return error_int();
  ulonglong arg1 = args[1]->val_uint();
  if (args[1]->null_value) return error_int();
  null_value = false;
  return (longlong)int_func(arg0, arg1);
}

/// Instantiations of the above
template longlong Item_func_bit_two_param::eval_int_op<std::bit_or<ulonglong>>(
    std::bit_or<ulonglong>);
template longlong Item_func_bit_two_param::eval_int_op<std::bit_and<ulonglong>>(
    std::bit_and<ulonglong>);
template longlong Item_func_bit_two_param::eval_int_op<std::bit_xor<ulonglong>>(
    std::bit_xor<ulonglong>);

/**
  Template function that evaluates the bitwise operation over binary arguments.
  Checks that both arguments have same length and applies the bitwise operation

   @param char_func  The Bitwise function used to evaluate unsigned chars.
   @param int_func   The Bitwise function used to evaluate unsigned long longs.
*/
template <class Char_func, class Int_func>
String *Item_func_bit_two_param::eval_str_op(String *, Char_func char_func,
                                             Int_func int_func) {
  assert(fixed);
  String arg0_buff;
  String *s1 = args[0]->val_str(&arg0_buff);

  if (args[0]->null_value || !s1) return error_str();

  String arg1_buff;
  String *s2 = args[1]->val_str(&arg1_buff);

  if (args[1]->null_value || !s2) return error_str();

  size_t arg_length = s1->length();
  if (arg_length != s2->length()) {
    my_error(ER_INVALID_BITWISE_OPERANDS_SIZE, MYF(0), func_name());
    return error_str();
  }

  if (tmp_value.alloc(arg_length)) return error_str();

  tmp_value.length(arg_length);
  tmp_value.set_charset(&my_charset_bin);

  const uchar *s1_c_p = pointer_cast<const uchar *>(s1->ptr());
  const uchar *s2_c_p = pointer_cast<const uchar *>(s2->ptr());
  char *res = tmp_value.ptr();
  size_t i = 0;
  while (i + sizeof(longlong) <= arg_length) {
    int8store(&res[i], int_func(uint8korr(&s1_c_p[i]), uint8korr(&s2_c_p[i])));
    i += sizeof(longlong);
  }
  while (i < arg_length) {
    res[i] = char_func(s1_c_p[i], s2_c_p[i]);
    i++;
  }

  null_value = false;
  return &tmp_value;
}

/// Instantiations of the above
template String *
Item_func_bit_two_param::eval_str_op<std::bit_or<char>, std::bit_or<ulonglong>>(
    String *, std::bit_or<char>, std::bit_or<ulonglong>);
template String *Item_func_bit_two_param::eval_str_op<
    std::bit_and<char>, std::bit_and<ulonglong>>(String *, std::bit_and<char>,
                                                 std::bit_and<ulonglong>);
template String *Item_func_bit_two_param::eval_str_op<
    std::bit_xor<char>, std::bit_xor<ulonglong>>(String *, std::bit_xor<char>,
                                                 std::bit_xor<ulonglong>);

bool Item::bit_func_returns_binary(const Item *a, const Item *b) {
  /*
    Checks if the bitwise function should return binary data.
    The conditions to return true are the following:

    1. If there's only one argument(so b is nullptr),
    then a must be a [VAR]BINARY Item, different from the hex/bit/NULL literal.

    2. If there are two arguments, both should be [VAR]BINARY
    and at least one of them should be different from the hex/bit/NULL literal
  */
  // Check if a is [VAR]BINARY Item
  const bool a_is_binary = a->result_type() == STRING_RESULT &&
                           a->collation.collation == &my_charset_bin;
  // Check if b is not null and is [VAR]BINARY Item
  const bool b_is_binary = b && b->result_type() == STRING_RESULT &&
                           b->collation.collation == &my_charset_bin;

  return a_is_binary && (!b || b_is_binary) &&
         ((a->type() != Item::HEX_BIN_ITEM && a->type() != Item::NULL_ITEM) ||
          (b && b->type() != Item::HEX_BIN_ITEM &&
           b->type() != Item::NULL_ITEM));
}

// Conversion functions

bool Item_func_int_val::resolve_type_inner(THD *) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("name %s", func_name()));
  assert(args[0]->data_type() != MYSQL_TYPE_INVALID);

  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;

  switch (args[0]->result_type()) {
    case STRING_RESULT:
    case REAL_RESULT:
      set_data_type_double();
      hybrid_type = REAL_RESULT;
      break;
    case INT_RESULT:
      set_data_type_longlong();
      unsigned_flag = args[0]->unsigned_flag;
      hybrid_type = INT_RESULT;
      break;
    case DECIMAL_RESULT: {
      // For historical reasons, CEILING and FLOOR convert DECIMAL inputs into
      // BIGINT (granted that they are small enough to fit) while ROUND and
      // TRUNCATE don't. As items are not yet evaluated at this point,
      // assumptions must be made about when a conversion from DECIMAL_RESULT to
      // INT_RESULT can be safely achieved.
      //
      // During the rounding operation, we account for signedness by always
      // assuming that the argument DECIMAL is signed. Additionally, since we
      // call set_data_type_decimal with a scale of 0, we must increment the
      // precision here, as the rounding operation may cause an increase in
      // order of magnitude.
      int precision = args[0]->decimal_precision() - args[0]->decimals;
      if (args[0]->decimals != 0) ++precision;
      precision = std::min(precision, DECIMAL_MAX_PRECISION);
      set_data_type_decimal(precision, 0);
      hybrid_type = DECIMAL_RESULT;

      // The max_length of the biggest INT_RESULT, BIGINT, is 20 regardless of
      // signedness, as a minus sign will be counted as one digit. A DECIMAL of
      // length 20 could be bigger than the max BIGINT value, thus requiring a
      // length < 20. DECIMAL_LONGLONG_DIGITS value is 22, which is presumably
      // the sum of 20 digits, a minus sign and a decimal point; requiring -2
      // when considering the conversion.
      if (max_length < (DECIMAL_LONGLONG_DIGITS - 2)) {
        set_data_type_longlong();
        hybrid_type = INT_RESULT;
      }

      break;
    }
    default:
      assert(0);
  }
  DBUG_PRINT("info",
             ("Type: %s", (hybrid_type == REAL_RESULT      ? "REAL_RESULT"
                           : hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT"
                           : hybrid_type == INT_RESULT     ? "INT_RESULT"
                                                       : "--ILLEGAL!!!--")));

  return false;
}

longlong Item_func_ceiling::int_op() {
  longlong result;
  switch (args[0]->result_type()) {
    case INT_RESULT:
      result = args[0]->val_int();
      null_value = args[0]->null_value;
      break;
    case DECIMAL_RESULT: {
      my_decimal dec_buf, *dec;
      if ((dec = Item_func_ceiling::decimal_op(&dec_buf)))
        my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
      else
        result = 0;
      break;
    }
    default:
      result = (longlong)Item_func_ceiling::real_op();
  };
  return result;
}

double Item_func_ceiling::real_op() {
  const double value = args[0]->val_real();
  null_value = args[0]->null_value;
  return ceil(value);
}

my_decimal *Item_func_ceiling::decimal_op(my_decimal *decimal_value) {
  my_decimal val, *value = args[0]->val_decimal(&val);
  if (!(null_value =
            (args[0]->null_value ||
             my_decimal_ceiling(E_DEC_FATAL_ERROR, value, decimal_value) > 1)))
    return decimal_value;
  return nullptr;
}

longlong Item_func_floor::int_op() {
  longlong result;
  switch (args[0]->result_type()) {
    case INT_RESULT:
      result = args[0]->val_int();
      null_value = args[0]->null_value;
      break;
    case DECIMAL_RESULT: {
      my_decimal dec_buf, *dec;
      if ((dec = Item_func_floor::decimal_op(&dec_buf)))
        my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
      else
        result = 0;
      break;
    }
    default:
      result = (longlong)Item_func_floor::real_op();
  };
  return result;
}

double Item_func_floor::real_op() {
  const double value = args[0]->val_real();
  null_value = args[0]->null_value;
  return floor(value);
}

my_decimal *Item_func_floor::decimal_op(my_decimal *decimal_value) {
  my_decimal val, *value = args[0]->val_decimal(&val);
  if (!(null_value =
            (args[0]->null_value ||
             my_decimal_floor(E_DEC_FATAL_ERROR, value, decimal_value) > 1)))
    return decimal_value;
  return nullptr;
}

bool Item_func_round::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_NEWDECIMAL)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONGLONG)) return true;

  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;

  switch (args[0]->result_type()) {
    case INT_RESULT:
      set_data_type_longlong();
      unsigned_flag = args[0]->unsigned_flag;
      hybrid_type = INT_RESULT;
      break;
    case DECIMAL_RESULT: {
      /*
        If the rounding precision is known at this stage (constant), use it
        to adjust the precision and scale of the result to the minimal
        values that will accommodate the answer. Otherwise, use the precision
        and scale from the first argument.
        If wanted scale is less than argument's scale, reduce it accordingly.
        When rounding, make sure to accommodate one extra digit in precision
        Example: ROUND(99.999, 2). Here, the type of the argument is
        DECIMAL(5, 3). The type of the result is DECIMAL(5,2), since the
        result of this operation is 100.00.
        Also make sure that precision is greater than zero.
      */
      longlong val1;
      if (args[1]->const_item() && args[1]->may_eval_const_item(thd)) {
        val1 = args[1]->val_int();
        if ((null_value = args[1]->is_null())) {
          val1 = 0;
        }
        if (args[1]->unsigned_flag) {
          if (val1 > DECIMAL_MAX_SCALE || val1 < 0) val1 = DECIMAL_MAX_SCALE;
        } else if (val1 > DECIMAL_MAX_SCALE) {
          val1 = DECIMAL_MAX_SCALE;
        } else if (val1 < -DECIMAL_MAX_SCALE) {
          val1 = -DECIMAL_MAX_SCALE;
        }
      } else {
        val1 = args[0]->decimals;
      }

      uint8 precision = args[0]->decimal_precision();
      uint8 new_scale = args[0]->decimals;
      if (val1 <= 0) {
        precision -= new_scale;
        if (!truncate) precision += 1;
        new_scale = 0;
      } else if (val1 < new_scale) {
        precision -= (new_scale - val1);
        if (!truncate) precision += 1;
        new_scale = val1;
      }
      if (precision == 0) precision = 1;
      precision = min<uint>(precision, DECIMAL_MAX_PRECISION);
      set_data_type_decimal(precision, new_scale);
      hybrid_type = DECIMAL_RESULT;
      break;
    }
    case REAL_RESULT:
    case STRING_RESULT:
      set_data_type_double();
      hybrid_type = REAL_RESULT;
      break;
    default:
      assert(0); /* This result type isn't handled */
  }
  return false;
}

double my_double_round(double value, longlong dec, bool dec_unsigned,
                       bool truncate) {
  const bool dec_negative = (dec < 0) && !dec_unsigned;
  int log_10_size = array_elements(log_10);  // 309
  if (dec_negative && dec <= -log_10_size) return 0.0;

  const ulonglong abs_dec = dec_negative ? -dec : dec;

  double tmp = (abs_dec < array_elements(log_10) ? log_10[abs_dec]
                                                 : pow(10.0, (double)abs_dec));

  const double value_mul_tmp = value * tmp;
  if (!dec_negative && !std::isfinite(value_mul_tmp)) return value;

  const double value_div_tmp = value / tmp;
  if (truncate) {
    if (value >= 0.0)
      return dec < 0 ? floor(value_div_tmp) * tmp : floor(value_mul_tmp) / tmp;
    else
      return dec < 0 ? ceil(value_div_tmp) * tmp : ceil(value_mul_tmp) / tmp;
  }

  return dec < 0 ? rint(value_div_tmp) * tmp : rint(value_mul_tmp) / tmp;
}

double Item_func_round::real_op() {
  const double value = args[0]->val_real();
  const longlong decimal_places = args[1]->val_int();

  if (!(null_value = args[0]->null_value || args[1]->null_value))
    return my_double_round(value, decimal_places, args[1]->unsigned_flag,
                           truncate);

  return 0.0;
}

/*
  Rounds a given value to a power of 10 specified as the 'to' argument.
*/
static inline ulonglong my_unsigned_round(ulonglong value, ulonglong to,
                                          bool *round_overflow) {
  const ulonglong tmp = value / to * to;
  if (value - tmp < (to >> 1)) {
    return tmp;
  } else {
    if (test_if_sum_overflows_ull(tmp, to)) {
      *round_overflow = true;
      return 0;
    }
    return tmp + to;
  }
}

longlong Item_func_round::int_op() {
  const longlong value = args[0]->val_int();
  const longlong dec = args[1]->val_int();
  decimals = 0;
  ulonglong abs_dec;
  if ((null_value = args[0]->null_value || args[1]->null_value)) return 0;
  if ((dec >= 0) || args[1]->unsigned_flag)
    return value;  // integer have not digits after point

  MY_COMPILER_DIAGNOSTIC_PUSH()
  // Suppress warning C4146 unary minus operator applied to unsigned type,
  // result still unsigned
  MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4146)
  abs_dec = -static_cast<ulonglong>(dec);
  MY_COMPILER_DIAGNOSTIC_POP()
  longlong tmp;

  if (abs_dec >= array_elements(log_10_int)) return 0;

  tmp = log_10_int[abs_dec];

  if (truncate)
    return (unsigned_flag) ? ((ulonglong)value / tmp) * tmp
                           : (value / tmp) * tmp;
  else if (unsigned_flag || value >= 0) {
    bool round_overflow = false;
    const ulonglong rounded_value =
        my_unsigned_round(static_cast<ulonglong>(value), tmp, &round_overflow);
    if (!unsigned_flag && rounded_value > LLONG_MAX)
      return raise_integer_overflow();
    if (round_overflow) return raise_integer_overflow();
    return rounded_value;
  } else {
    // We round "towards nearest", so
    // -9223372036854775808 should round to
    // -9223372036854775810 which underflows, or
    // -9223372036854775800 which is OK, or
    // -9223372036854776000 which underflows, and so on ...
    if (value == LLONG_MIN) {
      switch (abs_dec) {
        case 0:
          return LLONG_MIN;
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
        case 9:
        case 10:
        case 14:
        case 19:
          return raise_integer_overflow();
        default:
          return (LLONG_MIN / tmp) * tmp;
      }
    }
    bool not_used = false;
    const ulonglong rounded_value =
        my_unsigned_round(static_cast<ulonglong>(-value), tmp, &not_used);
    if (rounded_value > LLONG_MAX) return raise_integer_overflow();

    return -static_cast<longlong>(rounded_value);
  }
}

my_decimal *Item_func_round::decimal_op(my_decimal *decimal_value) {
  my_decimal val, *value = args[0]->val_decimal(&val);
  longlong dec = args[1]->val_int();
  if (dec >= 0 || args[1]->unsigned_flag)
    dec = min<ulonglong>(dec, decimals);
  else if (dec < INT_MIN)
    dec = INT_MIN;

  if (!(null_value = (args[0]->null_value || args[1]->null_value ||
                      my_decimal_round(E_DEC_FATAL_ERROR, value, (int)dec,
                                       truncate, decimal_value) > 1)))
    return decimal_value;
  return nullptr;
}

bool Item_func_rand::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  /*
    When RAND() is binlogged, the seed is binlogged too.  So the
    sequence of random numbers is the same on a replication slave as
    on the master.  However, if several RAND() values are inserted
    into a table, the order in which the rows are modified may differ
    between master and slave, because the order is undefined.  Hence,
    the statement is unsafe to log in statement format.
  */
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);

  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
  return false;
}

void Item_func_rand::seed_random(Item *arg) {
  /*
    TODO: do not do reinit 'rand' for every execute of PS/SP if
    args[0] is a constant.
  */
  const uint32 tmp = (uint32)arg->val_int();
  randominit(m_rand, (uint32)(tmp * 0x10001L + 55555555L),
             (uint32)(tmp * 0x10000001L));
}

bool Item_func_rand::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_DOUBLE)) return true;
  if (Item_real_func::resolve_type(thd)) return true;
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return false;
}

bool Item_func_rand::fix_fields(THD *thd, Item **ref) {
  if (Item_real_func::fix_fields(thd, ref)) return true;

  if (arg_count > 0) {  // Only use argument once in query
    /*
      Allocate rand structure once: we must use thd->stmt_arena
      to create rand in proper mem_root if it's a prepared statement or
      stored procedure.

      No need to send a Rand log event if seed was given eg: RAND(seed),
      as it will be replicated in the query as such.
    */
    assert(m_rand == nullptr);
    m_rand = pointer_cast<rand_struct *>(thd->alloc(sizeof(*m_rand)));
    if (m_rand == nullptr) return true;
  } else {
    /*
      Save the seed only the first time RAND() is used in the query
      Once events are forwarded rather than recreated,
      the following can be skipped if inside the slave thread
    */
    if (!thd->rand_used) {
      thd->rand_used = true;
      thd->rand_saved_seed1 = thd->rand.seed1;
      thd->rand_saved_seed2 = thd->rand.seed2;
    }
  }
  return false;
}

double Item_func_rand::val_real() {
  assert(fixed);
  rand_struct *rand;
  if (arg_count > 0) {
    if (!args[0]->const_for_execution())
      seed_random(args[0]);
    else if (first_eval) {
      /*
        Constantness of args[0] may be set during JOIN::optimize(), if arg[0]
        is a field item of "constant" table. Thus, we have to evaluate
        seed_random() for constant arg there but not at the fix_fields method.
      */
      first_eval = false;
      seed_random(args[0]);
    }
    rand = m_rand;
  } else {
    /*
      Save the seed only the first time RAND() is used in the query
      Once events are forwarded rather than recreated,
      the following can be skipped if inside the slave thread
    */
    THD *const thd = current_thd;
    if (!thd->rand_used) {
      thd->rand_used = true;
      thd->rand_saved_seed1 = thd->rand.seed1;
      thd->rand_saved_seed2 = thd->rand.seed2;
    }
    rand = &thd->rand;
  }
  return my_rnd(rand);
}

bool Item_func_sign::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_DOUBLE)) return true;
  if (Item_int_func::resolve_type(thd)) return true;
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return false;
}

longlong Item_func_sign::val_int() {
  assert(fixed);
  const double value = args[0]->val_real();
  null_value = args[0]->null_value;
  return value < 0.0 ? -1 : (value > 0 ? 1 : 0);
}

bool Item_func_units::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_DOUBLE)) return true;
  decimals = DECIMAL_NOT_SPECIFIED;
  max_length = float_length(decimals);
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return false;
}

double Item_func_units::val_real() {
  assert(fixed);
  const double value = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0;
  return check_float_overflow(value * mul + add);
}

bool Item_func_min_max::resolve_type(THD *thd) {
  // If no arguments have type, type of this operator cannot be determined yet
  uint i;
  for (i = 0; i < arg_count; i++) {
    if (args[i]->data_type() != MYSQL_TYPE_INVALID) break;
  }
  if (i == arg_count) return false;

  if (resolve_type_inner(thd)) return true;
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;
  return false;
}

TYPELIB *Item_func_min_max::get_typelib() const {
  if (data_type() == MYSQL_TYPE_ENUM || data_type() == MYSQL_TYPE_SET) {
    for (const Item *arg : make_array(args, arg_count)) {
      TYPELIB *typelib = arg->get_typelib();
      if (typelib != nullptr) return typelib;
    }
    assert(false);
  }
  return nullptr;
}

/*
  "rank" the temporal types, to get consistent results for cases like
  greatest(year, date) vs. greatest(date, year)
  We compare as 'date' regardless of the order of the arguments.
 */
static int temporal_rank(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_DATETIME:
      return 5;
    case MYSQL_TYPE_TIMESTAMP:
      return 4;
    case MYSQL_TYPE_DATE:
      return 3;
    case MYSQL_TYPE_TIME:
      return 2;
    case MYSQL_TYPE_YEAR:
      return 1;
    default:
      assert(false);
      return 0;
  }
}

bool Item_func_min_max::resolve_type_inner(THD *thd) {
  if (param_type_uses_non_param(thd)) return true;
  if (aggregate_type(func_name(), args, arg_count)) return true;
  hybrid_type = Field::result_merge_type(data_type());
  if (hybrid_type == STRING_RESULT) {
    /*
      If one or more of the arguments have a temporal data type, temporal_item
      must be set for correct conversion from temporal values to various result
      types.
    */
    fsp_for_string = 0;
    for (uint i = 0; i < arg_count; i++) {
      if (args[i]->is_temporal()) {
        /*
          If one of the arguments is DATETIME, overwrite any existing
          temporal_item since DATETIME contains both date and time and is the
          most general and detailed data type to which other temporal types can
          be converted without loss of information.
        */
        if (temporal_item == nullptr ||
            temporal_rank(args[i]->data_type()) >
                temporal_rank(temporal_item->data_type()))
          temporal_item = args[i];
      }
    }
    /*
      Calculate a correct datetime precision, also including  values that are
      converted from decimal and float numbers, and possibly adjust the
      maximum length of the resulting string accordingly.
    */
    if (temporal_item != nullptr) {
      if (temporal_item->data_type() == MYSQL_TYPE_TIME) {
        for (uint i = 0; i < arg_count; i++)
          fsp_for_string =
              max<uint8>(fsp_for_string, args[i]->time_precision());
      } else if (temporal_item->data_type() == MYSQL_TYPE_DATETIME ||
                 temporal_item->data_type() == MYSQL_TYPE_TIMESTAMP) {
        for (uint i = 0; i < arg_count; i++)
          fsp_for_string =
              max<uint8>(fsp_for_string, args[i]->datetime_precision());
      }
      if (temporal_item->data_type() != MYSQL_TYPE_DATE && fsp_for_string > 0) {
        uint32 new_size = 0;
        if (temporal_item->data_type() == MYSQL_TYPE_DATETIME ||
            temporal_item->data_type() == MYSQL_TYPE_TIMESTAMP)
          new_size = MAX_DATETIME_WIDTH + 1 + fsp_for_string;
        else if (temporal_item->data_type() == MYSQL_TYPE_TIME)
          new_size = MAX_TIME_WIDTH + 1 + fsp_for_string;
        if (new_size > max_char_length()) {
          set_data_type_string(new_size);
        }
      }
    }
  }
  /*
  LEAST and GREATEST convert JSON values to strings before they are
  compared, so their JSON nature is lost. Raise a warning to
  indicate to the users that the values are not compared using the
  JSON comparator, as they might expect. Also update the field type
  of the result to reflect that the result is a string.
*/
  unsupported_json_comparison(arg_count, args,
                              "comparison of JSON in the "
                              "LEAST and GREATEST operators");
  if (data_type() == MYSQL_TYPE_JSON) set_data_type(MYSQL_TYPE_VARCHAR);
  return false;
}

bool Item_func_min_max::compare_as_dates() const {
  return temporal_item != nullptr &&
         is_temporal_type_with_date(temporal_item->data_type());
}

bool Item_func_min_max::cmp_datetimes(longlong *value) {
  THD *thd = current_thd;
  longlong res = 0;
  for (uint i = 0; i < arg_count; i++) {
    Item **arg = args + i;
    bool is_null;
    const longlong tmp =
        get_datetime_value(thd, &arg, nullptr, temporal_item, &is_null);

    // Check if we need to stop (because of error or KILL)  and stop the loop
    if (thd->is_error()) {
      null_value = is_nullable();
      return true;
    }
    if ((null_value = args[i]->null_value)) return true;
    if (i == 0 || (tmp < res) == m_is_least_func) res = tmp;
  }
  *value = res;
  return false;
}

bool Item_func_min_max::cmp_times(longlong *value) {
  longlong res = 0;
  for (uint i = 0; i < arg_count; i++) {
    const longlong tmp = args[i]->val_time_temporal();
    if ((null_value = args[i]->null_value)) return true;
    if (i == 0 || (tmp < res) == m_is_least_func) res = tmp;
  }
  *value = res;
  return false;
}

String *Item_func_min_max::str_op(String *str) {
  assert(fixed);
  null_value = false;
  if (compare_as_dates()) {
    longlong result = 0;
    if (cmp_datetimes(&result)) return error_str();

    /*
      If result is greater than 0, the winning argument was successfully
      converted to a time value and should be converted to a string
      formatted in accordance with the data type in temporal_item. Otherwise,
      the arguments should be compared based on their raw string value.
    */
    if (result > 0) {
      MYSQL_TIME ltime;
      const enum_field_types field_type = temporal_item->data_type();
      TIME_from_longlong_packed(&ltime, field_type, result);
      null_value = my_TIME_to_str(&ltime, str, fsp_for_string);
      if (null_value) return nullptr;
      if (str->needs_conversion(collation.collation)) {
        uint errors = 0;
        StringBuffer<STRING_BUFFER_USUAL_SIZE * 2> convert_string(nullptr);
        bool copy_failed =
            convert_string.copy(str->ptr(), str->length(), str->charset(),
                                collation.collation, &errors);
        if (copy_failed || errors || str->copy(convert_string))
          return error_str();
      }
      return str;
    }
  }

  // Find the least/greatest argument based on string value.
  String *res = nullptr;
  bool res_in_str = false;
  for (uint i = 0; i < arg_count; i++) {
    /*
      Because val_str() may reallocate the underlying buffer of its String
      parameter, it is paramount the passed String argument do not share an
      underlying buffer with the currently stored result against which it will
      be compared to ensure that String comparison operates on two
      non-overlapping buffers.
    */
    String *val_buf = res_in_str ? &m_string_buf : str;
    assert(!res || (res != val_buf && !res->uses_buffer_owned_by(val_buf)));
    String *val = eval_string_arg(collation.collation, args[i], val_buf);
    if (val == nullptr) {
      assert(current_thd->is_error() || (args[i]->null_value && is_nullable()));
      return error_str();
    }
    if (i == 0 ||
        (sortcmp(val, res, collation.collation) < 0) == m_is_least_func) {
      res = val;
      res_in_str = !res_in_str;
    }
  }
  res->set_charset(collation.collation);
  return res;
}

bool Item_func_min_max::date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  assert(fixed);
  longlong result = 0;
  if (cmp_datetimes(&result)) return true;
  TIME_from_longlong_packed(ltime, data_type(), result);
  int warnings;
  return check_date(*ltime, non_zero_date(*ltime), fuzzydate, &warnings);
}

bool Item_func_min_max::time_op(MYSQL_TIME *ltime) {
  assert(fixed);
  longlong result = 0;
  if (compare_as_dates()) {
    if (cmp_datetimes(&result)) return true;
    TIME_from_longlong_packed(ltime, data_type(), result);
    datetime_to_time(ltime);
    return false;
  }

  if (cmp_times(&result)) return true;
  TIME_from_longlong_time_packed(ltime, result);
  return false;
}

double Item_func_min_max::real_op() {
  assert(fixed);
  null_value = false;
  if (compare_as_dates()) {
    longlong result = 0;
    if (cmp_datetimes(&result)) return 0;
    return double_from_datetime_packed(temporal_item->data_type(), result);
  }

  // Find the least/greatest argument based on double value.
  double result = 0.0;
  for (uint i = 0; i < arg_count; i++) {
    const double tmp = args[i]->val_real();
    if ((null_value = args[i]->null_value)) return 0.0;
    if (i == 0 || (tmp < result) == m_is_least_func) result = tmp;
  }
  return result;
}

longlong Item_func_min_max::int_op() {
  assert(fixed);
  null_value = false;
  longlong res = 0;
  if (compare_as_dates()) {
    if (cmp_datetimes(&res)) return 0;
    return longlong_from_datetime_packed(temporal_item->data_type(), res);
  }

  // Find the least/greatest argument based on integer value.
  for (uint i = 0; i < arg_count; i++) {
    const longlong val = args[i]->val_int();
    if ((null_value = args[i]->null_value)) return 0;
#ifndef NDEBUG
    const Integer_value arg_val(val, args[i]->unsigned_flag);
    assert(!unsigned_flag || !arg_val.is_negative());
#endif
    const bool val_is_smaller = unsigned_flag ? static_cast<ulonglong>(val) <
                                                    static_cast<ulonglong>(res)
                                              : val < res;
    if (i == 0 || val_is_smaller == m_is_least_func) res = val;
  }
  return res;
}

my_decimal *Item_func_min_max::decimal_op(my_decimal *dec) {
  assert(fixed);
  null_value = false;
  if (compare_as_dates()) {
    longlong result = 0;
    if (cmp_datetimes(&result)) return error_decimal(dec);
    return my_decimal_from_datetime_packed(dec, temporal_item->data_type(),
                                           result);
  }

  // Find the least/greatest argument based on decimal value.
  my_decimal tmp_buf, *res = args[0]->val_decimal(dec);
  for (uint i = 0; i < arg_count; i++) {
    my_decimal *tmp = args[i]->val_decimal(res == dec ? &tmp_buf : dec);
    if ((null_value = args[i]->null_value)) return nullptr;
    if (i == 0 || (my_decimal_cmp(tmp, res) < 0) == m_is_least_func) res = tmp;
  }
  //  Result must me copied from temporary buffer to remain valid after return.
  if (res == &tmp_buf) {
    my_decimal2decimal(res, dec);
    res = dec;
  }
  return res;
}

double Item_func_min_max::val_real() {
  assert(fixed);
  if (has_temporal_arg() && data_type() == MYSQL_TYPE_VARCHAR)
    return real_op();  // For correct conversion from temporal value to string.
  return Item_func_numhybrid::val_real();
}

longlong Item_func_min_max::val_int() {
  assert(fixed);
  if (has_temporal_arg() && data_type() == MYSQL_TYPE_VARCHAR)
    return int_op();  // For correct conversion from temporal value to int.
  return Item_func_numhybrid::val_int();
}

my_decimal *Item_func_min_max::val_decimal(my_decimal *dec) {
  assert(fixed);
  if (has_temporal_arg() && data_type() == MYSQL_TYPE_VARCHAR)
    return decimal_op(
        dec);  // For correct conversion from temporal value to dec
  return Item_func_numhybrid::val_decimal(dec);
}

bool Item_rollup_group_item::get_date(MYSQL_TIME *ltime,
                                      my_time_flags_t fuzzydate) {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return true;
  }
  return (null_value = args[0]->get_date(ltime, fuzzydate));
}

bool Item_rollup_group_item::get_time(MYSQL_TIME *ltime) {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return true;
  }
  return (null_value = args[0]->get_time(ltime));
}

double Item_rollup_group_item::val_real() {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return 0.0;
  }
  const double res = args[0]->val_real();
  if ((null_value = args[0]->null_value)) return 0.0;
  return res;
}

longlong Item_rollup_group_item::val_int() {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return 0;
  }
  const longlong res = args[0]->val_int();
  if ((null_value = args[0]->null_value)) return 0;
  return res;
}

String *Item_rollup_group_item::val_str(String *str) {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return nullptr;
  }
  String *res = args[0]->val_str(str);
  if ((null_value = args[0]->null_value)) return nullptr;
  return res;
}

my_decimal *Item_rollup_group_item::val_decimal(my_decimal *dec) {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return nullptr;
  }
  my_decimal *res = args[0]->val_decimal(dec);
  if ((null_value = args[0]->null_value)) return nullptr;
  return res;
}

bool Item_rollup_group_item::val_json(Json_wrapper *result) {
  assert(fixed);
  if (rollup_null()) {
    null_value = true;
    return false;
  }
  const bool res = args[0]->val_json(result);
  null_value = args[0]->null_value;
  return res;
}

void Item_rollup_group_item::print(const THD *thd, String *str,
                                   enum_query_type query_type) const {
  if (query_type & QT_HIDE_ROLLUP_FUNCTIONS) {
    print_args(thd, str, 0, query_type);
    return;
  }

  str->append(func_name());
  str->append('(');
  print_args(thd, str, 0, query_type);
  str->append(',');
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", m_min_rollup_level);
  str->append(buf);
  str->append(')');
}

bool Item_rollup_group_item::eq_specific(const Item *item) const {
  return min_rollup_level() ==
         down_cast<const Item_rollup_group_item *>(item)->min_rollup_level();
}

TYPELIB *Item_rollup_group_item::get_typelib() const {
  return inner_item()->get_typelib();
}

longlong Item_func_length::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  if (!res) {
    null_value = true;
    return 0; /* purecov: inspected */
  }
  null_value = false;
  return (longlong)res->length();
}

longlong Item_func_vector_dim::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  null_value = false;
  if (res == nullptr || res->ptr() == nullptr) {
    return error_int(); /* purecov: inspected */
  }
  uint32 dimensions = get_dimensions(res->length(), Field_vector::precision);
  if (dimensions == UINT32_MAX) {
    my_error(ER_TO_VECTOR_CONVERSION, MYF(0), res->length(), res->ptr());
    return error_int(); /* purecov: inspected */
  }
  return (longlong)dimensions;
}

longlong Item_func_char_length::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  if (!res) {
    null_value = true;
    return 0; /* purecov: inspected */
  }
  null_value = false;
  return (longlong)res->numchars();
}

longlong Item_func_coercibility::val_int() {
  assert(fixed);
  null_value = false;
  return (longlong)args[0]->collation.derivation;
}

bool Item_func_locate::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 2)) return true;
  if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_LONGLONG)) return true;
  max_length = MY_INT32_NUM_DECIMAL_DIGITS;
  if (agg_arg_charsets_for_string_result(collation, args, 1)) return true;
  if (simplify_string_args(thd, collation, args + 1, 1)) return true;
  return false;
}

/*
   LOCATE(substr,str), LOCATE(substr,str,pos)
   Note that the argument order is switched here,
   see Locate_instantiator::instantiate in item_create.cc
 */
longlong Item_func_locate::val_int() {
  assert(fixed);
  // Evaluate the string argument first
  const CHARSET_INFO *cs = collation.collation;
  String *haystack = eval_string_arg(cs, args[0], &value1);
  if (haystack == nullptr) return error_int();

  // Evaluate substring argument in same character set as string argument
  String *needle = eval_string_arg(cs, args[1], &value2);
  if (needle == nullptr) return error_int();

  null_value = false;
  /* must be longlong to avoid truncation */
  longlong start_byte = 0;
  longlong start_pos = 0;

  if (arg_count == 3) {
    const longlong tmp = args[2]->val_int();
    if ((null_value = args[2]->null_value) || tmp <= 0) return 0;
    start_pos = tmp - 1;

    if (start_pos > static_cast<longlong>(haystack->numchars())) return 0;

    /* start_pos is now sufficiently valid to pass to charpos function */
    start_byte = haystack->charpos(static_cast<size_t>(start_pos));
  }

  if (needle->length() == 0)  // Found empty string at start
    return start_pos + 1;

  my_match_t match;
  if (!cs->coll->strstr(cs, haystack->ptr() + start_byte,
                        static_cast<size_t>(haystack->length() - start_byte),
                        needle->ptr(), needle->length(), &match))
    return 0;
  return static_cast<longlong>(match.mb_len) + start_pos + 1;
}

void Item_func_locate::print(const THD *thd, String *str,
                             enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("locate("));
  args[1]->print(thd, str, query_type);
  str->append(',');
  args[0]->print(thd, str, query_type);
  if (arg_count == 3) {
    str->append(',');
    args[2]->print(thd, str, query_type);
  }
  str->append(')');
}

longlong Item_func_validate_password_strength::val_int() {
  char buff[STRING_BUFFER_USUAL_SIZE];
  String value(buff, sizeof(buff), system_charset_info);
  String *field = args[0]->val_str(&value);
  if ((null_value = args[0]->null_value) || field->length() == 0) return 0;
  return (my_calculate_password_strength(field->ptr(), field->length()));
}

longlong Item_func_field::val_int() {
  assert(fixed);

  if (cmp_type == STRING_RESULT) {
    const CHARSET_INFO *cs = collation.collation;
    String *field = eval_string_arg(cs, args[0], &value);
    if (field == nullptr) return 0;
    for (uint i = 1; i < arg_count; i++) {
      String *tmp_value = eval_string_arg(cs, args[i], &tmp);
      if (tmp_value != nullptr && !sortcmp(field, tmp_value, cs)) {
        return i;
      }
    }
  } else if (cmp_type == INT_RESULT) {
    const longlong val = args[0]->val_int();
    if (args[0]->null_value) return 0;
    for (uint i = 1; i < arg_count; i++) {
      if (val == args[i]->val_int() && !args[i]->null_value) {
        return i;
      }
    }
  } else if (cmp_type == DECIMAL_RESULT) {
    my_decimal dec_arg_buf, *dec_arg, dec_buf,
        *dec = args[0]->val_decimal(&dec_buf);
    if (args[0]->null_value) return 0;
    for (uint i = 1; i < arg_count; i++) {
      dec_arg = args[i]->val_decimal(&dec_arg_buf);
      if (!args[i]->null_value && !my_decimal_cmp(dec_arg, dec)) {
        return i;
      }
    }
  } else {
    const double val = args[0]->val_real();
    if (args[0]->null_value) return 0;
    for (uint i = 1; i < arg_count; i++) {
      if (val == args[i]->val_real() && !args[i]->null_value) {
        return i;
      }
    }
  }
  return 0;
}

bool Item_func_field::resolve_type(THD *thd) {
  if (Item_int_func::resolve_type(thd)) return true;
  set_nullable(false);
  max_length = 3;
  cmp_type = args[0]->result_type();
  for (uint i = 1; i < arg_count; i++)
    cmp_type = item_cmp_type(cmp_type, args[i]->result_type());
  if (cmp_type == STRING_RESULT) {
    if (agg_arg_charsets_for_string_result(collation, args, 1)) return true;
    if (simplify_string_args(thd, collation, args + 1, arg_count - 1))
      return true;
  }
  return false;
}

longlong Item_func_ascii::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  if (!res) {
    null_value = true;
    return 0;
  }
  null_value = false;
  return (longlong)(res->length() ? (uchar)(*res)[0] : (uchar)0);
}

longlong Item_func_ord::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  if (!res) {
    null_value = true;
    return 0;
  }
  null_value = false;
  if (!res->length()) return 0;
  if (use_mb(res->charset())) {
    const char *str = res->ptr();
    uint32 n = 0, l = my_ismbchar(res->charset(), str, str + res->length());
    if (!l) return (longlong)((uchar)*str);
    while (l--) n = (n << 8) | (uint32)((uchar)*str++);
    return (longlong)n;
  }
  return (longlong)((uchar)(*res)[0]);
}

/* Search after a string in a string of strings separated by ',' */
/* Returns number of found type >= 1 or 0 if not found */
/* This optimizes searching in enums to bit testing! */

bool Item_func_find_in_set::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, -1)) return true;
  max_length = 3;  // 1-999

  if (agg_arg_charsets_for_comparison(cmp_collation, args, 2)) {
    return true;
  }
  if (args[0]->const_item() && args[1]->type() == FIELD_ITEM &&
      args[0]->may_eval_const_item(thd)) {
    Field *field = down_cast<Item_field *>(args[1])->field;
    // Bail during CREATE TABLE/INDEX so we don't look for absent typelib.
    if (field->is_wrapper_field()) return false;
    if (field->real_type() == MYSQL_TYPE_SET) {
      String *find = args[0]->val_str(&value);
      if (thd->is_error()) return true;
      if (find != nullptr) {
        // find is not NULL pointer so args[0] is not a null-value
        assert(!args[0]->null_value);
        m_enum_value = find_type(down_cast<Field_enum *>(field)->typelib,
                                 find->ptr(), find->length(), false);
      }
    }
  }
  return false;
}

static const char separator = ',';

longlong Item_func_find_in_set::val_int() {
  assert(fixed);

  null_value = false;

  if (m_enum_value != 0) {
    // enum_value is set iff args[0]->const_item() in resolve_type().
    assert(args[0]->const_item());

    const ulonglong tmp = static_cast<ulonglong>(args[1]->val_int());
    if (args[1]->null_value) return error_int();
    /*
      No need to check args[0]->null_value since enum_value is set iff
      args[0] is a non-null const item. Note: no assert on
      args[0]->null_value here because args[0] may have been replaced
      by an Item_cache on which val_int() has not been called. See
      BUG#11766317
    */
    return (tmp & (1ULL << (m_enum_value - 1))) ? m_enum_value : 0;
  }

  String *find = args[0]->val_str(&value);
  if (find == nullptr) return error_int();

  if (args[1]->type() == FIELD_ITEM &&
      down_cast<Item_field *>(args[1])->field->real_type() == MYSQL_TYPE_SET) {
    Field *field = down_cast<Item_field *>(args[1])->field;

    const ulonglong tmp = static_cast<ulonglong>(args[1]->val_int());
    if (args[1]->null_value) return error_int();

    uint value = find_type(down_cast<Field_enum *>(field)->typelib, find->ptr(),
                           find->length(), false);
    return (value != 0 && (tmp & (1ULL << (value - 1)))) ? value : 0;
  }

  String *buffer = args[1]->val_str(&value2);
  if (buffer == nullptr) return error_int();

  if (buffer->length() >= find->length()) {
    my_wc_t wc = 0;
    const CHARSET_INFO *cs = cmp_collation.collation;
    const char *str_begin = buffer->ptr();
    const char *str_end = buffer->ptr();
    const char *real_end = str_end + buffer->length();
    const uchar *find_str = (const uchar *)find->ptr();
    const size_t find_str_len = find->length();
    int position = 0;
    while (true) {
      int symbol_len;
      if ((symbol_len =
               cs->cset->mb_wc(cs, &wc, pointer_cast<const uchar *>(str_end),
                               pointer_cast<const uchar *>(real_end))) > 0) {
        const char *substr_end = str_end + symbol_len;
        const bool is_last_item = (substr_end == real_end);
        const bool is_separator = (wc == (my_wc_t)separator);
        if (is_separator || is_last_item) {
          position++;
          if (is_last_item && !is_separator) str_end = substr_end;
          if (!my_strnncoll(cs, (const uchar *)str_begin,
                            (uint)(str_end - str_begin), find_str,
                            find_str_len))
            return (longlong)position;
          else
            str_begin = substr_end;
        }
        str_end = substr_end;
      } else if (str_end - str_begin == 0 && find_str_len == 0 &&
                 wc == (my_wc_t)separator) {
        return ++position;
      } else {
        return 0;
      }
    }
  }
  return 0;
}

longlong Item_func_bit_count::val_int() {
  assert(fixed);
  using std::popcount;
  if (bit_func_returns_binary(args[0], nullptr)) {
    String *s = args[0]->val_str(&str_value);
    if (args[0]->null_value || !s) return error_int();

    const auto val = pointer_cast<const unsigned char *>(s->ptr());

    longlong len = 0;
    size_t i = 0;
    const size_t arg_length = s->length();
    while (i + sizeof(longlong) <= arg_length) {
      len += popcount<ulonglong>(longlongget(&val[i]));
      i += sizeof(longlong);
    }
    if (i < arg_length) {
      ulonglong d = 0;
      memcpy(&d, &val[i], arg_length - i);
      len += popcount(d);
    }

    null_value = false;
    return len;
  }

  const ulonglong value = args[0]->val_uint();
  if (args[0]->null_value) return error_int(); /* purecov: inspected */

  null_value = false;
  return popcount(value);
}

/****************************************************************************
** Functions to handle dynamic loadable functions
****************************************************************************/

udf_handler::udf_handler(udf_func *udf_arg)
    : u_d(udf_arg),
      m_args_extension(),
      m_return_value_extension(&my_charset_bin, result_type()) {}

void udf_handler::cleanup() {
  if (!m_original || !m_initialized) return;

  clean_buffers();
  /*
    Make sure to not free the handler from the cleanup() call when
    (re)preparing the UDF function call. The handler allocated by
    udf_handler::fix_fields() will be used in execution.
  */
  THD *thd = current_thd;
  if (thd->stmt_arena->is_stmt_prepare() && thd->stmt_arena->is_repreparing)
    return;

  if (m_init_func_called && u_d->func_deinit != nullptr) {
    (*u_d->func_deinit)(&initid);
    m_init_func_called = false;
  }
  DEBUG_SYNC(current_thd, "udf_handler_destroy_sync");
  free_handler();
}

void udf_handler::clean_buffers() {
  if (buffers == nullptr) return;
  for (uint i = 0; i < f_args.arg_count; i++) {
    buffers[i].mem_free();
    arg_buffers[i].mem_free();
  }
}

void udf_handler::free_handler() {
  // deinit() should have been called by cleanup()
  assert(m_original && m_initialized && u_d != nullptr);
  free_udf(u_d);
  u_d = nullptr;
  m_initialized = false;
}

bool Item_udf_func::fix_fields(THD *thd, Item **) {
  assert(!fixed);
  assert(!thd->is_error());
  if (udf.fix_fields(thd, this, arg_count, args)) return true;
  if (thd->is_error()) return true;
  used_tables_cache = udf.used_tables_cache;
  m_non_deterministic = is_non_deterministic();
  fixed = true;
  return false;
}

bool udf_handler::fix_fields(THD *thd, Item_result_field *func, uint arg_count,
                             Item **arguments) {
  uchar buff[STACK_BUFF_ALLOC];  // Max argument in function
  DBUG_TRACE;

  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return true;  // Fatal error flag is set!

  udf_func *tmp_udf = find_udf(u_d->name.str, (uint)u_d->name.length, true);

  if (!tmp_udf) {
    my_error(ER_CANT_FIND_UDF, MYF(0), u_d->name.str);
    return true;
  }
  u_d = tmp_udf;
  args = arguments;

  m_initialized = true;  // Use count was incremented by find_udf()
  const bool is_in_prepare =
      thd->stmt_arena->is_stmt_prepare() && !thd->stmt_arena->is_repreparing;
  /*
    RAII wrapper to free the memory allocated in case of any failure while
    initializing the UDF
  */
  class cleanup_guard {
   public:
    cleanup_guard(udf_handler *udf) : m_udf(udf) { assert(udf); }
    ~cleanup_guard() {
      if (m_udf == nullptr) return;
      m_udf->clean_buffers();
      m_udf->free_handler();
    }
    void defer() { m_udf = nullptr; }

   private:
    udf_handler *m_udf;
  } udf_fun_guard(this);

  /* Fix all arguments */
  func->set_nullable(false);
  used_tables_cache = 0;

  if ((f_args.arg_count = arg_count)) {
    if (!(f_args.arg_type =
              (Item_result *)(*THR_MALLOC)
                  ->Alloc(f_args.arg_count * sizeof(Item_result)))) {
      return true;
    }
    uint i;
    Item **arg, **arg_end;
    for (i = 0, arg = arguments, arg_end = arguments + arg_count;
         arg != arg_end; arg++, i++) {
      if (!(*arg)->fixed && (*arg)->fix_fields(thd, arg)) {
        return true;
      }

      if ((*arg)->data_type() == MYSQL_TYPE_INVALID &&
          (*arg)->propagate_type(thd, MYSQL_TYPE_VARCHAR)) {
        return true;
      }

      // we can't assign 'item' before, because fix_fields() can change arg
      Item *item = *arg;
      if (item->check_cols(1)) {
        return true;
      }
      /*
        TODO: We should think about this. It is not always
        right way just to set an UDF result to return my_charset_bin
        if one argument has binary sorting order.
        The result collation should be calculated according to arguments
        derivations in some cases and should not in other cases.
        Moreover, some arguments can represent a numeric input
        which doesn't effect the result character set and collation.
        There is no a general rule for UDF. Everything depends on
        the particular user defined function.
      */
      if (item->collation.collation->state & MY_CS_BINSORT)
        func->collation.set(&my_charset_bin);
      func->m_nullable |= item->m_nullable;
      func->add_accum_properties(item);
      used_tables_cache |= item->used_tables();
      f_args.arg_type[i] = item->result_type();
    }

    if (!(buffers = (*THR_MALLOC)->ArrayAlloc<String>(arg_count)) ||
        !(arg_buffers = (*THR_MALLOC)->ArrayAlloc<String>(arg_count)) ||
        !(f_args.args =
              (char **)(*THR_MALLOC)->Alloc(arg_count * sizeof(char *))) ||
        !(f_args.lengths =
              (ulong *)(*THR_MALLOC)->Alloc(arg_count * sizeof(long))) ||
        !(f_args.maybe_null =
              (char *)(*THR_MALLOC)->Alloc(arg_count * sizeof(char))) ||
        !(num_buffer = (char *)(*THR_MALLOC)
                           ->Alloc(arg_count * ALIGN_SIZE(sizeof(double)))) ||
        !(f_args.attributes =
              (char **)(*THR_MALLOC)->Alloc(arg_count * sizeof(char *))) ||
        !(f_args.attribute_lengths =
              (ulong *)(*THR_MALLOC)->Alloc(arg_count * sizeof(long))) ||
        !(m_args_extension.charset_info =
              (*THR_MALLOC)
                  ->ArrayAlloc<const CHARSET_INFO *>(f_args.arg_count))) {
      return true;
    }
  }

  if (func->resolve_type(thd)) return true;

  /*
    Calculation of constness and non-deterministic property of a UDF is done
    according to this algorithm:
    - If any argument to the UDF is non-const, the used tables information
      and constness of the UDF is derived from the aggregated properties of
      the arguments.
    - If all arguments to the UDF are const and the init function specifies
      the UDF to be non-const, the UDF is marked as non-deterministic.
    Thus, initid.const_item is only considered when all arguments are const,
    and it's use is thus slightly inconsistent. However, the current behavior
    seems to work well in most circumstances.

    @todo Clarify the semantics of initid.const_item and make it affect
          the constness and non-deterministic property more consistently.
  */
  initid.max_length = func->max_length;
  initid.maybe_null = func->m_nullable;
  initid.const_item = used_tables_cache == 0;
  initid.decimals = func->decimals;
  initid.ptr = nullptr;
  initid.extension = &m_return_value_extension;

  if (is_in_prepare && !initid.const_item) {
    udf_fun_guard.defer();
    return false;
  }
  if (u_d->func_init) {
    if (call_init_func()) {
      return true;
    }
    func->max_length = min<uint32>(initid.max_length, MAX_BLOB_WIDTH);
    func->m_nullable = initid.maybe_null;
    if (!initid.const_item && used_tables_cache == 0)
      used_tables_cache = RAND_TABLE_BIT;
    func->decimals = min<uint>(initid.decimals, DECIMAL_NOT_SPECIFIED);
    /*
      For UDFs of type string, override character set and collation from
      return value extension specification.
    */
    if (result_type() == STRING_RESULT)
      func->set_data_type_string(func->max_length,
                                 m_return_value_extension.charset_info);
  }
  /*
    UDF initialization complete so leave the freeing up resources to
    cleanup method.
  */
  udf_fun_guard.defer();
  return false;
}

bool udf_handler::call_init_func() {
  char init_msg_buff[MYSQL_ERRMSG_SIZE];
  *init_msg_buff = '\0';
  char *to = num_buffer;
  f_args.extension = &m_args_extension;
  THD *thd = current_thd;

  for (uint i = 0; i < f_args.arg_count; i++) {
    /*
     For a constant argument i, args->args[i] points to the argument value.
     For non-constant, args->args[i] is NULL.
    */
    f_args.args[i] = nullptr;  // Non-const unless updated below

    f_args.lengths[i] = args[i]->max_length;
    f_args.maybe_null[i] = args[i]->m_nullable;
    f_args.attributes[i] = const_cast<char *>(args[i]->item_name.ptr());
    f_args.attribute_lengths[i] = args[i]->item_name.length();
    m_args_extension.charset_info[i] = args[i]->collation.collation;

    if (args[i]->const_for_execution() && !args[i]->has_subquery() &&
        !args[i]->has_stored_program()) {
      switch (args[i]->result_type()) {
        case STRING_RESULT:
        case DECIMAL_RESULT: {
          get_string(i);
          if (thd->is_error()) return true;
          break;
        }
        case INT_RESULT:
          *((longlong *)to) = args[i]->val_int();
          if (thd->is_error()) return true;
          if (args[i]->null_value) continue;
          f_args.args[i] = to;
          to += ALIGN_SIZE(sizeof(longlong));
          break;
        case REAL_RESULT:
          *((double *)to) = args[i]->val_real();
          if (thd->is_error()) return true;
          if (args[i]->null_value) continue;
          f_args.args[i] = to;
          to += ALIGN_SIZE(sizeof(double));
          break;
        case ROW_RESULT:
        default:
          // This case should never be chosen
          assert(0);
          break;
      }
    }
  }
  Udf_func_init init = u_d->func_init;
  if ((error = (uchar)init(&initid, &f_args, init_msg_buff))) {
    my_error(ER_CANT_INITIALIZE_UDF, MYF(0), u_d->name.str, init_msg_buff);
    return true;
  }
  m_init_func_called = true;
  return false;
}

bool udf_handler::get_arguments() {
  if (error) return true;  // Got an error earlier
  char *to = num_buffer;
  for (uint i = 0; i < f_args.arg_count; i++) {
    f_args.args[i] = nullptr;
    switch (f_args.arg_type[i]) {
      case STRING_RESULT:
        if (get_and_convert_string(i)) return true;
        break;
      case DECIMAL_RESULT:
        get_string(i);
        break;
      case INT_RESULT:
        *((longlong *)to) = args[i]->val_int();
        if (!args[i]->null_value) {
          f_args.args[i] = to;
          to += ALIGN_SIZE(sizeof(longlong));
        }
        break;
      case REAL_RESULT:
        *((double *)to) = args[i]->val_real();
        if (!args[i]->null_value) {
          f_args.args[i] = to;
          to += ALIGN_SIZE(sizeof(double));
        }
        break;
      case ROW_RESULT:
      default:
        // This case should never be chosen
        assert(0);
        break;
    }
  }
  return false;
}

double udf_handler::val_real(bool *null_value) {
  assert(is_initialized());
  is_null = 0;
  if (get_arguments()) {
    *null_value = true;
    return 0.0;
  }
  Udf_func_double func = (Udf_func_double)u_d->func;
  const double tmp = func(&initid, &f_args, &is_null, &error);
  if (is_null || error) {
    *null_value = true;
    return 0.0;
  }
  *null_value = false;
  return tmp;
}

longlong udf_handler::val_int(bool *null_value) {
  assert(is_initialized());
  is_null = 0;
  if (get_arguments()) {
    *null_value = true;
    return 0LL;
  }
  DEBUG_SYNC(current_thd, "execute_uninstall_component");
  Udf_func_longlong func = (Udf_func_longlong)u_d->func;
  const longlong tmp = func(&initid, &f_args, &is_null, &error);
  if (is_null || error) {
    *null_value = true;
    return 0LL;
  }
  *null_value = false;
  return tmp;
}

/**
  @return
    (String*)NULL in case of NULL values
*/
String *udf_handler::val_str(String *str, String *save_str) {
  uchar is_null_tmp = 0;
  ulong res_length;
  DBUG_TRACE;
  assert(is_initialized());

  if (get_arguments()) return nullptr;
  Udf_func_string func = reinterpret_cast<Udf_func_string>(u_d->func);

  if ((res_length = str->alloced_length()) <
      MAX_FIELD_WIDTH) {  // This happens VERY seldom
    if (str->alloc(MAX_FIELD_WIDTH)) {
      error = 1;
      return nullptr;
    }
  }
  char *res =
      func(&initid, &f_args, str->ptr(), &res_length, &is_null_tmp, &error);
  DBUG_PRINT("info", ("udf func returned, res_length: %lu", res_length));
  if (is_null_tmp || !res || error)  // The !res is for safety
  {
    DBUG_PRINT("info", ("Null or error"));
    return nullptr;
  }

  String *res_str = result_string(res, res_length, str, save_str);
  DBUG_PRINT("exit", ("res_str: %s", res_str->ptr()));
  return res_str;
}

/*
  For the moment, UDF functions are returning DECIMAL values as strings
*/

my_decimal *udf_handler::val_decimal(bool *null_value, my_decimal *dec_buf) {
  char buf[DECIMAL_MAX_STR_LENGTH + 1];
  ulong res_length = DECIMAL_MAX_STR_LENGTH;

  assert(is_initialized());

  if (get_arguments()) {
    *null_value = true;
    return nullptr;
  }
  Udf_func_string func = reinterpret_cast<Udf_func_string>(u_d->func);

  char *res = func(&initid, &f_args, buf, &res_length, &is_null, &error);
  if (is_null || error) {
    *null_value = true;
    return nullptr;
  }
  const char *end = res + res_length;
  str2my_decimal(E_DEC_FATAL_ERROR, res, dec_buf, &end);
  return dec_buf;
}

void udf_handler::clear() {
  assert(is_initialized());
  is_null = 0;
  Udf_func_clear func = u_d->func_clear;
  func(&initid, &is_null, &error);
}

void udf_handler::add(bool *null_value) {
  assert(is_initialized());
  if (get_arguments()) {
    *null_value = true;
    return;
  }
  Udf_func_add func = u_d->func_add;
  func(&initid, &f_args, &is_null, &error);
  *null_value = (bool)(is_null || error);
}

/**
  Process the result string returned by the udf() method. The charset
  info might have changed therefore updates the same String. If user
  used the input String as result string then return the same.

  @param [in] res the result string returned by the udf() method.
  @param [in] res_length  length of the string
  @param [out] str The input result string passed to the udf() method
  @param [out] save_str Keeps copy of the returned String

  @returns A pointer to either the str or save_str that points
            to final result String
*/
String *udf_handler::result_string(const char *res, size_t res_length,
                                   String *str, String *save_str) {
  const auto *charset = m_return_value_extension.charset_info;
  String *res_str = nullptr;
  if (res == str->ptr()) {
    res_str = str;
    res_str->length(res_length);
    res_str->set_charset(charset);
  } else {
    res_str = save_str;
    res_str->set(res, res_length, charset);
  }
  return res_str;
}

/**
  Get the details of the input String arguments.

  @param [in] index of the argument to be looked in the arguments array
*/
void udf_handler::get_string(uint index) {
  String *res = args[index]->val_str(&buffers[index]);
  if (!args[index]->null_value) {
    f_args.args[index] = res->ptr();
    f_args.lengths[index] = res->length();
  } else {
    f_args.lengths[index] = 0;
  }
}

/**
  Get the details of the input String argument.
  If the charset of the input argument is not same as specified by the
  user then convert the String.

  @param [in] index of the argument to be looked in the arguments array

  @retval false Able to fetch the argument details
  @retval true  Otherwise
*/
bool udf_handler::get_and_convert_string(uint index) {
  String *res = args[index]->val_str(&buffers[index]);

  if (!args[index]->null_value) {
    uint errors = 0;
    if (arg_buffers[index].copy(res->ptr(), res->length(), res->charset(),
                                m_args_extension.charset_info[index],
                                &errors)) {
      return true;
    }
    if (errors) {
      report_conversion_error(m_args_extension.charset_info[index], res->ptr(),
                              res->length(), res->charset());
      return true;
    }
    f_args.args[index] = arg_buffers[index].c_ptr_safe();
    f_args.lengths[index] = arg_buffers[index].length();
  } else {
    f_args.lengths[index] = 0;
  }
  return false;
}

bool Item_udf_func::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_has_udf();
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UDF);
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

void Item_udf_func::cleanup() {
  udf.cleanup();
  str_value.mem_free();
  Item_func::cleanup();
}

void Item_udf_func::print(const THD *thd, String *str,
                          enum_query_type query_type) const {
  str->append(func_name());
  str->append('(');
  for (uint i = 0; i < arg_count; i++) {
    if (i != 0) str->append(',');
    args[i]->print_item_w_name(thd, str, query_type);
  }
  str->append(')');
}

// RAII class to handle THD::in_loadable_function state.
class THD_in_loadable_function_handler {
 public:
  THD_in_loadable_function_handler() {
    m_thd = current_thd;
    m_saved_thd_in_loadable_function = m_thd->in_loadable_function;
    m_thd->in_loadable_function = true;
  }

  ~THD_in_loadable_function_handler() {
    m_thd->in_loadable_function = m_saved_thd_in_loadable_function;
  }

 private:
  THD *m_thd;
  bool m_saved_thd_in_loadable_function;
};

double Item_func_udf_float::val_real() {
  assert(fixed);
  THD_in_loadable_function_handler thd_in_loadable_function_handler;
  DBUG_PRINT("info", ("result_type: %d  arg_count: %d", args[0]->result_type(),
                      arg_count));
  return udf.val_real(&null_value);
}

String *Item_func_udf_float::val_str(String *str) {
  assert(fixed);
  const double nr = val_real();
  if (null_value) return nullptr; /* purecov: inspected */
  str->set_real(nr, decimals, &my_charset_bin);
  return str;
}

longlong Item_func_udf_int::val_int() {
  assert(fixed);
  THD_in_loadable_function_handler thd_in_loadable_function_handler;
  return udf.val_int(&null_value);
}

String *Item_func_udf_int::val_str(String *str) {
  assert(fixed);
  const longlong nr = val_int();
  if (null_value) return nullptr;
  str->set_int(nr, unsigned_flag, &my_charset_bin);
  return str;
}

longlong Item_func_udf_decimal::val_int() {
  my_decimal dec_buf, *dec = val_decimal(&dec_buf);
  longlong result;
  if (null_value) return 0;
  my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
  return result;
}

double Item_func_udf_decimal::val_real() {
  my_decimal dec_buf, *dec = val_decimal(&dec_buf);
  double result;
  if (null_value) return 0.0;
  my_decimal2double(E_DEC_FATAL_ERROR, dec, &result);
  return result;
}

my_decimal *Item_func_udf_decimal::val_decimal(my_decimal *dec_buf) {
  THD_in_loadable_function_handler thd_in_loadable_function_handler;
  return udf.val_decimal(&null_value, dec_buf);
}

String *Item_func_udf_decimal::val_str(String *str) {
  my_decimal dec_buf, *dec = val_decimal(&dec_buf);
  if (null_value) return nullptr;
  if (str->length() < DECIMAL_MAX_STR_LENGTH)
    str->length(DECIMAL_MAX_STR_LENGTH);
  my_decimal_round(E_DEC_FATAL_ERROR, dec, decimals, false, &dec_buf);
  my_decimal2string(E_DEC_FATAL_ERROR, &dec_buf, str);
  return str;
}

bool Item_func_udf_decimal::resolve_type(THD *) {
  set_data_type(MYSQL_TYPE_NEWDECIMAL);
  fix_num_length_and_dec();
  return false;
}

/* Default max_length is max argument length */

bool Item_func_udf_str::resolve_type(THD *) {
  uint result_length = 0;
  for (uint i = 0; i < arg_count; i++)
    result_length = max(result_length, args[i]->max_length);
  // If the UDF has an init function, this may be overridden later.
  set_data_type_string(result_length, &my_charset_bin);
  return false;
}

String *Item_func_udf_str::val_str(String *str) {
  assert(fixed);
  THD_in_loadable_function_handler thd_in_loadable_function_handler;
  String *res = udf.val_str(str, &str_value);
  null_value = !res;
  return res;
}

bool Item_source_pos_wait::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

/**
  Wait until we are at or past the given position in the master binlog
  on the slave.
*/

longlong Item_source_pos_wait::val_int() {
  assert(fixed);
  THD *thd = current_thd;
  String *log_name = args[0]->val_str(&value);
  int event_count = 0;

  null_value = false;
  if (thd->slave_thread || !log_name || !log_name->length()) {
    null_value = true;
    return 0;
  }
  Master_info *mi;
  const longlong pos = (ulong)args[1]->val_int();
  const double timeout = (arg_count >= 3) ? args[2]->val_real() : 0;
  if (timeout < 0) {
    if (thd->is_strict_mode()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "SOURCE_POS_WAIT.");
    } else {
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                          ER_THD(thd, ER_WRONG_ARGUMENTS), "SOURCE_POS_WAIT.");
      null_value = true;
    }
    return 0;
  }

  channel_map.rdlock();

  if (arg_count == 4) {
    String *channel_str;
    if (!(channel_str = args[3]->val_str(&value))) {
      null_value = true;
      return 0;
    }

    mi = channel_map.get_mi(channel_str->ptr());

  } else {
    if (channel_map.get_num_instances() > 1) {
      mi = nullptr;
      my_error(ER_REPLICA_MULTIPLE_CHANNELS_CMD, MYF(0));
    } else
      mi = channel_map.get_default_channel_mi();
  }

  if (mi != nullptr) mi->inc_reference();

  channel_map.unlock();

  if (mi == nullptr || (event_count = mi->rli->wait_for_pos(thd, log_name, pos,
                                                            timeout)) == -2) {
    null_value = true;
    event_count = 0;
  }

  if (mi != nullptr) mi->dec_reference();
  return event_count;
}

longlong Item_master_pos_wait::val_int() {
  push_deprecated_warn(current_thd, "MASTER_POS_WAIT", "SOURCE_POS_WAIT");
  return Item_source_pos_wait::val_int();
}

/**
  Enables a session to wait on a condition until a timeout or a network
  disconnect occurs.

  @remark The connection is polled every m_interrupt_interval nanoseconds.
*/

class Interruptible_wait {
  THD *m_thd;
  struct timespec m_abs_timeout;
  static const ulonglong m_interrupt_interval;

 public:
  Interruptible_wait(THD *thd) : m_thd(thd) {}

  ~Interruptible_wait() = default;

 public:
  /**
    Set the absolute timeout.

    @param timeout The amount of time in nanoseconds to wait
  */
  void set_timeout(ulonglong timeout) {
    /*
      Calculate the absolute system time at the start so it can
      be controlled in slices. It relies on the fact that once
      the absolute time passes, the timed wait call will fail
      automatically with a timeout error.
    */
    set_timespec_nsec(&m_abs_timeout, timeout);
  }

  /** The timed wait. */
  int wait(mysql_cond_t *, mysql_mutex_t *);
};

/** Time to wait before polling the connection status. */
const ulonglong Interruptible_wait::m_interrupt_interval = 5 * 1000000000ULL;

/**
  Wait for a given condition to be signaled.

  @param cond   The condition variable to wait on.
  @param mutex  The associated mutex.

  @remark The absolute timeout is preserved across calls.

  @retval return value from mysql_cond_timedwait
*/

int Interruptible_wait::wait(mysql_cond_t *cond, mysql_mutex_t *mutex) {
  int error;
  struct timespec timeout;

  while (true) {
    /* Wait for a fixed interval. */
    set_timespec_nsec(&timeout, m_interrupt_interval);

    /* But only if not past the absolute timeout. */
    if (cmp_timespec(&timeout, &m_abs_timeout) > 0) timeout = m_abs_timeout;

    error = mysql_cond_timedwait(cond, mutex, &timeout);
    if (is_timeout(error)) {
      /* Return error if timed out or connection is broken. */
      if (!cmp_timespec(&timeout, &m_abs_timeout) || !m_thd->is_connected())
        break;
    }
    /* Otherwise, propagate status to the caller. */
    else
      break;
  }

  return error;
}

/*
  User-level locks implementation.
*/

/**
  For locks with EXPLICIT duration, MDL returns a new ticket
  every time a lock is granted. This allows to implement recursive
  locks without extra allocation or additional data structures, such
  as below. However, if there are too many tickets in the same
  MDL_context, MDL_context::find_ticket() is getting too slow,
  since it's using a linear search.
  This is why a separate structure is allocated for a user
  level lock held by connection, and before requesting a new lock from MDL,
  GET_LOCK() checks thd->ull_hash if such lock is already granted,
  and if so, simply increments a reference counter.
*/

struct User_level_lock {
  MDL_ticket *ticket;
  uint refs;
};

/**
  Release all user level locks for this THD.
*/

void mysql_ull_cleanup(THD *thd) {
  DBUG_TRACE;

  for (const auto &key_and_value : thd->ull_hash) {
    User_level_lock *ull = key_and_value.second;
    thd->mdl_context.release_lock(ull->ticket);
    my_free(ull);
  }

  thd->ull_hash.clear();
}

/**
  Set explicit duration for metadata locks corresponding to
  user level locks to protect them from being released at the end
  of transaction.
*/

void mysql_ull_set_explicit_lock_duration(THD *thd) {
  DBUG_TRACE;

  for (const auto &key_and_value : thd->ull_hash) {
    User_level_lock *ull = key_and_value.second;
    thd->mdl_context.set_lock_duration(ull->ticket, MDL_EXPLICIT);
  }
}

/**
  When MDL detects a lock wait timeout, it pushes an error into the statement
  diagnostics area. For GET_LOCK(), lock wait timeout is not an error, but a
  special return value (0). NULL is returned in case of error. Capture and
  suppress lock wait timeout.
  We also convert ER_LOCK_DEADLOCK error to ER_USER_LOCK_DEADLOCK error.
  The former means that implicit rollback of transaction has occurred
  which doesn't (and should not) happen when we get deadlock while waiting
  for user-level lock.
*/

class User_level_lock_wait_error_handler : public Internal_error_handler {
 public:
  User_level_lock_wait_error_handler() : m_lock_wait_timeout(false) {}

  bool got_timeout() const { return m_lock_wait_timeout; }

  bool handle_condition(THD *, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *,
                        const char *) override {
    if (sql_errno == ER_LOCK_WAIT_TIMEOUT) {
      m_lock_wait_timeout = true;
      return true;
    } else if (sql_errno == ER_LOCK_DEADLOCK) {
      my_error(ER_USER_LOCK_DEADLOCK, MYF(0));
      return true;
    }

    return false;
  }

 private:
  bool m_lock_wait_timeout;
};

class MDL_lock_get_owner_thread_id_visitor : public MDL_context_visitor {
 public:
  MDL_lock_get_owner_thread_id_visitor() : m_owner_id(0) {}

  void visit_context(const MDL_context *ctx) override {
    m_owner_id = ctx->get_owner()->get_thd()->thread_id();
  }

  my_thread_id get_owner_id() const { return m_owner_id; }

 private:
  my_thread_id m_owner_id;
};

/**
  Helper function which checks if user-level lock name is acceptable
  and converts it to system charset (utf8). Error is emitted if name
  is not acceptable. Name is also lowercased to ensure that user-level
  lock names are treated in case-insensitive fashion even though MDL
  subsystem which used by implementation does binary comparison of keys.

  @param buff      Buffer for lowercased name in system charset of
                   NAME_LEN + 1 bytes length.
  @param org_name  Original string passed as name parameter to
                   user-level lock function.

  @return True in case of error, false on success.
*/

static bool check_and_convert_ull_name(char *buff, const String *org_name) {
  if (!org_name || !org_name->length()) {
    my_error(ER_USER_LOCK_WRONG_NAME, MYF(0), (org_name ? "" : "NULL"));
    return true;
  }

  const char *well_formed_error_pos;
  const char *cannot_convert_error_pos;
  const char *from_end_pos;
  size_t bytes_copied;

  bytes_copied = well_formed_copy_nchars(
      system_charset_info, buff, NAME_LEN, org_name->charset(), org_name->ptr(),
      org_name->length(), NAME_CHAR_LEN, &well_formed_error_pos,
      &cannot_convert_error_pos, &from_end_pos);

  if (well_formed_error_pos || cannot_convert_error_pos ||
      from_end_pos < org_name->ptr() + org_name->length()) {
    const ErrConvString err(org_name);
    if (well_formed_error_pos || cannot_convert_error_pos)
      my_error(ER_USER_LOCK_WRONG_NAME, MYF(0), err.ptr());
    else
      my_error(ER_USER_LOCK_OVERLONG_NAME, MYF(0), err.ptr(),
               (int)NAME_CHAR_LEN);
    return true;
  }

  buff[bytes_copied] = '\0';

  my_casedn_str(system_charset_info, buff);

  return false;
}

bool Item_func_get_lock::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/**
  Get a user level lock.

  @note Sets null_value to true on error.

  @note This means that SQL-function GET_LOCK() returns:
        1    - if lock was acquired.
        0    - if lock was not acquired due to timeout.
        NULL - in case of error such as bad lock name, deadlock,
               thread being killed (also error is emitted).

  @retval
    1    : Got lock
  @retval
    0    : Timeout, error.
*/

longlong Item_func_get_lock::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  ulonglong timeout = args[1]->val_int();
  char name[NAME_LEN + 1];
  THD *thd = current_thd;
  DBUG_TRACE;

  null_value = true;
  /*
    In slave thread no need to get locks, everything is serialized. Anyway
    there is no way to make GET_LOCK() work on slave like it did on master
    (i.e. make it return exactly the same value) because we don't have the
    same other concurrent threads environment. No matter what we return here,
    it's not guaranteed to be same as on master. So we always return 1.
  */
  if (thd->slave_thread) {
    null_value = false;
    return 1;
  }

  if (check_and_convert_ull_name(name, res)) return 0;

  DBUG_PRINT("info", ("lock %s, thd=%lu", name, (ulong)thd->real_id));

  /*
    Convert too big and negative timeout values to INT_MAX32.
    This gives robust, "infinite" wait on all platforms.
  */
  if (timeout > INT_MAX32) timeout = INT_MAX32;

  MDL_request ull_request;
  MDL_REQUEST_INIT(&ull_request, MDL_key::USER_LEVEL_LOCK, "", name,
                   MDL_EXCLUSIVE, MDL_EXPLICIT);
  std::string ull_key(pointer_cast<const char *>(ull_request.key.ptr()),
                      ull_request.key.length());

  const auto it = thd->ull_hash.find(ull_key);
  if (it != thd->ull_hash.end()) {
    /* Recursive lock. */
    it->second->refs++;
    null_value = false;
    return 1;
  }

  User_level_lock_wait_error_handler error_handler;

  thd->push_internal_handler(&error_handler);
  bool error =
      thd->mdl_context.acquire_lock(&ull_request, static_cast<ulong>(timeout));
  (void)thd->pop_internal_handler();

  if (error) {
    /*
      Return 0 in case of timeout and NULL in case of deadlock/other
      errors. In the latter case error (e.g. ER_USER_LOCK_DEADLOCK)
      will be reported as well.
    */
    if (error_handler.got_timeout()) null_value = false;
    return 0;
  }

  User_level_lock *ull = nullptr;
  ull = reinterpret_cast<User_level_lock *>(
      my_malloc(key_memory_User_level_lock, sizeof(User_level_lock), MYF(0)));

  if (ull == nullptr) {
    thd->mdl_context.release_lock(ull_request.ticket);
    return 0;
  }

  ull->ticket = ull_request.ticket;
  ull->refs = 1;

  thd->ull_hash.emplace(ull_key, ull);
  null_value = false;

  return 1;
}

bool Item_func_release_lock::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/**
  Release a user level lock.

  @note Sets null_value to true on error/if no connection holds such lock.

  @note This means that SQL-function RELEASE_LOCK() returns:
        1    - if lock was held by this connection and was released.
        0    - if lock was held by some other connection (and was not released).
        NULL - if name of lock is bad or if it was not held by any connection
               (in the former case also error will be emitted),

  @return
    - 1 if lock released
    - 0 if lock wasn't held/error.
*/

longlong Item_func_release_lock::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  char name[NAME_LEN + 1];
  THD *thd = current_thd;
  DBUG_TRACE;

  null_value = true;

  if (check_and_convert_ull_name(name, res)) return 0;

  DBUG_PRINT("info", ("lock %s", name));

  MDL_key ull_key;
  ull_key.mdl_key_init(MDL_key::USER_LEVEL_LOCK, "", name);

  const auto it = thd->ull_hash.find(
      std::string(pointer_cast<const char *>(ull_key.ptr()), ull_key.length()));
  if (it == thd->ull_hash.end()) {
    /*
      When RELEASE_LOCK() is called for lock which is not owned by the
      connection it should return 0 or NULL depending on whether lock
      is owned by any other connection or not.
    */
    MDL_lock_get_owner_thread_id_visitor get_owner_visitor;

    if (thd->mdl_context.find_lock_owner(&ull_key, &get_owner_visitor))
      return 0;

    null_value = get_owner_visitor.get_owner_id() == 0;

    return 0;
  }
  User_level_lock *ull = it->second;

  null_value = false;
  if (--ull->refs == 0) {
    thd->ull_hash.erase(it);
    thd->mdl_context.release_lock(ull->ticket);
    my_free(ull);
  }
  return 1;
}

bool Item_func_release_all_locks::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/**
  Release all user level lock held by connection.

  @return Number of locks released including recursive lock count.
*/

longlong Item_func_release_all_locks::val_int() {
  assert(fixed);
  THD *thd = current_thd;
  uint result = 0;
  DBUG_TRACE;

  for (const auto &key_and_value : thd->ull_hash) {
    User_level_lock *ull = key_and_value.second;
    thd->mdl_context.release_lock(ull->ticket);
    result += ull->refs;
    my_free(ull);
  }
  thd->ull_hash.clear();

  return result;
}

bool Item_func_is_free_lock::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/**
  Check if user level lock is free.

  @note Sets null_value=true on error.

  @note As result SQL-function IS_FREE_LOCK() returns:
        1    - if lock is free,
        0    - if lock is in use
        NULL - if lock name is bad or OOM (also error is emitted).

  @retval
    1		Available
  @retval
    0		Already taken, or error
*/

longlong Item_func_is_free_lock::val_int() {
  assert(fixed);
  value.length(0);
  String *res = args[0]->val_str(&value);
  char name[NAME_LEN + 1];
  THD *thd = current_thd;

  null_value = true;

  if (check_and_convert_ull_name(name, res)) return 0;

  MDL_key ull_key;
  ull_key.mdl_key_init(MDL_key::USER_LEVEL_LOCK, "", name);

  MDL_lock_get_owner_thread_id_visitor get_owner_visitor;

  if (thd->mdl_context.find_lock_owner(&ull_key, &get_owner_visitor)) return 0;

  null_value = false;
  return (get_owner_visitor.get_owner_id() == 0);
}

bool Item_func_is_used_lock::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/**
  Check if user level lock is used and return connection id of owner.

  @note Sets null_value=true if lock is free/on error.

  @note SQL-function IS_USED_LOCK() returns:
        #    - connection id of lock owner if lock is acquired.
        NULL - if lock is free or on error (in the latter case
               also error is emitted).

  @return Connection id of lock owner, 0 if lock is free/on error.
*/

longlong Item_func_is_used_lock::val_int() {
  assert(fixed);
  String *res = args[0]->val_str(&value);
  char name[NAME_LEN + 1];
  THD *thd = current_thd;

  null_value = true;

  if (check_and_convert_ull_name(name, res)) return 0;

  MDL_key ull_key;
  ull_key.mdl_key_init(MDL_key::USER_LEVEL_LOCK, "", name);

  MDL_lock_get_owner_thread_id_visitor get_owner_visitor;

  if (thd->mdl_context.find_lock_owner(&ull_key, &get_owner_visitor)) return 0;

  const my_thread_id thread_id = get_owner_visitor.get_owner_id();
  if (thread_id == 0) return 0;

  null_value = false;
  return thread_id;
}

bool Item_func_last_insert_id::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->safe_to_cache_query = false;
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

longlong Item_func_last_insert_id::val_int() {
  THD *thd = current_thd;
  assert(fixed);
  if (arg_count) {
    const longlong value = args[0]->val_int();
    null_value = args[0]->null_value;
    /*
      LAST_INSERT_ID(X) must affect the client's mysql_insert_id() as
      documented in the manual. We don't want to touch
      first_successful_insert_id_in_cur_stmt because it would make
      LAST_INSERT_ID(X) take precedence over an generated auto_increment
      value for this row.
    */
    thd->arg_of_last_insert_id_function = true;
    thd->first_successful_insert_id_in_prev_stmt = value;
    return value;
  }
  return static_cast<longlong>(
      thd->read_first_successful_insert_id_in_prev_stmt());
}

bool Item_func_benchmark::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/* This function is just used to test speed of different functions */

longlong Item_func_benchmark::val_int() {
  assert(fixed);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff, sizeof(buff), &my_charset_bin);
  my_decimal tmp_decimal;
  THD *thd = current_thd;
  ulonglong loop_count;

  loop_count = (ulonglong)args[0]->val_int();

  if (args[0]->null_value ||
      (!args[0]->unsigned_flag && (((longlong)loop_count) < 0))) {
    if (!args[0]->null_value) {
      char errbuff[22];
      llstr(((longlong)loop_count), errbuff);
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_WRONG_VALUE_FOR_TYPE,
                          ER_THD(current_thd, ER_WRONG_VALUE_FOR_TYPE), "count",
                          errbuff, "benchmark");
    }

    null_value = true;
    return 0;
  }

  null_value = false;
  for (ulonglong loop = 0; loop < loop_count && !thd->killed; loop++) {
    switch (args[1]->result_type()) {
      case REAL_RESULT:
        (void)args[1]->val_real();
        break;
      case INT_RESULT:
        (void)args[1]->val_int();
        break;
      case STRING_RESULT:
        (void)args[1]->val_str(&tmp);
        break;
      case DECIMAL_RESULT:
        (void)args[1]->val_decimal(&tmp_decimal);
        break;
      case ROW_RESULT:
      default:
        // This case should never be chosen
        assert(0);
        return 0;
    }
  }
  return 0;
}

void Item_func_benchmark::print(const THD *thd, String *str,
                                enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("benchmark("));
  args[0]->print(thd, str, query_type);
  str->append(',');
  args[1]->print(thd, str, query_type);
  str->append(')');
}

/**
  Lock which is used to implement interruptible wait for SLEEP() function.
*/

mysql_mutex_t LOCK_item_func_sleep;

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_item_func_sleep;

static PSI_mutex_info item_func_sleep_mutexes[] = {
    {&key_LOCK_item_func_sleep, "LOCK_item_func_sleep", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static void init_item_func_sleep_psi_keys() {
  int count;

  count = static_cast<int>(array_elements(item_func_sleep_mutexes));
  mysql_mutex_register("sql", item_func_sleep_mutexes, count);
}
#endif

static bool item_func_sleep_inited = false;

void item_func_sleep_init() {
#ifdef HAVE_PSI_INTERFACE
  init_item_func_sleep_psi_keys();
#endif

  mysql_mutex_init(key_LOCK_item_func_sleep, &LOCK_item_func_sleep,
                   MY_MUTEX_INIT_SLOW);
  item_func_sleep_inited = true;
}

void item_func_sleep_free() {
  if (item_func_sleep_inited) {
    item_func_sleep_inited = false;
    mysql_mutex_destroy(&LOCK_item_func_sleep);
  }
}

bool Item_func_sleep::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}

/** This function is just used to create tests with time gaps. */

longlong Item_func_sleep::val_int() {
  THD *thd = current_thd;
  Interruptible_wait timed_cond(thd);
  mysql_cond_t cond;
  double timeout;
  int error;

  assert(fixed);

  timeout = args[0]->val_real();

  /*
    Report error or warning depending on the value of SQL_MODE.
    If SQL is STRICT then report error, else report warning and continue
    execution.
  */

  if (args[0]->null_value || timeout < 0) {
    if (!thd->lex->is_ignore() && thd->is_strict_mode()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "sleep.");
      return 0;
    } else
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                          ER_THD(thd, ER_WRONG_ARGUMENTS), "sleep.");
  }
  /*
    On 64-bit OSX mysql_cond_timedwait() waits forever
    if passed abstime time has already been exceeded by
    the system time.
    When given a very short timeout (< 10 mcs) just return
    immediately.
    We assume that the lines between this test and the call
    to mysql_cond_timedwait() will be executed in less than 0.00001 sec.
  */
  if (timeout < 0.00001) return 0;

  timed_cond.set_timeout((ulonglong)(timeout * 1000000000.0));

  mysql_cond_init(key_item_func_sleep_cond, &cond);
  mysql_mutex_lock(&LOCK_item_func_sleep);

  thd->ENTER_COND(&cond, &LOCK_item_func_sleep, &stage_user_sleep, nullptr);

  error = 0;
  thd_wait_begin(thd, THD_WAIT_SLEEP);
  while (!thd->killed) {
    error = timed_cond.wait(&cond, &LOCK_item_func_sleep);
    if (is_timeout(error)) break;
    error = 0;
  }
  thd_wait_end(thd);
  mysql_mutex_unlock(&LOCK_item_func_sleep);
  thd->EXIT_COND(nullptr);

  mysql_cond_destroy(&cond);

  return (error == 0);  // Return 1 killed
}

/**
  Get variable with given name; conditionally create it if non-existing

  @param thd  thread context
  @param name name of user variable
  @param cs   character set;
              = NULL:  Do not create variable if non-existing.
              != NULL: Create variable with this character set.

  @returns pointer to variable entry.
           = NULL: variable does not exist (if cs == NULL), or
                   could not create variable (if cs != NULL)
*/
static user_var_entry *get_variable(THD *thd, const Name_string &name,
                                    const CHARSET_INFO *cs) {
  const std::string key(name.ptr(), name.length());

  /* Protects thd->user_vars. */
  mysql_mutex_assert_owner(&thd->LOCK_thd_data);

  user_var_entry *entry = find_or_nullptr(thd->user_vars, key);
  if (entry == nullptr && cs != nullptr) {
    entry = user_var_entry::create(thd, name, cs);
    if (entry == nullptr) return nullptr;
    thd->user_vars.emplace(
        key, unique_ptr_with_deleter<user_var_entry>(entry, &free_user_var));
  }
  return entry;
}

void Item_func_set_user_var::cleanup() {
  Item_func::cleanup();
  /*
    Ensure that a valid user variable object is rebound on next
    execution. This is important if the user variable is referenced by a
    trigger: the trigger references an Item_func_set_user_var, and may be
    used by another THD in the future (the trigger is cached in a TABLE). When
    that later happens, the other THD will use the same Item_func_set_user_var
    but shouldn't try to access the previous THD's entry. So we clear it here,
    and set it again later in Item_func_set_user_var::update().
  */
  entry = nullptr;
}

bool Item_func_set_user_var::set_entry(THD *thd, bool create_if_not_exists) {
  if (entry == nullptr) {
    const CHARSET_INFO *cs =
        create_if_not_exists
            ? (args[0]->collation.derivation == DERIVATION_NUMERIC
                   ? default_charset()
                   : args[0]->collation.collation)
            : nullptr;

    /* Protects thd->user_vars. */
    mysql_mutex_lock(&thd->LOCK_thd_data);
    entry = get_variable(thd, name, cs);
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    if (entry == nullptr) return true;
  }

  // Ensure this user variable is owned by the current session
  assert(entry->owner_session() == thd);

  return false;
}

/*
  When a user variable is updated (in a SET command or a query like
  SELECT @a:= ).
*/

bool Item_func_set_user_var::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);

  if (Item_func::fix_fields(thd, ref)) return true;

  // This is probably only to get an early validity check on user variable name
  if (set_entry(thd, true)) return true;
  entry = nullptr;

  null_item = (args[0]->type() == NULL_ITEM);

  return false;
}

bool Item_func_set_user_var::resolve_type(THD *thd) {
  if (Item_var_func::resolve_type(thd)) return true;
  set_nullable(args[0]->is_nullable());
  collation.set(DERIVATION_IMPLICIT);
  /*
     this sets the character set of the item immediately; rules for the
     character set of the variable ("entry" object) are different: if "entry"
     did not exist previously, set_entry () has created it and has set its
     character set; but if it existed previously, it keeps its previous
     character set, which may change only when we are sure that the assignment
     is to be executed, i.e. in user_var_entry::store ().
  */
  if (args[0]->collation.derivation == DERIVATION_NUMERIC)
    collation.collation = default_charset();
  else
    collation.collation = args[0]->collation.collation;

  const enum_field_types type = Item::type_for_variable(args[0]->data_type());
  switch (type) {
    case MYSQL_TYPE_LONGLONG:
      set_data_type_longlong();
      unsigned_flag = args[0]->unsigned_flag;
      max_length =
          args[0]->max_length;  // Preserves "length" of integer constants
      break;
    case MYSQL_TYPE_NEWDECIMAL:
      set_data_type_decimal(
          min<uint>(args[0]->decimal_precision(), DECIMAL_MAX_PRECISION),
          args[0]->decimals);
      break;
    case MYSQL_TYPE_DOUBLE:
      set_data_type_double();
      break;
    case MYSQL_TYPE_VARCHAR:
      set_data_type_string(args[0]->max_char_length());
      break;
    case MYSQL_TYPE_NULL:
    default:
      assert(false);
      set_data_type(MYSQL_TYPE_NULL);
      break;
  }

  cached_result_type = Item::type_to_result(data_type());

  return false;
}

// static
user_var_entry *user_var_entry::create(THD *thd, const Name_string &name,
                                       const CHARSET_INFO *cs) {
  if (check_column_name(name.ptr())) {
    my_error(ER_ILLEGAL_USER_VAR, MYF(0), name.ptr());
    return nullptr;
  }

  user_var_entry *entry;
  const size_t size =
      ALIGN_SIZE(sizeof(user_var_entry)) + (name.length() + 1) + extra_size;
  if (!(entry = (user_var_entry *)my_malloc(key_memory_user_var_entry, size,
                                            MYF(MY_WME | ME_FATALERROR))))
    return nullptr;
  entry->init(thd, name, cs);
  return entry;
}

bool user_var_entry::mem_realloc(size_t length) {
  if (length <= extra_size) {
    /* Enough space to store value in value struct */
    free_value();
    m_ptr = internal_buffer_ptr();
  } else {
    /* Allocate an external buffer */
    if (m_length != length) {
      if (m_ptr == internal_buffer_ptr()) m_ptr = nullptr;
      if (!(m_ptr = (char *)my_realloc(
                key_memory_user_var_entry_value, m_ptr, length,
                MYF(MY_ALLOW_ZERO_PTR | MY_WME | ME_FATALERROR))))
        return true;
    }
  }
  return false;
}

void user_var_entry::init(THD *thd, const Simple_cstring &name,
                          const CHARSET_INFO *cs) {
  assert(thd != nullptr);
  m_owner = thd;
  copy_name(name);
  reset_value();
  m_used_query_id = 0;
  collation.set(cs, DERIVATION_IMPLICIT, 0);
  unsigned_flag = false;
  m_type = STRING_RESULT;
}

bool user_var_entry::store(const void *from, size_t length, Item_result type) {
  assert_locked();

  // Store strings with end \0
  if (mem_realloc(length + (type == STRING_RESULT))) return true;
  if (type == STRING_RESULT) m_ptr[length] = 0;  // Store end \0

  // Avoid memcpy of a my_decimal object, use copy CTOR instead.
  if (type == DECIMAL_RESULT) {
    assert(length == sizeof(my_decimal));
    const my_decimal *dec = static_cast<const my_decimal *>(from);
    dec->sanity_check();
    new (m_ptr) my_decimal(*dec);
  } else if (length > 0) {
    memcpy(m_ptr, from, length);
  }

  m_length = length;
  m_type = type;

  set_used_query_id(current_thd->query_id);

  return false;
}

void user_var_entry::assert_locked() const {
  mysql_mutex_assert_owner(&m_owner->LOCK_thd_data);
}

bool user_var_entry::store(const void *ptr, size_t length, Item_result type,
                           const CHARSET_INFO *cs, Derivation dv,
                           bool unsigned_arg) {
  assert_locked();

  if (store(ptr, length, type)) return true;
  collation.set(cs, dv);
  unsigned_flag = unsigned_arg;
  return false;
}

void user_var_entry::lock() {
  assert(m_owner != nullptr);
  mysql_mutex_lock(&m_owner->LOCK_thd_data);
}

void user_var_entry::unlock() {
  assert(m_owner != nullptr);
  mysql_mutex_unlock(&m_owner->LOCK_thd_data);
}

bool Item_func_set_user_var::update_hash(const void *ptr, uint length,
                                         Item_result res_type,
                                         const CHARSET_INFO *cs, Derivation dv,
                                         bool unsigned_arg) {
  entry->lock();

  // args[0]->null_value could be outdated
  if (args[0]->type() == Item::FIELD_ITEM)
    null_value = down_cast<Item_field *>(args[0])->field->is_null();
  else
    null_value = args[0]->null_value;

  /*
    If we set a variable explicitly to NULL then keep the old
    result type of the variable
  */
  if (null_value && null_item) res_type = entry->type();

  if (null_value)
    entry->set_null_value(res_type);
  else if (entry->store(ptr, length, res_type, cs, dv, unsigned_arg)) {
    entry->unlock();
    null_value = true;
    return true;
  }
  entry->unlock();
  return false;
}

/** Get the value of a variable as a double. */

double user_var_entry::val_real(bool *null_value) const {
  if ((*null_value = (m_ptr == nullptr))) return 0.0;

  switch (m_type) {
    case REAL_RESULT:
      return *reinterpret_cast<double *>(m_ptr);
    case INT_RESULT:
      if (unsigned_flag)
        return static_cast<double>(*reinterpret_cast<ulonglong *>(m_ptr));
      else
        return static_cast<double>(*reinterpret_cast<longlong *>(m_ptr));
    case DECIMAL_RESULT: {
      double result;
      my_decimal2double(E_DEC_FATAL_ERROR, (my_decimal *)m_ptr, &result);
      return result;
    }
    case STRING_RESULT:
      return double_from_string_with_check(collation.collation, m_ptr,
                                           m_ptr + m_length);
    case ROW_RESULT:
    case INVALID_RESULT:
      assert(false);  // Impossible
      break;
  }
  return 0.0;  // Impossible
}

/** Get the value of a variable as an integer. */

longlong user_var_entry::val_int(bool *null_value) const {
  if ((*null_value = (m_ptr == nullptr))) return 0LL;

  switch (m_type) {
    case REAL_RESULT: {
      // TODO(tdidriks): Consider reporting a possible overflow warning.
      const double var_val = *(reinterpret_cast<double *>(m_ptr));
      longlong res;
      if (var_val <= LLONG_MIN) {
        res = LLONG_MIN;
      } else if (var_val >= LLONG_MAX_DOUBLE) {
        res = LLONG_MAX;
      } else
        res = var_val;

      return res;
    }
    case INT_RESULT:
      return *(longlong *)m_ptr;
    case DECIMAL_RESULT: {
      longlong result;
      my_decimal2int(E_DEC_FATAL_ERROR, (my_decimal *)m_ptr, false, &result);
      return result;
    }
    case STRING_RESULT: {
      int error;
      return my_strtoll10(m_ptr, nullptr,
                          &error);  // String is null terminated
    }
    case ROW_RESULT:
    case INVALID_RESULT:
      assert(false);  // Impossible
      break;
  }
  return 0LL;  // Impossible
}

/** Get the value of a variable as a string. */

String *user_var_entry::val_str(bool *null_value, String *str,
                                uint decimals) const {
  if ((*null_value = (m_ptr == nullptr))) return (String *)nullptr;

  switch (m_type) {
    case REAL_RESULT:
      str->set_real(*(double *)m_ptr, decimals, collation.collation);
      break;
    case INT_RESULT:
      if (!unsigned_flag)
        str->set(*(longlong *)m_ptr, collation.collation);
      else
        str->set(*(ulonglong *)m_ptr, collation.collation);
      break;
    case DECIMAL_RESULT:
      str_set_decimal(E_DEC_FATAL_ERROR, pointer_cast<my_decimal *>(m_ptr), str,
                      collation.collation, decimals);
      break;
    case STRING_RESULT:
      if (str->copy(m_ptr, m_length, collation.collation))
        str = nullptr;  // EOM error
      break;
    case ROW_RESULT:
    case INVALID_RESULT:
      assert(false);  // Impossible
      break;
  }
  return (str);
}

/** Get the value of a variable as a decimal. */

my_decimal *user_var_entry::val_decimal(bool *null_value,
                                        my_decimal *val) const {
  if ((*null_value = (m_ptr == nullptr))) return nullptr;

  switch (m_type) {
    case REAL_RESULT:
      double2my_decimal(E_DEC_FATAL_ERROR, *(double *)m_ptr, val);
      break;
    case INT_RESULT:
      int2my_decimal(E_DEC_FATAL_ERROR, *pointer_cast<longlong *>(m_ptr),
                     unsigned_flag, val);
      break;
    case DECIMAL_RESULT:
      my_decimal2decimal(reinterpret_cast<my_decimal *>(m_ptr), val);
      break;
    case STRING_RESULT:
      str2my_decimal(E_DEC_FATAL_ERROR, m_ptr, m_length, collation.collation,
                     val);
      break;
    case ROW_RESULT:
    case INVALID_RESULT:
      assert(false);  // Impossible
      break;
  }
  return (val);
}

/**
  This functions is invoked on SET \@variable or
  \@variable:= expression.

  Evaluate (and check expression), store results.

  @note
    For now it always return OK. All problem with value evaluating
    will be caught by thd->is_error() check in sql_set_variables().

  @retval
    false OK.
*/

bool Item_func_set_user_var::check(bool use_result_field) {
  DBUG_TRACE;
  if (use_result_field && !result_field) use_result_field = false;

  switch (cached_result_type) {
    case REAL_RESULT: {
      save_result.vreal =
          use_result_field ? result_field->val_real() : args[0]->val_real();
      break;
    }
    case INT_RESULT: {
      save_result.vint =
          use_result_field ? result_field->val_int() : args[0]->val_int();
      unsigned_flag = use_result_field ? result_field->is_unsigned()
                                       : args[0]->unsigned_flag;
      break;
    }
    case STRING_RESULT: {
      save_result.vstr = use_result_field ? result_field->val_str(&value)
                                          : args[0]->val_str(&value);
      break;
    }
    case DECIMAL_RESULT: {
      save_result.vdec = use_result_field
                             ? result_field->val_decimal(&decimal_buff)
                             : args[0]->val_decimal(&decimal_buff);
      break;
    }
    case ROW_RESULT:
    default:
      // This case should never be chosen
      assert(0);
      break;
  }
  return false;
}

/**
  @brief Evaluate and store item's result.
  This function is invoked on "SELECT ... INTO @var ...".

  @param    item    An item to get value from.
*/

void Item_func_set_user_var::save_item_result(Item *item) {
  DBUG_TRACE;

  switch (cached_result_type) {
    case REAL_RESULT:
      save_result.vreal = item->val_real();
      break;
    case INT_RESULT:
      save_result.vint = item->val_int();
      unsigned_flag = item->unsigned_flag;
      break;
    case STRING_RESULT:
      save_result.vstr = item->val_str(&value);
      break;
    case DECIMAL_RESULT:
      save_result.vdec = item->val_decimal(&decimal_buff);
      break;
    case ROW_RESULT:
    default:
      // Should never happen
      assert(0);
      break;
  }
}

/**
  Update user variable from value in save_result

  @returns false if success, true when error (EOM)
*/

bool Item_func_set_user_var::update() {
  DBUG_TRACE;

  // Ensure that a user variable object is bound for each execution.
  if (entry == nullptr && set_entry(current_thd, true)) return true;

  bool res = false;

  switch (cached_result_type) {
    case REAL_RESULT: {
      res = update_hash(&save_result.vreal, sizeof(save_result.vreal),
                        REAL_RESULT, default_charset(), DERIVATION_IMPLICIT,
                        false);
      break;
    }
    case INT_RESULT: {
      res = update_hash(&save_result.vint, sizeof(save_result.vint), INT_RESULT,
                        default_charset(), DERIVATION_IMPLICIT, unsigned_flag);
      break;
    }
    case STRING_RESULT: {
      if (!save_result.vstr)  // Null value
        res = update_hash(nullptr, 0, STRING_RESULT, &my_charset_bin,
                          DERIVATION_IMPLICIT, false);
      else
        res = update_hash(save_result.vstr->ptr(), save_result.vstr->length(),
                          STRING_RESULT, save_result.vstr->charset(),
                          DERIVATION_IMPLICIT, false);
      break;
    }
    case DECIMAL_RESULT: {
      if (!save_result.vdec)  // Null value
        res = update_hash(nullptr, 0, DECIMAL_RESULT, &my_charset_bin,
                          DERIVATION_IMPLICIT, false);
      else
        res = update_hash(save_result.vdec, sizeof(my_decimal), DECIMAL_RESULT,
                          default_charset(), DERIVATION_IMPLICIT, false);
      break;
    }
    case ROW_RESULT:
    default:
      // This case should never be chosen
      assert(0);
      break;
  }
  return res;
}

double Item_func_set_user_var::val_real() {
  assert(fixed);
  check(false);
  update();  // Store expression
  return entry->val_real(&null_value);
}

longlong Item_func_set_user_var::val_int() {
  assert(fixed);
  check(false);
  update();  // Store expression
  return entry->val_int(&null_value);
}

String *Item_func_set_user_var::val_str(String *str) {
  assert(fixed);
  check(false);
  update();  // Store expression
  return entry->val_str(&null_value, str, decimals);
}

my_decimal *Item_func_set_user_var::val_decimal(my_decimal *val) {
  assert(fixed);
  check(false);
  update();  // Store expression
  return entry->val_decimal(&null_value, val);
}

// just the assignment, for use in "SET @a:=5" type self-prints
void Item_func_set_user_var::print_assignment(
    const THD *thd, String *str, enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("@"));
  str->append(name);
  str->append(STRING_WITH_LEN(":="));
  args[0]->print(thd, str, query_type);
}

// parenthesize assignment for use in "EXPLAIN EXTENDED SELECT (@e:=80)+5"
void Item_func_set_user_var::print(const THD *thd, String *str,
                                   enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("("));
  print_assignment(thd, str, query_type);
  str->append(STRING_WITH_LEN(")"));
}

bool Item_func_set_user_var::send(Protocol *protocol, String *str_arg) {
  if (result_field) {
    check(true);
    update();
    /*
      TODO This func have to be changed to avoid sending data as a field.
    */
    return protocol->store_field(result_field);
  }
  return Item::send(protocol, str_arg);
}

void Item_func_set_user_var::make_field(Send_field *tmp_field) {
  if (result_field) {
    result_field->make_send_field(tmp_field);
    assert(tmp_field->table_name != nullptr);
    if (Item::item_name.is_set())
      tmp_field->col_name = Item::item_name.ptr();  // Use user supplied name
  } else
    Item::make_field(tmp_field);
}

/*
  Save the value of a user variable into a field

  SYNOPSIS
    save_in_field()
      field           target field to save the value to
      no_conversion   flag indicating whether conversions are allowed

  DESCRIPTION
    Save the function value into a field and update the user variable
    accordingly. If a result field is defined and the target field doesn't
    coincide with it then the value from the result field will be used as
    the new value of the user variable.

    The reason to have this method rather than simply using the result
    field in the val_xxx() methods is that the value from the result field
    not always can be used when the result field is defined.
    Let's consider the following cases:
    1) when filling a tmp table the result field is defined but the value of it
    is undefined because it has to be produced yet. Thus we can't use it.
    2) on execution of an INSERT ... SELECT statement the save_in_field()
    function will be called to fill the data in the new record. If the SELECT
    part uses a tmp table then the result field is defined and should be
    used in order to get the correct result.

    The difference between the SET_USER_VAR function and regular functions
    like CONCAT is that the Item_func objects for the regular functions are
    replaced by Item_field objects after the values of these functions have
    been stored in a tmp table. Yet an object of the Item_field class cannot
    be used to update a user variable.
    Due to this we have to handle the result field in a special way here and
    in the Item_func_set_user_var::send() function.

  RETURN VALUES
    TYPE_OK            Ok
    Everything else    Error
*/

type_conversion_status Item_func_set_user_var::save_in_field(
    Field *field, bool no_conversions, bool can_use_result_field) {
  const bool use_result_field =
      (!can_use_result_field ? 0 : (result_field && result_field != field));
  type_conversion_status error;

  /* Update the value of the user variable */
  check(use_result_field);
  update();

  if (result_type() == STRING_RESULT ||
      (result_type() == REAL_RESULT && field->result_type() == STRING_RESULT)) {
    String *result;
    const CHARSET_INFO *cs = collation.collation;
    char buff[MAX_FIELD_WIDTH];  // Alloc buffer for small columns
    str_value.set_quick(buff, sizeof(buff), cs);
    result = entry->val_str(&null_value, &str_value, decimals);

    if (null_value) {
      str_value.set_quick(nullptr, 0, cs);
      return set_field_to_null_with_conversions(field, no_conversions);
    }

    /* NOTE: If null_value == false, "result" must be not NULL.  */

    field->set_notnull();
    error = field->store(result->ptr(), result->length(), cs);
    str_value.set_quick(nullptr, 0, cs);
  } else if (result_type() == REAL_RESULT) {
    const double nr = entry->val_real(&null_value);
    if (null_value) return set_field_to_null(field);
    field->set_notnull();
    error = field->store(nr);
  } else if (result_type() == DECIMAL_RESULT) {
    my_decimal decimal_value;
    my_decimal *val = entry->val_decimal(&null_value, &decimal_value);
    if (null_value) return set_field_to_null(field);
    field->set_notnull();
    error = field->store_decimal(val);
  } else {
    const longlong nr = entry->val_int(&null_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error = field->store(nr, unsigned_flag);
  }
  return error;
}

String *Item_func_get_user_var::val_str(String *str) {
  assert(fixed);
  DBUG_TRACE;
  THD *thd = current_thd;
  if (var_entry == nullptr &&
      get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry))
    return error_str();
  if (var_entry == nullptr) return error_str();  // No such variable
  String *res = var_entry->val_str(&null_value, str, decimals);
  if (res && !my_charset_same(res->charset(), collation.collation)) {
    String tmpstr;
    uint error;
    if (tmpstr.copy(res->ptr(), res->length(), res->charset(),
                    collation.collation, &error) ||
        error > 0) {
      char tmp[32];
      convert_to_printable(tmp, sizeof(tmp), res->ptr(), res->length(),
                           res->charset(), 6);
      my_error(ER_INVALID_CHARACTER_STRING, MYF(0), collation.collation->csname,
               tmp);
      return error_str();
    }
    if (str->copy(tmpstr)) return error_str();
    return str;
  }
  return res;
}

double Item_func_get_user_var::val_real() {
  assert(fixed);
  THD *thd;
  if (var_entry == nullptr && (thd = current_thd) &&
      get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry))
    return 0.0;
  if (var_entry == nullptr) return 0.0;  // No such variable
  return (var_entry->val_real(&null_value));
}

my_decimal *Item_func_get_user_var::val_decimal(my_decimal *dec) {
  assert(fixed);
  THD *thd;
  if (var_entry == nullptr && (thd = current_thd) &&
      get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry))
    return nullptr;
  if (var_entry == nullptr) return nullptr;
  return var_entry->val_decimal(&null_value, dec);
}

longlong Item_func_get_user_var::val_int() {
  assert(fixed);
  THD *thd;
  if (var_entry == nullptr && (thd = current_thd) &&
      get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry))
    return 0LL;
  if (var_entry == nullptr) return 0LL;  // No such variable
  /*
    See bug#27969934 NO WARNING WHEN CAST OF USER VARIABLE
    TO NUMBER GOES WRONG.
  */
  return var_entry->val_int(&null_value);
}

const CHARSET_INFO *Item_func_get_user_var::charset_for_protocol() {
  assert(fixed);
  THD *thd;
  /*
    If the query reads the value of the variable's charset it depends on this
    variable, so the user var may need to be stored in the binlog: so we call
    get_var_with_binlog.
  */
  if (var_entry == nullptr && (thd = current_thd) &&
      get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry))
    return &my_charset_bin;
  if (var_entry == nullptr) return &my_charset_bin;  // No such variable
  // @todo WL#6570 Should we return collation of Item node or variable entry?
  return result_type() == STRING_RESULT ? collation.collation : &my_charset_bin;
}

/**
  Get variable by name and, if necessary, put the record of variable
  use into the binary log.

  When a user variable is invoked from an update query (INSERT, UPDATE etc),
  stores this variable and its value in thd->user_var_events, so that it can be
  written to the binlog (will be written just before the query is written, see
  log.cc).

  @param      thd         Current session.
  @param      sql_command The command the variable participates in.
  @param      name        Variable name
  @param[out] out_entry  variable structure or NULL. The pointer is set
                         regardless of whether function succeeded or not.

  @retval
    0  OK
  @retval
    1  Failed to put appropriate record into binary log

*/

static int get_var_with_binlog(THD *thd, enum_sql_command sql_command,
                               const Name_string &name,
                               user_var_entry **out_entry) {
  Binlog_user_var_event *user_var_event;
  user_var_entry *var_entry;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);
  var_entry = get_variable(thd, name, nullptr);
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  *out_entry = var_entry;

  /*
    In cases when this function is called for a sub-statement, we can't
    rely on OPTION_BIN_LOG flag in THD::variables.option_bits bitmap
    to determine whether binary logging is turned on, as this bit can be
    cleared before executing sub-statement. So instead we have to look
    at THD::variables::sql_log_bin member.
  */
  const bool log_on = mysql_bin_log.is_open() && thd->variables.sql_log_bin;

  /*
    Any reference to user-defined variable which is done from stored
    function or trigger affects their execution and the execution of the
    calling statement. We must log all such variables even if they are
    not involved in table-updating statements.
  */
  if (!(log_on && (is_update_query(sql_command) || thd->in_sub_stmt))) return 0;

  if (var_entry == nullptr) {
    /*
      If the variable does not exist, it's NULL, but we want to create it so
      that it gets into the binlog (if it didn't, the slave could be
      influenced by a variable of the same name previously set by another
      thread).
      We create it like if it had been explicitly set with SET before.
      The 'new' mimics what sql_yacc.yy does when 'SET @a=10;'.
      sql_set_variables() is what is called from 'case SQLCOM_SET_OPTION'
      in dispatch_command()). Instead of building a one-element list to pass to
      sql_set_variables(), we could instead manually call check() and update();
      this would save memory and time; but calling sql_set_variables() makes
      one unique place to maintain (sql_set_variables()).

      Manipulation with lex is necessary since free_underlaid_joins
      is going to release memory belonging to the main query.
    */

    List<set_var_base> tmp_var_list;
    LEX *sav_lex = thd->lex, lex_tmp;
    thd->lex = &lex_tmp;
    lex_start(thd);
    Item *source = new Item_null();
    if (source == nullptr) return 1;
    source->collation.set(Item::default_charset());
    tmp_var_list.push_back(new (thd->mem_root) set_var_user(
        new Item_func_set_user_var(name, source)));
    /* Create the variable */
    if (sql_set_variables(thd, &tmp_var_list, false)) {
      thd->lex = sav_lex;
      return 1;
    }
    thd->lex = sav_lex;
    mysql_mutex_lock(&thd->LOCK_thd_data);
    var_entry = get_variable(thd, name, nullptr);
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    *out_entry = var_entry;
    if (var_entry == nullptr) return 1;
  } else if (var_entry->used_query_id() == thd->query_id ||
             mysql_bin_log.is_query_in_union(thd, var_entry->used_query_id())) {
    /*
       If this variable was already stored in user_var_events by this query
       (because it's used in more than one place in the query), don't store
       it.
    */
    return 0;
  }

  /*
    First we need to store value of var_entry, when the next situation
    appears:
    > set @a:=1;
    > insert into t1 values (@a), (@a:=@a+1), (@a:=@a+1);
    We have to write to binlog value @a= 1.

    We allocate the user_var_event on user_var_events_alloc pool, not on
    the this-statement-execution pool because in SPs user_var_event objects
    may need to be valid after current [SP] statement execution pool is
    destroyed.
  */
  const size_t size =
      ALIGN_SIZE(sizeof(Binlog_user_var_event)) + var_entry->length();
  if (!(user_var_event =
            (Binlog_user_var_event *)thd->user_var_events_alloc->Alloc(size)))
    return 1;

  user_var_event->value =
      (char *)user_var_event + ALIGN_SIZE(sizeof(Binlog_user_var_event));
  user_var_event->user_var_event = var_entry;
  user_var_event->type = var_entry->type();
  user_var_event->charset_number = var_entry->collation.collation->number;
  user_var_event->unsigned_flag = var_entry->unsigned_flag;
  if (!var_entry->ptr()) {
    /* NULL value*/
    user_var_event->length = 0;
    user_var_event->value = nullptr;
  } else {
    // Avoid memcpy of a my_decimal object, use copy CTOR instead.
    user_var_event->length = var_entry->length();
    if (user_var_event->type == DECIMAL_RESULT) {
      assert(var_entry->length() == sizeof(my_decimal));
      const my_decimal *dec = static_cast<const my_decimal *>(
          static_cast<const void *>(var_entry->ptr()));
      dec->sanity_check();
      new (user_var_event->value) my_decimal(*dec);
    } else
      memcpy(user_var_event->value, var_entry->ptr(), var_entry->length());
  }
  // Mark that this variable has been used by this query
  var_entry->set_used_query_id(thd->query_id);
  if (thd->user_var_events.push_back(user_var_event)) return 1;

  return 0;
}

bool Item_func_get_user_var::resolve_type(THD *thd) {
  set_nullable(true);

  /*
    @todo WL#6570 change has effects:
    - bad: in packet.test (see comment there), ps_<n><engine>.test
    (e.g. ps_5merge.test) (see comment in include/ps_conv.inc),
    ps_w_max_indexes_64
    - good: type_temporal_fractional.test (see comment in that file)
    - fixes failure of: sysschema.pr_statement_performance_analyzer and
    sysschema.format_statement.
    Don't forget to grep for "WL#6570" in the whole tree, including mtr
    tests.
  */
  used_tables_cache =
      thd->lex->locate_var_assignment(name) ? RAND_TABLE_BIT : INNER_TABLE_BIT;

  mysql_mutex_lock(&thd->LOCK_thd_data);
  var_entry = get_variable(thd, name, nullptr);
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  if (var_entry != nullptr) {
    // Variable exists - assign type information from the entry.
    m_cached_result_type = var_entry->type();

    switch (m_cached_result_type) {
      case REAL_RESULT:
        set_data_type_double();
        break;
      case INT_RESULT:
        set_data_type_longlong();
        unsigned_flag = var_entry->unsigned_flag;
        break;
      case STRING_RESULT:
        set_data_type_string(uint32(MAX_BLOB_WIDTH - 1), var_entry->collation);
        break;
      case DECIMAL_RESULT:
        set_data_type_decimal(DECIMAL_MAX_PRECISION, DECIMAL_MAX_SCALE);
        break;
      case ROW_RESULT:  // Keep compiler happy
      default:
        assert(0);
        break;
    }

    // Override collation for all data types
    collation.set(var_entry->collation);
  } else {
    // Unknown user variable, assign expected type from context.
    null_value = true;
  }
  collation.set(DERIVATION_IMPLICIT);

  // Refresh the variable entry during execution with proper binlogging.
  var_entry = nullptr;

  return false;
}

bool Item_func_get_user_var::propagate_type(THD *,
                                            const Type_properties &type) {
  /*
    If the type is temporal: user variables don't support that type; so, we
    use a VARCHAR instead. Same for JSON and GEOMETRY.
    BIT and YEAR types are represented with LONGLONG.
  */
  switch (type.m_type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      set_data_type_longlong();
      unsigned_flag = type.m_unsigned_flag;
      break;
    case MYSQL_TYPE_BIT:
      set_data_type_longlong();
      unsigned_flag = true;
      break;
    case MYSQL_TYPE_YEAR:
      set_data_type_longlong();
      break;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL:
      set_data_type_decimal(DECIMAL_MAX_PRECISION, DECIMAL_MAX_SCALE);
      break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      set_data_type_double();
      break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_NULL:
      // Parameter type is VARCHAR of largest possible size
      set_data_type_string(65535U / type.m_collation.collation->mbmaxlen,
                           type.m_collation);
      break;
    case MYSQL_TYPE_GEOMETRY:
      set_data_type_string(MAX_BLOB_WIDTH, type.m_collation);
      break;
    case MYSQL_TYPE_JSON:
      set_data_type_string(MAX_BLOB_WIDTH, type.m_collation);
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      // Parameter type is BLOB of largest possible size
      set_data_type_string(MAX_BLOB_WIDTH, type.m_collation);
      break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      set_data_type_string(26, type.m_collation);
      break;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      set_data_type_string(10, type.m_collation);
      break;
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      set_data_type_string(15, type.m_collation);
      break;
    case MYSQL_TYPE_VECTOR:
      set_data_type_vector(
          Field_vector::dimension_bytes(Field_vector::max_dimensions));
      break;
    default:
      assert(false);
  }
  // User variables have implicit derivation
  collation.set(DERIVATION_IMPLICIT);

  // @todo - when result_type is refactored, this may not be necessary
  m_cached_result_type = type_to_result(data_type());

  return false;
}

void Item_func_get_user_var::cleanup() {
  Item_func::cleanup();
  /*
    Ensure that a valid user variable object is rebound on next execution. See
    comment in Item_func_set_user_var::cleanup().
  */
  var_entry = nullptr;
}

enum Item_result Item_func_get_user_var::result_type() const {
  return m_cached_result_type;
}

void Item_func_get_user_var::print(const THD *thd, String *str,
                                   enum_query_type) const {
  str->append(STRING_WITH_LEN("(@"));
  append_identifier(thd, str, name.ptr(), name.length());
  str->append(')');
}

bool Item_func_get_user_var::eq_specific(const Item *item) const {
  const Item_func_get_user_var *other =
      down_cast<const Item_func_get_user_var *>(item);
  return name.eq_bin(other->name);
}

bool Item_func_get_user_var::set_value(THD *thd, sp_rcontext * /*ctx*/,
                                       Item **it) {
  Item_func_set_user_var *suv = new Item_func_set_user_var(name, *it);
  /*
    Item_func_set_user_var is not fixed after construction, call
    fix_fields().
  */
  return (!suv || suv->fix_fields(thd, it) || suv->check(false) ||
          suv->update());
}

bool Item_user_var_as_out_param::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);

  assert(thd->lex->sql_command == SQLCOM_LOAD);
  auto exchange_cs =
      down_cast<Sql_cmd_load_table *>(thd->lex->m_sql_cmd)->m_exchange.cs;
  /*
    Let us set the same collation which is used for loading
    of fields in LOAD DATA INFILE.
    (Since Item_user_var_as_out_param is used only there).
  */
  const CHARSET_INFO *cs =
      exchange_cs ? exchange_cs : thd->variables.collation_database;

  if (Item::fix_fields(thd, ref)) return true;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);
  entry = get_variable(thd, name, cs);
  if (entry != nullptr) entry->set_type(STRING_RESULT);
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  return entry == nullptr;
}

void Item_user_var_as_out_param::set_null_value(const CHARSET_INFO *) {
  entry->lock();
  entry->set_null_value(STRING_RESULT);
  entry->unlock();
}

void Item_user_var_as_out_param::set_value(const char *str, size_t length,
                                           const CHARSET_INFO *cs) {
  entry->lock();
  entry->store(str, length, STRING_RESULT, cs, DERIVATION_IMPLICIT,
               false /* unsigned_arg */);
  entry->unlock();
}

double Item_user_var_as_out_param::val_real() {
  assert(0);
  return 0.0;
}

longlong Item_user_var_as_out_param::val_int() {
  assert(0);
  return 0;
}

String *Item_user_var_as_out_param::val_str(String *) {
  assert(0);
  return nullptr;
}

my_decimal *Item_user_var_as_out_param::val_decimal(my_decimal *) {
  assert(0);
  return nullptr;
}

void Item_user_var_as_out_param::print(const THD *thd, String *str,
                                       enum_query_type) const {
  str->append('@');
  append_identifier(thd, str, name.ptr(), name.length());
}

Item_func_get_system_var::Item_func_get_system_var(
    const System_variable_tracker &var_tracker, enum_var_type scope)
    : var_scope{scope}, cache_present{0}, var_tracker{var_tracker} {
  assert(scope != OPT_DEFAULT);
}

bool Item_func_get_system_var::resolve_type(THD *) {
  set_nullable(true);

  switch (var_tracker.cached_show_type()) {
    case SHOW_LONG:
    case SHOW_INT:
    case SHOW_HA_ROWS:
    case SHOW_LONGLONG:
      set_data_type_longlong();
      unsigned_flag = true;
      break;
    case SHOW_SIGNED_INT:
    case SHOW_SIGNED_LONG:
    case SHOW_SIGNED_LONGLONG:
      set_data_type_longlong();
      unsigned_flag = false;
      break;
    case SHOW_CHAR:
    case SHOW_CHAR_PTR:
    case SHOW_LEX_STRING:
      collation.set(system_charset_info, DERIVATION_SYSCONST);
      set_data_type_string(65535U / collation.collation->mbmaxlen);
      break;
    case SHOW_BOOL:
    case SHOW_MY_BOOL:
      set_data_type_longlong();
      max_length = 1;
      break;
    case SHOW_DOUBLE:
      set_data_type_double();
      // Override decimals and length calculation done above.
      decimals = 6;
      max_length = float_length(decimals);
      break;
    default:
      my_error(ER_VAR_CANT_BE_READ, MYF(0), var_tracker.get_var_name());
      return true;
  }
  return false;
}

void Item_func_get_system_var::print(const THD *, String *str,
                                     enum_query_type) const {
  str->append(item_name);
}

Audit_global_variable_get_event::Audit_global_variable_get_event(
    THD *thd, Item_func_get_system_var *item, uchar cache_type)
    : m_thd(thd), m_item(item), m_val_type(cache_type) {
  // Variable is of GLOBAL scope.
  const bool is_global_var = m_item->var_scope == OPT_GLOBAL;

  // Event is already audited for the same query.
  const bool event_is_audited =
      m_item->cache_present != 0 && m_item->used_query_id == m_thd->query_id;

  m_audit_event = (is_global_var && !event_is_audited);
}

Audit_global_variable_get_event::~Audit_global_variable_get_event() {
  /*
    While converting value to string, integer or real type, if the value is
    cached for the types other then m_val_type for intermediate type
    conversions then event is already notified.
  */
  const bool event_already_notified = (m_item->cache_present & (~m_val_type));

  if (m_audit_event && !event_already_notified) {
    String str;
    String *outStr = nullptr;

    if (!m_item->cached_null_value || !m_thd->is_error()) {
      outStr = &str;

      assert(m_item->cache_present != 0 &&
             m_item->used_query_id == m_thd->query_id);

      if (m_item->cache_present & GET_SYS_VAR_CACHE_STRING)
        outStr = &m_item->cached_strval;
      else if (m_item->cache_present & GET_SYS_VAR_CACHE_LONG)
        str.set(m_item->cached_llval, m_item->collation.collation);
      else if (m_item->cache_present & GET_SYS_VAR_CACHE_DOUBLE)
        str.set_real(m_item->cached_dval, m_item->decimals,
                     m_item->collation.collation);
    }

    mysql_event_tracking_global_variable_notify(
        m_thd, AUDIT_EVENT(EVENT_TRACKING_GLOBAL_VARIABLE_GET),
        m_item->var_tracker.get_var_name(), outStr ? outStr->ptr() : nullptr,
        outStr ? outStr->length() : 0);
  }
}

template <typename T>
longlong Item_func_get_system_var::get_sys_var_safe(THD *thd, sys_var *var) {
  T value = {};
  {
    MUTEX_LOCK(lock, &LOCK_global_system_variables);
    std::string_view keycache_name = var_tracker.get_keycache_name();
    value =
        *pointer_cast<const T *>(var->value_ptr(thd, var_scope, keycache_name));
  }
  cache_present |= GET_SYS_VAR_CACHE_LONG;
  used_query_id = thd->query_id;
  cached_llval = null_value ? 0LL : static_cast<longlong>(value);
  cached_null_value = null_value;
  return cached_llval;
}

longlong Item_func_get_system_var::val_int() {
  THD *thd = current_thd;
  const Audit_global_variable_get_event audit_sys_var(thd, this,
                                                      GET_SYS_VAR_CACHE_LONG);
  assert(fixed);

  if (cache_present && thd->query_id == used_query_id) {
    if (cache_present & GET_SYS_VAR_CACHE_LONG) {
      null_value = cached_null_value;
      return cached_llval;
    } else if (cache_present & GET_SYS_VAR_CACHE_DOUBLE) {
      null_value = cached_null_value;
      cached_llval = (longlong)cached_dval;
      cache_present |= GET_SYS_VAR_CACHE_LONG;
      return cached_llval;
    } else if (cache_present & GET_SYS_VAR_CACHE_STRING) {
      null_value = cached_null_value;
      if (!null_value)
        cached_llval = longlong_from_string_with_check(
            cached_strval.charset(), cached_strval.c_ptr(),
            cached_strval.c_ptr() + cached_strval.length(), unsigned_flag);
      else
        cached_llval = 0;
      cache_present |= GET_SYS_VAR_CACHE_LONG;
      return cached_llval;
    }
  }

  auto f = [this, thd](const System_variable_tracker &,
                       sys_var *var) -> longlong {
    switch (var->show_type()) {
      case SHOW_INT:
        return get_sys_var_safe<uint>(thd, var);
      case SHOW_LONG:
        return get_sys_var_safe<ulong>(thd, var);
      case SHOW_LONGLONG:
        return get_sys_var_safe<ulonglong>(thd, var);
      case SHOW_SIGNED_INT:
        return get_sys_var_safe<int>(thd, var);
      case SHOW_SIGNED_LONG:
        return get_sys_var_safe<long>(thd, var);
      case SHOW_SIGNED_LONGLONG:
        return get_sys_var_safe<longlong>(thd, var);
      case SHOW_HA_ROWS:
        return get_sys_var_safe<ha_rows>(thd, var);
      case SHOW_BOOL:
        return get_sys_var_safe<bool>(thd, var);
      case SHOW_MY_BOOL:
        return get_sys_var_safe<bool>(thd, var);
      case SHOW_DOUBLE: {
        const double dval = val_real();

        used_query_id = thd->query_id;
        cached_llval = (longlong)dval;
        cache_present |= GET_SYS_VAR_CACHE_LONG;
        return cached_llval;
      }
      case SHOW_CHAR:
      case SHOW_CHAR_PTR:
      case SHOW_LEX_STRING: {
        String *str_val = val_str(nullptr);
        // Treat empty strings as NULL, like val_real() does.
        if (str_val && str_val->length())
          cached_llval = longlong_from_string_with_check(
              system_charset_info, str_val->c_ptr(),
              str_val->c_ptr() + str_val->length(), unsigned_flag);
        else {
          null_value = true;
          cached_llval = 0;
        }

        cache_present |= GET_SYS_VAR_CACHE_LONG;
        return cached_llval;
      }

      default:
        my_error(ER_VAR_CANT_BE_READ, MYF(0), var->name.str);
        return 0;  // keep the compiler happy
    }
  };
  return var_tracker.access_system_variable<longlong>(thd, f).value_or(0);
}

String *Item_func_get_system_var::val_str(String *str) {
  DEBUG_SYNC(current_thd, "after_error_checking");
  THD *thd = current_thd;
  const Audit_global_variable_get_event audit_sys_var(thd, this,
                                                      GET_SYS_VAR_CACHE_STRING);
  assert(fixed);

  if (cache_present && thd->query_id == used_query_id) {
    if (cache_present & GET_SYS_VAR_CACHE_STRING) {
      null_value = cached_null_value;
      return null_value ? nullptr : &cached_strval;
    } else if (cache_present & GET_SYS_VAR_CACHE_LONG) {
      null_value = cached_null_value;
      if (!null_value) cached_strval.set(cached_llval, collation.collation);
      cache_present |= GET_SYS_VAR_CACHE_STRING;
      return null_value ? nullptr : &cached_strval;
    } else if (cache_present & GET_SYS_VAR_CACHE_DOUBLE) {
      null_value = cached_null_value;
      if (!null_value)
        cached_strval.set_real(cached_dval, decimals, collation.collation);
      cache_present |= GET_SYS_VAR_CACHE_STRING;
      return null_value ? nullptr : &cached_strval;
    }
  }

  str = &cached_strval;
  null_value = false;

  auto f = [this, thd, &str](const System_variable_tracker &, sys_var *var) {
    switch (var->show_type()) {
      case SHOW_CHAR:
      case SHOW_CHAR_PTR:
      case SHOW_LEX_STRING: {
        mysql_mutex_lock(&LOCK_global_system_variables);
        const char *cptr =
            var->show_type() == SHOW_CHAR
                ? pointer_cast<const char *>(var->value_ptr(
                      thd, var_scope, var_tracker.get_keycache_name()))
                : *pointer_cast<const char *const *>(var->value_ptr(
                      thd, var_scope, var_tracker.get_keycache_name()));
        if (cptr) {
          size_t len =
              var->show_type() == SHOW_LEX_STRING
                  ? (pointer_cast<const LEX_STRING *>(var->value_ptr(
                         thd, var_scope, var_tracker.get_keycache_name())))
                        ->length
                  : strlen(cptr);
          if (str->copy(cptr, len, collation.collation)) {
            null_value = true;
            str = nullptr;
          }
        } else {
          null_value = true;
          str = nullptr;
        }
        mysql_mutex_unlock(&LOCK_global_system_variables);
        break;
      }

      case SHOW_INT:
      case SHOW_LONG:
      case SHOW_LONGLONG:
      case SHOW_SIGNED_INT:
      case SHOW_SIGNED_LONG:
      case SHOW_SIGNED_LONGLONG:
      case SHOW_HA_ROWS:
      case SHOW_BOOL:
      case SHOW_MY_BOOL:
        if (unsigned_flag)
          str->set((ulonglong)val_int(), collation.collation);
        else
          str->set(val_int(), collation.collation);
        break;
      case SHOW_DOUBLE:
        str->set_real(val_real(), decimals, collation.collation);
        break;

      default:
        my_error(ER_VAR_CANT_BE_READ, MYF(0), var->name.str);
        str = error_str();
        break;
    }
  };
  if (var_tracker.access_system_variable(thd, f)) {
    str = error_str();
  }

  cache_present |= GET_SYS_VAR_CACHE_STRING;
  used_query_id = thd->query_id;
  cached_null_value = null_value;
  return str;
}

double Item_func_get_system_var::val_real() {
  THD *thd = current_thd;
  const Audit_global_variable_get_event audit_sys_var(thd, this,
                                                      GET_SYS_VAR_CACHE_DOUBLE);
  assert(fixed);

  if (cache_present && thd->query_id == used_query_id) {
    if (cache_present & GET_SYS_VAR_CACHE_DOUBLE) {
      null_value = cached_null_value;
      return cached_dval;
    } else if (cache_present & GET_SYS_VAR_CACHE_LONG) {
      null_value = cached_null_value;
      cached_dval = (double)cached_llval;
      cache_present |= GET_SYS_VAR_CACHE_DOUBLE;
      return cached_dval;
    } else if (cache_present & GET_SYS_VAR_CACHE_STRING) {
      null_value = cached_null_value;
      if (!null_value)
        cached_dval = double_from_string_with_check(
            cached_strval.charset(), cached_strval.c_ptr(),
            cached_strval.c_ptr() + cached_strval.length());
      else
        cached_dval = 0;
      cache_present |= GET_SYS_VAR_CACHE_DOUBLE;
      return cached_dval;
    }
  }

  auto f = [this, thd](const System_variable_tracker &,
                       sys_var *var) -> double {
    switch (var->show_type()) {
      case SHOW_DOUBLE:
        mysql_mutex_lock(&LOCK_global_system_variables);
        cached_dval = *pointer_cast<const double *>(
            var->value_ptr(thd, var_scope, var_tracker.get_keycache_name()));
        mysql_mutex_unlock(&LOCK_global_system_variables);
        used_query_id = thd->query_id;
        cached_null_value = null_value;
        if (null_value) cached_dval = 0;
        cache_present |= GET_SYS_VAR_CACHE_DOUBLE;
        return cached_dval;
      case SHOW_CHAR:
      case SHOW_LEX_STRING:
      case SHOW_CHAR_PTR: {
        mysql_mutex_lock(&LOCK_global_system_variables);
        const char *cptr =
            var->show_type() == SHOW_CHAR
                ? pointer_cast<const char *>(var->value_ptr(
                      thd, var_scope, var_tracker.get_keycache_name()))
                : *pointer_cast<const char *const *>(var->value_ptr(
                      thd, var_scope, var_tracker.get_keycache_name()));
        // Treat empty strings as NULL, like val_int() does.
        if (cptr && *cptr)
          cached_dval = double_from_string_with_check(system_charset_info, cptr,
                                                      cptr + strlen(cptr));
        else {
          null_value = true;
          cached_dval = 0;
        }
        mysql_mutex_unlock(&LOCK_global_system_variables);
        used_query_id = thd->query_id;
        cached_null_value = null_value;
        cache_present |= GET_SYS_VAR_CACHE_DOUBLE;
        return cached_dval;
      }
      case SHOW_INT:
      case SHOW_LONG:
      case SHOW_LONGLONG:
      case SHOW_SIGNED_INT:
      case SHOW_SIGNED_LONG:
      case SHOW_SIGNED_LONGLONG:
      case SHOW_HA_ROWS:
      case SHOW_BOOL:
      case SHOW_MY_BOOL:
        cached_dval = (double)val_int();
        cache_present |= GET_SYS_VAR_CACHE_DOUBLE;
        used_query_id = thd->query_id;
        cached_null_value = null_value;
        return cached_dval;
      default:
        my_error(ER_VAR_CANT_BE_READ, MYF(0), var->name.str);
        return 0;
    }
  };
  return var_tracker.access_system_variable<double>(thd, f).value_or(0);
}

bool Item_func_get_system_var::eq_specific(const Item *item) const {
  const Item_func_get_system_var *other =
      down_cast<const Item_func_get_system_var *>(item);
  return var_tracker == other->var_tracker;
}

void Item_func_get_system_var::cleanup() {
  Item_func::cleanup();
  cache_present = 0;
  cached_strval.mem_free();
}

bool Item_func_match::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res) || against->itemize(pc, &against)) return true;
  add_accum_properties(against);

  pc->select->add_ftfunc_to_list(this);
  pc->thd->lex->set_using_match();

  switch (pc->select->parsing_place) {
    case CTX_WHERE:
    case CTX_ON:
      used_in_where_only = true;
      break;
    default:
      used_in_where_only = false;
  }

  return false;
}

/**
  Initialize searching within full-text index.

  @param thd    Thread handler

  @returns false if success, true if error
*/

bool Item_func_match::init_search(THD *thd) {
  DBUG_TRACE;

  /*
    We will skip execution if the item is not fixed
    with fix_field
  */
  if (!fixed) return false;

  TABLE *const table = table_ref->table;
  /* Check if init_search() has been called before */
  if (ft_handler && !master) {
    // Update handler::ft_handler even if the search is already initialized.
    // If the first call to init_search() was done before we had decided if
    // an index scan should be used, and we later decide that we will use
    // one, ft_handler is not set. For example, the optimizer calls
    // init_search() early if it needs to call Item_func_match::get_count()
    // to perform COUNT(*) optimization or decide if a LIMIT clause can be
    // satisfied by an index scan.
    if (score_from_index_scan) table->file->ft_handler = ft_handler;
    return false;
  }

  if (key == NO_SUCH_KEY) {
    mem_root_deque<Item *> fields(thd->mem_root);
    fields.push_back(new Item_string(" ", 1, cmp_collation.collation));
    for (uint i = 0; i < arg_count; i++) fields.push_back(args[i]);
    concat_ws = new Item_func_concat_ws(&fields);
    if (concat_ws == nullptr) return true;
    /*
      Above function used only to get value and do not need fix_fields for it:
      Item_string - basic constant
      fields - fix_fields() was already called for this arguments
      Item_func_concat_ws - do not need fix_fields() to produce value
    */
    concat_ws->quick_fix_field();
  }

  if (master) {
    if (master->init_search(thd)) return true;

    ft_handler = master->ft_handler;
    return false;
  }

  String *ft_tmp = nullptr;

  // MATCH ... AGAINST (NULL) is meaningless, but possible
  if (!(ft_tmp = key_item()->val_str(&value))) {
    ft_tmp = &value;
    value.set("", 0, cmp_collation.collation);
  }

  if (ft_tmp->charset() != cmp_collation.collation) {
    uint dummy_errors;
    search_value.copy(ft_tmp->ptr(), ft_tmp->length(), ft_tmp->charset(),
                      cmp_collation.collation, &dummy_errors);
    ft_tmp = &search_value;
  }

  if (!table->is_created()) {
    my_error(ER_NO_FT_MATERIALIZED_SUBQUERY, MYF(0));
    return true;
  }

  assert(master == nullptr);
  ft_handler = table->file->ft_init_ext_with_hints(key, ft_tmp, get_hints());
  if (ft_handler == nullptr || thd->is_error()) {
    return true;
  }

  if (score_from_index_scan) table->file->ft_handler = ft_handler;

  return false;
}

float Item_func_match::get_filtering_effect(THD *thd,
                                            table_map filter_for_table,
                                            table_map read_tables,
                                            const MY_BITMAP *fields_to_ignore,
                                            double rows_in_table) {
  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  /*
    MATCH () ... AGAINST" is similar to "LIKE '...'" which has the
    same selectivity as "col BETWEEN ...".
  */
  return fld->get_cond_filter_default_probability(rows_in_table,
                                                  COND_FILTER_BETWEEN);
}

/**
   Add field into table read set.

   @param field field to be added to the table read set.
*/
static void update_table_read_set(const Field *field) {
  TABLE *table = field->table;

  if (!bitmap_test_and_set(table->read_set, field->field_index()))
    table->covering_keys.intersect(field->part_of_key);
}

bool Item_func_match::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);
  assert(arg_count > 0);
  Item *item = nullptr;  // Safe as arg_count is > 1

  set_nullable(true);

  /*
    const_item is assumed in quite a bit of places, so it would be difficult
    to remove;  If it would ever to be removed, this should include
    modifications to find_best and auto_close as complement to auto_init code
    above.
  */
  const enum_mark_columns save_mark_used_columns = thd->mark_used_columns;
  /*
    Since different engines require different columns for FTS index lookup
    we prevent updating of table read_set in argument's ::fix_fields().
  */
  thd->mark_used_columns = MARK_COLUMNS_NONE;
  if (Item_func::fix_fields(thd, ref) || fix_func_arg(thd, &against) ||
      !against->const_for_execution()) {
    thd->mark_used_columns = save_mark_used_columns;
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "AGAINST");
    return true;
  }
  thd->mark_used_columns = save_mark_used_columns;

  if (against->propagate_type(thd)) return true;

  bool allows_multi_table_search = true;
  for (uint i = 0; i < arg_count; i++) {
    item = args[i] = args[i]->real_item();
    if (item->type() != Item::FIELD_ITEM ||
        /* Cannot use FTS index with outer table field */
        item->is_outer_reference()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "MATCH");
      return true;
    }
    allows_multi_table_search &= allows_search_on_non_indexed_columns(
        ((Item_field *)item)->field->table);
    // MATCH should only operate on fields, so don't let constant propagation
    // replace them with constants. (Only relevant to MyISAM, which allows any
    // type of column in the MATCH clause. InnoDB requires the columns to have a
    // full-text index, which again requires the column type to be a string
    // type, and constant propagation is already disabled for strings.)
    item->disable_constant_propagation(nullptr);
  }

  /*
    Check that all columns come from the same table.
    We've already checked that columns in MATCH are fields so
    INNER_TABLE_BIT can only appear from AGAINST argument.
  */
  if ((used_tables_cache & ~INNER_TABLE_BIT) != item->used_tables())
    key = NO_SUCH_KEY;

  if (key == NO_SUCH_KEY && !allows_multi_table_search) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "MATCH");
    return true;
  }
  table_ref = down_cast<Item_field *>(item)->m_table_ref;

  if (table_ref != nullptr) table_ref->set_fulltext_searched();

  /*
    Here we make an assumption that if the engine supports
    fulltext extension(HA_CAN_FULLTEXT_EXT flag) then table
    can have FTS_DOC_ID column. Atm this is the only way
    to distinguish MyISAM and InnoDB engines.
    Generally table_ref should be available, but in case of
    a generated column's generation expression it's not. Thus
    we use field's table, at this moment it's already available.
  */
  TABLE *const table = table_ref != nullptr
                           ? table_ref->table
                           : down_cast<Item_field *>(item)->field->table;

  if (!(table->file->ha_table_flags() & HA_CAN_FULLTEXT)) {
    my_error(ER_TABLE_CANT_HANDLE_FT, MYF(0));
    return true;
  }

  if ((table->file->ha_table_flags() & HA_CAN_FULLTEXT_EXT)) {
    Field *doc_id_field = table->fts_doc_id_field;
    /*
      Update read set with FTS_DOC_ID column so that indexes that have
      FTS_DOC_ID part can be considered as a covering index.
    */
    if (doc_id_field)
      update_table_read_set(doc_id_field);
    else {
      /* read_set needs to be updated for MATCH arguments */
      for (uint i = 0; i < arg_count; i++)
        update_table_read_set(((Item_field *)args[i])->field);
      /*
        Prevent index only access by non-FTS index if table does not have
        FTS_DOC_ID column, find_relevance does not work properly without
        FTS_DOC_ID value. Decision for FTS index about index only access
        is made later by JOIN::fts_index_access() function.
      */
      table->covering_keys.clear_all();
    }

  } else {
    /*
      Since read_set is not updated for MATCH arguments
      it's necessary to update it here for MyISAM.
    */
    for (uint i = 0; i < arg_count; i++)
      update_table_read_set(((Item_field *)args[i])->field);
  }

  if (!master) {
    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    hints = new (thd->mem_root) Ft_hints(flags);
    if (!hints) {
      my_error(ER_TABLE_CANT_HANDLE_FT, MYF(0));
      return true;
    }
  }
  return agg_item_collations_for_comparison(cmp_collation, func_name(), args,
                                            arg_count, 0);
}

void Item_func_match::update_used_tables() {
  Item_func::update_used_tables();
  against->update_used_tables();
  used_tables_cache |= against->used_tables();
  add_accum_properties(against);
}

bool Item_func_match::fix_index(const THD *thd) {
  TABLE *table;
  uint ft_to_key[MAX_KEY], ft_cnt[MAX_KEY], fts = 0, keynr;
  uint max_cnt = 0, mkeys = 0, i;

  if (table_ref == nullptr) goto err;

  /*
    We will skip execution if the item is not fixed
    with fix_field
  */
  if (!fixed) {
    if (allows_search_on_non_indexed_columns(table_ref->table))
      key = NO_SUCH_KEY;

    return false;
  }
  if (key == NO_SUCH_KEY) return false;

  table = table_ref->table;
  for (keynr = 0; keynr < table->s->keys; keynr++) {
    if ((table->key_info[keynr].flags & HA_FULLTEXT) &&
        (flags & FT_BOOL ? table->keys_in_use_for_query.is_set(keynr)
                         : table->s->usable_indexes(thd).is_set(keynr)))

    {
      ft_to_key[fts] = keynr;
      ft_cnt[fts] = 0;
      fts++;
    }
  }

  if (!fts) goto err;

  for (i = 0; i < arg_count; i++) {
    Item_field *item =
        down_cast<Item_field *>(unwrap_rollup_group(args[i])->real_item());
    for (keynr = 0; keynr < fts; keynr++) {
      KEY *ft_key = &table->key_info[ft_to_key[keynr]];
      const uint key_parts = ft_key->user_defined_key_parts;

      for (uint part = 0; part < key_parts; part++) {
        if (item->field->eq(ft_key->key_part[part].field)) ft_cnt[keynr]++;
      }
    }
  }

  for (keynr = 0; keynr < fts; keynr++) {
    if (ft_cnt[keynr] > max_cnt) {
      mkeys = 0;
      max_cnt = ft_cnt[mkeys] = ft_cnt[keynr];
      ft_to_key[mkeys] = ft_to_key[keynr];
      continue;
    }
    if (max_cnt && ft_cnt[keynr] == max_cnt) {
      mkeys++;
      ft_cnt[mkeys] = ft_cnt[keynr];
      ft_to_key[mkeys] = ft_to_key[keynr];
      continue;
    }
  }

  for (keynr = 0; keynr <= mkeys; keynr++) {
    // partial keys doesn't work
    if (max_cnt < arg_count ||
        max_cnt < table->key_info[ft_to_key[keynr]].user_defined_key_parts)
      continue;

    key = ft_to_key[keynr];

    return false;
  }

err:
  if (table_ref != nullptr &&
      allows_search_on_non_indexed_columns(table_ref->table)) {
    key = NO_SUCH_KEY;
    return false;
  }
  my_error(ER_FT_MATCHING_KEY_NOT_FOUND, MYF(0));
  return true;
}

bool Item_func_match::eq_specific(const Item *item) const {
  const Item_func_match *ifm = down_cast<const Item_func_match *>(item);

  /*
    Ignore FT_SORTED flag when checking for equality since result is
    equivalent regardless of sorting
  */
  if ((flags | FT_SORTED) != (ifm->flags | FT_SORTED)) {
    return false;
  }
  if (key == ifm->key && table_ref == ifm->table_ref &&
      key_item()->eq(ifm->key_item()))
    return true;

  return false;
}

double Item_func_match::val_real() {
  assert(fixed);

  // MATCH only knows how to get the score for base columns. Other types of
  // expressions (such as function calls or rollup columns) should have been
  // rejected during resolving.
  assert(!has_grouping_set_dep());
  assert(std::all_of(args, args + arg_count, [](const Item *item) {
    return item->real_item()->type() == FIELD_ITEM;
  }));

  DBUG_TRACE;
  if (ft_handler == nullptr) return -1.0;

  TABLE *const table = table_ref->table;
  if (key != NO_SUCH_KEY && table->has_null_row())  // NULL row from outer join
    return 0.0;

  if (get_master()->score_from_index_scan) {
    assert(table->file->ft_handler == ft_handler);
    return ft_handler->please->get_relevance(ft_handler);
  }

  if (key == NO_SUCH_KEY) {
    String *a = concat_ws->val_str(&value);
    if ((null_value = (a == nullptr)) || !a->length()) return 0;
    return ft_handler->please->find_relevance(ft_handler, (uchar *)a->ptr(),
                                              a->length());
  }
  return ft_handler->please->find_relevance(ft_handler, table->record[0], 0);
}

void Item_func_match::print(const THD *thd, String *str,
                            enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("(match "));
  print_args(thd, str, 0, query_type);
  str->append(STRING_WITH_LEN(" against ("));
  against->print(thd, str, query_type);
  if (flags & FT_BOOL)
    str->append(STRING_WITH_LEN(" in boolean mode"));
  else if (flags & FT_EXPAND)
    str->append(STRING_WITH_LEN(" with query expansion"));
  str->append(STRING_WITH_LEN("))"));
}

void Item_func_match::add_json_info(Json_object *obj) {
  if (flags & FT_BOOL)
    obj->add_alias("match_options",
                   create_dom_ptr<Json_string>("in boolean mode"));
  else if (flags & FT_EXPAND)
    obj->add_alias("match_options",
                   create_dom_ptr<Json_string>("with query expansion"));
}

/**
  Function sets FT hints(LIMIT, flags) depending on
  various join conditions.

  @param join     Pointer to JOIN object.
  @param ft_flag  FT flag value.
  @param ft_limit Limit value.
  @param no_cond  true if MATCH is not used in WHERE condition.
*/

void Item_func_match::set_hints(JOIN *join, uint ft_flag, ha_rows ft_limit,
                                bool no_cond) {
  assert(!master);

  if (!join)  // used for count() optimization
  {
    hints->set_hint_flag(ft_flag);
    return;
  }

  /* skip hints setting if there are aggregates(except of FT_NO_RANKING) */
  if (join->implicit_grouping || !join->group_list.empty() ||
      join->select_distinct) {
    /* 'No ranking' is possible even if aggregates are present */
    if ((ft_flag & FT_NO_RANKING)) hints->set_hint_flag(FT_NO_RANKING);
    return;
  }

  hints->set_hint_flag(ft_flag);

  /**
    Only one table is used, there is no aggregates,
    WHERE condition is a single MATCH expression
    (WHERE MATCH(..) or WHERE MATCH(..) [>=,>] value) or
    there is no WHERE condition.
  */
  if (join->primary_tables == 1 && (no_cond || is_simple_expression()))
    hints->set_hint_limit(ft_limit);
}

NonAggregatedFullTextSearchVisitor::NonAggregatedFullTextSearchVisitor(
    std::function<bool(Item_func_match *)> func)
    : m_func(std::move(func)) {}

bool NonAggregatedFullTextSearchVisitor::operator()(Item *item) {
  if (is_stopped(item)) {
    // Inside a skipped subtree.
    return false;
  }

  switch (item->type()) {
    case Item::SUM_FUNC_ITEM:
      // We're only visiting non-aggregated expressions, so skip subtrees under
      // aggregate functions.
      stop_at(item);
      return false;
    case Item::REF_ITEM:
      switch (down_cast<Item_ref *>(item)->ref_type()) {
        case Item_ref::REF:
        case Item_ref::OUTER_REF:
        case Item_ref::AGGREGATE_REF:
        case Item_ref::NULL_HELPER_REF:
          // Skip all these. REF means the expression is already handled
          // elsewhere in the SELECT list. OUTER_REF is already handled in an
          // outer query block. AGGREGATE_REF means it's aggregated, and we're
          // only interested in non-aggregated expressions.
          stop_at(item);
          break;
        case Item_ref::VIEW_REF:
          // These are references to items in the SELECT list of a query block
          // that has been merged into this one. Might not be in the SELECT list
          // of the query block it was merged into, so we need to look into its
          // sub-items.
          break;
      }
      return false;
    case Item::FUNC_ITEM:
      if (down_cast<Item_func *>(item)->functype() == Item_func::FT_FUNC) {
        if (m_func(down_cast<Item_func_match *>(item))) {
          return true;
        }
        stop_at(item);
      }
      return false;
    default:
      return false;
  }
}

/***************************************************************************
  System variables
****************************************************************************/

/**
  Return value of an system variable base[.name] as a constant item.

  @param pc                     Current parse context
  @param scope                  Global / session
  @param prefix                 Optional prefix part of the variable name
  @param suffix                 Trivial name of suffix part of the variable name
  @param unsafe_for_replication If true and if the variable is written to a
                                binlog then mark the statement as unsafe.

  @note
    If component.str = 0 then the variable name is in 'name'

  @return
    - 0  : error
    - #  : constant item
*/

Item *get_system_variable(Parse_context *pc, enum_var_type scope,
                          const LEX_CSTRING &prefix, const LEX_CSTRING &suffix,
                          bool unsafe_for_replication) {
  THD *thd = pc->thd;

  enum_var_type resolved_scope;
  bool written_to_binlog_flag = false;
  auto f = [thd, scope, &resolved_scope, &written_to_binlog_flag](
               const System_variable_tracker &, sys_var *v) -> bool {
    if (scope == OPT_DEFAULT) {
      if (v->check_scope(OPT_SESSION)) {
        resolved_scope = OPT_SESSION;
      } else {
        /* As there was no local variable, return the global value */
        assert(v->check_scope(OPT_GLOBAL));
        resolved_scope = OPT_GLOBAL;
      }
    } else if (v->check_scope(scope)) {
      resolved_scope = scope;
    } else {
      my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), v->name.str,
               scope == OPT_GLOBAL ? "SESSION" : "GLOBAL");
      return true;
    }

    written_to_binlog_flag = v->is_written_to_binlog(resolved_scope);
    v->do_deprecated_warning(thd);
    return false;
  };
  const System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(to_string_view(prefix),
                                            to_string_view(suffix));
  if (var_tracker.access_system_variable<bool>(thd, f).value_or(true)) {
    return nullptr;
  }

  if (unsafe_for_replication && !written_to_binlog_flag)
    thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_VARIABLE);

  thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);

  return new Item_func_get_system_var(var_tracker, resolved_scope);
}

bool Item_func_row_count::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;

  LEX *lex = pc->thd->lex;
  lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  lex->safe_to_cache_query = false;
  return false;
}

longlong Item_func_row_count::val_int() {
  assert(fixed);
  THD *thd = current_thd;

  return thd->get_row_count_func();
}

Item_func_sp::Item_func_sp(const POS &pos, const LEX_STRING &db_name,
                           const LEX_STRING &fn_name, bool use_explicit_name,
                           PT_item_list *opt_list)
    : Item_func(pos, opt_list) {
  /*
    Set to false here, which is the default according to SQL standard.
    RETURNS NULL ON NULL INPUT can be implemented by modifying this member.
  */
  null_on_null = false;
  set_nullable(true);
  set_stored_program();
  THD *thd = current_thd;
  m_name = new (thd->mem_root)
      sp_name(to_lex_cstring(db_name), fn_name, use_explicit_name);
}

bool Item_func_sp::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  if (m_name == nullptr) return true;  // OOM

  THD *thd = pc->thd;
  LEX *lex = thd->lex;

  m_name_resolution_ctx = lex->current_context();
  lex->safe_to_cache_query = false;

  if (m_name->m_db.str == nullptr) {
    if (thd->lex->copy_db_to(&m_name->m_db.str, &m_name->m_db.length)) {
      my_error(ER_NO_DB_ERROR, MYF(0));
      return true;
    }
  }

  m_name->init_qname(thd);
  sp_add_own_used_routine(lex, thd, Sroutine_hash_entry::FUNCTION, m_name);

  return false;
}

void Item_func_sp::cleanup() {
  if (sp_result_field != nullptr) {
    sp_result_field->mem_free();
    sp_result_field->table->in_use = nullptr;
  }
  m_sp = nullptr;
  Item_func::cleanup();
}

const char *Item_func_sp::func_name() const {
  const THD *thd = current_thd;
  /* Calculate length to avoid reallocation of string for sure */
  const size_t len =
      (((m_name->m_explicit_name ? m_name->m_db.length : 0) +
        m_name->m_name.length) *
           2 +                              // characters*quoting
       2 +                                  // ` and `
       (m_name->m_explicit_name ? 3 : 0) +  // '`', '`' and '.' for the db
       1 +                                  // end of string
       ALIGN_SIZE(1));                      // to avoid String reallocation
  String qname((char *)thd->mem_root->Alloc(len), len, system_charset_info);

  qname.length(0);
  if (m_name->m_explicit_name) {
    append_identifier(thd, &qname, m_name->m_db.str, m_name->m_db.length);
    qname.append('.');
  }
  append_identifier(thd, &qname, m_name->m_name.str, m_name->m_name.length);
  return qname.ptr();
}

table_map Item_func_sp::get_initial_pseudo_tables() const {
  /*
    INNER_TABLE_BIT prevents function from being evaluated in preparation phase.
    @todo - make this dependent on READS SQL or MODIFIES SQL.
            Due to bug#26422182, a function cannot be executed before tables
            are locked, even though it accesses no tables.
  */
  return m_deterministic ? INNER_TABLE_BIT : RAND_TABLE_BIT;
}

static void my_missing_function_error(const LEX_STRING &token,
                                      const char *func_name) {
  if (token.length && is_lex_native_function(&token))
    my_error(ER_FUNC_INEXISTENT_NAME_COLLISION, MYF(0), func_name);
  else
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION", func_name);
}

/**
  @brief Initialize the result field by creating a temporary dummy table
    and assign it to a newly created field object. Meta data used to
    create the field is fetched from the sp_head belonging to the stored
    procedure found in the stored procedure function cache.

  @note This function should be called from fix_fields to init the result
    field. It is some what related to Item_field.

  @see Item_field

  @param thd A pointer to the session and thread context.

  @return Function return error status.
  @retval true is returned on an error
  @retval false is returned on success.
*/

bool Item_func_sp::init_result_field(THD *thd) {
  const LEX_CSTRING empty_name = {STRING_WITH_LEN("")};
  DBUG_TRACE;

  assert(m_sp == nullptr);
  assert(sp_result_field == nullptr);

  Internal_error_handler_holder<View_error_handler, Table_ref> view_handler(
      thd, m_name_resolution_ctx->view_error_handler,
      m_name_resolution_ctx->view_error_handler_arg);
  m_sp = sp_find_routine(thd, enum_sp_type::FUNCTION, m_name,
                         &thd->sp_func_cache, true);
  if (m_sp == nullptr) {
    my_missing_function_error(m_name->m_name, m_name->m_qname.str);
    return true;
  }

  m_deterministic = m_sp->m_chistics->detistic;

  /*
     A Field need to be attached to a Table.
     Below we "create" a dummy table by initializing
     the needed pointers.
   */
  TABLE *dummy_table = new (thd->mem_root) TABLE;
  if (dummy_table == nullptr) return true;
  TABLE_SHARE *share = new (thd->mem_root) TABLE_SHARE;
  if (share == nullptr) return true;

  dummy_table->s = share;
  dummy_table->alias = "";
  if (is_nullable()) dummy_table->set_nullable();
  dummy_table->in_use = thd;
  dummy_table->copy_blobs = true;
  share->table_cache_key = empty_name;
  share->db = empty_name;
  share->table_name = empty_name;

  sp_result_field =
      m_sp->create_result_field(thd, max_length, item_name.ptr(), dummy_table);
  return sp_result_field == nullptr;
}

/**
  @brief Initialize local members with values from the Field interface.

  @note called from Item::fix_fields.
*/

bool Item_func_sp::resolve_type(THD *) {
  DBUG_TRACE;

  assert(sp_result_field);
  set_data_type(sp_result_field->type());
  decimals = sp_result_field->decimals();
  max_length = sp_result_field->field_length;
  collation.set(sp_result_field->charset());
  set_nullable(true);
  unsigned_flag = sp_result_field->is_flag_set(UNSIGNED_FLAG);

  return false;
}

longlong Item_func_sp::val_int() {
  if (execute()) return error_int();
  if (null_value) return 0;
  return sp_result_field->val_int();
}

double Item_func_sp::val_real() {
  if (execute()) return error_real();
  if (null_value) return 0.0;
  return sp_result_field->val_real();
}

bool Item_func_sp::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  if (execute() || null_value) return true;
  return sp_result_field->get_date(ltime, fuzzydate);
}

bool Item_func_sp::get_time(MYSQL_TIME *ltime) {
  if (execute() || null_value) return true;
  return sp_result_field->get_time(ltime);
}

my_decimal *Item_func_sp::val_decimal(my_decimal *dec_buf) {
  if (execute()) return error_decimal(dec_buf);
  if (null_value) return nullptr;
  return sp_result_field->val_decimal(dec_buf);
}

String *Item_func_sp::val_str(String *str) {
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buf(str->charset());
  if (execute()) return error_str();
  if (null_value) return nullptr;
  /*
    result_field will set buf pointing to internal buffer
    of the resul_field. Due to this it will change any time
    when SP is executed. In order to prevent occasional
    corruption of returned value, we make here a copy.
  */
  sp_result_field->val_str(&buf);
  str->copy(buf);
  return str;
}

bool Item_func_sp::val_json(Json_wrapper *result) {
  if (sp_result_field->type() == MYSQL_TYPE_JSON) {
    if (execute()) return true;

    if (null_value) return false;

    Field_json *json_value = down_cast<Field_json *>(sp_result_field);
    return json_value->val_json(result);
  }

  /* purecov: begin deadcode */
  assert(false);
  my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
  return error_json();
  /* purecov: end */
}

/**
  @brief Execute function & store value in field.
         Will set null_value properly only for a successful execution.
  @return Function returns error status.
  @retval false on success.
  @retval true if an error occurred.
*/

bool Item_func_sp::execute() {
  THD *thd = current_thd;

  Internal_error_handler_holder<View_error_handler, Table_ref> view_handler(
      thd, m_name_resolution_ctx->view_error_handler,
      m_name_resolution_ctx->view_error_handler_arg);

  // Bind to an instance of the stored function:
  if (m_sp == nullptr) {
    m_sp = sp_setup_routine(thd, enum_sp_type::FUNCTION, m_name,
                            &thd->sp_func_cache);
    if (m_sp == nullptr) return true;
    if (sp_result_field != nullptr) {
      assert(sp_result_field->table->in_use == nullptr);
      sp_result_field->table->in_use = thd;
    }
  }

  /* Execute function and store the return value in the field. */

  if (execute_impl(thd)) {
    null_value = true;
    if (thd->killed) thd->send_kill_message();
    return true;
  }

  /* Check that the field (the value) is not NULL. */
  null_value = sp_result_field->is_null();

  return false;
}

/**
   @brief Execute function and store the return value in the field.
          Will set null_value properly only for a successful execution.

   @note This function was intended to be the concrete implementation of
    the interface function execute. This was never realized.

   @return The error state.
   @retval false on success
   @retval true if an error occurred.
*/
bool Item_func_sp::execute_impl(THD *thd) {
  bool err_status = true;
  Sub_statement_state statement_state;
  Security_context *save_security_ctx = thd->security_context();
  const enum enum_sp_data_access access =
      (m_sp->m_chistics->daccess == SP_DEFAULT_ACCESS)
          ? SP_DEFAULT_ACCESS_MAPPING
          : m_sp->m_chistics->daccess;

  DBUG_TRACE;

  if (m_name_resolution_ctx->security_ctx != nullptr) {
    /* Set view definer security context */
    thd->set_security_context(m_name_resolution_ctx->security_ctx);
  }
  if (sp_check_access(thd)) goto error;

  /*
    Throw an error if a non-deterministic function is called while
    statement-based replication (SBR) is active.
  */

  if (!m_deterministic && !trust_function_creators &&
      (access == SP_CONTAINS_SQL || access == SP_MODIFIES_SQL_DATA) &&
      (mysql_bin_log.is_open() &&
       thd->variables.binlog_format == BINLOG_FORMAT_STMT)) {
    my_error(ER_BINLOG_UNSAFE_ROUTINE, MYF(0));
    goto error;
  }

  /*
    Disable the binlogging if this is not a SELECT statement. If this is a
    SELECT, leave binlogging on, so execute_function() code writes the
    function call into binlog.
  */
  thd->reset_sub_statement_state(&statement_state, SUB_STMT_FUNCTION);
  err_status = m_sp->execute_function(thd, args, arg_count, sp_result_field);
  thd->restore_sub_statement_state(&statement_state);

error:
  thd->set_security_context(save_security_ctx);

  return err_status;
}

void Item_func_sp::make_field(Send_field *tmp_field) {
  DBUG_TRACE;
  assert(sp_result_field);
  sp_result_field->make_send_field(tmp_field);
  if (item_name.is_set()) tmp_field->col_name = item_name.ptr();
}

Item_result Item_func_sp::result_type() const {
  DBUG_TRACE;
  DBUG_PRINT("info", ("m_sp = %p", (void *)m_sp));
  assert(sp_result_field);
  return sp_result_field->result_type();
}

bool Item_func_found_rows::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->safe_to_cache_query = false;
  push_warning(current_thd, Sql_condition::SL_WARNING,
               ER_WARN_DEPRECATED_SYNTAX,
               ER_THD(current_thd, ER_WARN_DEPRECATED_FOUND_ROWS));
  return false;
}

longlong Item_func_found_rows::val_int() {
  assert(fixed);
  return current_thd->found_rows();
}

Field *Item_func_sp::tmp_table_field(TABLE *) {
  DBUG_TRACE;

  assert(sp_result_field);
  return sp_result_field;
}

/**
  @brief Checks if requested access to function can be granted to user.
    If function isn't found yet, it searches function first.
    If function can't be found or user don't have requested access
    error is raised.

  @param thd thread handler

  @return Indication if the access was granted or not.
  @retval false Access is granted.
  @retval true Requested access can't be granted or function doesn't exists.

*/

bool Item_func_sp::sp_check_access(THD *thd) {
  DBUG_TRACE;
  assert(m_sp);
  if (check_routine_access(thd, EXECUTE_ACL, m_sp->m_db.str, m_sp->m_name.str,
                           false, false))
    return true;

  return false;
}

bool Item_func_sp::fix_fields(THD *thd, Item **ref) {
  Security_context *save_security_ctx = thd->security_context();

  DBUG_TRACE;
  assert(!fixed);

  /*
    Checking privileges to execute the function while creating view and
    executing the function of select.
   */
  if (!thd->lex->is_view_context_analysis() ||
      (thd->lex->sql_command == SQLCOM_CREATE_VIEW)) {
    if (m_name_resolution_ctx->security_ctx != nullptr) {
      /* Set view definer security context */
      thd->set_security_context(m_name_resolution_ctx->security_ctx);
    }

    /*
      Check whether user has execute privilege or not
     */

    Internal_error_handler_holder<View_error_handler, Table_ref> view_handler(
        thd, m_name_resolution_ctx->view_error_handler,
        m_name_resolution_ctx->view_error_handler_arg);

    const bool res = check_routine_access(thd, EXECUTE_ACL, m_name->m_db.str,
                                          m_name->m_name.str, false, false);
    thd->set_security_context(save_security_ctx);

    if (res) return res;
  }

  /*
    We must call init_result_field before Item_func::fix_fields()
    to make m_sp and result_field members available to resolve_type(),
    which is called from Item_func::fix_fields().
  */
  if (init_result_field(thd)) return true;

  sp_pcontext *sp_ctx = m_sp->get_root_parsing_context();

  if (arg_count != sp_ctx->context_var_count()) {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "FUNCTION", m_sp->m_qname.str,
             sp_ctx->context_var_count(), arg_count);
    return true;
  }

  if (Item_func::fix_fields(thd, ref)) return true;

  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->data_type() == MYSQL_TYPE_INVALID) {
      sp_variable *var = sp_ctx->find_variable(i);
      if (args[i]->propagate_type(
              thd, is_numeric_type(var->type)
                       ? Type_properties(var->type, var->field_def.is_unsigned)
                   : is_string_type(var->type)
                       ? Type_properties(var->type, var->field_def.charset)
                       : Type_properties(var->type)))
        return true;
    }
  }

  if (thd->lex->is_view_context_analysis()) {
    /*
      Here we check privileges of the stored routine only during view
      creation, in order to validate the view.  A runtime check is
      performed in Item_func_sp::execute(), and this method is not
      called during context analysis.  Notice, that during view
      creation we do not infer into stored routine bodies and do not
      check privileges of its statements, which would probably be a
      good idea especially if the view has SQL SECURITY DEFINER and
      the used stored procedure has SQL SECURITY DEFINER.
    */
    if (sp_check_access(thd)) return true;
    /*
      Try to set and restore the security context to see whether it's valid
    */
    Security_context *save_security_context;
    if (m_sp->set_security_ctx(thd, &save_security_context)) return true;
    m_sp->m_security_ctx.restore_security_context(thd, save_security_context);
  }

  // Cleanup immediately, thus execute() will always attach to the routine.
  cleanup();

  return false;
}

void Item_func_sp::update_used_tables() {
  Item_func::update_used_tables();

  /* This is reset by Item_func::update_used_tables(). */
  set_stored_program();
}

void Item_func_sp::fix_after_pullout(Query_block *parent_query_block,
                                     Query_block *removed_query_block) {
  Item_func::fix_after_pullout(parent_query_block, removed_query_block);
}

/*
  uuid_short handling.

  The short uuid is defined as a longlong that contains the following bytes:

  Bytes  Comment
  1      Server_id & 255
  4      Startup time of server in seconds
  3      Incrementor

  This means that an uuid is guaranteed to be unique
  even in a replication environment if the following holds:

  - The last byte of the server id is unique
  - If you between two shutdown of the server don't get more than
    an average of 2^24 = 16M calls to uuid_short() per second.
*/

ulonglong uuid_value;

void uuid_short_init() {
  uuid_value =
      ((((ulonglong)server_id) << 56) + (((ulonglong)server_start_time) << 24));
}

bool Item_func_uuid_short::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

longlong Item_func_uuid_short::val_int() {
  ulonglong val;
  mysql_mutex_lock(&LOCK_uuid_generator);
  val = uuid_value++;
  mysql_mutex_unlock(&LOCK_uuid_generator);
  return (longlong)val;
}

bool Item_func_version::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  return false;
}

/**
  Check if schema and table are hidden by NDB engine.

  @param    thd           Thread handle.
  @param    schema_name   Schema name.
  @param    table_name    Table name.

  @retval   true          If schema and table are hidden by NDB.
  @retval   false         If schema and table are not hidden by NDB.
*/

static inline bool is_hidden_by_ndb(THD *thd, String *schema_name,
                                    String *table_name) {
  if (!strncmp(schema_name->ptr(), "ndb", 3)) {
    List<LEX_STRING> list;

    // Check if schema is of ndb and if it is hidden by it.
    LEX_STRING sch_name = schema_name->lex_string();
    list.push_back(&sch_name);
    ha_find_files(thd, nullptr, nullptr, nullptr, true, &list);
    if (list.elements == 0) {
      // Schema is hidden by ndb engine.
      return true;
    }

    // Check if table is hidden by ndb.
    if (table_name != nullptr) {
      list.clear();
      LEX_STRING tbl_name = table_name->lex_string();
      list.push_back(&tbl_name);
      ha_find_files(thd, schema_name->ptr(), nullptr, nullptr, false, &list);
      if (list.elements == 0) {
        // Table is hidden by ndb engine.
        return true;
      }
    }
  }

  return false;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    In order for INFORMATION_SCHEMA to skip listing database for which
    the user does not have rights, the following internal functions are used.

  Syntax:
    int CAN_ACCCESS_DATABASE(schema_name);

  @returns,
    1 - If current user has access.
    0 - If not.
*/

longlong Item_func_can_access_database::val_int() {
  DBUG_TRACE;

  // Read schema_name
  String schema_name;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  if (schema_name_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  schema_name_ptr->c_ptr_safe();

  // Check if schema is hidden.
  THD *thd = current_thd;
  if (is_hidden_by_ndb(thd, schema_name_ptr, nullptr)) return 0;

  // Skip INFORMATION_SCHEMA database
  if (is_infoschema_db(schema_name_ptr->ptr())) return 1;

  // Skip PERFORMANCE_SCHEMA database
  if (is_perfschema_db(schema_name_ptr->ptr())) return 1;

  if (lower_case_table_names == 2) {
    /*
      ACL code assumes that in l-c-t-n > 0 modes schema name passed to it
      is in lower case.
    */
    my_casedn_str(files_charset_info, schema_name_ptr->ptr());
  }

  // Check access
  Security_context *sctx = thd->security_context();
  if (!(sctx->master_access(schema_name_ptr->ptr()) &
            (DB_OP_ACLS | SHOW_DB_ACL) ||
        sctx->check_db_level_access(thd, schema_name_ptr->ptr(),
                                    schema_name_ptr->length()) ||
        !check_grant_db(thd, schema_name_ptr->ptr()))) {
    return 0;
  }

  return 1;
}

static bool check_table_and_trigger_access(Item **args, bool check_trigger_acl,
                                           bool *null_value) {
  DBUG_TRACE;

  // Read schema_name, table_name
  String schema_name;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String table_name;
  String *table_name_ptr = args[1]->val_str(&table_name);
  if (schema_name_ptr == nullptr || table_name_ptr == nullptr) {
    *null_value = true;
    return false;
  }

  // Make sure we have safe string to access.
  schema_name_ptr->c_ptr_safe();
  table_name_ptr->c_ptr_safe();

  // Check if table is hidden.
  THD *thd = current_thd;
  if (is_hidden_by_ndb(thd, schema_name_ptr, table_name_ptr)) return false;

  // Skip INFORMATION_SCHEMA database
  if (is_infoschema_db(schema_name_ptr->ptr())) return true;

  if (lower_case_table_names == 2) {
    /*
      ACL code assumes that in l-c-t-n > 0 modes schema and table names
      passed to it are in lower case.
    */
    schema_name_ptr->length(
        my_casedn_str(files_charset_info, schema_name_ptr->ptr()));
    table_name_ptr->length(
        my_casedn_str(files_charset_info, table_name_ptr->ptr()));
  }

  // Check access
  Access_bitmask db_access = 0;
  if (check_access(thd, SELECT_ACL, schema_name_ptr->ptr(), &db_access, nullptr,
                   false, true))
    return false;

  Table_ref table_list;
  table_list.db = schema_name_ptr->ptr();
  table_list.db_length = schema_name_ptr->length();
  table_list.table_name = table_name_ptr->ptr();
  table_list.table_name_length = table_name_ptr->length();
  table_list.grant.privilege = db_access;

  if (check_trigger_acl == false) {
    if (db_access & TABLE_OP_ACLS) return true;

    // Check table access
    if (check_grant(thd, TABLE_OP_ACLS, &table_list, true, 1, true))
      return false;
  } else  // Trigger check.
  {
    // Check trigger access
    if (check_trigger_acl &&
        check_table_access(thd, TRIGGER_ACL, &table_list, false, 1, true))
      return false;
  }

  return true;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from new DD using system views.
    In order for INFORMATION_SCHEMA to skip listing table for which
    the user does not have rights, the following UDF's is used.

  Syntax:
    int CAN_ACCCESS_TABLE(schema_name, table_name);

  @returns,
    1 - If current user has access.
    0 - If not.
*/
longlong Item_func_can_access_table::val_int() {
  DBUG_TRACE;

  if (check_table_and_trigger_access(args, false, &null_value)) return 1;

  return 0;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from new DD using system views.
    In order for INFORMATION_SCHEMA to skip listing user accpounts for which
    the user does not have rights, the following SQL function is used.

  Syntax:
    int CAN_ACCCESS_USER(user_part, host_part);

  @returns,
    1 - If current user has access.
    0 - If not.

  @sa @ref acl_can_access_user
*/
longlong Item_func_can_access_user::val_int() {
  DBUG_TRACE;

  THD *thd = current_thd;
  // Read user, host
  String user_name;
  String *user_name_ptr = args[0]->val_str(&user_name);
  String host_name;
  String *host_name_ptr = args[1]->val_str(&host_name);
  if (host_name_ptr == nullptr || user_name_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  host_name_ptr->c_ptr_safe();
  user_name_ptr->c_ptr_safe();
  MYSQL_LEX_STRING user_str = {const_cast<char *>(user_name_ptr->ptr()),
                               user_name_ptr->length()};
  MYSQL_LEX_STRING
  host_str = {const_cast<char *>(host_name_ptr->ptr()),
              host_name_ptr->length()};
  LEX_USER user;
  if (!LEX_USER::init(&user, thd, &user_str, &host_str)) return 0;

  return acl_can_access_user(thd, &user) ? 1 : 0;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from new DD using system views. In
    order for INFORMATION_SCHEMA to skip listing table for which the user
    does not have rights on triggers, the following UDF's is used.

  Syntax:
    int CAN_ACCCESS_TRIGGER(schema_name, table_name);

  @returns,
    1 - If current user has access.
    0 - If not.
*/
longlong Item_func_can_access_trigger::val_int() {
  DBUG_TRACE;

  if (check_table_and_trigger_access(args, true, &null_value)) return 1;

  return 0;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views. In
    order for INFORMATION_SCHEMA to skip listing routine for which the user
    does not have rights, the following UDF's is used.

  Syntax:
    int CAN_ACCESS_ROUTINE(schema_name, name, type, user, definer,
                           check_full_access);

  @returns,
    1 - If current user has access.
    0 - If not.
*/
longlong Item_func_can_access_routine::val_int() {
  DBUG_TRACE;

  // Read schema_name, table_name
  String schema_name;
  String routine_name;
  String type;
  String definer;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String *routine_name_ptr = args[1]->val_str(&routine_name);
  String *type_ptr = args[2]->val_str(&type);
  String *definer_ptr = args[3]->val_str(&definer);
  const bool check_full_access = args[4]->val_int();
  if (schema_name_ptr == nullptr || routine_name_ptr == nullptr ||
      type_ptr == nullptr || definer_ptr == nullptr || args[4]->null_value) {
    null_value = true;
    return 0;
  }

  // Make strings safe.
  schema_name_ptr->c_ptr_safe();
  routine_name_ptr->c_ptr_safe();
  type_ptr->c_ptr_safe();
  definer_ptr->c_ptr_safe();

  const bool is_procedure = (strcmp(type_ptr->ptr(), "PROCEDURE") == 0);

  // Skip INFORMATION_SCHEMA database
  if (is_infoschema_db(schema_name_ptr->ptr()) ||
      !my_strcasecmp(system_charset_info, schema_name_ptr->ptr(), "sys"))
    return 1;

  /*
    Check if user has full access to the routine properties (i.e including
    stored routine code), or partial access (i.e to view its other properties).
  */

  char user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING user_name = {user_name_holder, USERNAME_LENGTH};

  char host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING host_name = {host_name_holder, HOSTNAME_LENGTH};

  parse_user(definer_ptr->ptr(), definer_ptr->length(), user_name.str,
             &user_name.length, host_name.str, &host_name.length);

  if (lower_case_table_names == 2) {
    /*
      ACL code assumes that in l-c-t-n > 0 modes schema name passed to it
      is in lower case.
    */
    my_casedn_str(files_charset_info, schema_name_ptr->ptr());
  }

  THD *thd = current_thd;
  const bool full_access = has_full_view_routine_access(
      thd, schema_name_ptr->ptr(), user_name.str, host_name.str);

  if (check_full_access) {
    return full_access ? 1 : 0;
  } else if (!full_access && !has_partial_view_routine_access(
                                 thd, schema_name_ptr->ptr(),
                                 routine_name_ptr->ptr(), is_procedure)) {
    return 0;
  }

  return 1;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    In order for INFORMATION_SCHEMA to skip listing event for which
    the user does not have rights, the following internal functions are used.

  Syntax:
    int CAN_ACCCESS_EVENT(schema_name);

  @returns,
    1 - If current user has access.
    0 - If not.
*/

longlong Item_func_can_access_event::val_int() {
  DBUG_TRACE;

  // Read schema_name
  String schema_name;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  if (schema_name_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  schema_name_ptr->c_ptr_safe();

  // Check if schema is hidden.
  THD *thd = current_thd;
  if (is_hidden_by_ndb(thd, schema_name_ptr, nullptr)) return 0;

  // Skip INFORMATION_SCHEMA database
  if (is_infoschema_db(schema_name_ptr->ptr())) return 1;

  if (lower_case_table_names == 2) {
    /*
      ACL code assumes that in l-c-t-n > 0 modes schema name passed to it
      is in lower case.
    */
    my_casedn_str(files_charset_info, schema_name_ptr->ptr());
  }

  // Check access
  if (check_access(thd, EVENT_ACL, schema_name_ptr->ptr(), nullptr, nullptr,
                   false, true)) {
    return 0;
  }

  return 1;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    In order for INFORMATION_SCHEMA to skip listing resource groups for which
    the user does not have rights, the following internal functions are used.

  Syntax:
    int CAN_ACCCESS_RESOURCE_GROUP(resource_group_name);

  @returns,
    1 - If current user has access.
    0 - If not.
*/

longlong Item_func_can_access_resource_group::val_int() {
  DBUG_TRACE;

  auto mgr_ptr = resourcegroups::Resource_group_mgr::instance();
  if (!mgr_ptr->resource_group_support()) {
    null_value = true;
    return false;
  }

  // Read resource group name.
  String res_grp_name;
  String *res_grp_name_ptr = args[0]->val_str(&res_grp_name);

  if (res_grp_name_ptr == nullptr) {
    null_value = true;
    return false;
  }

  // Make sure we have safe string to access.
  res_grp_name_ptr->c_ptr_safe();

  MDL_ticket *ticket = nullptr;
  if (mgr_ptr->acquire_shared_mdl_for_resource_group(
          current_thd, res_grp_name_ptr->c_ptr(), MDL_EXPLICIT, &ticket, false))
    return false;

  auto res_grp_ptr = mgr_ptr->get_resource_group(res_grp_name_ptr->c_ptr());
  longlong result = true;
  if (res_grp_ptr != nullptr) {
    Security_context *sctx = current_thd->security_context();
    if (res_grp_ptr->type() == resourcegroups::Type::SYSTEM_RESOURCE_GROUP) {
      if (!(sctx->check_access(SUPER_ACL) ||
            sctx->has_global_grant(STRING_WITH_LEN("RESOURCE_GROUP_ADMIN"))
                .first))
        result = false;
    } else {
      if (!(sctx->check_access(SUPER_ACL) ||
            sctx->has_global_grant(STRING_WITH_LEN("RESOURCE_GROUP_ADMIN"))
                .first ||
            sctx->has_global_grant(STRING_WITH_LEN("RESOURCE_GROUP_USER"))
                .first))
        result = false;
    }
  }
  mgr_ptr->release_shared_mdl_for_resource_group(current_thd, ticket);
  return res_grp_ptr != nullptr ? result : false;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    In order for INFORMATION_SCHEMA to skip listing column for which
    the user does not have rights, the following UDF's is used.

  Syntax:
    int CAN_ACCCESS_COLUMN(schema_name,
                           table_name,
                           field_name);

  @returns,
    1 - If current user has access.
    0 - If not.
*/
longlong Item_func_can_access_column::val_int() {
  DBUG_TRACE;

  // Read schema_name, table_name
  String schema_name;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String table_name;
  String *table_name_ptr = args[1]->val_str(&table_name);
  if (schema_name_ptr == nullptr || table_name_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  schema_name_ptr->c_ptr_safe();
  table_name_ptr->c_ptr_safe();

  // Check if table is hidden.
  THD *thd = current_thd;
  if (is_hidden_by_ndb(thd, schema_name_ptr, table_name_ptr)) return 0;

  // Read column_name.
  String column_name;
  String *column_name_ptr = args[2]->val_str(&column_name);
  if (column_name_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  column_name_ptr->c_ptr_safe();

  // Skip INFORMATION_SCHEMA database
  if (is_infoschema_db(schema_name_ptr->ptr())) return 1;

  // Check access
  GRANT_INFO grant_info;

  if (lower_case_table_names == 2) {
    /*
      ACL code assumes that in l-c-t-n > 0 modes schema and table names
      passed to it are in lower case.
    */
    my_casedn_str(files_charset_info, schema_name_ptr->ptr());
    my_casedn_str(files_charset_info, table_name_ptr->ptr());
  }

  if (check_access(thd, SELECT_ACL, schema_name_ptr->ptr(),
                   &grant_info.privilege, nullptr, false, true))
    return 0;

  const uint col_access =
      get_column_grant(thd, &grant_info, schema_name_ptr->ptr(),
                       table_name_ptr->ptr(), column_name_ptr->ptr()) &
      COL_ACLS;
  if (!col_access) {
    return 0;
  }

  return 1;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    In order for INFORMATION_SCHEMA to skip listing view definition
    for the user without rights, the following UDF's is used.

  Syntax:
    int CAN_ACCESS_VIEW(schema_name, view_name, definer, options);

  @returns,
    1 - If current user has access.
    0 - If not.
*/
longlong Item_func_can_access_view::val_int() {
  DBUG_TRACE;

  // Read schema_name, table_name
  String schema_name;
  String table_name;
  String definer;
  String options;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String *table_name_ptr = args[1]->val_str(&table_name);
  String *definer_ptr = args[2]->val_str(&definer);
  String *options_ptr = args[3]->val_str(&options);
  if (schema_name_ptr == nullptr || table_name_ptr == nullptr ||
      definer_ptr == nullptr || options_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Make strings safe.
  schema_name_ptr->c_ptr_safe();
  table_name_ptr->c_ptr_safe();
  definer_ptr->c_ptr_safe();
  options_ptr->c_ptr_safe();

  // Skip INFORMATION_SCHEMA database
  if (is_infoschema_db(schema_name_ptr->ptr()) ||
      !my_strcasecmp(system_charset_info, schema_name_ptr->ptr(), "sys"))
    return 1;

  if (lower_case_table_names == 2) {
    /*
      ACL code assumes that in l-c-t-n > 0 modes schema and table names
      passed to it are in lower case. Although for view names lowercasing
      is not strictly necessary until bug#20356 is fixed we still do it
      to be consistent with CAN_ACCESS_TABLE().
    */
    schema_name_ptr->length(
        my_casedn_str(files_charset_info, schema_name_ptr->ptr()));
    table_name_ptr->length(
        my_casedn_str(files_charset_info, table_name_ptr->ptr()));
  }

  // Check if view is valid. If view is invalid then push invalid view
  // warning.
  bool is_view_valid = true;
  std::unique_ptr<dd::Properties> view_options(
      dd::Properties::parse_properties(options_ptr->c_ptr_safe()));

  // Warn if the property string is corrupt.
  if (!view_options.get()) {
    LogErr(WARNING_LEVEL, ER_WARN_PROPERTY_STRING_PARSE_FAILED,
           options_ptr->c_ptr_safe());
    assert(false);
    return 0;
  }

  if (view_options->get("view_valid", &is_view_valid)) return 0;

  // Show warning/error if view is invalid.
  THD *thd = current_thd;
  const String db_str(schema_name_ptr->c_ptr_safe(), system_charset_info);
  const String name_str(table_name_ptr->c_ptr_safe(), system_charset_info);
  if (!is_view_valid &&
      !thd->lex->m_IS_table_stats.check_error_for_key(db_str, name_str)) {
    const std::string err_message = push_view_warning_or_error(
        current_thd, schema_name_ptr->ptr(), table_name_ptr->ptr());

    /*
      Cache the error message, so that we do not show the same error multiple
      times.
     */
    thd->lex->m_IS_table_stats.store_error_message(db_str, name_str, nullptr,
                                                   err_message.c_str());
  }

  //
  // Check if definer user/host has access.
  //

  Security_context *sctx = thd->security_context();

  // NOTE: this is a copy/paste from sp_head::set_definer().

  char user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING user_name = {user_name_holder, USERNAME_LENGTH};

  char host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING host_name = {host_name_holder, HOSTNAME_LENGTH};

  parse_user(definer_ptr->ptr(), definer_ptr->length(), user_name.str,
             &user_name.length, host_name.str, &host_name.length);

  const std::string definer_user(user_name.str, user_name.length);
  const std::string definer_host(host_name.str, host_name.length);

  if (!strcmp(definer_user.c_str(), sctx->priv_user().str) &&
      !my_strcasecmp(system_charset_info, definer_host.c_str(),
                     sctx->priv_host().str))
    return 1;

  //
  // Check for ACL's
  //
  Table_ref table_list;
  table_list.db = schema_name_ptr->ptr();
  table_list.db_length = schema_name_ptr->length();
  table_list.table_name = table_name_ptr->ptr();
  table_list.table_name_length = table_name_ptr->length();

  if (!check_table_access(thd, (SHOW_VIEW_ACL | SELECT_ACL), &table_list, false,
                          1, true))
    return 1;

  return 0;
}

/**
  Skip hidden tables, columns, indexes and index elements. Additionally,
  skip generated invisible primary key(GIPK) and key column when system
  variable show_gipk_in_create_table_and_information_schema is set to
  OFF.
  Do *not* skip hidden tables, columns, indexes and index elements,
  when SHOW EXTENDED command are run. GIPK and key column are skipped
  even for SHOW EXTENDED command.

  Syntax:
    longlong IS_VISIBLE_DD_OBJECT(type_of_hidden_table [, is_object_hidden
                                  [, object_options]])

  @returns,
    1 - If dd object is visible
    0 - If not visible.
*/
longlong Item_func_is_visible_dd_object::val_int() {
  DBUG_TRACE;

  assert(arg_count > 0 && arg_count <= 3);
  assert(args[0]->null_value == false);

  if (args[0]->null_value || (arg_count >= 2 && args[1]->null_value)) {
    null_value = true;
    return false;
  }

  null_value = false;
  THD *thd = current_thd;

  auto table_type =
      static_cast<dd::Abstract_table::enum_hidden_type>(args[0]->val_int());

  bool show_table = (table_type == dd::Abstract_table::HT_VISIBLE);

  // Make I_S.TABLES show the hidden system view 'show_statistics' for
  // testing purpose.
  DBUG_EXECUTE_IF("fetch_system_view_definition", { return 1; });

  if (thd->lex->m_extended_show)
    show_table =
        show_table || (table_type == dd::Abstract_table::HT_HIDDEN_DDL);

  if (arg_count == 1 || show_table == false) return (show_table ? 1 : 0);

  // Skip generated invisible primary key and key columns.
  if (arg_count == 3 && !args[2]->is_null() &&
      !thd->variables.show_gipk_in_create_table_and_information_schema) {
    String options;
    String *options_ptr = args[2]->val_str(&options);

    if (options_ptr != nullptr) {
      // Read options from properties
      std::unique_ptr<dd::Properties> p(
          dd::Properties::parse_properties(options_ptr->c_ptr_safe()));

      if (p.get()) {
        if (p->exists("gipk")) {
          bool gipk_value = false;
          p->get("gipk", &gipk_value);
          if (gipk_value) return 0;
        }
      } else {
        // Warn if the property string is corrupt.
        LogErr(WARNING_LEVEL, ER_WARN_PROPERTY_STRING_PARSE_FAILED,
               options_ptr->c_ptr_safe());
        assert(false);
      }
    }
    /*
      Even if object is not a GIPK column/key we still need to check if it is
      marked as hidden.
    */
  }

  bool show_non_table_objects;
  if (thd->lex->m_extended_show)
    show_non_table_objects = true;
  else
    show_non_table_objects = (args[1]->val_bool() == false);

  return show_non_table_objects ? 1 : 0;
}

/**
  Get table statistics from dd::info_schema::get_table_statistics.

  @param      args       List of parameters in following order,

                         - Schema_name
                         - Table_name
                         - Engine_name
                         - se_private_id
                         - Hidden_table
                         - Tablespace_se_private_data
                         - Table_se_private_data (Used if stype is AUTO_INC)
                         - Partition name (optional argument).

  @param      arg_count  Number of arguments in 'args'

  @param      stype      Type of statistics that is requested

  @param[out] null_value Marked true indicating NULL, if there is no value.

  @returns ulonglong representing the statistics requested.
*/

static ulonglong get_table_statistics(
    Item **args, uint arg_count, dd::info_schema::enum_table_stats_type stype,
    bool *null_value) {
  DBUG_TRACE;
  *null_value = false;

  // Reads arguments
  String schema_name;
  String table_name;
  String engine_name;
  String ts_se_private_data;
  String tbl_se_private_data;
  String partition_name;
  String *partition_name_ptr = nullptr;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String *table_name_ptr = args[1]->val_str(&table_name);
  String *engine_name_ptr = args[2]->val_str(&engine_name);
  const bool skip_hidden_table = args[4]->val_int();
  String *ts_se_private_data_ptr = args[5]->val_str(&ts_se_private_data);
  const ulonglong stat_data = args[6]->val_uint();
  const ulonglong cached_timestamp = args[7]->val_uint();

  String *tbl_se_private_data_ptr = nullptr;

  /*
    The same native function used by I_S.TABLES is used by I_S.PARTITIONS.
    We invoke native function with partition name only with I_S.PARTITIONS
    as a last argument. So, we check for argument count below, before
    reading partition name.
  */
  if (stype == dd::info_schema::enum_table_stats_type::AUTO_INCREMENT) {
    tbl_se_private_data_ptr = args[8]->val_str(&tbl_se_private_data);
    if (arg_count == 10) partition_name_ptr = args[9]->val_str(&partition_name);
  } else if (arg_count == 9)
    partition_name_ptr = args[8]->val_str(&partition_name);

  if (schema_name_ptr == nullptr || table_name_ptr == nullptr ||
      engine_name_ptr == nullptr || skip_hidden_table) {
    *null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  schema_name_ptr->c_ptr_safe();
  table_name_ptr->c_ptr_safe();
  engine_name_ptr->c_ptr_safe();

  // Do not read dynamic stats for I_S tables.
  if (is_infoschema_db(schema_name_ptr->ptr())) return 0;

  // Read the statistic value from cache.
  THD *thd = current_thd;
  const dd::Object_id se_private_id = (dd::Object_id)args[3]->val_uint();
  const ulonglong result = thd->lex->m_IS_table_stats.read_stat(
      thd, *schema_name_ptr, *table_name_ptr, *engine_name_ptr,
      (partition_name_ptr ? partition_name_ptr->c_ptr_safe() : nullptr),
      se_private_id,
      (ts_se_private_data_ptr ? ts_se_private_data_ptr->c_ptr_safe() : nullptr),
      (tbl_se_private_data_ptr ? tbl_se_private_data_ptr->c_ptr_safe()
                               : nullptr),
      stat_data, cached_timestamp, stype);

  return result;
}

longlong Item_func_internal_table_rows::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::TABLE_ROWS,
      &null_value);

  if (null_value == false && result == (ulonglong)-1) null_value = true;

  return result;
}

longlong Item_func_internal_avg_row_length::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count,
      dd::info_schema::enum_table_stats_type::TABLE_AVG_ROW_LENGTH,
      &null_value);
  return result;
}

longlong Item_func_internal_data_length::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::DATA_LENGTH,
      &null_value);
  return result;
}

longlong Item_func_internal_max_data_length::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::MAX_DATA_LENGTH,
      &null_value);
  return result;
}

longlong Item_func_internal_index_length::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::INDEX_LENGTH,
      &null_value);
  return result;
}

longlong Item_func_internal_data_free::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::DATA_FREE,
      &null_value);

  if (null_value == false && result == (ulonglong)-1) null_value = true;

  return result;
}

longlong Item_func_internal_auto_increment::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::AUTO_INCREMENT,
      &null_value);

  if (null_value == false && result < (ulonglong)1) null_value = true;

  return result;
}

longlong Item_func_internal_checksum::val_int() {
  DBUG_TRACE;

  const ulonglong result = get_table_statistics(
      args, arg_count, dd::info_schema::enum_table_stats_type::CHECKSUM,
      &null_value);

  if (null_value == false && result == 0) null_value = true;

  return result;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    INFORMATION_SCHEMA.STATISTICS.COMMENT is used to indicate if the indexes are
    disabled by ALTER TABLE ... DISABLE KEYS. This property of table is stored
    in mysql.tables.options as 'keys_disabled=0/1/'. This internal function
    returns value of option 'keys_disabled' for a given table.

  Syntax:
    int INTERNAL_KEYS_DISABLED(table_options);

  @returns,
    1 - If keys are disabled.
    0 - If not.
*/
longlong Item_func_internal_keys_disabled::val_int() {
  DBUG_TRACE;

  // Read options.
  String options;
  String *options_ptr = args[0]->val_str(&options);
  if (options_ptr == nullptr) return 0;

  // Read table option from properties
  std::unique_ptr<dd::Properties> p(
      dd::Properties::parse_properties(options_ptr->c_ptr_safe()));

  // Warn if the property string is corrupt.
  if (!p.get()) {
    LogErr(WARNING_LEVEL, ER_WARN_PROPERTY_STRING_PARSE_FAILED,
           options_ptr->c_ptr_safe());
    assert(false);
    return 0;
  }

  // Read keys_disabled sub type.
  uint keys_disabled = 0;
  p->get("keys_disabled", &keys_disabled);

  return keys_disabled;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    INFORMATION_SCHEMA.STATISTICS.CARDINALITY reads data from SE.

  Syntax:
    int INTERNAL_INDEX_COLUMN_CARDINALITY(
          schema_name,
          table_name,
          index_name,
          column_name,
          index_ordinal_position,
          column_ordinal_position,
          engine,
          se_private_id,
          is_hidden,
          stat_cardinality,
          cached_timestamp);

  @returns Cardinatily. Or sets null_value to true if cardinality is -1.
*/
longlong Item_func_internal_index_column_cardinality::val_int() {
  DBUG_TRACE;
  null_value = false;

  // Read arguments
  String schema_name;
  String table_name;
  String index_name;
  String column_name;
  String engine_name;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String *table_name_ptr = args[1]->val_str(&table_name);
  String *index_name_ptr = args[2]->val_str(&index_name);
  String *column_name_ptr = args[3]->val_str(&column_name);
  const uint index_ordinal_position = args[4]->val_uint();
  const uint column_ordinal_position = args[5]->val_uint();
  String *engine_name_ptr = args[6]->val_str(&engine_name);
  const dd::Object_id se_private_id = (dd::Object_id)args[7]->val_uint();
  const bool hidden_index = args[8]->val_int();
  const ulonglong stat_cardinality = args[9]->val_uint();
  const ulonglong cached_timestamp = args[10]->val_uint();

  // stat_cardinality and cached_timestamp from mysql.index_stats can be null
  // when stat is fetched for 1st time without executing ANALYZE command.
  if (schema_name_ptr == nullptr || table_name_ptr == nullptr ||
      index_name_ptr == nullptr || engine_name_ptr == nullptr ||
      column_name_ptr == nullptr || args[4]->null_value ||
      args[5]->null_value || args[8]->null_value || hidden_index) {
    null_value = true;
    return 0;
  }

  // Make sure we have safe string to access.
  schema_name_ptr->c_ptr_safe();
  table_name_ptr->c_ptr_safe();
  index_name_ptr->c_ptr_safe();
  column_name_ptr->c_ptr_safe();
  engine_name_ptr->c_ptr_safe();

  ulonglong result = 0;
  THD *thd = current_thd;
  result = thd->lex->m_IS_table_stats.read_stat(
      thd, *schema_name_ptr, *table_name_ptr, *index_name_ptr, nullptr,
      *column_name_ptr, index_ordinal_position - 1, column_ordinal_position - 1,
      *engine_name_ptr, se_private_id, nullptr, nullptr, stat_cardinality,
      cached_timestamp,
      dd::info_schema::enum_table_stats_type::INDEX_COLUMN_CARDINALITY);

  if (result == (ulonglong)-1) null_value = true;

  return result;
}

/**
  Retrieve tablespace statistics from SE

  @param      thd        The current thread.

  @param      args       List of parameters in following order,

                         - Tablespace_name
                         - Engine_name
                         - Tablespace_se_private_data

  @param[out] null_value Marked true indicating NULL, if there is no value.
*/

void retrieve_tablespace_statistics(THD *thd, Item **args, bool *null_value) {
  DBUG_TRACE;
  *null_value = false;

  // Reads arguments
  String tablespace_name;
  String *tablespace_name_ptr = args[0]->val_str(&tablespace_name);
  String file_name;
  String *file_name_ptr = args[1]->val_str(&file_name);
  String engine_name;
  String *engine_name_ptr = args[2]->val_str(&engine_name);
  String ts_se_private_data;
  String *ts_se_private_data_ptr = args[3]->val_str(&ts_se_private_data);

  if (tablespace_name_ptr == nullptr || file_name_ptr == nullptr ||
      engine_name_ptr == nullptr) {
    *null_value = true;
    return;
  }

  // Make sure we have safe string to access.
  tablespace_name_ptr->c_ptr_safe();
  file_name_ptr->c_ptr_safe();
  engine_name_ptr->c_ptr_safe();

  // Read the statistic value from cache.
  if (thd->lex->m_IS_tablespace_stats.read_stat(
          thd, *tablespace_name_ptr, *file_name_ptr, *engine_name_ptr,
          (ts_se_private_data_ptr ? ts_se_private_data_ptr->c_ptr_safe()
                                  : nullptr)))
    *null_value = true;
}

longlong Item_func_internal_tablespace_id::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_ID, &result);
    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_logfile_group_number::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_LOGFILE_GROUP_NUMBER,
        &result);
    if (result == (ulonglong)-1) null_value = true;

    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_free_extents::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_FREE_EXTENTS, &result);
    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_total_extents::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_TOTAL_EXTENTS, &result);
    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_extent_size::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_EXTENT_SIZE, &result);
    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_initial_size::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_INITIAL_SIZE, &result);
    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_maximum_size::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_MAXIMUM_SIZE, &result);
    if (result == (ulonglong)-1) null_value = true;

    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_autoextend_size::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_AUTOEXTEND_SIZE,
        &result);
    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_version::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_VERSION, &result);
    if (result == (ulonglong)-1) null_value = true;

    return result;
  }

  return result;
}

longlong Item_func_internal_tablespace_data_free::val_int() {
  DBUG_TRACE;
  ulonglong result = -1;

  THD *thd = current_thd;
  retrieve_tablespace_statistics(thd, args, &null_value);
  if (null_value == false) {
    thd->lex->m_IS_tablespace_stats.get_stat(
        dd::info_schema::enum_tablespace_stats_type::TS_DATA_FREE, &result);
    return result;
  }

  return result;
}

Item_func_version::Item_func_version(const POS &pos)
    : Item_static_string_func(pos, NAME_STRING("version()"), server_version,
                              strlen(server_version), system_charset_info,
                              DERIVATION_SYSCONST) {}

/*
    Syntax:
      string get_dd_char_length()
*/
longlong Item_func_internal_dd_char_length::val_int() {
  DBUG_TRACE;
  null_value = false;

  const dd::enum_column_types col_type =
      (dd::enum_column_types)args[0]->val_int();
  uint field_length = args[1]->val_int();
  String cs_name;
  String *cs_name_ptr = args[2]->val_str(&cs_name);
  const uint flag = args[3]->val_int();

  // Stop if we found a NULL argument.
  if (args[0]->null_value || args[1]->null_value || cs_name_ptr == nullptr ||
      args[3]->null_value) {
    null_value = true;
    return 0;
  }

  // Read character set.
  CHARSET_INFO *cs = get_charset_by_name(cs_name_ptr->c_ptr_safe(), MYF(0));
  if (!cs) {
    null_value = true;
    return 0;
  }

  // Check data types for getting info
  const enum_field_types field_type = dd_get_old_field_type(col_type);

  if (field_type == MYSQL_TYPE_VECTOR) {
    /* For vector types, we can return the field_length as is. */
    return field_length;
  }

  const bool blob_flag = is_blob(field_type);
  if (!blob_flag && field_type != MYSQL_TYPE_ENUM &&
      field_type != MYSQL_TYPE_SET &&
      field_type != MYSQL_TYPE_VARCHAR &&  // For varbinary type
      field_type != MYSQL_TYPE_STRING)     // For binary type
  {
    null_value = true;
    return 0;
  }

  std::ostringstream oss("");
  switch (field_type) {
    case MYSQL_TYPE_BLOB:
      field_length = 65535;
      break;
    case MYSQL_TYPE_TINY_BLOB:
      field_length = 255;
      break;
    case MYSQL_TYPE_MEDIUM_BLOB:
      field_length = 16777215;
      break;
    case MYSQL_TYPE_LONG_BLOB:
      field_length = 4294967295;
      break;
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
      break;
    default:
      break;
  }

  if (!flag && field_length) {
    if (blob_flag)
      return field_length / cs->mbminlen;
    else
      return field_length / cs->mbmaxlen;
  } else if (flag && field_length) {
    return field_length;
  }

  return 0;
}

longlong Item_func_internal_get_view_warning_or_error::val_int() {
  DBUG_TRACE;

  String schema_name;
  String table_name;
  String table_type;
  String *schema_name_ptr = args[0]->val_str(&schema_name);
  String *table_name_ptr = args[1]->val_str(&table_name);
  String *table_type_ptr = args[2]->val_str(&table_type);

  if (table_type_ptr == nullptr || schema_name_ptr == nullptr ||
      table_name_ptr == nullptr) {
    return 0;
  }

  String options;
  String *options_ptr = args[3]->val_str(&options);
  if (strcmp(table_type_ptr->c_ptr_safe(), "VIEW") == 0 &&
      options_ptr != nullptr) {
    bool is_view_valid = true;
    std::unique_ptr<dd::Properties> view_options(
        dd::Properties::parse_properties(options_ptr->c_ptr_safe()));

    // Warn if the property string is corrupt.
    if (!view_options.get()) {
      LogErr(WARNING_LEVEL, ER_WARN_PROPERTY_STRING_PARSE_FAILED,
             options_ptr->c_ptr_safe());
      assert(false);
      return 0;
    }

    // Return 0 if get_bool() or push_view_warning_or_error() fails
    if (view_options->get("view_valid", &is_view_valid)) return 0;

    if (is_view_valid == false) {
      push_view_warning_or_error(current_thd, schema_name_ptr->c_ptr_safe(),
                                 table_name_ptr->c_ptr_safe());
      return 0;
    }
  }

  return 1;
}

/**
  @brief
    INFORMATION_SCHEMA picks metadata from DD using system views.
    INFORMATION_SCHEMA.STATISTICS.SUB_PART represents index sub part length.
    This internal function is used to get index sub part length.

  Syntax:
    int GET_DD_INDEX_SUB_PART_LENGTH(
          index_column_usage_length,
          column_type,
          column_length,
          column_collation_id,
          index_type);

  @returns Index sub part length.
*/
longlong Item_func_get_dd_index_sub_part_length::val_int() {
  DBUG_TRACE;
  null_value = true;

  // Read arguments
  const uint key_part_length = args[0]->val_int();
  const dd::enum_column_types col_type =
      static_cast<dd::enum_column_types>(args[1]->val_int());
  const uint column_length = args[2]->val_int();
  const uint csid = args[3]->val_int();
  const dd::Index::enum_index_type idx_type =
      static_cast<dd::Index::enum_index_type>(args[4]->val_int());
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value || args[4]->null_value)
    return 0;

  // Read server col_type and check if we have key part.
  const enum_field_types field_type = dd_get_old_field_type(col_type);
  if (!Field::type_can_have_key_part(field_type)) return 0;

  // Calculate the key length for the column. Note that we pass inn dummy values
  // for "decimals", "is_unsigned" and "elements" since none of those arguments
  // will affect the key length for any of the data types that can have a prefix
  // index (see Field::type_can_have_key_part above).
  const uint32 column_key_length =
      calc_key_length(field_type, column_length, 0, false, 0);

  // Read column charset id from args[3]
  const CHARSET_INFO *column_charset = &my_charset_latin1;
  if (csid) {
    column_charset = get_charset(csid, MYF(0));
    assert(column_charset);
  }

  if ((idx_type != dd::Index::IT_FULLTEXT) &&
      (key_part_length != column_key_length)) {
    const longlong sub_part_length = key_part_length / column_charset->mbmaxlen;
    null_value = false;
    return sub_part_length;
  }

  return 0;
}

/**
  @brief
   Internal function used by INFORMATION_SCHEMA implementation to check
   if a role is a mandatory role.

  Syntax:
    int INTERNAL_IS_MANDATORY_ROLE(role_user, role_host);

  @returns,
    1 - If the role is mandatory.
    0 - If not.
*/

longlong Item_func_internal_is_mandatory_role::val_int() {
  DBUG_TRACE;

  // Read schema_name
  String role_name;
  String *role_name_ptr = args[0]->val_str(&role_name);
  String role_host;
  String *role_host_ptr = args[1]->val_str(&role_host);
  if (role_name_ptr == nullptr || role_host_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  // Create Auth_id for ID being searched.
  LEX_CSTRING lex_user;
  lex_user.str = role_name_ptr->c_ptr_safe();
  lex_user.length = role_name_ptr->length();

  LEX_CSTRING lex_host;
  lex_host.str = role_host_ptr->c_ptr_safe();
  lex_host.length = role_host_ptr->length();

  bool is_mandatory{false};
  if (is_mandatory_role(lex_user, lex_host, &is_mandatory)) {
    push_warning_printf(
        current_thd, Sql_condition::SL_WARNING,
        ER_FAILED_TO_DETERMINE_IF_ROLE_IS_MANDATORY,
        ER_THD(current_thd, ER_FAILED_TO_DETERMINE_IF_ROLE_IS_MANDATORY),
        lex_user.str, lex_host.str);
  }

  return is_mandatory;
}

longlong Item_func_internal_use_terminology_previous::val_int() {
  DBUG_TRACE;
  bool use_previous{false};
  THD *thd = current_thd;
  if (thd) {
    if (thd->variables.terminology_use_previous !=
            terminology_use_previous::enum_compatibility_version::NONE &&
        thd->variables.terminology_use_previous <=
            (ulong)terminology_use_previous::enum_compatibility_version::
                BEFORE_8_2_0) {
      use_previous = true;
    }
  }
  return use_previous;
}

/**
  @brief
   Internal function used by INFORMATION_SCHEMA implementation to check
   if a role enabled.

  Syntax:
    int INTERNAL_IS_ENABLED_ROLE(role_user, role_host);

  @returns,
    1 - If the role is enabled.
    0 - If not.
*/

longlong Item_func_internal_is_enabled_role::val_int() {
  DBUG_TRACE;
  THD *thd = current_thd;

  // Read schema_name
  String role_name;
  String *role_name_ptr = args[0]->val_str(&role_name);
  String role_host;
  String *role_host_ptr = args[1]->val_str(&role_host);
  if (role_name_ptr == nullptr || role_host_ptr == nullptr) {
    null_value = true;
    return 0;
  }

  if (thd->m_main_security_ctx.get_active_roles()->size() == 0) return 0;

  // Create Auth_id for ID being searched.
  LEX_CSTRING lex_user;
  lex_user.str = role_name_ptr->c_ptr_safe();
  lex_user.length = role_name_ptr->length();

  LEX_CSTRING lex_host;
  lex_host.str = role_host_ptr->c_ptr_safe();
  lex_host.length = role_host_ptr->length();

  // Match the ID and return true if found.
  for (auto &rid : *thd->m_main_security_ctx.get_active_roles()) {
    if (rid == std::make_pair(lex_user, lex_host)) return 1;
  }

  return 0;
}
