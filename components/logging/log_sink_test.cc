/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#define MY_NAME "log_sink_test"
#define LOG_COMPONENT_TAG MY_NAME
// Test override. No non-test components should use a non-approved value here!
#define LOG_SUBSYSTEM_TAG MY_NAME

#include "log_service_imp.h"

#include <assert.h>
#include <mysqld_error.h>  // ER_*
#include "my_compiler.h"

#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/log_builtins_filter.h>
#include <mysql/components/services/log_sink_perfschema.h>

extern REQUIRES_SERVICE_PLACEHOLDER(registry);

REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_filter);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_filter_debug);
REQUIRES_SERVICE_PLACEHOLDER(log_sink_perfschema);

SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
SERVICE_TYPE(log_builtins_filter) *log_bf = nullptr;
SERVICE_TYPE(log_builtins_filter_debug) *log_fd = nullptr;
SERVICE_TYPE(log_sink_perfschema) *log_ps = nullptr;

static bool inited = false;
static bool failed = false;
static bool run_tests = true;

/**
  delete a rule from the given rule-set

  @param   rs     the rule-set
  @param   t      type of match-item
  @param   key    key  of the match-item if generic (ignored otherwise)
  @param   cond   the condition-code
  @param   action the action-verb

  @retval  false  no problems, rule deleted
  @retval  true   error / no rule deleted
*/
static bool rule_delete(log_filter_ruleset *rs, log_item_type t,
                        log_item_key key, log_filter_cond cond,
                        log_filter_verb action)

{
  size_t rn;
  log_filter_rule *r;

  for (rn = 0; (rn < rs->count); rn++) {
    r = &rs->rule[rn];

    if ((r->match.type == t) &&
        (!log_bi->item_generic_type(t) ||
         (0 == log_bs->compare(key, r->match.key, 0, false))) &&
        (r->verb == action) && (r->cond == cond) &&
        (r->flags & LOG_FILTER_FLAG_SYNTHETIC)) {
      // found our rule. remove it.
      size_t rt;

      if (r->match.alloc & LOG_ITEM_FREE_KEY)
        log_bs->free(const_cast<char *>(r->match.key));
      if ((r->match.alloc & LOG_ITEM_FREE_VALUE) &&
          (r->match.item_class == LOG_LEX_STRING))
        log_bs->free(const_cast<char *>(r->match.data.data_string.str));

      if (r->aux.alloc & LOG_ITEM_FREE_KEY)
        log_bs->free(const_cast<char *>(r->aux.key));
      if ((r->aux.alloc & LOG_ITEM_FREE_VALUE) &&
          (r->aux.item_class == LOG_LEX_STRING))
        log_bs->free(const_cast<char *>(r->aux.data.data_string.str));

      rs->count--;
      for (rt = rn; rt < rs->count; rt++) {
        rs->rule[rt] = rs->rule[rt + 1];
      }

      return false;
    }
  }

  return true;
}

#define KEY_PRIO_CHANGE "prio_change"
#define VAL_PRIO_CHANGE "yes"

#define KEY_DEL_ITEM "bark"
#define VAL_DEL_ITEM "delete_by_rule"

#define KEY_ADD_ITEM "far"
#define VAL_ADD_ITEM "added_by_rule"

#define KEY_PRS_ITEM "meow"

static void test_add_item_log_me(log_filter_ruleset *rs, const char *label,
                                 uint32 orig_count) {
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .errcode(ER_PARSER_TRACE)
      .string_value(KEY_PRS_ITEM, "test_me_for_presence")
      .string_value(KEY_DEL_ITEM, "delete_me_by_rule")
      .string_value(KEY_PRIO_CHANGE, VAL_PRIO_CHANGE)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .source_file(MY_NAME)
      .message("filter_rules: (add_item %s) %d", label, rs->count - orig_count);
}

/**
  Show that flow control actually works.
  This is intended to work in tandem with the log_components_filter.test;
  the test sets up the filter-rules, and we provided some input to them.
*/
static void test_if_then_else() {
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .int_value("wl9651_val1", 0)
      .int_value("wl9651_val2", 1)
      .int_value("wl9651_val3a", 1)
      .int_value("wl9651_val3b", 2)
      .int_value("wl9651_val3c", 3)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .message("WL#9651 expected: r1-IF, r2-SUCCESS, r3-SUCCESS");
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .int_value("wl9651_val1", 1)
      .int_value("wl9651_val2", 2)
      .int_value("wl9651_val3a", 0)
      .int_value("wl9651_val3b", 2)
      .int_value("wl9651_val3c", 3)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .message("WL#9651 expected: r1-ELSEIF1, r2-SUCCESS, r3-FAILURE");
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .int_value("wl9651_val1", 2)
      .float_value("wl9651_val2", 3.1)
      .int_value("wl9651_val3a", 1)
      .int_value("wl9651_val3b", 0)
      .int_value("wl9651_val3c", 3)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .message("WL#9651 expected: r1-ELSEIF2, r2-FAILURE, r3-FAILURE");
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .int_value("wl9651_val1", 3)
      .int_value("wl9651_val2", 4)
      .int_value("wl9651_val3a", 1)
      .int_value("wl9651_val3b", 2)
      .int_value("wl9651_val3c", 0)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .message("WL#9651 expected: r1-ELSE, r2-FAILURE, r3-FAILURE");
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .int_value("wl9651_val1", -7)
      .string_value("wl9651_val2", "1")
      .int_value("wl9651_val3a", 1)
      .int_value("wl9651_val3c", 3)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .message("WL#9651 expected: r1-IF, r2-FAILURE, r3-FAILURE");
}

/**
  Show that adding key/value pairs actually works.

  @retval  0  success
  @retval -1  could not acquire ruleset (to add throttle)
  @retval -2  could not initialize new rule
  @retval -3  could not acquire ruleset (to delete throttle)
  @retval -4  could not delete all rules
*/
static int test_add_item(log_filter_ruleset *rs) {
  log_filter_rule *r;
  int rr = -99;
  uint32 orig_count;

  // if we can't lock the rules, return directly
  if ((log_bf->filter_ruleset_lock(rs, LOG_BUILTINS_LOCK_EXCLUSIVE)) < 0)
    return -1;

  orig_count = rs->count;

  if ((r = (log_filter_rule *)log_bf->filter_rule_init(rs)) == nullptr) {
    rr = -2;
    goto done;
  } else
  // set up a demo prio change
  {
    log_item_data *d;
    // we don't really need to copy here, but let's cover that code path
    char *s = log_bs->strndup(KEY_PRIO_CHANGE, log_bs->length(KEY_PRIO_CHANGE));

    if (s != nullptr) {
      // condition/comparator: equal (string)
      r->cond = LOG_FILTER_COND_EQ;
      // match information: generic lex string, class lex string
      d = log_bi->item_set_with_key(&r->match, LOG_ITEM_GEN_LEX_STRING, s,
                                    LOG_ITEM_FREE_KEY | LOG_ITEM_FREE_VALUE);

      if (d != nullptr) {
        s = log_bs->strndup(VAL_PRIO_CHANGE, log_bs->length(VAL_PRIO_CHANGE));
        if (s != nullptr) {
          d->data_string.str = s;
          d->data_string.length = log_bs->length(s);
        } else
          d->data_string.str = nullptr;
      }

      // action/verb: set numeric value
      r->verb = LOG_FILTER_ITEM_SET;
      // auxiliary information: new priority
      log_bi->item_set(&r->aux, LOG_ITEM_LOG_PRIO)->data_integer =
          WARNING_LEVEL;

      // not requested by the user
      r->flags = LOG_FILTER_FLAG_SYNTHETIC;
      // rule complete, be counted
      rs->count++;
    }
  }

  if ((r = (log_filter_rule *)log_bf->filter_rule_init(rs)) == nullptr) {
    rr = -2;
    goto done;
  } else
  // set up a demo item delete
  {
    log_item_data *d;
    // we don't really need to copy here, but let's cover that code path
    char *s = log_bs->strndup(KEY_DEL_ITEM, log_bs->length(KEY_DEL_ITEM));

    if (s != nullptr) {
      // condition/comparator: not equal (string)
      r->cond = LOG_FILTER_COND_NE;
      // match information: generic lex string, class lex string
      d = log_bi->item_set_with_key(&r->match, LOG_ITEM_GEN_LEX_STRING, s,
                                    LOG_ITEM_FREE_KEY | LOG_ITEM_FREE_VALUE);

      if (d != nullptr) {
        s = log_bs->strndup(VAL_DEL_ITEM, log_bs->length(VAL_DEL_ITEM));
        if (s != nullptr) {
          d->data_string.str = s;
          d->data_string.length = log_bs->length(s);
        } else
          d->data_string.str = nullptr;
      }

      // action/verb: delete item
      r->verb = LOG_FILTER_ITEM_DEL;
      // auxiliary information: delete this item
      r->aux.key = nullptr;  // delete uses same item as in cond

      // not requested by the user
      r->flags = LOG_FILTER_FLAG_SYNTHETIC;
      // rule complete, be counted
      rs->count++;
    }
  }

  if ((r = (log_filter_rule *)log_bf->filter_rule_init(rs)) == nullptr) {
    rr = -2;
    goto done;
  } else
  // set up a demo item add
  {
    log_item_data *d;
    // we don't really need to copy here, but let's cover that code path
    char *s = log_bs->strndup(KEY_PRS_ITEM, log_bs->length(KEY_PRS_ITEM));

    if (s != nullptr) {
      // condition/comparator: not equal (string)
      r->cond = LOG_FILTER_COND_PRESENT;
      // match information: generic lex string, class lex string
      d = log_bi->item_set_with_key(&r->match, LOG_ITEM_GEN_LEX_STRING, s,
                                    LOG_ITEM_FREE_KEY);

      // action/verb: add item
      r->verb = LOG_FILTER_ITEM_SET;
      // auxiliary information: new priority
      d = log_bi->item_set_with_key(&r->aux, LOG_ITEM_GEN_LEX_STRING,
                                    KEY_ADD_ITEM, LOG_ITEM_FREE_NONE);
      if (d != nullptr) {
        d->data_string.str = VAL_ADD_ITEM;
        d->data_string.length = log_bs->length(VAL_ADD_ITEM);
      }

      // not requested by the user
      r->flags = LOG_FILTER_FLAG_SYNTHETIC;
      // rule complete, be counted
      rs->count++;
    }
  }

  log_bf->filter_ruleset_unlock(rs);

  // modify and log event
  test_add_item_log_me(rs, "delta in medias res", orig_count);

  // if we can't lock the rules, return directly
  if ((log_bf->filter_ruleset_lock(rs, LOG_BUILTINS_LOCK_EXCLUSIVE)) < 0)
    return -3;

  rr = 0;

done:
  /*
    If we arrive here from a goto, something went wrong, and we don't
    know what rules were set up correctly. Therefore, some of these
    deletes may fail (since we never added the corresponding rule).
    We stick with the original return-code here however as we're
    interested in the root issue, not subsequent issue.

    If we made it this far without goto (rr==0), all 3 rules have
    been set up, so not being able to delete any one is clearly an
    error and should be flagged as such.
  */
  rule_delete(rs, LOG_ITEM_GEN_LEX_STRING, KEY_PRIO_CHANGE, LOG_FILTER_COND_EQ,
              LOG_FILTER_ITEM_SET);
  rule_delete(rs, LOG_ITEM_GEN_LEX_STRING, KEY_DEL_ITEM, LOG_FILTER_COND_NE,
              LOG_FILTER_ITEM_DEL);
  rule_delete(rs, LOG_ITEM_GEN_LEX_STRING, KEY_PRS_ITEM,
              LOG_FILTER_COND_PRESENT, LOG_FILTER_ITEM_SET);

  if ((rr == 0) && (rs->count != orig_count)) rr = -4;

  log_bf->filter_ruleset_unlock(rs);

  // log unchanged event
  test_add_item_log_me(rs, "delta ex post", orig_count);

  return rr;
}

/**
  Get coverage for some of the built-ins.

  @retval  number of problems detected
*/
static int test_builtins() {
  int ret = 0;

#if !defined(NDEBUG)
  // test classifiers
  assert(log_bi->item_numeric_class(LOG_INTEGER));
  assert(log_bi->item_numeric_class(LOG_FLOAT));
  assert(!log_bi->item_numeric_class(LOG_LEX_STRING));
  assert(!log_bi->item_numeric_class(LOG_CSTRING));

  assert(!log_bi->item_string_class(LOG_INTEGER));
  assert(!log_bi->item_string_class(LOG_FLOAT));
  assert(log_bi->item_string_class(LOG_LEX_STRING));
  assert(log_bi->item_string_class(LOG_CSTRING));

  // test functions for wellknowns
  int wellknown = log_bi->wellknown_by_type(LOG_ITEM_LOG_LABEL);
  assert(LOG_ITEM_LOG_LABEL == log_bi->wellknown_get_type(wellknown));

  wellknown = log_bi->wellknown_by_type(LOG_ITEM_GEN_INTEGER);
  const char *wk = log_bi->wellknown_get_name(wellknown);
  assert(LOG_ITEM_TYPE_RESERVED ==
         log_bi->wellknown_by_name(wk, log_bs->length(wk)));
#endif

  // make a bag, then create a couple of key/value pairs on it
  log_line *ll = log_bi->line_init();
  assert(ll != nullptr);
  if (ll == nullptr) return 1;
  assert(log_bi->line_item_count(ll) == 0);

  log_item_data *d = log_bi->line_item_set(ll, LOG_ITEM_LOG_LABEL);
  assert(d != nullptr);
  assert(log_bi->line_item_count(ll) == 1);

  log_item_data *d1 = log_bi->line_item_set(ll, LOG_ITEM_SQL_ERRCODE);
  ret += log_bi->item_set_int(d1, ER_PARSER_TRACE) ? 1 : 0;
  assert(ret == 0);
  assert(d1 != nullptr);
  assert(log_bi->line_item_count(ll) == 2);

  // setters (woof)
  ret += log_bi->item_set_float(d, 3.1415926927) ? 1 : 0;
  assert(ret == 0);

  ret += log_bi->item_set_int(d, 31415926927) ? 1 : 0;
  assert(ret == 0);

  ret += log_bi->item_set_cstring(d, "pi==3.14") ? 1 : 0;
  assert(ret == 0);

  ret += log_bi->item_set_lexstring(d, "pi", 2) ? 1 : 0;
  assert(ret == 0);

  // get our item from the bag
  log_item_iter *it;
  log_item *li;

  it = log_bi->line_item_iter_acquire(ll);
  assert(it != nullptr);

  li = log_bi->line_item_iter_first(it);
  assert(li != nullptr);

  // break item, then detect that it is broken
  li->item_class = LOG_FLOAT;
  assert(log_bi->item_inconsistent(li) < 0);

  // release iter
  log_bi->line_item_iter_release(it);

  // try to log item we broke.  logger should handle this gracefully.
  log_bi->line_submit(ll);

  // release line
  log_bi->line_exit(ll);

  return ret;
}

/**
  Show that the rate-limiter actually works.

  @retval  0  success!
  @retval -1  could not acquire ruleset (to add throttle)
  @retval -2  could not initialize new rule
  @retval -3  could not acquire ruleset (to delete throttle)
*/
static int test_throttle(log_filter_ruleset *rs) {
  log_filter_rule *r;
  int rr = -99;
  uint32 orig_count;

  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .source_line(__LINE__)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .source_file(MY_NAME)
      .message(
          "below: 3*unknown error per writer == correct.  "
          ">3*unknown error per writer == filter fail. "
          "0*unknown error == " MY_NAME " fail.");

  if ((log_bf->filter_ruleset_lock(rs, LOG_BUILTINS_LOCK_EXCLUSIVE)) < 0) {
    /* purecov: begin inspected */
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message(MY_NAME
                 ": could not get a lock on built-in filter's "
                 "ruleset (for add)");
    return -1;
    /* purecov: end */
  }

  orig_count = rs->count;

  if ((r = (log_filter_rule *)log_bf->filter_rule_init(rs)) == nullptr) {
    /* purecov: begin inspected */
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message(MY_NAME
                 ": could not init a rule in built-in filter's ruleset");
    rr = -2;
    goto done;
    /* purecov: end */
  }

  // set up a demo rate-limiter
  {
    // condition/comparator: equal
    r->cond = LOG_FILTER_COND_EQ;
    // match information: type MySQL error code, class integer
    log_bi->item_set(&r->match, LOG_ITEM_SQL_ERRCODE)->data_integer =
        ER_SERVER_TEST_MESSAGE;

    // action/verb: throttle (rate-limit)
    r->verb = LOG_FILTER_THROTTLE;
    // auxiliary information: maximum number of messages per minute
    log_bi->item_set(&r->aux, LOG_ITEM_GEN_INTEGER)->data_integer = 3;

    // not requested by the user
    r->flags = LOG_FILTER_FLAG_SYNTHETIC;
    // rule complete, be counted
    rs->count++;
  }

  log_bf->filter_ruleset_unlock(rs);

  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .source_line(__LINE__)
      .source_file(MY_NAME)
      .message("filter_rules: (throttle: delta in medias res) %d",
               rs->count - orig_count);

  {
    int c;

    for (c = 0; c < 16; c++)
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .prio(INFORMATION_LEVEL)
          .subsys(LOG_SUBSYSTEM_TAG)
          .component(LOG_COMPONENT_TAG)
          .source_line(__LINE__)
          .source_file(MY_NAME)
          .lookup(ER_SERVER_TEST_MESSAGE);
  }

  if ((log_bf->filter_ruleset_lock(rs, LOG_BUILTINS_LOCK_EXCLUSIVE)) < 0) {
    /* purecov: begin inspected */
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message(MY_NAME
                 ": could not get a lock on built-in filter's "
                 "ruleset (for delete)");
    return -3;
    /* purecov: end */
  }

  rule_delete(rs, LOG_ITEM_SQL_ERRCODE, nullptr, LOG_FILTER_COND_EQ,
              LOG_FILTER_THROTTLE);

  rr = 0;

done:
  log_bf->filter_ruleset_unlock(rs);

  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .source_line(__LINE__)
      .source_file(MY_NAME)
      .message("filter_rules: (throttle: delta ex post) %d",
               rs->count - orig_count);

  return rr;
}

/**
  Log a message each from the C and the C++ API to the error logger,
  showing that we can log from external services!
*/
static void banner() {
  /*
    Use this if for some bizarre reason you really can't or won't use C++
  */
  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO,
                  (longlong)INFORMATION_LEVEL, LOG_ITEM_SQL_ERRSYMBOL,
                  "ER_PARSER_TRACE", LOG_ITEM_LOG_MESSAGE,
                  "using log_message() in external service");

  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_SQL_ERRSYMBOL, "ER_PARSER_TRACE", LOG_ITEM_SRC_LINE,
                  (longlong)1234, LOG_ITEM_SRC_LINE, (longlong)9876,
                  LOG_ITEM_LOG_MESSAGE,
                  "using log_message() with duplicate source-line k/v pair");

  log_bi->message(
      LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
      LOG_ITEM_SQL_ERRSYMBOL, "ER_PARSER_TRACE", LOG_ITEM_GEN_CSTRING, "key",
      "val", LOG_ITEM_GEN_CSTRING, "key", "val", LOG_ITEM_LOG_MESSAGE,
      "using log_message() with duplicate generic C-string k/v pair");

  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_SQL_ERRSYMBOL, "ER_PARSER_TRACE",
                  LOG_ITEM_GEN_CSTRING, "key", "val", LOG_ITEM_GEN_INTEGER,
                  "key", (longlong)4711, LOG_ITEM_LOG_VERBATIM,
                  "using log_message() with duplicate generic mixed k/v pair");

  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_SQL_ERRSYMBOL, "ER_PARSER_TRACE", LOG_ITEM_SYS_ERRNO,
                  (longlong)0, LOG_ITEM_LOG_VERBATIM,
                  "using log_message() with errno 0");

  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, (longlong)ER_SERVER_TEST_MESSAGE);

  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_SQL_ERRSYMBOL, "ER_SERVER_TEST_MESSAGE",
                  LOG_ITEM_LOG_VERBATIM, "using log_message() with errsymbol");

  char my_buff[LOG_BUFF_MAX + 32];
  memset(my_buff, 'a', sizeof(my_buff));
  my_buff[LOG_BUFF_MAX + 32 - 1] = 0;

  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_VERBATIM, my_buff);

  /*
    Fluent C++ API.  Use this free-form constructor if-and-only-if
    you do NOT have error messages registered with the server (and
    therefore need to use ad hoc messages with message() or verbatim().
  */
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(INFORMATION_LEVEL)
      .errcode(ER_PARSER_TRACE)
      .subsys(LOG_SUBSYSTEM_TAG)
      .component(LOG_COMPONENT_TAG)
      .source_line(__LINE__)
      .source_file(MY_NAME)
      .float_value("test_float", 3.1415926927)
      .int_value("test_int", 739241)
      .string_value("test_cstring", "cstring")
      .string_value("test_lexstring", "lexstring", 9)
      .message("using LogEvent() object in external service");

  // built-in API test: test "well-known" lookups
#if !defined(NDEBUG)
  {
    int wellknown = log_bi->wellknown_by_type(LOG_ITEM_LOG_LABEL);
    const char *label_key = log_bi->wellknown_get_name(wellknown);
    int wellagain =
        log_bi->wellknown_by_name(label_key, log_bs->length(label_key));

    assert(wellknown == wellagain);

    assert(LOG_ITEM_TYPE_NOT_FOUND == log_bi->wellknown_by_name("", 0));
  }
#endif

  // built-in API test: test item_consistent() checks
  {
    log_item my_item, *li = &my_item;

    const char *consistent[] = {
        "OK",           "NOT_FOUND",   "RESERVED", "CLASS_MISMATCH",
        "KEY_MISMATCH", "STRING_NULL", "KEY_NULL"};

    // My_item is in some undefined, garbage state so far.

    // LOG_ITEM_TYPE_NOT_FOUND
    li->type = ((log_item_type)(LOG_ITEM_LOG_TYPE + LOG_ITEM_SQL_ERRCODE));
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(INFORMATION_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message("item_inconsistent(#%d): %s", 1,
                 consistent[-log_bi->item_inconsistent(li)]);

    // LOG_ITEM_CLASS_MISMATCH
    li->type = LOG_ITEM_LOG_MESSAGE;
    li->item_class = LOG_INTEGER;
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(INFORMATION_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message("item_inconsistent(#%d): %s", 2,
                 consistent[-log_bi->item_inconsistent(li)]);

    // LOG_ITEM_KEY_MISMATCH
    li->type = LOG_ITEM_LOG_PRIO;
    li->item_class = LOG_INTEGER;
    li->key = "-fail-";
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(INFORMATION_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message("item_inconsistent(#%d): %s", 2,
                 consistent[-log_bi->item_inconsistent(li)]);

    // LOG_ITEM_KEY_NULL
    li->type = LOG_ITEM_LOG_PRIO;
    li->item_class = LOG_INTEGER;
    li->key = nullptr;
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(INFORMATION_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message("item_inconsistent(#%d): %s", 3,
                 consistent[-log_bi->item_inconsistent(li)]);

    // LOG_ITEM_STRING_NULL
    li->type = LOG_ITEM_LOG_MESSAGE;
    li->item_class = LOG_LEX_STRING;
    li->key = log_bi->wellknown_get_name(
        log_bi->wellknown_by_type(LOG_ITEM_LOG_MESSAGE));
    li->data.data_string.str = nullptr;
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(INFORMATION_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message("item_inconsistent(#%d): %s", 4,
                 consistent[-log_bi->item_inconsistent(li)]);

    // LOG_ITEM_OK
    li->type = LOG_ITEM_LOG_MESSAGE;
    li->item_class = LOG_LEX_STRING;
    li->key = log_bi->wellknown_get_name(
        log_bi->wellknown_by_type(LOG_ITEM_LOG_MESSAGE));
    li->data.data_string.str = "";
    li->data.data_string.length = 0;
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(INFORMATION_LEVEL)
        .errcode(ER_PARSER_TRACE)
        .subsys(LOG_SUBSYSTEM_TAG)
        .component(LOG_COMPONENT_TAG)
        .message("item_inconsistent(#%d): %s", 5,
                 consistent[-log_bi->item_inconsistent(li)]);
  }
}

/**
  services: log sinks: basic structured dump writer

  This is intended for testing and debugging, not production.

  Writes all fields. No escaping is done. Submits various
  events of its own to demonstrate the availability of the error
  event submission interface from within external service, as well
  as the correct functioning of said interface.

  @returns          int                  number of accepted fields, if any
  @returns         <0                   failure
*/
DEFINE_METHOD(int, log_service_imp::run,
              (void *instance [[maybe_unused]], log_line *ll)) {
  char out_buff[LOG_BUFF_MAX];
  char *out_writepos = out_buff;
  size_t out_left = LOG_BUFF_MAX - 1,  // bytes left in output buffer
      len;                             // length of current output particle
  int out_fields = 0,                  // number of fields in output
      wellknown_label;                 // index of label in wellknowns[]
  enum loglevel level = ERROR_LEVEL;   // default severity
  log_item_type t = LOG_ITEM_END;
  log_item_type_mask out_types = 0;
  log_item_iter *it;
  log_item *li;

  /*
    If we have detected some sort of massive failure (disk full, out of
    memory, etc.), we set the "failed" flag.  While this is set, any call
    to run() will immediately return.  As a result of this, we may call
    the error logger with information about this failure (AFTER first
    setting the failed flag to prevent a potential endless loop!) in case
    another log_sink is active that may show this alert.
  */
  if (failed) return LOG_SERVICE_NOT_AVAILABLE;

  if ((it = log_bi->line_item_iter_acquire(ll)) == nullptr)
    return LOG_SERVICE_MISC_ERROR; /* purecov: inspected */

  li = log_bi->line_item_iter_first(it);

  // Iterate until we're out of items, or out of space in the resulting row.
  while ((li != nullptr) && (out_left > 0)) {
    t = li->type;

    // Sanity-check the item.
    if (log_bi->item_inconsistent(li)) {
      len = log_bs->substitute(out_writepos, out_left,
                               "[%s=log_sink_test: broken item with class %d, "
                               "type %d];",
                               (li->key == nullptr) ? "_null" : li->key,
                               li->item_class, li->type);
      t = LOG_ITEM_END;  // do not flag current item-type as added
      goto broken_item;  // add this notice if there is enough space left
    }

    if (t == LOG_ITEM_LOG_PRIO) {
      level = static_cast<enum loglevel>(li->data.data_integer);
    }

    // Write the item according to its item-class.
    switch (li->item_class) {
      case LOG_LEX_STRING:
        if (li->data.data_string.str != nullptr)
          len = log_bs->substitute(out_writepos, out_left, "[%s=%.*s];",
                                   li->key, (int)li->data.data_string.length,
                                   li->data.data_string.str);
        else
          len = 0;
        break;
      case LOG_INTEGER:
        len = log_bs->substitute(out_writepos, out_left, "[%s=%lld];", li->key,
                                 li->data.data_integer);
        break;
      case LOG_FLOAT:
        len = log_bs->substitute(out_writepos, out_left, "[%s=%.12lf];",
                                 li->key, li->data.data_float);
        break;

      default:          // unknown item-class
        goto skip_item; /* purecov: inspected */
    }

    // label: item is malformed or otherwise broken; notice inserted instead
  broken_item:

    // item is too large, skip it
    if (len >= out_left) {  // "remove" truncated write
      *out_writepos = '\0'; /* purecov: inspected */
      goto skip_item;       /* purecov: inspected */
    }

    out_types |= t;

    out_fields++;
    out_left -= len;
    out_writepos += len;

  skip_item:
    li = log_bi->line_item_iter_next(it);
  }

  log_bi->line_item_iter_release(it);

  if (out_fields > 0) {
    // If we have prio, but no label, auto-generate a label now.
    // If there's no space, we'll go on without a label.
    if (!(out_types & LOG_ITEM_LOG_LABEL) && (out_left > 0) &&
        (out_types & LOG_ITEM_LOG_PRIO)) {
      const char *label = log_bi->label_from_prio(level);

      wellknown_label = log_bi->wellknown_by_type(LOG_ITEM_LOG_LABEL);
      len = log_bs->substitute(
          out_writepos, out_left, "[%s=%.*s];",
          log_bi->wellknown_get_name((log_item_type)wellknown_label),
          (int)log_bs->length(label), label);
      if (len < out_left) {
        out_fields++;
        out_left -= len;
        out_writepos += len;
        out_types |= LOG_ITEM_LOG_LABEL;
      } else
        *out_writepos = '\0'; /* purecov: inspected */
    }

    log_bi->write_errstream(nullptr, out_buff, (size_t)LOG_BUFF_MAX - out_left);
  }

  /*
    Run some tests of the error logging system.
  */

  /*
    Normally, each filter has its own separate rule-set.
    This debug-function gets us the rule-set used by the
    built-in filter, log_filter_internal, that provides
    log-error-verbosity and log-error-suppression-rules.

    This function must not be used by anything that isn't
    a test. (Also, this depends on the built-in's characteristic
    that the built-in ruleset's header never moves or goes away;
    otherwise a get-function that did not also acquire a lock
    would be questionable.)

    Conversely, this entire sink should not be used in production.
  */
  log_filter_ruleset *test_rules = log_fd->filter_debug_ruleset_get();

  // "This should never happen."
  if (log_bf->filter_ruleset_lock(test_rules, LOG_BUILTINS_LOCK_EXCLUSIVE) < 0)
    return LOG_SERVICE_LOCK_ERROR; /* purecov: inspected */

  if (run_tests) {
    /*
      We'll be calling the logger below, so let's first
      prevent any more activations of these tests, otherwise,
      we might create an endless loop!

      We're changing this while holding the above X-lock to
      prevent race conditions.
    */
    run_tests = false;

    /*
      The first rule in the internal filter is,

        IF prio<WARNING THEN RETURN.

      which prevents users from setting configurations
      (verbosity, suppression-list) in the subsequent
      rules that drop or modify SYSTEM or ERROR level
      events.

      For our tests, we temporarily disable that rule,
      so our tests can in fact drop and modify SYSTEM
      or ERROR level events.

      If we suspended the first rule, we note the fact
      in prio_protection_suspended so we'll know to
      unsuspend the rule once we're down.
    */
    bool prio_protection_suspended = false;
    if ((test_rules->count > 0) &&
        (test_rules->rule[0].verb == LOG_FILTER_RETURN) &&
        !(test_rules->rule[0].flags & LOG_FILTER_FLAG_DISABLED)) {
      prio_protection_suspended = true;
      test_rules->rule[0].flags |= LOG_FILTER_FLAG_DISABLED;
    }

    // subsequent tests need to re-acquired S- or X-lock as needed
    log_bf->filter_ruleset_unlock(test_rules);

    // log a message from this here external service
    banner();

    // show that the rate-limiter actually works
    test_throttle(test_rules);

    // show that adding key/value pairs actually works
    test_add_item(test_rules);

    // show that flow control works
    test_if_then_else();

    // get coverage for assorted built-ins
    test_builtins();

    /*
      There wasn't actually a failure; we're just testing
      the failure code: this disables this log_writer.

      Similar to "run_tests" above, if we had hit a real
      error and wanted to report on it using the error logger,
      we would need to set "failed" before calling the logger
      to prevent potential endless loops!
    */
    failed = true;

    /*
      We do not release the rule-set here, as it's not ours.
      We do however re-enable the first rule in the set
      (that normally prevents us from dropping or modifying SYSTEM/ERROR
      level events) if we previously disabled it.
    */
    if (log_bf->filter_ruleset_lock(test_rules, LOG_BUILTINS_LOCK_EXCLUSIVE) <
        0)
      return LOG_SERVICE_LOCK_ERROR; /* purecov: inspected */

    if (prio_protection_suspended)
      test_rules->rule[0].flags &= ~LOG_FILTER_FLAG_DISABLED;
  }

  // release lock on rule-set (held here no matter whether if() fired).
  log_bf->filter_ruleset_unlock(test_rules);

  return out_fields;  // returning number of processed items
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
mysql_service_status_t log_service_exit() {
  if (inited) {
    inited = false;

    log_bi = nullptr;
    log_bs = nullptr;
    log_bf = nullptr;
    log_fd = nullptr;

    failed = false;
    run_tests = false;

    return false;
  }
  return true;
}

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
mysql_service_status_t log_service_init() {
  if (inited) return true;

  inited = true;
  failed = false;

  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
  log_bf = mysql_service_log_builtins_filter;
  log_fd = mysql_service_log_builtins_filter_debug;
  log_ps = mysql_service_log_sink_perfschema;

  // test the event_add() fail paths for gcov
  log_ps->event_add(/* microseconds */ 0, /* thread_id */ 1, SYSTEM_LEVEL,
                    "MY-010000", /* len */ 9, "Server", 6, "valid line", 10);
  log_ps->event_add(0, 2, 99, "MY-010000", 9, "Server", 6, "bad prio", 8);
  log_ps->event_add(0, 2, SYSTEM_LEVEL, "MY-010000", 9, "Server", 6, nullptr,
                    7);
  log_ps->event_add(0, 2, SYSTEM_LEVEL, "MY-010000", 9, "Server", 6,
                    "bad length", 0);
  log_ps->event_add(0, 2, SYSTEM_LEVEL, "MY-010000", 9, nullptr, 6, "bad sys",
                    7);
  log_ps->event_add(0, 2, SYSTEM_LEVEL, "MY-010000", 9, "Server", 1000,
                    "bad sys", 7);
  log_ps->event_add(0, 2, SYSTEM_LEVEL, nullptr, 9, "Server", 6, "bad err", 7);
  log_ps->event_add(0, 2, SYSTEM_LEVEL, "MY-010000", 1000, "Server", 6,
                    "bad err", 7);

  // run some examples/tests
  run_tests = true;

  return false;
}

/* flush logs */
DEFINE_METHOD(log_service_error, log_service_imp::flush,
              (void **instance [[maybe_unused]])) {
  int res;

  if (inited) log_service_exit();

  res = log_service_init();
  run_tests = false;

  return (!res) ? LOG_SERVICE_SUCCESS : LOG_SERVICE_MISC_ERROR;
}

/**
  Open a new instance.

  @retval  LOG_SERVICE_SUCCESS        success, returned handle is valid
  @retval  otherwise                  a new instance could not be created
*/
DEFINE_METHOD(log_service_error, log_service_imp::open,
              (log_line * ll [[maybe_unused]], void **instance)) {
  if (instance == nullptr) return LOG_SERVICE_INVALID_ARGUMENT;

  *instance = nullptr;

  return LOG_SERVICE_SUCCESS;
}

/**
  Close and release an instance. Flushes any buffers.

  @retval  LOG_SERVICE_SUCCESS        success
  @retval  otherwise                  an error occurred
*/
DEFINE_METHOD(log_service_error, log_service_imp::close,
              (void **instance [[maybe_unused]])) {
  return LOG_SERVICE_SUCCESS;
}

/**
  Get characteristics of a log-service.

  @retval  <0        an error occurred
  @retval  >=0       characteristics (a set of log_service_chistics flags)
*/
DEFINE_METHOD(int, log_service_imp::characteristics, (void)) {
  return LOG_SERVICE_SINK | LOG_SERVICE_SINGLETON;
}

/* implementing a service: log_service */
BEGIN_SERVICE_IMPLEMENTATION(log_sink_test, log_service)
log_service_imp::run, log_service_imp::flush, log_service_imp::open,
    log_service_imp::close, log_service_imp::characteristics, nullptr,
    nullptr END_SERVICE_IMPLEMENTATION();

/* component provides: just the log_service service, for now */
BEGIN_COMPONENT_PROVIDES(log_sink_test)
PROVIDES_SERVICE(log_sink_test, log_service), END_COMPONENT_PROVIDES();

/* component requires: log-builtins */
BEGIN_COMPONENT_REQUIRES(log_sink_test)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(log_builtins_filter),
    REQUIRES_SERVICE(log_builtins_filter_debug),
    REQUIRES_SERVICE(log_sink_perfschema), END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(log_sink_test)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("log_service_type", "sink"),
    END_COMPONENT_METADATA();

/* component declaration */
DECLARE_COMPONENT(log_sink_test, "mysql:log_sink_test")
log_service_init, log_service_exit END_DECLARE_COMPONENT();

/* components contained in this library.
   for now assume that each library will have exactly one component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(log_sink_test)
    END_DECLARE_LIBRARY_COMPONENTS

    /* EOT */
