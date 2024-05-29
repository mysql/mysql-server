/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#include "sql/event_parse_data.h"

#include <string.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/thread_type.h"
#include "mysql_time.h"
#include "mysqld_error.h"  // ER_INVALID_CHARACTER_STRING
#include "sql/derror.h"    // ER_THD
#include "sql/events.h"
#include "sql/item.h"
#include "sql/item_timefunc.h"  // get_interval_value
#include "sql/mysqld.h"         // server_id
#include "sql/sp_head.h"        // sp_name
#include "sql/sql_class.h"      // THD
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql_parse.h"
#include "sql_string.h"  // validate_string
#include "tztime.h"      // Time_zone

/**
  Set a name of the event.

  @param thd THD
  @param spn the name extracted in the parser
*/

void Event_parse_data::init_name(THD *thd, sp_name *spn) {
  DBUG_TRACE;

  /* We have to copy strings to get them into the right memroot */
  dbname.length = spn->m_db.length;
  dbname.str = thd->strmake(spn->m_db.str, spn->m_db.length);
  name.length = spn->m_name.length;
  name.str = thd->strmake(spn->m_name.str, spn->m_name.length);

  if (spn->m_qname.length == 0) spn->init_qname(thd);
}

/**
  This function is called on CREATE EVENT or ALTER EVENT.  When either
  ENDS or AT is in the past, we are trying to create an event that
  will never be executed.  If it has ON COMPLETION NOT PRESERVE
  (default), then it would normally be dropped already, so on CREATE
  EVENT we give a warning, and do not create anything.  On ALTER EVENT
  we give a error, and do not change the event.

  If the event has ON COMPLETION PRESERVE, then we see if the event is
  created or altered to the ENABLED (default) state.  If so, then we
  give a warning, and change the state to DISABLED.

  Otherwise it is a valid event in ON COMPLETION PRESERVE DISABLE
  state.

  @param thd THD
  @param ltime_utc either ENDS or AT time for event
  @return true on error, false otherwise
*/

bool Event_parse_data::check_if_in_the_past(THD *thd, my_time_t ltime_utc) {
  if (ltime_utc >= (my_time_t)thd->query_start_in_secs()) return false;

  /*
    We'll come back later when we have the real on_completion value
  */
  if (on_completion == Event_parse_data::ON_COMPLETION_DEFAULT) return false;

  if (on_completion == Event_parse_data::ON_COMPLETION_DROP) {
    do_not_create = true;

    if (thd->lex->sql_command == SQLCOM_CREATE_EVENT) {
      push_warning(thd, Sql_condition::SL_NOTE,
                   ER_EVENT_CANNOT_CREATE_IN_THE_PAST,
                   ER_THD(thd, ER_EVENT_CANNOT_CREATE_IN_THE_PAST));
      return false;
    }
    my_error(ER_EVENT_CANNOT_ALTER_IN_THE_PAST, MYF(0));
    return true;
  }

  if (status == Event_parse_data::ENABLED) {
    status = Event_parse_data::DISABLED;
    status_changed = true;
    push_warning(thd, Sql_condition::SL_NOTE, ER_EVENT_EXEC_TIME_IN_THE_PAST,
                 ER_THD(thd, ER_EVENT_EXEC_TIME_IN_THE_PAST));
  }
  return false;
}

/**
  Check time/dates in ALTER EVENT

  We check whether ALTER EVENT was given dates that are in the past.
  However to know how to react, we need the ON COMPLETION type. Hence,
  the check is deferred until we have the previous ON COMPLETION type
  from the event-db to fall back on if nothing was specified in the
  ALTER EVENT-statement.

  @param thd            Thread
  @param previous_on_completion  ON COMPLETION value currently in event-db.
                     Will be overridden by value in ALTER EVENT if given.

  @return true on error, false otherwise
*/
bool Event_parse_data::check_dates(THD *thd,
                                   enum_on_completion previous_on_completion) {
  if (on_completion == Event_parse_data::ON_COMPLETION_DEFAULT) {
    on_completion = previous_on_completion;
    if (!ends_null && check_if_in_the_past(thd, ends)) {
      return true;
    }
    if (!execute_at_null && check_if_in_the_past(thd, execute_at)) {
      return true;
    }
  }
  return do_not_create;
}

/// Resolves an item and checks that it returns a single column.
static bool ResolveScalarItem(THD *thd, Item **item) {
  if (!(*item)->fixed) {
    if ((*item)->fix_fields(thd, item)) {
      return true;
    }
  }

  if ((*item)->check_cols(1)) {
    return true;
  }

  return false;
}

/**
  Sets time for execution for one-time event.

  @param thd            Thread
  @return true on error, false otherwise
*/

bool Event_parse_data::init_execute_at(THD *thd) {
  MYSQL_TIME ltime;
  my_time_t ltime_utc = 0;

  DBUG_TRACE;

  if (!item_execute_at) return false;

  if (ResolveScalarItem(thd, &item_execute_at)) {
    return true;
  }

  /* no starts and/or ends in case of execute_at */
  DBUG_PRINT("info", ("starts_null && ends_null should be 1 is %d",
                      (starts_null && ends_null)));
  assert(starts_null && ends_null);

  if ((item_execute_at->get_date(&ltime, TIME_NO_ZERO_DATE))) {
    report_bad_value(thd, "AT", item_execute_at);
    return true;
  }

  bool is_in_dst_gap_ignored;
  ltime_utc = thd->time_zone()->TIME_to_gmt_sec(&ltime, &is_in_dst_gap_ignored);

  if (!ltime_utc) {
    DBUG_PRINT("error", ("Execute AT after year 2037"));
    report_bad_value(thd, "AT", item_execute_at);
    return true;
  }

  if (check_if_in_the_past(thd, ltime_utc)) {
    return true;
  }

  execute_at_null = false;
  execute_at = ltime_utc;
  return false;
}

/**
  Sets time for execution of multi-time event.

  @param thd Thread
  @return true on error, false otherwise
*/

bool Event_parse_data::init_interval(THD *thd) {
  DBUG_TRACE;
  if (item_expression == nullptr) return false;

  switch (interval) {
    case INTERVAL_MINUTE_MICROSECOND:
    case INTERVAL_HOUR_MICROSECOND:
    case INTERVAL_DAY_MICROSECOND:
    case INTERVAL_SECOND_MICROSECOND:
    case INTERVAL_MICROSECOND:
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "MICROSECOND");
      return true;
    default:
      break;
  }

  StringBuffer<MAX_DATETIME_FULL_WIDTH + 1> value;
  Interval interval_tmp;

  if (ResolveScalarItem(thd, &item_expression)) {
    return true;
  }

  if (get_interval_value(item_expression, interval, &value, &interval_tmp)) {
    report_bad_value(thd, "INTERVAL", item_expression);
    return true;
  }

  expression = 0;

  switch (interval) {
    case INTERVAL_YEAR:
      expression = interval_tmp.year;
      break;
    case INTERVAL_QUARTER:
    case INTERVAL_MONTH:
      expression = interval_tmp.month;
      break;
    case INTERVAL_WEEK:
    case INTERVAL_DAY:
      expression = interval_tmp.day;
      break;
    case INTERVAL_HOUR:
      expression = interval_tmp.hour;
      break;
    case INTERVAL_MINUTE:
      expression = interval_tmp.minute;
      break;
    case INTERVAL_SECOND:
      expression = interval_tmp.second;
      break;
    case INTERVAL_YEAR_MONTH:  // Allow YEAR-MONTH YYYYYMM
      expression = interval_tmp.year * 12 + interval_tmp.month;
      break;
    case INTERVAL_DAY_HOUR:
      expression = interval_tmp.day * 24 + interval_tmp.hour;
      break;
    case INTERVAL_DAY_MINUTE:
      expression = (interval_tmp.day * 24 + interval_tmp.hour) * 60 +
                   interval_tmp.minute;
      break;
    case INTERVAL_HOUR_SECOND: /* day is anyway 0 */
    case INTERVAL_DAY_SECOND:
      /* DAY_SECOND having problems because of leap seconds? */
      expression = ((interval_tmp.day * 24 + interval_tmp.hour) * 60 +
                    interval_tmp.minute) *
                       60 +
                   interval_tmp.second;
      break;
    case INTERVAL_HOUR_MINUTE:
      expression = interval_tmp.hour * 60 + interval_tmp.minute;
      break;
    case INTERVAL_MINUTE_SECOND:
      expression = interval_tmp.minute * 60 + interval_tmp.second;
      break;
    case INTERVAL_LAST:
      assert(0);
    default:; /* these are the microsec stuff */
  }
  if (interval_tmp.neg || expression == 0 ||
      expression > EVEX_MAX_INTERVAL_VALUE) {
    my_error(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, MYF(0));
    return true;
  }

  return false;
}

/**
  Sets STARTS.

  @note
    Note that activation time is not execution time.
    EVERY 5 MINUTE STARTS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    start at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- start tomorrow at
    same time.
  @param thd Thread

   @return true on error, false otherwise
*/

bool Event_parse_data::init_starts(THD *thd) {
  MYSQL_TIME ltime;
  my_time_t ltime_utc = 0;

  DBUG_TRACE;
  if (item_starts == nullptr) return false;

  if (ResolveScalarItem(thd, &item_starts)) {
    return true;
  }

  if ((item_starts->get_date(&ltime, TIME_NO_ZERO_DATE))) {
    report_bad_value(thd, "STARTS", item_starts);
    return true;
  }

  bool is_in_dst_gap_ignored;
  ltime_utc = thd->time_zone()->TIME_to_gmt_sec(&ltime, &is_in_dst_gap_ignored);

  if (ltime_utc == 0) {
    report_bad_value(thd, "STARTS", item_starts);
    return true;
  }

  DBUG_PRINT("info", ("now: %ld  starts: %ld", (long)thd->query_start_in_secs(),
                      (long)ltime_utc));

  starts_null = false;
  starts = ltime_utc;
  return false;
}

/**
  Sets ENDS (deactivation time).

  @note Note that activation time is not execution time.
    EVERY 5 MINUTE ENDS "2004-12-12 10:00:00" means that
    the event will be executed every 5 minutes but this will
    end at the date shown above. Expressions are possible :
    DATE_ADD(NOW(), INTERVAL 1 DAY)  -- end tomorrow at
    same time.

  @param thd Thread

  @return true on error, false otherwise
*/

bool Event_parse_data::init_ends(THD *thd) {
  MYSQL_TIME ltime;
  my_time_t ltime_utc = 0;

  DBUG_TRACE;
  if (item_ends == nullptr) return false;

  if (ResolveScalarItem(thd, &item_ends)) {
    return true;
  }

  DBUG_PRINT("info", ("convert to TIME"));

  if (item_ends->get_date(&ltime, TIME_NO_ZERO_DATE)) {
    my_error(ER_EVENT_ENDS_BEFORE_STARTS, MYF(0));
    return true;
  }

  bool is_in_dst_gap_ignored = false;
  ltime_utc = thd->time_zone()->TIME_to_gmt_sec(&ltime, &is_in_dst_gap_ignored);
  if (ltime_utc == 0) {
    my_error(ER_EVENT_ENDS_BEFORE_STARTS, MYF(0));
    return true;
  }

  /* Check whether ends is after starts */
  DBUG_PRINT("info", ("ENDS after STARTS?"));
  if (!starts_null && starts >= ltime_utc) {
    my_error(ER_EVENT_ENDS_BEFORE_STARTS, MYF(0));
    return true;
  }

  if (check_if_in_the_past(thd, ltime_utc)) {
    return true;
  }

  ends_null = false;
  ends = ltime_utc;
  return false;
}

/**
  Prints an error message about invalid value. Internally used
  during input data verification

  @param thd       THD object
  @param item_name The name of the parameter
  @param bad_item  The parameter
*/
void Event_parse_data::report_bad_value(THD *thd, const char *item_name,
                                        Item *bad_item) {
  /// Don't proceed to val_str() if an error has already been raised.
  if (thd->is_error()) return;

  char buff[120];
  String str(buff, sizeof(buff), system_charset_info);
  String *str2 = bad_item->fixed ? bad_item->val_str(&str) : nullptr;
  my_error(ER_WRONG_VALUE, MYF(0), item_name,
           str2 ? str2->c_ptr_safe() : "NULL");
}

/**
  Resolves the event parse data by checking the validity of the data gathered
  during the parsing phase.

  @param thd THD
  @return true on error, false otherwise.
*/
bool Event_parse_data::resolve(THD *thd) {
  DBUG_TRACE;

  // Validate event comment string
  std::string invalid_sub_str;
  if (is_invalid_string(comment, system_charset_info, invalid_sub_str)) {
    my_error(ER_COMMENT_CONTAINS_INVALID_STRING, MYF(0), "event",
             (std::string(identifier->m_db.str) + "." +
              std::string(identifier->m_name.str))
                 .c_str(),
             system_charset_info->csname, invalid_sub_str.c_str());
    return true;
  }

  init_name(thd, identifier);

  // Only if a definer clause is present (found by the parser). Otherwise defer
  // the init of the definer to execution time.
  if (thd->lex->definer != nullptr) init_definer(thd);

  if (init_execute_at(thd) || init_interval(thd) || init_starts(thd) ||
      init_ends(thd)) {
    return true;
  }
  return false;
}

/**
  Checks performed on every execute. This includes
  checking and possibly initializing the definer, and
  checking the originator id.

  @param thd THD
  @return true on error, false otherwise.
 */
bool Event_parse_data::check_for_execute(THD *thd) {
  if (definer.str == nullptr) init_definer(thd);
  check_originator_id(thd);
  return false;
}

/**
  Inits definer (definer_user and definer_host) during parsing.

  @param thd  Thread
*/

void Event_parse_data::init_definer(THD *thd) {
  DBUG_TRACE;

  assert(thd->lex->definer);

  const char *definer_user = thd->lex->definer->user.str;
  const char *definer_host = thd->lex->definer->host.str;
  const size_t definer_user_len = thd->lex->definer->user.length;
  const size_t definer_host_len = thd->lex->definer->host.length;

  DBUG_PRINT("info", ("init definer_user thd->mem_root: %p  "
                      "definer_user: %p",
                      thd->mem_root, definer_user));

  /* + 1 for @ */
  DBUG_PRINT("info", ("init definer as whole"));
  definer.length = definer_user_len + definer_host_len + 1;
  char *buf = pointer_cast<char *>(thd->alloc(definer.length + 1));
  definer.str = buf;

  DBUG_PRINT("info", ("copy the user"));
  memcpy(buf, definer_user, definer_user_len);
  buf[definer_user_len] = '@';

  DBUG_PRINT("info", ("copy the host"));
  memcpy(buf + definer_user_len + 1, definer_host, definer_host_len);
  buf[definer.length] = '\0';

  DBUG_PRINT("info", ("definer [%s] initted", definer.str));
}

/**
  Set the originator id of the event to the server_id if executing on
  the source or set to the server_id of the source if executing on
  the replica. If executing on replica, also set status to
  REPLICA_SIDE_DISABLED.

  @param thd Thread
*/
void Event_parse_data::check_originator_id(THD *thd) {
  /* Disable replicated events on slave. */
  if ((thd->system_thread == SYSTEM_THREAD_SLAVE_SQL) ||
      (thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER) ||
      (thd->system_thread == SYSTEM_THREAD_SLAVE_IO)) {
    DBUG_PRINT("info", ("Invoked object status set to REPLICA_SIDE_DISABLED."));
    if ((status == Event_parse_data::ENABLED) ||
        (status == Event_parse_data::DISABLED)) {
      status = Event_parse_data::REPLICA_SIDE_DISABLED;
      status_changed = true;
    }
    originator = thd->server_id;
  } else
    originator = server_id;
}

namespace {

/**
  Check and report error if event schedule expression
  contains subqueries or stored function calls.
*/
bool check_event_schedule_expression(THD *thd) {
  if (thd->lex->table_or_sp_used()) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "Event schedule expressions which contain subqueries or stored "
             "function calls");
    return true;
  }
  return false;
}

/**
  Base class which holds the Event_parse_data object. This allows the parser to
  downcast to Sql_cmd_event_base to get access to Event_parse_data without
  having to check which event command it is currently working on.
  */
struct Sql_cmd_event_base : public Sql_cmd_ddl {
  Event_parse_data event_parse_data;
};

/**
 Override Sql_cmd for EVENTs to get a customization point for prepare.
 execute() member function is not used.
 */
template <enum_sql_command SQLCOM>
struct Sql_cmd_event : public Sql_cmd_event_base {
  enum_sql_command sql_command_code() const override { return SQLCOM; }

  bool execute(THD *thd) override {
    if constexpr (SQLCOM == SQLCOM_DROP_EVENT) {
      auto &id = event_parse_data.identifier;
      if (Events::drop_event(thd, id->m_db, to_lex_cstring(id->m_name),
                             thd->lex->drop_if_exists)) {
        return true;
      }
      // Don't bother checking killed flag here...
      my_ok(thd);
      return false;
    }

    if (!is_prepared()) {
      // For a non-prepared statement the sp_head created by the parser
      // is copied so that the event code can always find it in the same
      // place (as is done in prepare()).
      // But note that thd->lex->sphead is left intact in this case, as it may
      // not be an event body sp_head if this statement is part of an SP. This
      // also ensures that it is correctly cleaned up by lex_end() at the end of
      // the statement, so that no extra cleanup is required here.
      if (event_parse_data.body_changed) {
        // When body_changed is true the parser has completed parsing an event
        // body and updated thd->lex->sphead to refer to it.
        event_parse_data.event_body = thd->lex->sphead;
      }
    } else {
      assert(thd->lex->sphead == nullptr);
    }

    // Event schedule expression cannot contain subqueries or stored function
    // calls.
    if (check_event_schedule_expression(thd)) {
      return true;
    }

    // Use the hypergraph optimizer if it's enabled.
    thd->lex->set_using_hypergraph_optimizer(
        thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER));

    if (sp_process_definer(thd)) {
      return true;
    }

    if constexpr (SQLCOM == SQLCOM_CREATE_EVENT) {
      const bool if_not_exists =
          (thd->lex->create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS) != 0U;

      if (Events::create_event(thd, &event_parse_data, if_not_exists)) {
        return true;
      }
    }
    if constexpr (SQLCOM == SQLCOM_ALTER_EVENT) {
      LEX_CSTRING name_lex_str = NULL_CSTR;
      if (thd->lex->spname != nullptr) {
        name_lex_str.str = thd->lex->spname->m_name.str;
        name_lex_str.length = thd->lex->spname->m_name.length;
      }

      if (Events::update_event(
              thd, &event_parse_data,
              thd->lex->spname != nullptr ? &thd->lex->spname->m_db : nullptr,
              thd->lex->spname != nullptr ? &name_lex_str : nullptr)) {
        return true;
      }
    }

    if (thd->killed == THD::NOT_KILLED) my_ok(thd);
    return false;
  }

  bool prepare(THD *thd) override {
    if constexpr (SQLCOM != SQLCOM_DROP_EVENT) {
      if (event_parse_data.body_changed) {
        // When body_changed is true the parser has completed parsing an event
        // body and updated thd->lex->sphead to refer to it.
        event_parse_data.event_body = thd->lex->sphead;

        // Prevents lex_end() from destroying event body sp_head
        // so that it can be destroyed by ~Prepared_statement()
        // instead.
        thd->lex->sphead = nullptr;
      }
      Prepared_stmt_arena_holder ps_arena_holder{thd};

      // Event schedule expression cannot contain subqueries or stored function
      // calls.
      if (check_event_schedule_expression(thd)) {
        return true;
      }

      if (event_parse_data.resolve(thd)) {
        return true;
      }
    }

    return Sql_cmd::prepare(thd);
  }
};
}  // namespace

/**
  Factory function used by the parser to create the actual Sql_cmd for create
  event, since no Parse_tree node is created for EVENT statements.
 */
Sql_cmd *make_create_event_sql_cmd(THD *thd, sp_name *event_ident) {
  auto *cmd = new (thd->mem_root) Sql_cmd_event<SQLCOM_CREATE_EVENT>();
  cmd->event_parse_data.identifier = event_ident;
  cmd->event_parse_data.on_completion = Event_parse_data::ON_COMPLETION_DROP;
  return cmd;
}

/**
  Factory function used by the parser to create the actual Sql_cmd for alter
  event, since no Parse_tree node is created for EVENT statements.
 */
Sql_cmd *make_alter_event_sql_cmd(THD *thd, sp_name *event_ident) {
  auto *cmd = new (thd->mem_root) Sql_cmd_event<SQLCOM_ALTER_EVENT>();
  cmd->event_parse_data.identifier = event_ident;
  return cmd;
}

/**
  Factory function used by the parser to create the actual Sql_cmd for drop
  event, since no Parse_tree node is created for EVENT statements.
 */
Sql_cmd *make_drop_event_sql_cmd(THD *thd, sp_name *event_ident) {
  auto *cmd = new (thd->mem_root) Sql_cmd_event<SQLCOM_DROP_EVENT>();
  cmd->event_parse_data.identifier = event_ident;
  return cmd;
}

/**
  Helper function to retrieve Event_parse_data from the type erased Sql_cmd.
  This is needed since the parser currently need to refer to Event_parse_data
  after the Sql_cmd has been created.
 */
Event_parse_data *get_event_parse_data(LEX *lex) {
  return &pointer_cast<Sql_cmd_event_base *>(lex->m_sql_cmd)->event_parse_data;
}

/**
  Cleans up event parse data by destroying the event_body sp_head.
 */
void cleanup_event_parse_data(LEX *lex) {
  if (lex->sql_command == SQLCOM_CREATE_EVENT ||
      lex->sql_command == SQLCOM_ALTER_EVENT) {
    sp_head::destroy(pointer_cast<Sql_cmd_event_base *>(lex->m_sql_cmd)
                         ->event_parse_data.event_body);
  }
}