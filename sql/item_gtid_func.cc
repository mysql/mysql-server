/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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
#include "item_gtid_func.h"

#include <algorithm>

#include "sql/derror.h"     // ER_THD
#include "sql/rpl_mi.h"     // Master_info
#include "sql/rpl_msr.h"    // channel_map
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"

using std::max;

bool Item_wait_for_executed_gtid_set::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res)) return true;
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

  Gtid_set wait_for_gtid_set(global_sid_map, nullptr);

  Checkable_rwlock::Guard global_sid_lock_guard(*global_sid_lock,
                                                Checkable_rwlock::READ_LOCK);
  if (global_gtid_mode.get() == Gtid_mode::OFF) {
    my_error(ER_GTID_MODE_OFF, MYF(0), "use WAIT_FOR_EXECUTED_GTID_SET");
    return error_int();
  }

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
    thd->owned_gtid.to_string(global_sid_map, buf);
    my_error(ER_CANT_WAIT_FOR_EXECUTED_GTID_SET_WHILE_OWNING_A_GTID, MYF(0),
             buf);
    return error_int();
  }

  gtid_state->begin_gtid_wait();
  bool result = gtid_state->wait_for_gtid_set(thd, &wait_for_gtid_set, timeout);
  gtid_state->end_gtid_wait();

  null_value = false;
  return result;
}

Item_master_gtid_set_wait::Item_master_gtid_set_wait(const POS &pos, Item *a)
    : Item_int_func(pos, a) {
  null_on_null = false;
  push_deprecated_warn(current_thd, "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS",
                       "WAIT_FOR_EXECUTED_GTID_SET");
}

Item_master_gtid_set_wait::Item_master_gtid_set_wait(const POS &pos, Item *a,
                                                     Item *b)
    : Item_int_func(pos, a, b) {
  null_on_null = false;
  push_deprecated_warn(current_thd, "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS",
                       "WAIT_FOR_EXECUTED_GTID_SET");
}

Item_master_gtid_set_wait::Item_master_gtid_set_wait(const POS &pos, Item *a,
                                                     Item *b, Item *c)
    : Item_int_func(pos, a, b, c) {
  null_on_null = false;
  push_deprecated_warn(current_thd, "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS",
                       "WAIT_FOR_EXECUTED_GTID_SET");
}

bool Item_master_gtid_set_wait::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res)) return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->safe_to_cache_query = false;
  return false;
}

longlong Item_master_gtid_set_wait::val_int() {
  assert(fixed);
  DBUG_TRACE;
  THD *thd = current_thd;

  String *gtid = args[0]->val_str(&gtid_value);
  if (gtid == nullptr) {
    if (!thd->is_error()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0),
               "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS.");
    }
    return error_int();
  }

  double timeout = 0;
  if (arg_count > 1) {
    timeout = args[1]->val_real();
    if (args[1]->null_value || timeout < 0) {
      if (!thd->is_error()) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0),
                 "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS.");
      }
      return error_int();
    }
  }

  if (thd->slave_thread) {
    return error_int();
  }

  Master_info *mi = nullptr;
  channel_map.rdlock();

  /* If replication channel is mentioned */
  if (arg_count > 2) {
    String *channel_str = args[2]->val_str(&channel_value);
    if (channel_str == nullptr) {
      channel_map.unlock();
      if (!thd->is_error()) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0),
                 "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS.");
      }
      return error_int();
    }
    mi = channel_map.get_mi(channel_str->ptr());
  } else {
    if (channel_map.get_num_instances() > 1) {
      channel_map.unlock();
      mi = nullptr;
      my_error(ER_SLAVE_MULTIPLE_CHANNELS_CMD, MYF(0));
      return error_int();
    } else
      mi = channel_map.get_default_channel_mi();
  }

  if ((mi != nullptr) &&
      mi->rli->m_assign_gtids_to_anonymous_transactions_info.get_type() >
          Assign_gtids_to_anonymous_transactions_info::enum_type::AGAT_OFF) {
    my_error(ER_CANT_SET_ANONYMOUS_TO_GTID_AND_WAIT_UNTIL_SQL_THD_AFTER_GTIDS,
             MYF(0));
    channel_map.unlock();
    return error_int();
  }
  if (global_gtid_mode.get() == Gtid_mode::OFF) {
    channel_map.unlock();
    return error_int();
  }
  gtid_state->begin_gtid_wait();

  if (mi != nullptr) mi->inc_reference();

  channel_map.unlock();

  bool null_result = false;
  int event_count = 0;

  if (mi != nullptr && mi->rli != nullptr) {
    event_count = mi->rli->wait_for_gtid_set(thd, gtid, timeout);
    if (event_count == -2) {
      null_result = true;
    }
  } else {
    /*
      Replication has not been set up, we should return NULL;
     */
    null_result = true;
  }
  if (mi != nullptr) mi->dec_reference();

  gtid_state->end_gtid_wait();

  null_value = false;
  return null_result ? error_int() : event_count;
}

/**
  Return 1 if both arguments are Gtid_sets and the first is a subset
  of the second.  Generate an error if any of the arguments is not a
  Gtid_set.
*/
longlong Item_func_gtid_subset::val_int() {
  DBUG_TRACE;
  assert(fixed);

  // Evaluate strings without lock
  String *string1 = args[0]->val_str(&buf1);
  if (string1 == nullptr) {
    return error_int();
  }
  String *string2 = args[1]->val_str(&buf2);
  if (string2 == nullptr) {
    return error_int();
  }

  const char *charp1 = string1->c_ptr_safe();
  assert(charp1 != nullptr);
  const char *charp2 = string2->c_ptr_safe();
  assert(charp2 != nullptr);
  int ret = 1;
  enum_return_status status;

  Sid_map sid_map(nullptr /*no rwlock*/);
  // compute sets while holding locks
  const Gtid_set sub_set(&sid_map, charp1, &status);
  if (status == RETURN_STATUS_OK) {
    const Gtid_set super_set(&sid_map, charp2, &status);
    if (status == RETURN_STATUS_OK) {
      ret = sub_set.is_subset(&super_set) ? 1 : 0;
    }
  }

  null_value = false;
  return ret;
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
      max<ulonglong>(args[1]->max_length - binary_log::Uuid::TEXT_LENGTH, 0) *
          5 / 2);
  return false;
}

String *Item_func_gtid_subtract::val_str_ascii(String *str) {
  DBUG_TRACE;
  assert(fixed);

  String *str1 = args[0]->val_str_ascii(&buf1);
  if (str1 == nullptr) {
    return error_str();
  }
  String *str2 = args[1]->val_str_ascii(&buf2);
  if (str2 == nullptr) {
    return error_str();
  }

  const char *charp1 = str1->c_ptr_safe();
  assert(charp1 != nullptr);
  const char *charp2 = str2->c_ptr_safe();
  assert(charp2 != nullptr);

  enum_return_status status;

  Sid_map sid_map(nullptr /*no rwlock*/);
  // compute sets while holding locks
  Gtid_set set1(&sid_map, charp1, &status);
  if (status == RETURN_STATUS_OK) {
    Gtid_set set2(&sid_map, charp2, &status);
    size_t length;
    // subtract, save result, return result
    if (status == RETURN_STATUS_OK) {
      set1.remove_gtid_set(&set2);
      if (!str->mem_realloc((length = set1.get_string_length()) + 1)) {
        set1.to_string(str->ptr());
        str->length(length);
        null_value = false;
        return str;
      }
    }
  }
  return error_str();
}
