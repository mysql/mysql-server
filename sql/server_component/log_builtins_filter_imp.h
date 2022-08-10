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

#ifndef LOG_FILTER_INTERNAL_H
#define LOG_FILTER_INTERNAL_H
#include <mysql/components/services/log_builtins_filter.h>

class log_builtins_filter_imp {
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
    Create a new set of filter rules.

    @param   tag       identifying tag of the rule-set creator
    @param   count     number of rules to allocate

    @returns            a pointer to a ruleset structure, or nullptr on failure
  */
  static DEFINE_METHOD(log_filter_ruleset *, filter_ruleset_new,
                       (log_filter_tag * tag, size_t count));

  /**
    Lock and get the filter rules.

    @param  ruleset    a ruleset (usually allocated with filter_ruleset_new())
    @param  locktype   LOG_BUILTINS_LOCK_SHARED     lock for reading
                       LOG_BUILTINS_LOCK_EXCLUSIVE  lock for writing

    @returns  0         lock acquired
    @returns !0         failed to acquire lock
  */
  static DEFINE_METHOD(int, filter_ruleset_lock,
                       (log_filter_ruleset * ruleset,
                        log_builtins_filter_lock locktype));

  /**
    Release lock on filter rules.

    @param  ruleset    a ruleset (usually allocated with filter_ruleset_new())
  */
  static DEFINE_METHOD(void, filter_ruleset_unlock,
                       (log_filter_ruleset * ruleset));

  /**
    Drop an entire filter rule-set. Must hold lock.

    @param  ruleset    a ruleset * (usually allocated with filter_ruleset_new())
  */
  static DEFINE_METHOD(void, filter_ruleset_drop,
                       (log_filter_ruleset * ruleset));

  /**
    Free an entire filter rule-set. Must hold lock. Lock will be destroyed.

    @param ruleset    a ruleset * (usually allocated with filter_ruleset_new())
                      the pointer pointed to will be a nullptr on return.
  */
  static DEFINE_METHOD(void, filter_ruleset_free,
                       (log_filter_ruleset * *ruleset));

  /**
    Move rules from one ruleset to another. Origin will be empty afterwards.

    @param  from   source      ruleset
    @param  to     destination ruleset
  */
  static DEFINE_METHOD(int, filter_ruleset_move,
                       (log_filter_ruleset * from, log_filter_ruleset *to));

  /**
    Initialize a new rule.
    This clears the first unused rule. It does not update the rules
    count; this is for the caller to do if it succeeds in setting up
    the rule to its satisfaction. If the caller fails, it should
    log_builtins_filter_rule_free() the incomplete rule.

    @param   ruleset  a ruleset (usually allocated with filter_ruleset_new())

    @returns  nullptr  could not initialize rule. Do not call rule_free.
    @returns !nullptr  the address of the rule. fill in. on success,
                      caller must increase rule count.  on failure,
                      it must call rule_free.
  */
  static DEFINE_METHOD(void *, filter_rule_init,
                       (log_filter_ruleset * ruleset));

  /**
    Apply all matching rules from a filter rule set to a given log line.

    @param  ruleset  a ruleset (usually allocated with filter_ruleset_new())
    @param           ll        the current log line

    @returns          int       number of matched rules
  */
  static DEFINE_METHOD(int, filter_run,
                       (log_filter_ruleset * ruleset, log_line *ll));
};

class log_builtins_filter_debug_imp {
 public:
  /**
    Initialize built-in log filter debug functionality.
  */
  static void init();

  /**
    De-initialize built-in log filter debug functionality.
  */
  static void deinit();

 public: /* Service Implementations */
  /**
    Get filter rules used in built-in filter. For debug purposes only.
    Third party code should not use this, nor rely on this API to be stable.

    @returns            a pointer to a ruleset structure, or nullptr
  */
  static DEFINE_METHOD(log_filter_ruleset *, filter_debug_ruleset_get, (void));
};

/**
  Deinitialize filtering engine.

  @returns  0   Success!
  @returns -1   De-initialize?  Filter wasn't even initialized!
*/
int log_builtins_filter_exit();

/**
  Initialize filtering engine.
  We need to do this early, before the component system is up.

  @returns  0   Success!
  @returns -1   Couldn't initialize ruleset lock
  @returns -2   Filter was already initialized?
*/
int log_builtins_filter_init();

int log_builtins_filter_run(log_filter_ruleset *ruleset, log_line *ll);

#ifdef MYSQL_SERVER

int log_builtins_filter_update_verbosity(int verbosity);

int log_builtins_filter_parse_suppression_list(char *list, bool update);

#endif /* MYSQL_SERVER */

#endif /* LOG_FILTER_INTERNAL_H */
