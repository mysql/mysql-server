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

#ifndef LOG_FILTER_INTERNAL_H
#define LOG_FILTER_INTERNAL_H
#include <mysql/components/services/log_builtins_filter.h>


class log_builtins_filter_imp
{
public:
  /**
    Initialize built-in log filter.
  */
  static void init();

  /**
    De-initialize built-in log filter.
  */
  static void deinit();

public: /* Service Implementations */
  /**
    Apply all matching rules from a filter rule set to a given log line.

    @param           instance             instance (currently unused)
    @param           ll                   the current log line

    @retval          int                  number of matched rules
  */
  static DEFINE_METHOD(int,              filter_run, (void *instance,
                                                      log_line *ll));

  /**
    Lock and get the filter rules.

    @param  locktype   LOG_BUILTINS_LOCK_SHARED     lock for reading
                       LOG_BUILTINS_LOCK_EXCLUSIVE  lock for writing

    @retval            a pointer to a ruleset structure
  */
  static DEFINE_METHOD(void *,           filter_ruleset_get,
                                           (log_builtins_filter_lock locktype));

  /**
    Drop an entire filter rule-set. Must hold lock.
  */
  static DEFINE_METHOD(void,             filter_ruleset_drop, ());

  /**
    Release lock on filter rules.
  */
  static DEFINE_METHOD(void,             filter_ruleset_release, ());

  /**
    Initialize a new rule.
    This clears the first unused rule. It does not update the rules
    count; this is for the caller to do if it succeeds in setting up
    the rule to its satisfaction. If the caller fails, it should
    log_builtins_filter_rule_free() the incomplete rule.

    @retval  nullptr  could not initialize rule. Do not call rule_free.
    @retval !nullptr  the address of the rule. fill in. on success,
                      caller must increase rule count.  on failure,
                      it must call rule_free.
  */
  static DEFINE_METHOD(void *,           filter_rule_init, ());
};


/**
  Deinitialize filtering engine.

  @retval  0   Success!
  @retval -1   De-initialize?  Filter wasn't even initialized!
*/
int log_builtins_filter_exit();


/**
  Initialize filtering engine.
  We need to do this early, before the component system is up.

  @retval  0   Success!
  @retval -1   Couldn't initialize ruleset lock
  @retval -2   Filter was already initialized?
*/
int log_builtins_filter_init();


/**
  Apply all matching rules from a filter rule set to a given log line.

  @param           ll                   the current log line
  @param           instance             instance

  @retval          int                  number of matched rules
*/
int log_builtins_filter_run(void *instance, log_line *ll);


#  ifdef MYSQL_SERVER

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
int log_builtins_filter_update_verbosity(int verbosity);

#  endif /* MYSQL_SERVER */

#endif /* LOG_FILTER_INTERNAL_H */
