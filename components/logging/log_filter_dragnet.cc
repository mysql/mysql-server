/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @brief

  There is a new filter engine in the server proper
  (components/mysql_server/log_builtins_filter.cc).
  It can apply highly versatile filtering rules to
  log events.  By default however, it loads a rule-set
  that emulates mysqld 5.7 behavior, so as far as the
  users are concerned, the configuration variables
  (--log_error_verbosity ...) and the behavior haven't
  changed (much).

  The loadable service implemented in this file is
  noteworthy in that it does not implement a complete
  filtering service; instead, it implements a
  configuration language for the internal filter that
  gives users access to all its features (rather than
  just to the small 5.7 compatibility subset).

  Therefore, this file contains the parsing of the
  new configuration language (the "configuration engine"),
  whereas log_builtins_filter.cc contains the filtering
  engine.

  CONFIGURATION PARSING STAGE

  As a courtesy, during parsing (e.g. "IF prio>=3 THEN drop."),
  the filter configuration engine checks whether it knows the field
  ("prio"), and if so, whether the storage class it expects
  for the field (integer) matches that of the argument (3).  In our
  example, it does; if it didn't, the configuration engine would
  throw an error.

  The same applies if a well-known field appears in the action
  (e.g. the action 'set log_label:="HELO".' in the rule
  'IF err_code==1408 THEN set label:="HELO".')
  label is a well-known field here, its well-known storage
  class is string, and since "HELO" is a string, all's well.
  (Otherwise, again, we'd throw an error.)
*/

#include "log_service_imp.h"

#define LOG_FILTER_DUMP_BUFF_SIZE 8192
#define LOG_FILTER_LANGUAGE_NAME "dragnet"
#define LOG_FILTER_SYSVAR_NAME "log_error_filter_rules"
#define LOG_FILTER_STATUS_NAME "Status"
#define LOG_FILTER_DEFAULT_RULES        \
  "IF prio>=INFORMATION THEN drop. IF " \
  "EXISTS source_line THEN unset source_line."

#include <mysqld_error.h>
#include "../sql/sql_error.h"

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/plugin.h>

#include "../sql/set_var.h"

REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);

STR_CHECK_ARG(str) sys_var_filter_rules;  ///< limits and default for sysvar

static char *log_error_filter_rules = nullptr;  ///< sysvar containing rules

static char log_error_filter_decompile[LOG_FILTER_DUMP_BUFF_SIZE] = "";

static SHOW_VAR show_var_filter_rules_decompile[] = {
    {LOG_FILTER_LANGUAGE_NAME "." LOG_FILTER_STATUS_NAME,
     (char *)&log_error_filter_decompile, SHOW_CHAR, SHOW_SCOPE_GLOBAL},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}  // null terminator required
};

/*
  Accessors for log items etc.
*/
#include <mysql/components/services/log_builtins.h>

/*
  As this service comprises the configuration language for the
  built-in default filter, we need to know about that filter.
  Other external filter services do not, so they should not
  normally include this.
*/
#include <mysql/components/services/log_builtins_filter.h>

/*
  C_STRING_WITH_LEN
*/
#include <m_string.h>

static bool inited = false;
static int opened = 0;

REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_filter);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_tmp);

SERVICE_TYPE(log_builtins) *log_bi = nullptr;         ///< accessor built-ins
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;  ///< string   built-ins
SERVICE_TYPE(log_builtins_filter) *log_bf = nullptr;  ///< filter   built-ins
SERVICE_TYPE(log_builtins_tmp) *log_bt = nullptr;     ///< notify   built-in

log_filter_tag rule_tag_dragnet = {"log_filter_" LOG_FILTER_LANGUAGE_NAME,
                                   nullptr};
log_filter_ruleset *log_filter_dragnet_rules = nullptr;

/**
  Flags to use in log_filter_xlate_by_name() / log_filter_xlate_by_opcode()
  when looking up a token by its opcode, or vice versa.
 */
typedef enum enum_log_filter_xlate_flags {
  LOG_FILTER_XLATE_NONE = 0,     ///< we don't know what it is
  LOG_FILTER_XLATE_COND = 1,     ///< it's a condition
  LOG_FILTER_XLATE_REF = 2,      ///< needs reference item to compare with
  LOG_FILTER_XLATE_PREFIX = 4,   ///< prefix rather than infix, no ref item
  LOG_FILTER_XLATE_LITERAL = 8,  ///< operator only (no field-name)

  LOG_FILTER_XLATE_VERB = 32,     ///< it's an action
  LOG_FILTER_XLATE_AUXNAME = 64,  ///< aux item name  required
  LOG_FILTER_XLATE_AUXVAL = 128,  ///< aux item value required

  LOG_FILTER_XLATE_FLOW = 1024,  ///< if/then/else/...
  LOG_FILTER_XLATE_CHAIN = 2048  ///< or/and
} log_filter_xlate_flags;

/**
  What kind of token should log_filter_get_token() look for?
*/
typedef enum enum_log_filter_token_flags {
  LOG_FILTER_TOKEN_NONE = 0,      ///< undef
  LOG_FILTER_TOKEN_NAME = 1,      ///< grab a field name
  LOG_FILTER_TOKEN_NUMERIC = 2,   ///< grab a number
  LOG_FILTER_TOKEN_COMP = 4,      ///< grab a known comparator
  LOG_FILTER_TOKEN_ARG = 32,      ///< grab an argument, possibly quoted
  LOG_FILTER_TOKEN_ACTION = 64,   ///< part of action,    ultimately ends in .
  LOG_FILTER_TOKEN_KEYWORD = 128  ///< if/else/etc.
} log_filter_token_flags;

/**
  Element in an array of known tokens in the filter configuration language
*/
typedef struct {
  uint item;         ///< opcode. may be shared by several entries.
  uint flags;        ///< bit vector of log_filter_xlate_flags
  const char *name;  ///< operator name (string literal)
  size_t len;        ///< name's length
} log_filter_xlate_key;

/**
  A few keywords that we look for while parsing, but that do not
  necessarily generate an opcode in the rule-set.
*/
typedef enum enum_log_filter_syntax {
  LOG_FILTER_WORD_NONE = 0,    ///< no previous statement, or stmt complete
  LOG_FILTER_WORD_IF = 1,      ///< "if"
  LOG_FILTER_WORD_THEN = 2,    ///< "then"
  LOG_FILTER_WORD_ELSEIF = 3,  ///< "elseif"
  LOG_FILTER_WORD_ELSE = 4,    ///< "else"
} log_filter_syntax;

/**
  Array of known tokens in the filter configuration language
*/
static const log_filter_xlate_key log_filter_xlate_keys[] = {
    // keywords. order matters: we want to dump "else if" as "elseif" etc.
    {LOG_FILTER_WORD_IF, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("IF")},
    {LOG_FILTER_WORD_ELSEIF, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("ELSEIF")},
    {LOG_FILTER_WORD_ELSEIF, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("ELSE IF")},
    {LOG_FILTER_WORD_ELSEIF, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("ELSIF")}, /* PL/SQL style */
    {LOG_FILTER_WORD_ELSE, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("ELSE")},

    {LOG_FILTER_WORD_THEN, LOG_FILTER_XLATE_FLOW, C_STRING_WITH_LEN("THEN")},

    // conditions

    // absence required
    {LOG_FILTER_COND_ABSENT, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("NOT EXISTS")},
    {LOG_FILTER_COND_ABSENT, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("NOT")},

    // presence required
    {LOG_FILTER_COND_PRESENT, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_PREFIX,
     C_STRING_WITH_LEN("EXISTS")},

    {LOG_FILTER_COND_EQ, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("==")},

    {LOG_FILTER_COND_NE, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("!=")},
    {LOG_FILTER_COND_NE, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("<>")},

    {LOG_FILTER_COND_LT, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("<")},

    {LOG_FILTER_COND_LE, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("<=")},
    {LOG_FILTER_COND_LE, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("=<")},

    {LOG_FILTER_COND_GE, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN(">=")},
    {LOG_FILTER_COND_GE, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN("=>")},

    {LOG_FILTER_COND_GT, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_REF,
     C_STRING_WITH_LEN(">")},

    // verbs/actions

    {LOG_FILTER_DROP, LOG_FILTER_XLATE_VERB, C_STRING_WITH_LEN("drop")},
    {LOG_FILTER_THROTTLE, LOG_FILTER_XLATE_VERB | LOG_FILTER_XLATE_AUXVAL,
     C_STRING_WITH_LEN("throttle")},
    {LOG_FILTER_ITEM_SET,
     LOG_FILTER_XLATE_VERB | LOG_FILTER_XLATE_AUXNAME | LOG_FILTER_XLATE_AUXVAL,
     C_STRING_WITH_LEN("set")},
    {LOG_FILTER_ITEM_DEL, LOG_FILTER_XLATE_VERB | LOG_FILTER_XLATE_AUXNAME,
     C_STRING_WITH_LEN("unset")},

    {LOG_FILTER_CHAIN_AND, LOG_FILTER_XLATE_CHAIN, C_STRING_WITH_LEN("AND")},
    {LOG_FILTER_CHAIN_OR, LOG_FILTER_XLATE_CHAIN, C_STRING_WITH_LEN("OR")}};

/**
  result codes used in dumping/decompiling rules
*/
typedef enum enum_log_filter_result {
  LOG_FILTER_LANGUAGE_OK = 0,          ///< processed without error
  LOG_FILTER_LANGUAGE_DK_COND = 1,     ///< don't know condition
  LOG_FILTER_LANGUAGE_DK_VERB = 2,     ///< don't know verb
  LOG_FILTER_LANGUAGE_DK_CLASS = 3,    ///< don't know class
  LOG_FILTER_LANGUAGE_OOM = 4,         ///< out of memory
  LOG_FILTER_LANGUAGE_GET_FAILED = 5,  ///< filter_ruleset_get() failed
  LOG_FILTER_LANGUAGE_CHAIN = 6        ///< chain conditions (AND/OR)
} log_filter_result;

/**
  result codes of log_filter_set_arg()
*/
typedef enum enum_set_arg_result {
  SET_ARG_SUCCESS = 0,            ///< argument was assigned
  SET_ARG_OOM = -1,               ///< out of memory while assigning argument
  SET_ARG_MALFORMED_FLOAT = -2,   ///< too many decimal points
  SET_ARG_DK_CLASS = -3,          ///< unhandled class
  SET_ARG_UNWANTED_NUMERIC = -4,  ///< numeric value found for non-numeric item
  SET_ARG_UNWANTED_STRING = -5,   ///< string  value found for non-string  item
  SET_ARG_MALFORMED_VALUE = -6,   ///< malformed value
  SET_ARG_UNWANTED_FLOAT = -7,    ///< float   value found for non-float   item
  SET_ARG_FRACTION_FOUND = -8     ///< fraction found. may or may not be legal
} set_arg_result;

/**
  Find a given token in log_filter_xlate_keys[], the table of known
  tokens.  A token in the array will only be considered a valid match
  if it features at least one flag requested by the caller (i.e. if
  it is of the requested class -- comparator, action-verb, etc.).
  Used by log_filter_dragnet_set() to convert tokens into opcodes.

  @param token  token to look up
  @param len    length of token in bytes
  @param flags  combination of log_filter_xlate_flags

  @retval <0    token not found
  @retval >=0   index into log_filter_xlate_keys[]
*/
static int log_filter_xlate_by_name(const char *token, size_t len, uint flags) {
  uint c;

  for (c = 0;
       (c < (sizeof(log_filter_xlate_keys) / sizeof(log_filter_xlate_key)));
       c++) {
    if (((log_filter_xlate_keys[c].flags & flags) == flags) &&
        (len == log_filter_xlate_keys[c].len) &&
        (0 == log_bs->compare(log_filter_xlate_keys[c].name, token, len, true)))
      return c;
  }

  return -1;
}

/**
  Find a given opcode in log_filter_xlate_keys[], the table of known
  tokens.  An opcode in the array will only be considered a valid match
  if it features at least one flag requested by the caller (i.e. if
  it is of the requested class -- comparator, action-verb, etc.).
  Used by log_filter_rule_dump() to convert opcodes into printable tokens.

  @param opcode opcode to look up
  @param flags  combination of log_filter_xlate_flags

  @retval -1    opcode not found
  @retval >=0   index into log_filter_xlate_keys[]
*/
static int log_filter_xlate_by_opcode(uint opcode, uint flags) {
  uint c;

  // optimize and safeify lookup
  for (c = 0;
       (c < (sizeof(log_filter_xlate_keys) / sizeof(log_filter_xlate_key)));
       c++) {
    if ((log_filter_xlate_keys[c].item == opcode) &&
        ((log_filter_xlate_keys[c].flags & flags) != 0))
      return c;
  }

  return -1;
}

/**
  Helper for dumping filter rules.  Append a string literal to a buffer.
  Used by log_filter_rule_dump().

  @param out_buf  NTBS buffer to append to. must contain at least '\0'
  @param out_siz  size of that buffer
  @param str      NTBS to append to that buffer
*/
static void log_filter_append(char *out_buf, size_t out_siz, const char *str) {
  size_t out_used = log_bs->length(out_buf);
  size_t out_left = out_siz - out_used;
  char *out_writepos = out_buf + out_used;
  size_t out_needed = log_bs->substitute(out_writepos, out_left, "%s", str);

  if (out_needed >= out_left)    /* buffer exhausted. '\0' terminate */
    out_buf[out_siz - 1] = '\0'; /* purecov: inspected */
}

/**
  Helper for dumping filter rules.  Append an item's data/value to a buffer.
  Used by log_filter_rule_dump().

  @param out_buf  NTBS buffer to append to. must contain at least '\0'
  @param out_siz  size of that buffer
  @param li       log-item whose value to append to that buffer
*/
static void log_filter_append_item_value(char *out_buf, size_t out_siz,
                                         log_item *li) {
  size_t len = log_bs->length(out_buf);  // used bytes
  size_t out_left = out_siz - len;
  char *out_writepos = out_buf + len;

  if (li->item_class == LOG_FLOAT)
    len =
        log_bs->substitute(out_writepos, out_left, "%lf", li->data.data_float);

  else if (li->item_class == LOG_INTEGER) {
    if (li->type == LOG_ITEM_LOG_PRIO) {
      switch (li->data.data_integer) {
        case ERROR_LEVEL:
          len = log_bs->substitute(out_writepos, out_left, "ERROR");
          break;
        case WARNING_LEVEL:
          len = log_bs->substitute(out_writepos, out_left, "WARNING");
          break;
        case INFORMATION_LEVEL:
          len = log_bs->substitute(out_writepos, out_left, "INFORMATION");
          break;
        default:
          /*
            We have no idea what this is (either breakage, or new
            severities were added to the server that we don't yet
            know about. That's OK though, we can still write the
            numeric value and thereby generate a valid config.
          */
          len = log_bs->substitute(out_writepos, out_left, "%lld",
                                   li->data.data_integer);
      }
    } else if (li->type == LOG_ITEM_SQL_ERRCODE) {
      len = log_bs->substitute(out_writepos, out_left, "MY-%06lld",
                               li->data.data_integer);
    } else {
      len = log_bs->substitute(out_writepos, out_left, "%lld",
                               li->data.data_integer);
    }
  }

  else if (log_bi->item_string_class(li->item_class) &&
           (li->data.data_string.str != nullptr)) {
    len = log_bs->substitute(out_writepos, out_left, "\"%.*s\"",
                             (int)li->data.data_string.length,
                             li->data.data_string.str);
  } else {
    // unknown item class
    log_filter_append(out_writepos, out_left, "???");
    return;
  }

  if (len >= out_left)           /* buffer exhausted. '\0' terminate */
    out_buf[out_siz - 1] = '\0'; /* purecov: inspected */
}

/**
  Decompile an individual rule.
  At this point, we only ever decompile rules we've previously compiled
  ourselves, so short of memory corruption or running out of space, this
  should not fail. We check for failure all the same so all this will
  remain safe if we ever allow decompiles of other components' rule-sets.

  @param rule      rule to decompile
  @param state     resulting state of previous rule's decompile, if any
                   (so we can identify chained rules etc.)
  @param out_buf   buffer to hold the decompiled rule
  @param out_size  size of that buffer

  @retval LOG_FILTER_LANGUAGE_OK       decompile succeeded
  @retval LOG_FILTER_LANGUAGE_DK_COND  rule corrupted; condition unknown
  @retval LOG_FILTER_LANGUAGE_DK_VERB  rule corrupted; action-verb unknown
  @retval LOG_FILTER_LANGUAGE_CHAIN    chain rules (AND/OR)
*/
static log_filter_result log_filter_rule_dump(log_filter_rule *rule,
                                              log_filter_result state,
                                              char *out_buf, size_t out_size) {
  log_filter_result ret = LOG_FILTER_LANGUAGE_OK;
  int cond;
  int verb;
  const log_filter_xlate_key *token;

  DBUG_ASSERT(out_buf != nullptr);

  out_buf[0] = '\0';

  if ((state != LOG_FILTER_LANGUAGE_CHAIN) &&
      (rule->cond != LOG_FILTER_COND_NONE))
    log_filter_append(out_buf, out_size, "IF ");

  if (rule->cond != LOG_FILTER_COND_NONE) {
    // find cond opcode
    if ((cond = log_filter_xlate_by_opcode(rule->cond, LOG_FILTER_XLATE_COND)) <
        0)
      return LOG_FILTER_LANGUAGE_DK_COND; /* purecov: inspected */

    // write condition

    token = &log_filter_xlate_keys[cond];

    if (token->flags & LOG_FILTER_XLATE_PREFIX)  // prefix, if any
    {
      log_filter_append(out_buf, out_size, token->name);
      log_filter_append(out_buf, out_size, " ");
    }
    if (!(token->flags & LOG_FILTER_XLATE_LITERAL))  // field name
      log_filter_append(out_buf, out_size, rule->match.key);
    if (token->flags & LOG_FILTER_XLATE_REF)  // infix
    {
      log_filter_append(out_buf, out_size, token->name);  // comparator
      log_filter_append_item_value(out_buf, out_size, &rule->match);
    }
  }

  // write action

  verb = log_filter_xlate_by_opcode(
      rule->verb, LOG_FILTER_XLATE_VERB | LOG_FILTER_XLATE_CHAIN);

  if (verb < 0) return LOG_FILTER_LANGUAGE_DK_VERB; /* purecov: inspected */

  token = &log_filter_xlate_keys[verb];

  if ((token->item == LOG_FILTER_CHAIN_AND) ||  // AND
      (token->item == LOG_FILTER_CHAIN_OR))     // OR
  {
    log_filter_append(out_buf, out_size, " ");
    log_filter_append(out_buf, out_size, token->name);  // verb name
    log_filter_append(out_buf, out_size, " ");
    return LOG_FILTER_LANGUAGE_CHAIN;
  }

  if (rule->cond != LOG_FILTER_COND_NONE)
    log_filter_append(out_buf, out_size, " THEN ");  // THEN
  else
    log_filter_append(out_buf, out_size, " ");  // space after ELSE

  log_filter_append(out_buf, out_size, token->name);  // verb name

  if (token->flags & (LOG_FILTER_XLATE_AUXNAME | LOG_FILTER_XLATE_AUXVAL))
    log_filter_append(out_buf, out_size, " ");  // space, if needed

  if (token->flags & LOG_FILTER_XLATE_AUXNAME)  // aux item name, if needed
  {
    log_filter_append(out_buf, out_size, rule->aux.key);  // name
    if (token->flags & LOG_FILTER_XLATE_AUXVAL)
      log_filter_append(out_buf, out_size, ":=");  // assignment operator
  }

  if (token->flags & LOG_FILTER_XLATE_AUXVAL)  // aux item value, if needed
  {
    log_filter_append_item_value(out_buf, out_size, &rule->aux);

    if (token->item == LOG_FILTER_THROTTLE)  // denominator of "throttle 5/30"
    {
      log_item dli;
      memset(&dli, 0, sizeof(log_item));
      dli.data.data_integer = (long long)rule->throttle_window_size;
      dli.item_class = LOG_INTEGER;
      dli.type = LOG_ITEM_GEN_INTEGER;
      log_filter_append(out_buf, out_size, "/");
      log_filter_append_item_value(out_buf, out_size, &dli);
    }
  }

  if (rule->jump != 0)
    log_filter_append(out_buf, out_size, " ELSE");  // ELSE/ELSEIF
  else
    log_filter_append(out_buf, out_size, ". ");  // end statement

  return ret;
}

/**
  Dump an entire filter rule-set.

  @param ruleset      rule-set to decompile
  @param ruleset_buf  buffer to write the decompiled ruleset to
  @param siz          size of that buffer in bytes

  @retval  LOG_FILTER_LANGUAGE_OK          decompiling succeeded
  @retval  LOG_FILTER_LANGUAGE_GET_FAILED  couldn't get rules
  @retval  LOG_FILTER_LANGUAGE_OOM         supplied buffer too small
*/
log_filter_result log_filter_ruleset_dump(log_filter_ruleset *ruleset,
                                          char *ruleset_buf, size_t siz) {
  log_filter_result rr = LOG_FILTER_LANGUAGE_OK;  ///< return this result
  uint32 rule_index;                              ///< index of current rule
  log_filter_rule *rule;                          ///< rule to decompile
  char rule_buf[LOG_BUFF_MAX];                    ///< current decompiled rule
  char *out_writepos = ruleset_buf;               ///< write pointer
  size_t out_left = siz - 1;                      ///< bytes left (out buffer)
  size_t len;                                     ///< bytes used in a buffer

  ruleset_buf[0] = '\0';

  // get and lock rule-set

  log_bf->filter_ruleset_lock(ruleset, LOG_BUILTINS_LOCK_SHARED);

  // on failure, we do not need to unlock
  if (ruleset == nullptr)
    return LOG_FILTER_LANGUAGE_GET_FAILED; /* purecov: inspected */

  // dump each rule (if no parse-errors and enough memory)
  for (rule_index = 0; rule_index < ruleset->count; rule_index++) {
    rule = &ruleset->rule[rule_index];
    rr = log_filter_rule_dump(rule, rr, rule_buf, sizeof(rule_buf));

    if ((rr != LOG_FILTER_LANGUAGE_OK) && (rr != LOG_FILTER_LANGUAGE_CHAIN))
      goto done;

    len = log_bs->length(rule_buf);
    if (len >= out_left) {
      rr = LOG_FILTER_LANGUAGE_OOM;
      goto done;
    }

    strcpy(out_writepos, rule_buf);
    out_writepos += len;
    out_left -= len;
  }

  // remove trailing whitespace generated by log_filter_rule_dump()
  if ((len = log_bs->length(ruleset_buf)) > 0) {
    do {
      ruleset_buf[len--] = '\0';
    } while (isspace(ruleset_buf[len]));
  }

done:
  log_bf->filter_ruleset_unlock(ruleset);

  return rr;
}

/**
  Skip whitespace. Helper for parsing.
  Advances a read-pointer to the next non-space character.

  @param[in,out]  inp_readpos  Pointer to a NTBS.
*/
static inline void log_filter_skip_white(const char **inp_readpos) {
  while (isspace(**inp_readpos)) (*inp_readpos)++;
}

/**
  Gets a token from a filter configuration.

  @param[in,out]  inp_readpos  parse position pointer,
                                 in:  current token,
                                 out: next token (on success),
                                      current token (on failure)
  @param[out]     token        pointer to start of (if arg-type,
                               possible quoted) token
  @param[out]     len          length of (if arg-type, possibly quoted)
                               token (0 on fail)
  @param          types        what kind of token (and in what context)
                               do we want?

  @retval  0  OK
  @retval -1  incorrect quotation
  @retval -2  unknown values for 'types'
*/
static int log_filter_get_token(const char **inp_readpos, const char **token,
                                size_t *len, uint types) {
  log_filter_skip_white(inp_readpos);

  *token = *inp_readpos;
  *len = 0;

  // get (quoted) argument
  if ((types & LOG_FILTER_TOKEN_ARG) &&
      ((**inp_readpos == '\"') || (**inp_readpos == '\''))) {
    // Remember what quotation mark was used to start quotation
    const char *delim = *inp_readpos;

    for (++(*inp_readpos); (**inp_readpos != '\0') && (**inp_readpos != *delim);
         (*inp_readpos)++) {
      // skip escaped characters
      if ((**inp_readpos == '\\') && (*(*inp_readpos + 1) != '\0'))
        ++(*inp_readpos);
    }

    // If all went well, opening quotation mark == closing one
    if (**inp_readpos == *delim)
      ++(*inp_readpos);
    else {
      // on failure, rewind
      *inp_readpos = *token;
      return -1;
    }
  }

  // get (unquoted) argument
  else if (types & LOG_FILTER_TOKEN_ARG) {
    // parse up to ' ', '.' (unless part of float value)
    while ((**inp_readpos != '\0') && (!isspace(**inp_readpos)) &&
           ((**inp_readpos != '.') || isdigit(*(*inp_readpos + 1))))
      (*inp_readpos)++;
  }
  // get a comparator
  else if (types & LOG_FILTER_TOKEN_COMP) {
    // stop parsing at digit, for space-less ("field<1" type) rules
    while ((**inp_readpos != '\0') && (!isspace(**inp_readpos)) &&
           (**inp_readpos != '\"') && (**inp_readpos != '\'') &&
           !isalnum(**inp_readpos))
      (*inp_readpos)++;
  }
  // get a field name
  else if (types & LOG_FILTER_TOKEN_NAME) {
    // field names may contain underscores '_'
    while (isalnum(**inp_readpos) || (**inp_readpos == '_')) (*inp_readpos)++;
  }
  // get a keyword
  else if (types & LOG_FILTER_TOKEN_KEYWORD) {
    while (isalpha(**inp_readpos)) (*inp_readpos)++;
  } else {
    *inp_readpos = *token;
    return -2;
  }

  *len = *inp_readpos - *token;

  if (*len < 1)  // Empty argument not allowed. Even "" has length 2!
  {
    *inp_readpos = *token;
    return -2;
  }

  log_filter_skip_white(inp_readpos);

  return 0;
}

/**
  Set up a log-item from filtering rules.

  @param  name   pointer to field name (ad hoc or well-known)
  @param  len    length of field name
  @param  li     log item to set up

  @retval  0     OK
  @retval -1     invalid log-item type (reserved name)
  @retval -2     copy failed (out of memory?)
*/
static int log_filter_make_field(const char **name, const size_t *len,
                                 log_item *li) {
  int wellknown = log_bi->wellknown_by_name(*name, *len);
  log_item_type item_type;
  char *key = nullptr;

  if (wellknown == LOG_ITEM_TYPE_RESERVED) return -1;

  if (wellknown != LOG_ITEM_TYPE_NOT_FOUND)  // it's a well-known type
    item_type = log_bi->wellknown_get_type(wellknown);  // get type
  else if ((key = log_bs->strndup(*name, *len)) ==
           nullptr)  // generic; copy key
    return -2;       /* purecov: inspected */
  else  // it's important that "unknown key" sets a generic type (but not which)
    item_type = LOG_ITEM_GEN_LEX_STRING;

  log_bi->item_set_with_key(
      li, item_type, key,
      (key == nullptr) ? LOG_ITEM_FREE_NONE : LOG_ITEM_FREE_KEY);

  return 0;
}

/**
  Helper: Does a field require a certain data class, or can it morph
  into whatever value we wish to assign to it?  The former is the case
  if the field either has a generic (rather than well-known) type, or
  if it has no type at all (this is the case if a rule has an unnamed
  aux item).

  @param   type  the type to examine

  @retval  true  if field is of generic type or no type
  @retval  false otherwise
*/
static inline bool log_filter_generic_type(log_item_type type) {
  return (type == LOG_ITEM_END) || log_bi->item_generic_type(type);
}

/**
  Set argument (i.e., the value) on a list-item.
  If the item is of any generic type, we'll set the value, and adjust
  the type to be of an appropriate ad hoc type.
  If the item is of a well-known type, we'll set the value on it if
  it's of an appropriate type, but will fail otherwise.
  For this, an integer constitutes a valid float, but not vice versa.
  (A string containing nothing but a number is still not a number.)

  @param       token  pointer to the beginning of the value-token
  @param       len    length of the argument/token
  @param       li     log-item to set the value on
  @param[out]  state  a pointer to additional info about the state.
                      (this is free-text intended for error messages.)

  @return        set_arg_result; 0 for success, !0 for failure
*/
static set_arg_result log_filter_set_arg(const char **token, const size_t *len,
                                         log_item *li, const char **state) {
  char *val;
  size_t val_len;
  bool is_symbol = false;

  // sanity check
  DBUG_ASSERT(!(li->alloc & LOG_ITEM_FREE_VALUE));
  if (li->alloc & LOG_ITEM_FREE_VALUE) {
    log_bs->free((void *)li->data.data_string.str);
    li->data.data_string.str = nullptr;
    li->alloc &= ~LOG_ITEM_FREE_VALUE;
  }

  *state = "Setting argument ...";

  // ER_* -- convenience: we convert symbol(ER_STARTUP) -> int(1234)
  if ((is_symbol = (log_bs->compare(*token, "ER_", 3, false) == 0)) ||
      (log_bs->compare(*token, "MY-", 3, true) == 0)) {
    char *sym = log_bs->strndup(*token, *len);
    longlong errcode = 0;

    *state = is_symbol ? "Resolving ER_symbol ..." : "Resolving MY-code ...";

    if (sym == nullptr) return SET_ARG_OOM; /* purecov: inspected */

    if (is_symbol)
      errcode = log_bi->errcode_by_errsymbol(sym);
    else
      errcode = atoll(&sym[3]);

    log_bs->free(sym);

    if (errcode < 1) {
      *state = is_symbol ? "unknown ER_code" : "invalid MY-code";
      return SET_ARG_MALFORMED_VALUE;
    }

    // if it's any ad hoc type, we set it to "ad hoc int"
    if (log_filter_generic_type(li->type)) {
      li->type = LOG_ITEM_GEN_INTEGER;
      li->item_class = LOG_INTEGER;
    }
    // if it's a well-known type, but not errcode, we fail
    else if (li->type != LOG_ITEM_SQL_ERRCODE) {
      *state =
          "\'err_code\' is the only built-in field-type "
          "we will resolve ER_symbols and MY-codes for";
      return SET_ARG_UNWANTED_NUMERIC;
    }

    li->data.data_integer = errcode;

    return SET_ARG_SUCCESS;
  }

  // prio -- convenience: we convert ERROR / WARNING / INFO -> int
  else if ((li->type == LOG_ITEM_LOG_PRIO) && !isdigit(**token)) {
    int prio = -1;

    *state = "Resolving prio ...";

    if (log_bs->compare(*token, "ERROR", 5, true) == 0)
      prio = ERROR_LEVEL;
    else if (log_bs->compare(*token, "WARNING", 7, true) == 0)
      prio = WARNING_LEVEL;
    else if ((log_bs->compare(*token, "NOTE", 4, true) == 0) ||
             (log_bs->compare(*token, "INFO", 4, true) == 0) ||
             (log_bs->compare(*token, "INFORMATION", 11, true) == 0))
      prio = INFORMATION_LEVEL;
    else {
      *state = "unknown prio";
      return SET_ARG_MALFORMED_VALUE;
    }

    li->data.data_integer = prio;

    return SET_ARG_SUCCESS;
  }

  // quoted string
  else if (((**token == '\"') || (**token == '\''))) {
    *state = "setting quoted string argument";

    // if it's any ad hoc type, we set it to "ad hoc string"
    if (log_filter_generic_type(li->type)) {
      li->type = LOG_ITEM_GEN_LEX_STRING;
      li->item_class = LOG_LEX_STRING;
    }
    // if it's a well-known type, but not a string type, we fail
    else if (!log_bi->item_string_class(li->item_class)) {
      *state = "Argument is of string type, field is not.";
      return SET_ARG_UNWANTED_STRING;
    }

    val_len = *len - 1;

    if ((val = log_bs->strndup(*token + 1, val_len)) == nullptr)
      return SET_ARG_OOM; /* purecov: inspected */

    DBUG_ASSERT(val_len > 0);
    val[--val_len] = '\0';  // cut trailing quotation mark

    li->data.data_string.str = val;
    li->data.data_string.length = val_len;
    li->alloc |= LOG_ITEM_FREE_VALUE;

    return SET_ARG_SUCCESS;
  }

  // numeric
  else {
    set_arg_result ret = SET_ARG_SUCCESS;
    const char *num_read;
    uint dots = 0;

    num_read = *token;
    val_len = *len;

    if ((val_len > 0) && ((*num_read == '+') || (*num_read == '-'))) {
      val_len--;
      num_read++;
    }

    while (val_len > 0) {
      if (*num_read == '.')
        dots++;
      else if (*num_read == '/') {
        *state = "fraction found";
        ret = SET_ARG_FRACTION_FOUND;
        break;
      } else if (!isdigit(*num_read)) {
        *state = "malformed number";
        return SET_ARG_MALFORMED_VALUE;
      }
      num_read++;
      val_len--;
    }

    // floats should not contain multiple decimal points
    if (dots > 1) {
      *state =
          "There should only be one decimal point "
          "in a floating point number.";
      return SET_ARG_MALFORMED_FLOAT;
    }

    if ((val = log_bs->strndup(*token, *len - val_len)) == nullptr)
      return SET_ARG_OOM;

    // found integer
    if (dots == 0) {
      long long num_temp;

      // if it's any ad hoc type, we set it to "ad hoc int"
      if (log_filter_generic_type(li->type)) {
        li->type = LOG_ITEM_GEN_INTEGER;
        li->item_class = LOG_INTEGER;
      }

      num_temp = (long long)atoll(val);

      if (li->item_class == LOG_FLOAT)
        li->data.data_float = (double)num_temp;
      else if (li->item_class == LOG_INTEGER)
        li->data.data_integer = num_temp;
      // if it's a well-known type, but not a numeric type, we fail
      else {
        *state = "Argument is of numeric type, field is not.";
        ret = SET_ARG_UNWANTED_NUMERIC;
      }
    }

    // found float
    else {
      // if it's any ad hoc type, we set it to "ad hoc float"
      if (log_filter_generic_type(li->type)) {
        li->type = LOG_ITEM_GEN_FLOAT;
        li->item_class = LOG_FLOAT;
      }

      // if it's a well-known type, but not of float class, we fail
      if (li->item_class != LOG_FLOAT) {
        *state = "Argument is of float type, field is not.";
        ret = SET_ARG_UNWANTED_FLOAT;
      } else {
        li->data.data_float = atof(val);
      }
    }

    log_bs->free(val);

    return ret;
  }

  // unhandled class
  *state = "argument is of unhandled class";
  return SET_ARG_DK_CLASS;
}

/**
  Set filtering rules from human-readable configuration string.

  @param       ruleset  ruleset to update
  @param       rules    a NTBS containing zero, one, or many rules
  @param[out]  state    a pointer to additional info about the state.
                        (this is free-text intended for error messages.)

  @retval     0  no problems
  @retval    -1  could not acquire ruleset
  @retval    -2  out of memory
  @retval    -3  invalid arguments
  @retval    >0  parse problem at this index in rule string
*/
static int log_filter_dragnet_set(log_filter_ruleset *ruleset,
                                  const char *rules, const char **state) {
  log_filter_rule *rule;                 ///< current  rule
  log_filter_rule *rule_prvs = nullptr;  ///< previous rule, if any
  const char *inp_readpos = rules;       ///< read position in submitted rules
  const char *backtrack;                 ///< retry from here on misparse
  const char *token;                     ///< current token in input
  size_t len;                            ///< token's length
  int c;                                 ///< counter
  int rr = 0;                            ///< return code for caller
  int flow_old,                          ///< previous flow control command
      flow_new = 0,                      ///< current flow control command
      flow_first = 0;                    ///< rule that had the opening IF
  int cond_count;                        ///< number of conditions in branch
  log_filter_ruleset *tmp_filter_rules;  ///< the rule-set we're creating
  log_item *delete_item = nullptr;       ///< implicit item for "unset"
  bool inflight = false;                 ///< have half-finished rule?

  *state = nullptr;

  if (ruleset == nullptr) return -3; /* purecov: inspected */

  tmp_filter_rules = log_bf->filter_ruleset_new(&rule_tag_dragnet, 0);

  if (tmp_filter_rules == nullptr) return -2; /* purecov: inspected */

  log_bf->filter_ruleset_lock(tmp_filter_rules, LOG_BUILTINS_LOCK_EXCLUSIVE);

  if (inp_readpos == nullptr)  // if given a nullptr, we drop the rule-set
    goto done;

  while (*inp_readpos) {
    cond_count = 0;
    flow_old = flow_new;

    rule = (log_filter_rule *)log_bf->filter_rule_init(tmp_filter_rules);

    if (rule == nullptr) {
      *state = "failed to allocate a rule in the current rule-set ...";
      goto parse_error;
    } else
      inflight = true;

    // --1--  expecting IF/ELSE/...

    *state = "getting first token ...";

    // get token
    if (log_filter_get_token(&inp_readpos, &token, &len,
                             LOG_FILTER_TOKEN_KEYWORD) < 0)
      goto parse_error;

    // match token
    if ((c = log_filter_xlate_by_name(
             token, len, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX)) < 0)
      goto parse_error;

    *state = "identified first token ...";

    flow_new = log_filter_xlate_keys[c].item;

    // IF statement must start with IF (not ELSE/ELSEIF/...)
    if (flow_old == LOG_FILTER_WORD_NONE) {
      if (flow_new != LOG_FILTER_WORD_IF) {
        *state = "IF expected";
        goto parse_error;
      }

      flow_first = tmp_filter_rules->count;  // remember where we opened the IF
    }

    // ELSE takes no condition => go straight to action
    if (flow_new == LOG_FILTER_WORD_ELSE) {
      // ELSE unexpected here?
      if (flow_old == LOG_FILTER_WORD_ELSE) {
        *state = "ELSE not expected here ...";
        goto parse_error;
      }

      rule_prvs->jump = 1;
      rule->cond = LOG_FILTER_COND_NONE;

      // test for ELSE IF
      backtrack = inp_readpos;
      // get token
      if (log_filter_get_token(&inp_readpos, &token, &len,
                               LOG_FILTER_TOKEN_KEYWORD) < 0) {
        *state = "failed to get token after ELSE ...";
        goto parse_error;
      }

      // match token
      if (((c = log_filter_xlate_by_name(
                token, len, LOG_FILTER_XLATE_FLOW | LOG_FILTER_XLATE_PREFIX)) >=
           0) &&
          (log_filter_xlate_keys[c].item == LOG_FILTER_WORD_IF))
        flow_new = LOG_FILTER_WORD_ELSEIF;

      else {
        inp_readpos = backtrack;
        *state = "ELSE needs no condition, parsing action-verb next ...";
        goto parse_action;  // skip over cond parsing
      }
    }

    /*
      If it's not ELSE, reset implicit UNSET item here;
      it should be set up when the condition is parsed.
    */
    delete_item = nullptr;

    if (flow_new == LOG_FILTER_WORD_ELSEIF) {
      // ELSEIF unexpected here?
      if (flow_old == LOG_FILTER_WORD_ELSE) {
        *state = "ELSEIF not expected here ...";
        goto parse_error;
      }

      rule_prvs->jump = 1;
    }

    *state = "testing for prefix";

  parse_cond:

    // --2--  testing for prefix (NOT EXISTS / EXISTS)

    log_filter_get_token(&inp_readpos, &token, &len, LOG_FILTER_TOKEN_KEYWORD);

    backtrack = token;  // save in case it's not actually a prefix

    if ((c = log_filter_xlate_by_name(
             token, len, LOG_FILTER_XLATE_COND | LOG_FILTER_XLATE_PREFIX)) >=
        0) {
      // ok, we matched a prefix operator
      rule->cond = (log_filter_cond)log_filter_xlate_keys[c].item;

      // "NOT EXISTS" is special as it's multi-word
      if (rule->cond == LOG_FILTER_COND_ABSENT) {
        size_t len_not_exists;
        int i = 0;

        // find full "NOT EXISTS" record, no matter which one we matched
        while (log_filter_xlate_keys[i].item != LOG_FILTER_COND_ABSENT) i++;

        // make sure we've actually got both words
        len_not_exists = log_filter_xlate_keys[i].len;
        if ((log_bs->compare(log_filter_xlate_keys[i].name, token,
                             len_not_exists, true) != 0) ||
            (!isspace(token[len_not_exists]))) {
          *state = "NOT requires EXISTS";
          goto parse_error;
        }

        // if so, hooray, skip over them
        inp_readpos = token + len_not_exists;
      }

      // field name
      if (log_filter_get_token(&inp_readpos, &token, &len,
                               LOG_FILTER_TOKEN_NAME) < 0) {
        *state = "field name missing or invalid after EXISTS";
        goto parse_error;
      }

      if (log_filter_make_field(&token, &len, &rule->match) < 0) {
        *state = "could not set up field for EXISTS";
        goto parse_error;
      }

      delete_item = &rule->match;
    }

    // --3--  infix conditional
    else {
      inp_readpos = backtrack;

      // field name
      if (log_filter_get_token(&inp_readpos, &token, &len,
                               LOG_FILTER_TOKEN_NAME)) {
        *state = "field name missing or invalid before comparator";
        goto parse_error;
      }

      if (log_filter_make_field(&token, &len, &rule->match) < 0) {
        *state = "could not set up field before comparator";
        goto parse_error;
      }

      delete_item = &rule->match;

      // comparator (infix)
      log_filter_get_token(&inp_readpos, &token, &len, LOG_FILTER_TOKEN_COMP);
      if ((c = log_filter_xlate_by_name(token, len, LOG_FILTER_XLATE_REF)) <
          0) {
        *state = "unknown comparator";
        inp_readpos = token;
        goto parse_error;
      }

      rule->cond = (log_filter_cond)log_filter_xlate_keys[c].item;

      // condition value
      if (log_filter_get_token(&inp_readpos, &token, &len,
                               LOG_FILTER_TOKEN_ARG)) {
        inp_readpos = token;
        goto parse_error;
      }

      if (log_filter_set_arg(&token, &len, &rule->match, state) < 0) {
        inp_readpos = token;
        goto parse_error;
      }
    }

    cond_count++;

    // expect THEN
    *state = "looking for THEN";

    log_filter_get_token(&inp_readpos, &token, &len, LOG_FILTER_TOKEN_KEYWORD);
    if (((c = log_filter_xlate_by_name(token, len, LOG_FILTER_XLATE_FLOW)) <
         0) ||
        (log_filter_xlate_keys[c].item != LOG_FILTER_WORD_THEN)) {
      // THEN not found, try AND or OR
      if ((c = log_filter_xlate_by_name(token, len, LOG_FILTER_XLATE_CHAIN)) <
          0) {
        inp_readpos = token;
        goto parse_error;
      }

      // AND/OR found
      rule->verb = (log_filter_verb)log_filter_xlate_keys[c].item;

      rule_prvs = rule;
      tmp_filter_rules->count++;
      rule = (log_filter_rule *)log_bf->filter_rule_init(tmp_filter_rules);

      if (rule == nullptr) {
        *state = "failed to allocate a rule in the current rule-set ...";
        inflight = false;
        goto parse_error;
      }

      // continue with another condition
      goto parse_cond;
    }

    // THEN found, parse action.

  parse_action:

    /*
      If we're here, we're either not inside an IF/ELSEIF/ELSE
      conditional, or we've just seen either a THEN or an ELSE.
    */

    *state = "looking for action verb";

    // verb
    if (log_filter_get_token(&inp_readpos, &token, &len, LOG_FILTER_TOKEN_NAME))
      goto parse_error;

    if ((c = log_filter_xlate_by_name(token, len, LOG_FILTER_XLATE_VERB)) < 0) {
      inp_readpos = token;
      goto parse_error;
    }

    rule->verb = (log_filter_verb)log_filter_xlate_keys[c].item;

    // aux name
    if (log_filter_xlate_keys[c].flags & LOG_FILTER_XLATE_AUXNAME) {
      *state = "looking for action's field name";

      // convenience: if item_del has no object, it uses the one from cond
      if ((log_filter_xlate_keys[c].item == LOG_FILTER_ITEM_DEL) &&
          (*inp_readpos == '.'))  // end of statement reached, unset had no arg.
      {
        int c;
        char *n;
        size_t key_len;

        if ((cond_count != 1) || (delete_item == nullptr)) {
          *state =
              "implicit field name only allowed for "
              "IFs with exactly 1 condition.";
          goto parse_error;
        }

        // see whether it's a well-known type
        c = log_bi->wellknown_by_name(
            delete_item->key, (key_len = log_bs->length(rule->match.key)));

        // for ad-hoc names, copy the key; otherwise use well-known record.
        if (c != LOG_ITEM_TYPE_NOT_FOUND)
          n = nullptr;
        else if ((n = log_bs->strndup(rule->match.key, key_len)) == nullptr) {
          rr = -2; /* purecov: inspected */
          goto done;
        }

        log_bi->item_set_with_key(
            &rule->aux, rule->match.type, n,
            (n == nullptr) ? LOG_ITEM_FREE_NONE : LOG_ITEM_FREE_KEY);
      } else  // explicit aux name required
      {
        if (log_filter_get_token(&inp_readpos, &token, &len,
                                 LOG_FILTER_TOKEN_NAME) < 0)
          goto parse_error;

        if (log_filter_make_field(&token, &len, &rule->aux) < 0) {
          *state = "could not set up field in action";
          goto parse_error;
        }
      }

      // skip optional assignment operator (:= or =)
      if (log_filter_xlate_keys[c].flags & LOG_FILTER_XLATE_AUXVAL) {
        if ((*inp_readpos == ':') && (inp_readpos[1] == '=')) {
          inp_readpos += 2;
          log_filter_skip_white(&inp_readpos);
        } else if ((*inp_readpos == '=') && (inp_readpos[1] != '=')) {
          inp_readpos++;
          log_filter_skip_white(&inp_readpos);
        }
      }
    }

    if (log_filter_xlate_keys[c].flags & LOG_FILTER_XLATE_AUXVAL) {
      *state = "looking for action field's value";

      // aux value
      if (log_filter_get_token(&inp_readpos, &token, &len,
                               LOG_FILTER_TOKEN_ARG)) {
        inp_readpos = token;
        goto parse_error;
      }

      {
        int auxval_success =
            log_filter_set_arg(&token, &len, &rule->aux, state);

        if ((rule->verb == LOG_FILTER_THROTTLE) &&
            ((auxval_success == SET_ARG_SUCCESS) ||
             (auxval_success == SET_ARG_FRACTION_FOUND))) {
          if (rule->aux.item_class != LOG_INTEGER) {
            *state = "action \"throttle\" requires integer or fraction";
            inp_readpos = token;
            goto parse_error;
          }

          // handle denominator
          else if (auxval_success == SET_ARG_FRACTION_FOUND) {
            ulong window_size;
            const char *denominator = log_bs->find_first(token, '/');
            const char *fail =
                "failed to parse denominator of fraction "
                "(0 < integer number of seconds <= 604800)";

            if (denominator != nullptr) {
              log_item dli;

              memset(&dli, 0, sizeof(log_item));
              denominator++;
              len = len - (denominator - token);

              if ((log_filter_set_arg(&denominator, &len, &dli, state) < 0) ||
                  (dli.type != LOG_ITEM_GEN_INTEGER) ||
                  (dli.data.data_integer <= 0) ||
                  (dli.data.data_integer >
                   604800))  // 7 days * 60 minutes * 60s
              {
                *state = fail;
                inp_readpos = denominator;
                goto parse_error;
              }
              window_size = (ulong)dli.data.data_integer;

              rule->throttle_window_size = window_size;
            }
          }

          // check numerator
          if (rule->aux.data.data_integer <= 0) {
            *state = "numerator must be larger than 0";
            inp_readpos = token;
            goto parse_error;
          }
        }

        // not throttle
        else if (auxval_success < 0) {
          inp_readpos = token;
          goto parse_error;
        }
      }
    }

    log_filter_skip_white(&inp_readpos);

    if (*inp_readpos == '.') {
      inp_readpos++;
      log_filter_skip_white(&inp_readpos);
      flow_new = LOG_FILTER_WORD_NONE;

      {  // add relative jumps to the end of all blocks in THEN/ELSEIF
        int flow_last = tmp_filter_rules->count;
        for (c = flow_first; c <= flow_last; c++) {
          if (tmp_filter_rules->rule[c].jump != 0)
            tmp_filter_rules->rule[c].jump = flow_last - c + 1;
        }
      }
    }

    rule_prvs = rule;

    tmp_filter_rules->count++;
    inflight = false;
  }

  if (flow_new != LOG_FILTER_WORD_NONE) {
    *state = "statement incomplete";
    goto parse_error;
  }

  goto done;

parse_error:
  if (inflight)                 // make sure we also remove
    tmp_filter_rules->count++;  // the half-finished rule!

  rr = (int)(inp_readpos + 1 - rules);
  log_bf->filter_ruleset_free(&tmp_filter_rules);  // discard incomplete ruleset
  return rr;

done:
  log_bf->filter_ruleset_lock(ruleset, LOG_BUILTINS_LOCK_EXCLUSIVE);
  log_bf->filter_ruleset_move(tmp_filter_rules, ruleset);
  log_bf->filter_ruleset_free(&tmp_filter_rules);  // free old ruleset
  log_bf->filter_ruleset_unlock(ruleset);

  return rr;
}

/**
  Variable listener.  This is a temporary solution until we have
  per-component system variables.  "check" is called when the user
  uses SQL statements trying to assign a value to certain server
  system variables; the function can prevent assignment if e.g.
  the supplied value has the wrong format.

  If several listeners are registered, an error will be signaled
  to the user on the SQL level as soon as one service identifies
  a problem with the value.

  @param   ll  a log_line describing the variable update (name, new value)

  @retval   0  for allow (including when we don't feel the event is for us),
  @retval  <0  deny (nullptr, malformed structures, etc. -- caller broken?)
  @retval  >0  deny (user input rejected)
*/
DEFINE_METHOD(int, log_service_imp::variable_check,
              (log_line * ll MY_ATTRIBUTE((unused)))) {
  /*
    We allow changing this even when we're not in the error "stack",
    so users can configure this service, then enable it, rather than
    enable it, and have it start logging with perhaps unwanted settings
    until the user manages to update them.
  */
  return 0;
}

/**
  Variable listener.  This is a temporary solution until we have
  per-component system variables. "update" is called when the user
  uses SQL statements trying to assign a value to certain server
  system variables. If we got this far, we have already been called
  upon to "check" the new value, and have confirmed that it meets
  the requirements. "update" should now update the internal
  representation of the value. Since we have already checked the
  new value, failure should be a rare occurance (out of memory,
  the operating system did not let us open the new file name, etc.).

  If several listeners are registered, all will currently be called
  with the new value, even if one of them signals failure.

  @param  ll  a list-item describing the variable (name, new value)

  @retval  0  the event is not for us
  @retval <0  for failure
  @retval >0  for success (at least one item was updated)
*/
DEFINE_METHOD(int, log_service_imp::variable_update,
              (log_line * ll MY_ATTRIBUTE((unused)))) {
  return 0;
}

static int check_var_filter_rules(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                                  SYS_VAR *self MY_ATTRIBUTE((unused)),
                                  void *save MY_ATTRIBUTE((unused)),
                                  struct st_mysql_value *value) {
  int ret;
  log_filter_ruleset *log_filter_temp_rules;
  const char *state = nullptr;

  char notify_buffer[LOG_FILTER_DUMP_BUFF_SIZE];
  int value_len = 0;
  const char *proposed_rules;

  if (value == nullptr) return true;

  proposed_rules = value->val_str(value, nullptr, &value_len);

  if (proposed_rules == nullptr) return true;

  DBUG_ASSERT(proposed_rules[value_len] == '\0');

  log_filter_temp_rules = log_bf->filter_ruleset_new(&rule_tag_dragnet, 0);

  if (log_filter_temp_rules == nullptr) return true; /* purecov: inspected */

  ret = log_filter_dragnet_set(log_filter_temp_rules, proposed_rules, &state);

  if (ret > 0)
    log_bt->notify_client(
        thd, Sql_condition::SL_WARNING, ER_COMPONENT_FILTER_FLABBERGASTED,
        notify_buffer, sizeof(notify_buffer) - 1,
        "The log-filter component \"%s\" got confused at "
        "\"%s\" (state: %s) ...",
        LOG_FILTER_LANGUAGE_NAME, &proposed_rules[ret - 1], state);
  else if (ret == 0) {
    log_filter_result dump_result;  ///< result code from dump

    *static_cast<const char **>(save) = proposed_rules;

    dump_result = log_filter_ruleset_dump(log_filter_temp_rules,
                                          log_error_filter_decompile,
                                          LOG_FILTER_DUMP_BUFF_SIZE - 1);

    if (dump_result != LOG_FILTER_LANGUAGE_OK) {
      log_bt->notify_client(
          thd, Sql_condition::SL_NOTE, ER_COMPONENT_FILTER_DIAGNOSTICS,
          notify_buffer, sizeof(notify_buffer) - 1,
          "The log-filter component \"%s\" updated its configuration from "
          "its system variable \"%s.%s\", but could not update its status "
          "variable \"%s.%s\" to reflect the decompiled rule-set.",
          LOG_FILTER_LANGUAGE_NAME, LOG_FILTER_LANGUAGE_NAME,
          LOG_FILTER_SYSVAR_NAME, LOG_FILTER_LANGUAGE_NAME,
          LOG_FILTER_STATUS_NAME);
    }
  }

  log_bf->filter_ruleset_lock(log_filter_temp_rules,
                              LOG_BUILTINS_LOCK_EXCLUSIVE);
  log_bf->filter_ruleset_free(&log_filter_temp_rules);

  return (ret != 0);
}

/**
  Update value of component variable

  @param  thd      session
  @param  self     the system variable we're changing
  @param  var_ptr  where to save the resulting (char *) value
  @param  save     pointer to the new value (see check function)
*/
static void update_var_filter_rules(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                                    SYS_VAR *self MY_ATTRIBUTE((unused)),
                                    void *var_ptr, const void *save) {
  const char *state = nullptr;
  const char *new_val = *((const char **)save);

  if ((log_filter_dragnet_set(log_filter_dragnet_rules, new_val, &state) ==
       0) &&
      (var_ptr != nullptr)) {
    // the caller will free the old value, don't double free it here!
    *((const char **)var_ptr) = new_val;
  }
}

/**
  services: log filter: basic filtering

  The actual dragnet filtering engine is currently the default filter
  and therefore built-in (see components/mysql_server/log_builtins_filter.cc);
  this service is just a configuration language parser/compiler/decompiler;
  once it has set up the filter rule-set according to the user's instructions,
  it calls on the built-in engine to do the actual filtering.

  Third parties could in theory write their own filtering language on top of
  that engine; they could also create their own filtering engine and use it
  instead of the provided one.

  @param           ll                   the log line to filter
  @param           instance             instance (unused in dragnet as it's
                                        not currently multi-open; we just
                                        always use log_filter_dragnet_rules)

  @retval          int                  number of matched rules
*/
DEFINE_METHOD(int, log_service_imp::run,
              (void *instance MY_ATTRIBUTE((unused)), log_line *ll)) {
  return log_bf->filter_run(log_filter_dragnet_rules, ll);
}

/**
  Open a new instance.

  @param   ll        optional arguments
  @param   instance  If state is needed, the service may allocate and
                     initialize it and return a pointer to it here.
                     (This of course is particularly pertinent to
                     components that may be opened multiple times,
                     such as the JSON log writer.)
                     This state is for use of the log-service component
                     in question only and can take any layout suitable
                     to that component's need. The state is opaque to
                     the server/logging framework. It must be released
                     on close.

  @retval  <0        a new instance could not be created
  @retval  =0        success, returned hande is valid
*/
DEFINE_METHOD(int, log_service_imp::open,
              (log_line * ll MY_ATTRIBUTE((unused)), void **instance)) {
  if (instance == nullptr) return -1;

  *instance = nullptr;

  opened++;

  return 0;
}

/**
  Close and release an instance. Flushes any buffers.

  @param   instance  State-pointer that was returned on open.
                     If memory was allocated for this state,
                     it should be released, and the pointer
                     set to nullptr.

  @retval  <0        an error occurred
  @retval  =0        success
*/
DEFINE_METHOD(int, log_service_imp::close, (void **instance)) {
  if (instance == nullptr) return -1;

  *instance = nullptr;

  opened--;

  return 0;
}

/**
  Flush any buffers.  This function will be called by the server
  on FLUSH ERROR LOGS.  The service may write its buffers, close
  and re-open any log files to work with log-rotation, etc.
  The flush function MUST NOT itself log anything!
  A service implementation may provide a nullptr if it does not
  wish to provide a flush function.

  @param   instance  State-pointer that was returned on open.
                     Value may be changed in flush.

  @retval  <0        an error occurred
  @retval  =0        no work was done
  @retval  >0        flush completed without incident
*/
DEFINE_METHOD(int, log_service_imp::flush,
              (void **instance MY_ATTRIBUTE((unused)))) {
  return 0;
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
mysql_service_status_t log_filter_exit() {
  if (inited) {
    mysql_service_component_sys_variable_unregister->unregister_variable(
        LOG_FILTER_LANGUAGE_NAME, LOG_FILTER_SYSVAR_NAME);

    mysql_service_status_variable_registration->unregister_variable(
        (SHOW_VAR *)&show_var_filter_rules_decompile);

    log_bf->filter_ruleset_lock(log_filter_dragnet_rules,
                                LOG_BUILTINS_LOCK_EXCLUSIVE);
    log_bf->filter_ruleset_free(&log_filter_dragnet_rules);

    inited = false;
    opened = 0;
    log_error_filter_rules = nullptr;

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
mysql_service_status_t log_filter_init() {
  const char *state = nullptr;
  char *var_value;
  size_t var_len = 0;
  int rr = -1;

  if (inited) return true; /* purecov: inspected */

  inited = true;
  var_value = new char[LOG_FILTER_DUMP_BUFF_SIZE];

  sys_var_filter_rules.def_val = (char *)LOG_FILTER_DEFAULT_RULES;

  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
  log_bf = mysql_service_log_builtins_filter;
  log_bt = mysql_service_log_builtins_tmp;

  if (((log_filter_dragnet_rules =
            log_bf->filter_ruleset_new(&rule_tag_dragnet, 0)) == nullptr) ||
      mysql_service_component_sys_variable_register->register_variable(
          LOG_FILTER_LANGUAGE_NAME, LOG_FILTER_SYSVAR_NAME,
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "Error log filter rules (for the dragnet filter "
          "configuration language)",
          check_var_filter_rules, update_var_filter_rules,
          (void *)&sys_var_filter_rules, (void *)&log_error_filter_rules) ||
      mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&show_var_filter_rules_decompile) ||
      mysql_service_component_sys_variable_register->get_variable(
          LOG_FILTER_LANGUAGE_NAME, LOG_FILTER_SYSVAR_NAME, (void **)&var_value,
          &var_len) ||
      ((rr = log_filter_dragnet_set(log_filter_dragnet_rules, var_value,
                                    &state)) != 0)) {
    /*
      If the actual setup worked, but we were passed an invalid value
      for the variable, try to throw diagnostics!
    */

    if ((rr > 0) && (log_bs != nullptr)) {
      rr--;
      if (var_value[rr] == '\0') rr = 0;

      LogErr(ERROR_LEVEL, ER_COMPONENT_FILTER_WRONG_VALUE,
             LOG_FILTER_LANGUAGE_NAME "." LOG_FILTER_SYSVAR_NAME,
             (var_value == nullptr) ? "<NULL>" : (const char *)var_value);

      if (var_value != nullptr)
        LogErr(WARNING_LEVEL, ER_COMPONENT_FILTER_CONFUSED,
               LOG_FILTER_LANGUAGE_NAME, &var_value[rr], state);

      // try to set default value. if that fails as well, refuse to load.
      if ((rr = log_filter_dragnet_set(log_filter_dragnet_rules,
                                       sys_var_filter_rules.def_val, &state)) ==
          0) {
        char *old = log_error_filter_rules;
        if ((log_error_filter_rules = log_bs->strndup(
                 sys_var_filter_rules.def_val,
                 log_bs->length(sys_var_filter_rules.def_val) + 1)) !=
            nullptr) {
          if (old != nullptr) log_bs->free((void *)old);
          goto success;
        }

        // if we failed to copy the default, restore the previous value
        log_error_filter_rules = old;
      }

      LogErr(ERROR_LEVEL, ER_COMPONENT_FILTER_WRONG_VALUE,
             LOG_FILTER_LANGUAGE_NAME "." LOG_FILTER_SYSVAR_NAME, "DEFAULT");
    }

    delete[] var_value; /* purecov: begin inspected */
    log_filter_exit();
    return true; /* purecov: end */
  }

success:
  DBUG_ASSERT(var_value[var_len] == '\0');
  delete[] var_value;
  return false;
}

/* implementing a service: log_filter */
BEGIN_SERVICE_IMPLEMENTATION(log_filter_dragnet, log_service)
log_service_imp::run, log_service_imp::flush, nullptr, nullptr,
    log_service_imp::variable_check,
    log_service_imp::variable_update END_SERVICE_IMPLEMENTATION();

/* component provides: just the log_filter service, for now */
BEGIN_COMPONENT_PROVIDES(log_filter_dragnet)
PROVIDES_SERVICE(log_filter_dragnet, log_service), END_COMPONENT_PROVIDES();

/* component requires: pluggable system variables, log-builtins */
BEGIN_COMPONENT_REQUIRES(log_filter_dragnet)
REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(status_variable_registration),
    REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(log_builtins_filter), REQUIRES_SERVICE(log_builtins_tmp),
    END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(log_filter_dragnet)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("log_service_type", "filter"),
    END_COMPONENT_METADATA();

/* component declaration */
DECLARE_COMPONENT(log_filter_dragnet, "mysql:log_filter_dragnet")
log_filter_init, log_filter_exit END_DECLARE_COMPONENT();

/* components contained in this library.
   for now assume that each library will have exactly one component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(log_filter_dragnet)
    END_DECLARE_LIBRARY_COMPONENTS

    /* EOT */
