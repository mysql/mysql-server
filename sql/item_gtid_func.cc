/* Copyright (c) 2000, 2026, Oracle and/or its affiliates.

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
#include "item_gtid_func.h"

#include <algorithm>

#include "include/scope_guard.h"   //Scope_guard
#include "include/sql_string.h"    // to_string_view
#include "mysql/gtids/gtids.h"     // Gtid_set
#include "sql/derror.h"            // ER_THD
#include "sql/lib_glue/gtids.h"    // git_set_from_text_report_errors
#include "sql/lib_glue/strconv.h"  // out_str_growable(String &)
#include "sql/rpl_mi.h"            // Master_info
#include "sql/rpl_msr.h"           // channel_map
#include "sql/sql_class.h"         // THD
#include "sql/sql_lex.h"

using std::max;

bool Item_wait_for_executed_gtid_set::do_itemize(Parse_context *pc,
                                                 Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;
  /*
    It is unsafe because the return value depends on timing. If the timeout
    happens, the return value is different from the one in which the function
    returns with success.
  */
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

/**
  Wait until the given gtid_set is found in the executed gtid_set independent
  of the slave threads.
*/
longlong Item_wait_for_executed_gtid_set::val_int() {
  DBUG_TRACE;
  assert(fixed);
  THD *thd = current_thd;

  String *gtid_text = args[0]->val_str(&value);
  if (gtid_text == nullptr) {
    /*
      Usually, an argument that is NULL causes an SQL function to return NULL,
      however since this is a function with side-effects, a NULL value is
      treated as an error.
    */
    if (!thd->is_error()) {
      my_error(ER_MALFORMED_GTID_SET_SPECIFICATION, MYF(0), "NULL");
    }
    return error_int();
  }

  double timeout = 0;
  if (arg_count > 1) {
    timeout = args[1]->val_real();
    if (args[1]->null_value || timeout < 0) {
      if (!thd->is_error()) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "WAIT_FOR_EXECUTED_GTID_SET.");
      }
      return error_int();
    }
  }

  // Waiting for a GTID in a slave thread could cause the slave to
  // hang/deadlock.
  // @todo: Return error instead of NULL
  if (thd->slave_thread) {
    return error_int();
  }

  Gtid_set wait_for_gtid_set(global_tsid_map, nullptr);

  const Checkable_rwlock::Guard global_tsid_lock_guard(
      *global_tsid_lock, Checkable_rwlock::READ_LOCK);
  if (global_gtid_mode.get() == Gtid_mode::OFF) {
    my_error(ER_GTID_MODE_OFF, MYF(0), "use WAIT_FOR_EXECUTED_GTID_SET");
    return error_int();
  }
  gtid_state->begin_gtid_wait();
  Scope_guard x([&] { gtid_state->end_gtid_wait(); });
  if (wait_for_gtid_set.add_gtid_text(gtid_text->c_ptr_safe()) !=
      RETURN_STATUS_OK) {
    // Error has already been generated.
    return error_int();
  }

  // Cannot wait for a GTID that the thread owns since that would
  // immediately deadlock.
  if (thd->owned_gtid.sidno > 0 &&
      wait_for_gtid_set.contains_gtid(thd->owned_gtid)) {
    char buf[Gtid::MAX_TEXT_LENGTH + 1];
    thd->owned_gtid.to_string(global_tsid_map, buf);
    my_error(ER_CANT_WAIT_FOR_EXECUTED_GTID_SET_WHILE_OWNING_A_GTID, MYF(0),
             buf);
    return error_int();
  }

  bool result = gtid_state->wait_for_gtid_set(thd, &wait_for_gtid_set, timeout);

  null_value = false;
  return result;
}

/**
  Return 1 if both arguments are Gtid_sets and the first is a subset
  of the second.  Generate an error if any of the arguments is not a
  Gtid_set.
*/
longlong Item_func_gtid_subset::val_int() {
  DBUG_TRACE;
  using mysql::utils::Return_status;
  assert(fixed);

  // Evaluate strings.
  String *string1 = args[0]->val_str(&buf1);
  if (string1 == nullptr) {
    return error_int();
  }
  String *string2 = args[1]->val_str(&buf2);
  if (string2 == nullptr) {
    return error_int();
  }

  // Convert to sets
  mysql::gtids::Gtid_set set1;
  mysql::gtids::Gtid_set set2;
  if (gtid_set_decode_text_report_errors(to_string_view(*string1), set1) ==
      Return_status::error)
    return 1;
  if (gtid_set_decode_text_report_errors(to_string_view(*string2), set2) ==
      Return_status::error)
    return 1;

  // Compute the result.
  null_value = false;
  return mysql::sets::is_subset(set1, set2) ? 1 : 0;
}

bool Item_func_gtid_subtract::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, -1)) return true;

  collation.set(default_charset(), DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
  /*
    In the worst case, the string grows after subtraction. This
    happens when a GTID in args[0] is split by a GTID in args[1],
    e.g., UUID:1-6 minus UUID:3-4 becomes UUID:1-2,5-6.  The worst
    case is UUID:1-100 minus UUID:9, where the two characters ":9" in
    args[1] yield the five characters "-8,10" in the result.
  */
  set_data_type_string(
      args[0]->max_length +
      max<ulonglong>(args[1]->max_length - mysql::gtid::Uuid::TEXT_LENGTH, 0) *
          5 / 2);
  return false;
}

String *Item_func_gtid_subtract::val_str_ascii(String *str) {
  DBUG_TRACE;
  static constexpr auto return_ok = mysql::utils::Return_status::ok;
  assert(fixed);

  // Evaluate strings.
  String *string1 = args[0]->val_str_ascii(&buf1);
  if (string1 == nullptr) return error_str();
  String *string2 = args[1]->val_str_ascii(&buf2);
  if (string2 == nullptr) return error_str();

  // Convert to sets
  mysql::gtids::Gtid_set set1;
  mysql::gtids::Gtid_set set2;
  if (gtid_set_decode_text_report_errors(to_string_view(*string1), set1) !=
      return_ok) {
    return error_str();
  }
  if (gtid_set_decode_text_report_errors(to_string_view(*string2), set2) !=
      return_ok) {
    return error_str();
  }

  // Compute the result
  if (set1.inplace_subtract(set2) != return_ok) {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    return error_str();
  }

  if (mysql::strconv::encode_text(out_str_growable(*str), set1) != return_ok) {
    return error_str();
  }
  null_value = false;
  return str;
}
