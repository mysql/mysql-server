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

#ifndef _EVENT_PARSE_DATA_H_
#define _EVENT_PARSE_DATA_H_

#include "lex_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_time.h"  // interval_type

class Item;
struct LEX;
class THD;
class sp_head;
class sp_name;
class Sql_cmd;

#define EVEX_MICROSECOND_UNSUP -6
#define EVEX_MAX_INTERVAL_VALUE 1000000000L

class Event_parse_data {
 public:
  /*
    ENABLED = feature can function normally (is turned on)
    REPLICA_SIDE_DISABLED = feature is turned off on replica
    DISABLED = feature is turned off
  */
  enum enum_status { ENABLED = 1, DISABLED, REPLICA_SIDE_DISABLED };

  enum enum_on_completion {
    /*
      On CREATE EVENT, DROP is the DEFAULT as per the docs.
      On ALTER  EVENT, "no change" is the DEFAULT.
    */
    ON_COMPLETION_DEFAULT = 0,
    ON_COMPLETION_DROP,
    ON_COMPLETION_PRESERVE
  };

  enum_on_completion on_completion{ON_COMPLETION_DEFAULT};
  enum_status status{Event_parse_data::ENABLED};
  bool status_changed{false};

  std::uint64_t originator{0};
  /*
    do_not_create will be set if STARTS time is in the past and
    on_completion == ON_COMPLETION_DROP.
  */
  bool do_not_create{false};

  bool body_changed{false};

  LEX_CSTRING dbname{};
  LEX_CSTRING name{};
  LEX_CSTRING definer{};  // combination of user and host
  LEX_CSTRING comment{};

  Item *item_starts{nullptr};
  Item *item_ends{nullptr};
  Item *item_execute_at{nullptr};

  my_time_t starts{0};
  my_time_t ends{0};
  my_time_t execute_at{0};
  bool starts_null{true};
  bool ends_null{true};
  bool execute_at_null{true};

  sp_name *identifier{nullptr};
  Item *item_expression{nullptr};
  longlong expression{0};
  interval_type interval{INTERVAL_LAST};

  sp_head *event_body{nullptr};

  Event_parse_data() = default;
  Event_parse_data(const Event_parse_data &) = delete;
  void operator=(Event_parse_data &) = delete;

  bool resolve(THD *);
  bool check_for_execute(THD *);
  bool check_dates(THD *thd, enum_on_completion previous_on_completion);

 private:
  void init_definer(THD *thd);

  void init_name(THD *thd, sp_name *spn);

  bool init_execute_at(THD *thd);

  bool init_interval(THD *thd);

  bool init_starts(THD *thd);

  bool init_ends(THD *thd);

  void report_bad_value(THD *thd, const char *item_name, Item *bad_item);

  [[nodiscard("Need to check for errors!")]] bool check_if_in_the_past(
      THD *thd, my_time_t ltime_utc);

  void check_originator_id(THD *thd);
};

Sql_cmd *make_create_event_sql_cmd(THD *, sp_name *);
Sql_cmd *make_alter_event_sql_cmd(THD *, sp_name *);
Sql_cmd *make_drop_event_sql_cmd(THD *, sp_name *);
Event_parse_data *get_event_parse_data(LEX *);
void cleanup_event_parse_data(LEX *);
#endif
