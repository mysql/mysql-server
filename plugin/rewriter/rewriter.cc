/*  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/rewriter/rewriter.h"

#include "my_config.h"

#include <mysql/service_parser.h>
#include <mysql/service_rules_table.h>
#include <stddef.h>
#include <memory>
#include <string>

#include "m_string.h"  // Needed because debug_sync.h is not self-sufficient.
#include "my_dbug.h"
#include "mysqld_error.h"
#include "nullable.h"
#include "plugin/rewriter/messages.h"
#include "plugin/rewriter/persisted_rule.h"
#include "plugin/rewriter/rule.h"
#include "sql/debug_sync.h"
#include "template_utils.h"

using Mysql::Nullable;
using rules_table_service::Cursor;
using std::string;
namespace messages = rewriter_messages;

namespace {

std::string hash_key_from_digest(const uchar *digest) {
  return std::string(pointer_cast<const char *>(digest),
                     PARSER_SERVICE_DIGEST_LENGTH);
}

}  // namespace

/**
  @file rewriter.cc
  Implementation of the Rewriter class's member functions.
*/

Rewriter::Rewriter() {}

Rewriter::~Rewriter() {}

bool Rewriter::load_rule(MYSQL_THD thd, Persisted_rule *diskrule) {
  std::unique_ptr<Rule> memrule_ptr(new Rule);
  Rule *memrule = memrule_ptr.get();
  Rule::Load_status load_status = memrule->load(thd, diskrule);

  switch (load_status) {
    case Rule::OK:
      m_digests.emplace(hash_key_from_digest(memrule_ptr->digest_buffer()),
                        std::move(memrule_ptr));
      diskrule->message = Nullable<string>();
      diskrule->pattern_digest =
          services::print_digest(memrule->digest_buffer());
      diskrule->normalized_pattern = memrule->normalized_pattern();
      return false;
      break;
    case Rule::PATTERN_GOT_NO_DIGEST:
      diskrule->set_message(messages::PATTERN_GOT_NO_DIGEST);
      break;
    case Rule::PATTERN_PARSE_ERROR:
      diskrule->set_message(string(messages::PATTERN_PARSE_ERROR) +
                            ": "
                            ">>" +
                            memrule->pattern_parse_error_message() + "<<");
      break;
    case Rule::PATTERN_NOT_A_SELECT_STATEMENT:
      diskrule->set_message(messages::PATTERN_NOT_A_SELECT_STATEMENT);
      break;
    case Rule::REPLACEMENT_PARSE_ERROR:
      diskrule->set_message(string(messages::REPLACEMENT_PARSE_ERROR) +
                            ": "
                            ">>" +
                            memrule->replacement_parse_error_message() + "<<");
      break;
    case Rule::REPLACEMENT_HAS_MORE_MARKERS:
      diskrule->set_message(messages::REPLACEMENT_HAS_MORE_MARKERS);
      break;
  }

  return true;
}

#ifndef DBUG_OFF
/**
  Normal debug sync points will not work in the THD that the plugin creates,
  so we have to call the debug sync functions ourselves.
*/
static void do_debug_sync(MYSQL_THD thd) {
  DBUG_ASSERT(opt_debug_sync_timeout > 0);
  const char act[] = "now signal parked wait_for go";
  DBUG_ASSERT(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
}
#endif

void Rewriter::do_refresh(MYSQL_THD session_thd) {
  bool saw_rule_error = false;

  DBUG_ENTER("Rewriter::do_refresh");
  Cursor c(session_thd);

  DBUG_PRINT("info", ("Rewriter::do_refresh cursor opened"));
  DBUG_EXECUTE_IF("dbug.block_do_refresh", { do_debug_sync(session_thd); });

  if (c.table_is_malformed()) {
    m_refresh_status = ER_REWRITER_TABLE_MALFORMED_ERROR;
    DBUG_VOID_RETURN;
  }
  m_digests.clear();

  for (; c != rules_table_service::end(); ++c) {
    Persisted_rule diskrule(&c);
    if (diskrule.is_enabled) {
      if (!diskrule.pattern.has_value()) {
        diskrule.set_message("Pattern is NULL.");
        saw_rule_error = true;
      } else if (!diskrule.replacement.has_value()) {
        diskrule.set_message("Replacement is NULL.");
        saw_rule_error = true;
      } else
        saw_rule_error |= load_rule(session_thd, &diskrule);
      diskrule.write_to(&c);
    }
  }
  if (c.had_serious_read_error())
    m_refresh_status = ER_REWRITER_READ_FAILED;
  else if (saw_rule_error)
    m_refresh_status = ER_REWRITER_LOAD_FAILED;
  else
    m_refresh_status = 0;
  DBUG_VOID_RETURN;
}

namespace {

struct Refresh_callback_args {
  Rewriter *me;
  MYSQL_THD session_thd;
};

extern "C" void *refresh_callback(void *p_args) {
  Refresh_callback_args *args = pointer_cast<Refresh_callback_args *>(p_args);
  (args->me->do_refresh)(args->session_thd);
  return NULL;
}

}  // namespace

longlong Rewriter::refresh(MYSQL_THD thd) {
  services::Session session(thd);

  Refresh_callback_args args = {this, session.thd()};

  m_refresh_status = 0;

  my_thread_handle handle;
  mysql_parser_start_thread(session.thd(), refresh_callback, &args, &handle);

  mysql_parser_join_thread(&handle);

  return m_refresh_status;
}

Rewrite_result Rewriter::rewrite_query(MYSQL_THD thd, const uchar *key) {
  Rewrite_result result;
  bool digest_matched = false;

  auto it_range = m_digests.equal_range(hash_key_from_digest(key));
  for (auto it = it_range.first; it != it_range.second; ++it) {
    Rule *rule = it->second.get();
    if (rule->matches(thd)) {
      result = rule->create_new_query(thd);
      if (result.was_rewritten) return result;
    } else {
      digest_matched = true;
    }
  }

  result.was_rewritten = false;
  result.digest_matched = digest_matched;
  return result;
}
