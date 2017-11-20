/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  services: log filter: basic filtering

  This implementation, "draugnet" is currently the default filter
  and therefore built-in.  Basic configuration is built into the
  server proper (via log_error_verbosity etc.); for advanced configuration,
  load the service log_filter_draugnet which implements a configuration
  language for this engine.  See there for details about the Configuration
  Stage.  Some of the code-paths are only available via the configuration
  language or an equivalent service (but not without any such service loaded).

  At present, the design is such that multiple threads can call the
  filter concurrently; the ruleset is global and shared between all
  users.


  FILTERING STAGE

  At run time, the filter iterates over its rule-set.  For each
  rule, if the condition contains a well-known item, it looks for
  an item of that type in the event.  If the condition contains
  an ad hoc-item, it looks for an item of any ad hoc-type with
  the given key within the event.

  If there is a match, the filter will verify whether the storage
  class of the value in the event and that in the condition are
  either both strings, or both not.  If the classes do not match,
  it flags an error.  Otherwise, it now compares both values using
  the requested comparator, and reports the result.

  If a log event matches a rule, an action ("suppress log line",
  "delete field", etc.) will be applied to that event.


  LOCKING

  During the filtering stage, a shared lock on the ruleset is held.
  An exclusive lock on the ruleset is only taken as response to the
  user's changing of the filter configuration, which should be rare.

  For debugging puroposes, rules feature a counter of how often events
  matched them; this counter is updated atomically.

  Rate-limiting ("throttle") needs some bookkeeping data (when does
  the current window expire? how many matches have we had so far
  within the current window? etc.). A write-lock is taken on the
  individual rule (not the entire ruleset) to update this information;
  any throttling-related actions taken on the event happen after this
  lock has been released.

  The event itself is not locked.
*/

#include <my_atomic.h>
#include <mysqld_error.h>

#include "log_builtins_filter_imp.h"
#include "log_builtins_imp.h"
#include "sql/log.h"
// for the default rules
#include "sql/mysqld.h"


static bool  filter_inited=    false;
static ulong filter_rule_uuid= 0;

static PSI_rwlock_key key_rwlock_THR_LOCK_log_builtins_filter;

#ifdef HAVE_PSI_INTERFACE

static PSI_rwlock_info log_builtins_filter_rwlocks[]=
{
  { &key_rwlock_THR_LOCK_log_builtins_filter,
    "THR_LOCK_log_builtin_filter", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}
};
#endif


log_filter_ruleset log_filter_rules;


/**
  Lock and get the filter rules.

  @param  lt   LOG_BUILTINS_LOCK_SHARED     lock for reading
               LOG_BUILTINS_LOCK_EXCLUSIVE  lock for writing

  @retval      a pointer to a ruleset structure
*/
static log_filter_ruleset
*log_builtins_filter_ruleset_get(log_builtins_filter_lock lt)
{
  if (!filter_inited)
    return nullptr;

  switch (lt) {
  case LOG_BUILTINS_LOCK_SHARED:
    mysql_rwlock_rdlock(&log_filter_rules.ruleset_lock);
    break;
  case LOG_BUILTINS_LOCK_EXCLUSIVE:
    mysql_rwlock_wrlock(&log_filter_rules.ruleset_lock);
    break;
  default:
    return nullptr;
  }

  return &log_filter_rules;
}


/**
  Release lock on filter rules.
*/
static void log_builtins_filter_ruleset_release()
{
  mysql_rwlock_unlock(&log_filter_rules.ruleset_lock);
}


/**
  Predicate: can we add any more rules?

  @param  rs     the ruleset to check

  @retval true   full, no more rules can be added
  @retval false  not full, further rules can be added
*/
static bool log_filter_ruleset_full(log_filter_ruleset *rs)
{
  return (rs->count >= LOG_FILTER_MAX);
}


/**
  Initialize a new rule.
  This clears the first unused rule. It does not update the rules
  count; this is for the caller to do if it succeeds in setting up
  the rule to its satisfaction. If the caller fails, it should
  log_builtins_filter_rule_free() the incomplete rule.

  @retval  nullptr: could not initialize rule. Do not call rule_free.
  @retval !nullptr: the address of the rule. fill in. on success,
                    caller must increase rule count.  on failure,
                    it must call rule_free.
*/
static log_filter_rule *log_builtins_filter_rule_init()
{
  log_filter_rule *r= &log_filter_rules.rule[log_filter_rules.count];

  memset(r, 0, sizeof(log_filter_rule));

  r->id= ++filter_rule_uuid;

  if (mysql_rwlock_init(0, &(r->rule_lock)))
    return nullptr;

  return r;
}


/**
  release all resources associated with a filter rule.

  @param  ri  the rule to release

  @retval     the return value from mysql_rwlock_destroy()
*/
static int log_builtins_filter_rule_free(log_filter_rule *ri)
{
  ri->cond= LOG_FILTER_COND_NONE;
  ri->verb= LOG_FILTER_NOP;

  // release memory if needed
  log_item_free(&(ri->match));
  log_item_free(&(ri->aux));

  return mysql_rwlock_destroy(&(ri->rule_lock));
}


/**
  Drop an entire filter rule-set. Must hold lock.
*/
static void log_builtins_filter_ruleset_drop()
{
  log_filter_rule *ri;

  while (log_filter_rules.count > 0)
  {
    log_filter_rules.count--;
    ri= &log_filter_rules.rule[log_filter_rules.count];
    log_builtins_filter_rule_free(ri);
  }
}


/**
  Defaults for when the configuration engine isn't loaded;
  aim for 5.7 compatibilty.
*/
static void log_builtins_filter_defaults()
{
  log_filter_rule *r;

  // sys_var: log_error_verbosity
  r= log_builtins_filter_rule_init();
  log_item_set_with_key(&r->match, LOG_ITEM_LOG_PRIO, nullptr,
                        LOG_ITEM_FREE_NONE)->data_integer= log_error_verbosity;
  r->cond=   LOG_FILTER_COND_GE;
  r->verb=   LOG_FILTER_GAG;
  r->flags=  LOG_FILTER_FLAG_SYNTHETIC;
  log_filter_rules.count++;


  // example: error code => re-prio start-up message
  // "MySQL_error==1408? set_priority 0."
  r= log_builtins_filter_rule_init();
  log_item_set_with_key(&r->match, LOG_ITEM_SQL_ERRCODE, nullptr,
                        LOG_ITEM_FREE_NONE)->data_integer= ER_STARTUP;
  r->cond=  LOG_FILTER_COND_EQ;
  r->verb=  LOG_FILTER_PRIO_ABS;
  // new prio
  log_item_set(&r->aux, LOG_ITEM_GEN_INTEGER)->data_integer= ERROR_LEVEL;
  log_filter_rules.count++;


  // example: error code => re-label start-up message
  // "MySQL_error==1408? add_field log_label:=\"HELO\"."
  r= log_builtins_filter_rule_init();
  log_item_set_with_key(&r->match, LOG_ITEM_SQL_ERRCODE, nullptr,
                        LOG_ITEM_FREE_NONE)->data_integer= ER_STARTUP;
  r->cond=  LOG_FILTER_COND_EQ;
  r->verb=  LOG_FILTER_ITEM_ADD;
  // new label
  // as the aux item is added to the bag, it has to be a fully set up.
  log_item_set(&r->aux, LOG_ITEM_LOG_LABEL)->data_string=
    { C_STRING_WITH_LEN("Note") };
  log_filter_rules.count++;

  // example: remove all source-line log items
  // "+source_line? delete_field."
  // these are not desirable by default, only while debugging.
  r= log_builtins_filter_rule_init();
  log_item_set(&r->match, LOG_ITEM_SRC_LINE);
  r->cond=  LOG_FILTER_COND_PRESENT;
  r->verb=  LOG_FILTER_ITEM_DEL;
  // aux optional
  log_filter_rules.count++;
}


/**
  Deinitialize filtering engine.

  @retval  0   Success!
  @retval -1   De-initialize?  Filter wasn't even initialized!
*/
int log_builtins_filter_exit()
{
  if (!filter_inited)
    return -1;

  /*
    Nobody else should run at this point anyway, but
    since we made a big song and dance about having
    to hold this log above ...
  */
  mysql_rwlock_wrlock(&log_filter_rules.ruleset_lock);
  filter_inited= false;
  log_builtins_filter_ruleset_drop();
  mysql_rwlock_unlock(&log_filter_rules.ruleset_lock);

  mysql_rwlock_destroy(&log_filter_rules.ruleset_lock);

  return 0;
}


/**
  Initialize filtering engine.
  We need to do this early, before the component system is up.

  @retval  0   Success!
  @retval -1   Couldn't initialize ruleset lock
  @retval -2   Filter was already initialized?
*/
int log_builtins_filter_init()
{
  if (!filter_inited)
  {
#ifdef HAVE_PSI_INTERFACE
    const char *category= "log";
    int         count= array_elements(log_builtins_filter_rwlocks);

    mysql_rwlock_register(category, log_builtins_filter_rwlocks, count);
#endif

    if (mysql_rwlock_init(key_rwlock_THR_LOCK_log_builtins_filter,
                          &log_filter_rules.ruleset_lock))
      return -1;

    log_filter_rules.count= 0;
    log_builtins_filter_defaults();
    filter_inited=          true;

    return 0;
  }
  else
    return -2;
}


/**
  Apply the action of an individual rule to an individual log line (or
  a part thereof, i.e. a "field"). At this point, we already know that
  the current log line matches the condition.

  @param[in,out]   ll                   the current log line
  @param           ln                   index of the matching field,
                                        -1 for none (when a test for absence
                                        matched)
  @param           r                    the rule to apply. internal state may
                                        be changed (i.e. the number of seen
                                        matches for a throttle rule)

  @retval          log_filter_apply     0 on success, an error-code otherwise
*/

static log_filter_apply
  log_filter_try_apply(log_line *ll, int ln, log_filter_rule *r)
{
  switch (r->verb)
  {
  case LOG_FILTER_GAG:
    log_line_item_free_all(ll);
    break;

  case LOG_FILTER_PRIO_ABS:
  case LOG_FILTER_PRIO_REL:
    {
      int pi;

      if (!(ll->seen & LOG_ITEM_LOG_PRIO))
      {
        // no priority set, add item
        if (log_line_full(ll))
          return LOG_FILTER_APPLY_ITEM_BAG_FULL;

        log_line_item_set(ll, LOG_ITEM_LOG_PRIO)->data_integer= ERROR_LEVEL;
      }

      pi= log_line_index_by_type(ll, LOG_ITEM_LOG_PRIO);
      DBUG_ASSERT(pi >= 0);

      if (r->verb == LOG_FILTER_PRIO_ABS)
        ll->item[pi].data.data_integer=   r->aux.data.data_integer;
      else
        ll->item[pi].data.data_integer+=  r->aux.data.data_integer;
    }
    break;

  case LOG_FILTER_THROTTLE:
    {
      ulonglong now=        my_micro_time();
      ulong     rate=       (ulong) ((r->aux.data.data_integer < 0)
                                     ? 0
                                     : r->aux.data.data_integer);
      ulong     suppressed= 0;
      ulong     matches;

      /*
        Check whether we're still in the current window. (If not, we
        will want to print a summary (if the logging of any lines was
        suppressed) and start a new window.)
      */
      mysql_rwlock_wrlock(&(r->rule_lock));

      if (now >= r->throttle_window_end)
      {
        suppressed= (r->throttle_matches > rate)
                    ? (r->throttle_matches - rate)
                    : 0;

        // new window
        r->throttle_matches= 0;
        r->throttle_window_end= now + 60000000; // window fixed 1 min for now

      }

      matches= ++r->throttle_matches;

      mysql_rwlock_unlock(&(r->rule_lock));

      /*
        If it's over the limit for the current window, gag the whole
        log line.  A rate of 0 ("gag all") is allowed for convenience,
        but the gag verb should be used instead.
      */
      if (matches > rate)
      {
        log_line_item_free_all(ll);
      }

      // if we actually suppressed any lines, add info declaring that
      else if ((suppressed > 0) && !log_line_full(ll))
      {
        log_line_item_set(ll, LOG_ITEM_LOG_SUPPRESSED)->data_integer=
                                                               suppressed;
      }
    }
    break;

  case LOG_FILTER_ITEM_ADD:
    // TBD: if a match exists, we should simply overwrite it! ###
    if (!log_line_full(ll))
    {
      ll->item[ll->count]=       r->aux;
      /*
         It's a shallow copy, don't try to free it.
         As a downside, the filter rules must remain shared-locked until
         the line is logged. The assumption is that logging happens vastly
         more than changing of the ruleset.
      */
      ll->item[ll->count].alloc= LOG_ITEM_FREE_NONE;
      ll->seen |= r->aux.type;
      ll->count++;
    }
    break;

  case LOG_FILTER_ITEM_DEL:
    // might want to delete a field other than that from the cond:
    if ((r->aux.key != nullptr) &&
        ((ln < 0) ||
         (0 != native_strcasecmp(r->aux.key, ll->item[ln].key))))
    {
      ln= log_line_index_by_item(ll, &r->aux);
    }

    if (ln < 0)
      return LOG_FILTER_APPLY_TARGET_NOT_IN_LOG_LINE;

    {
      log_item_type t= ll->item[ln].type;

      log_line_item_remove(ll, ln);

      /*
        If it's a well-known type (and therefore unique), or if it's
        the last one of a generic type, unflag the presence of that type.
      */
      if (!log_item_generic_type(t) ||
          (log_line_index_by_type(ll, t) < 0))
        ll->seen &= ~t;
    }

    break;

  default:
    return LOG_FILTER_APPLY_UNKNOWN_OPERATION;
  }

  return LOG_FILTER_APPLY_SUCCESS;
}


/**
  Try to match an individual log line-field against an individual
  rule's condition

  @param           li                   the log item we try to match
  @param           ri                   the rule containing the condition

  @retval          log_filter_match     0 (LOG_FILTER_MATCH_SUCCESS) on match,
                                        1 (LOG_FILTER_MATCH_UNSATISFIED) or
                                        an error-code otherwise
*/
static log_filter_match log_filter_try_match(log_item *li, log_filter_rule *ri)
{
  bool             rc, lc;
  log_filter_match e= LOG_FILTER_MATCH_UNCOMPARED;

  DBUG_ASSERT(ri != nullptr);

  /*
    If there is no match, the only valid scenarios are "success"
    (if we tested for absence), or failure (otherwise).  Handle
    them here to make any derefs of li beyond this point safe.
  */
  if (li == nullptr)
    return (ri->cond == LOG_FILTER_COND_ABSENT)
      ? LOG_FILTER_MATCH_SUCCESS
      : LOG_FILTER_MATCH_UNSATISFIED;

  if (ri->cond == LOG_FILTER_COND_PRESENT)
    return LOG_FILTER_MATCH_SUCCESS;

  rc= log_item_string_class(ri->match.item_class);
  lc= log_item_string_class(li->item_class);

  if (rc != lc)
    e= LOG_FILTER_MATCH_CLASSES_DIFFER;

  else
  {
    e= LOG_FILTER_MATCH_UNSATISFIED;
    double rf, lf;

    if (rc)
    {
      switch(ri->cond)
      {
      case LOG_FILTER_COND_EQ:
        if ((ri->match.data.data_string.length == li->data.data_string.length)
            &&
            (strncmp(ri->match.data.data_string.str,
                     li->data.data_string.str,
                     li->data.data_string.length) == 0))
          e= LOG_FILTER_MATCH_SUCCESS;
        break;
      case LOG_FILTER_COND_NE:
        if ((ri->match.data.data_string.length != li->data.data_string.length)
            ||
            (strncmp(ri->match.data.data_string.str,
                     li->data.data_string.str,
                     li->data.data_string.length) != 0))
          e= LOG_FILTER_MATCH_SUCCESS;
        break;
      default:
        return LOG_FILTER_MATCH_UNSUPPORTED_FOR_TYPE;
      }
    }

    else
    {
      log_item_get_float(&ri->match, &rf);
      log_item_get_float(li, &lf);

      switch(ri->cond)
      {
      case LOG_FILTER_COND_EQ:
        if (lf == rf)
          e= LOG_FILTER_MATCH_SUCCESS;
        break;

      case LOG_FILTER_COND_NE:
        if (lf != rf)
          e= LOG_FILTER_MATCH_SUCCESS;
        break;

      case LOG_FILTER_COND_GE:
        if (lf >= rf)
          e= LOG_FILTER_MATCH_SUCCESS;
        break;

      // WL#9651: will probably want to add the following
      case LOG_FILTER_COND_LT:
      case LOG_FILTER_COND_LE:
      case LOG_FILTER_COND_GT:
        e= LOG_FILTER_MATCH_UNSUPPORTED_FOR_TYPE;
        break;

        // unknown comparison type
      default:
        e= LOG_FILTER_MATCH_COMPARATOR_UNKNOWN;
        DBUG_ASSERT(0);
      } // comparator switch
    }  // numeric/string
  } // class mismatch?

  return e;
}


/**
  Apply all matching rules from a filter rule set to a given log line.

  @param           instance             instance (currently unused)
  @param           ll                   the current log line

  @retval          int                  number of matched rules
*/
int log_builtins_filter_run(void *instance MY_ATTRIBUTE((unused)),
                            log_line *ll)
{
  size_t           rn;
  int              ln;
  log_filter_rule *r;
  int              p= 0;

  DBUG_ASSERT(filter_inited);

  mysql_rwlock_rdlock(&log_filter_rules.ruleset_lock);

  for (rn= 0;
       ((rn < log_filter_rules.count) && (ll->seen != LOG_ITEM_END));
       rn++)
  {
    r= &log_filter_rules.rule[rn];

    /*
      If the rule is temporarily disabled, skip over it.
      If and when LOG_FILTER_CHAIN_AND/LOG_FILTER_CHAIN_OR
      are added, those chained conditions must be muted/unmuted
      along with the first one, i.e. as a group.
    */
    if (r->flags & LOG_FILTER_FLAG_DISABLED)
      continue;

    /*
      Look for a matching field in the event!
    */
    /*
      WL#9651: currently applies to 0 or 1 match, do we ever have multi-match?
    */
    ln= log_line_index_by_item(ll, &r->match);

    /*
      If we found a suitable field, see whether its value satisfies
      the condition given in the rule.  If so, apply the action.

      ln == -1  would indicate "not found", which we can actually
      match against for cases like, "if one doesn't exist, create one now."
    */
    if (log_filter_try_match((ln >= 0) ? &ll->item[ln] : nullptr, r) ==
        LOG_FILTER_MATCH_SUCCESS)
    {
      ++r->match_count;

      log_filter_try_apply(ll, ln, r);
      p++;
    }
  }

  mysql_rwlock_unlock(&log_filter_rules.ruleset_lock);

  return p;
}


/**
  This is part of the 5.7 emulation:
  If --log_error_verbosity is changed, we generate an
  artificial filter rule from it here.
  These synthetic filter rules are only used if no other
  filter service (including the loadable filter
  configuration engine that extends the built-in filtering
  engine with a configuration language that exposes all
  the filter's features to the DBA) is loaded.

  @param verbosity  log_error_verbosity style, range(1,3)
                    1:errors,   2:+=warnings,  3:+=notes

  @retval            0: success
  @retval           !0: failure
*/
int log_builtins_filter_update_verbosity(int verbosity)
{
  size_t           rn;
  log_filter_rule *r;
  int              rr= -99;

  if (!filter_inited)
    return -1;

  mysql_rwlock_wrlock(&log_filter_rules.ruleset_lock);

  // if a log_error_verbosity item already exists, update it
  for (rn= 0; (rn < log_filter_rules.count); rn++)
  {
    r= &log_filter_rules.rule[rn];

    if ((r->match.type == LOG_ITEM_LOG_PRIO) &&
        (r->verb       == LOG_FILTER_GAG) &&
        (r->cond       == LOG_FILTER_COND_GE) &&
        (r->flags      &  LOG_FILTER_FLAG_SYNTHETIC))
    {
      r->match.data.data_integer= verbosity;
      r->flags &= ~LOG_FILTER_FLAG_DISABLED;
      rr= 0;
      goto done;
    }
  }

  // if no log_error_verbosity item already exists, create one
  if (log_filter_ruleset_full(&log_filter_rules))
  {
    rr= -2;
    goto done;
  }

  r= log_builtins_filter_rule_init();
  log_item_set_with_key(&r->match, LOG_ITEM_LOG_PRIO, nullptr,
                        LOG_ITEM_FREE_NONE)->data_integer= verbosity;
  r->cond=   LOG_FILTER_COND_GE;
  r->verb=   LOG_FILTER_GAG;
  r->flags=  LOG_FILTER_FLAG_SYNTHETIC;

  log_filter_rules.count++;

  rr= 1;

done:
  mysql_rwlock_unlock(&log_filter_rules.ruleset_lock);

  return rr;
}


/*
  Service: built-in filter
*/


DEFINE_METHOD(int, log_builtins_filter_imp::filter_run, (void *instance,
                                                         log_line *ll))
{
  return log_builtins_filter_run(instance, ll);
}


DEFINE_METHOD(void *, log_builtins_filter_imp::filter_ruleset_get,
                                         (log_builtins_filter_lock locktype))
{
  return (void *) log_builtins_filter_ruleset_get(locktype);
}


DEFINE_METHOD(void,   log_builtins_filter_imp::filter_ruleset_drop, ())
{
  log_builtins_filter_ruleset_drop();
}


DEFINE_METHOD(void,   log_builtins_filter_imp::filter_ruleset_release, ())
{
  log_builtins_filter_ruleset_release();
}


DEFINE_METHOD(void *, log_builtins_filter_imp::filter_rule_init, ())
{
  if (log_filter_ruleset_full(&log_filter_rules))
    return nullptr;
  return (void *) log_builtins_filter_rule_init();
}
