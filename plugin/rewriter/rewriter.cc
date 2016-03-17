/*  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
    02110-1301  USA */

#include "my_config.h"
#include "messages.h"
#include "persisted_rule.h"
#include "rewriter.h"
#include "rule.h"
#include "nullable.h"
#include <mysql/service_parser.h>
#include <mysql/service_rules_table.h>
#include <hash.h>

#include "template_utils.h"
#include <m_string.h> // Needed because debug_sync.h is not self-sufficient.
#include "debug_sync.h"
#include <memory>
#include <string>

using std::string;
using rules_table_service::Cursor;
using Mysql::Nullable;
namespace messages = rewriter_messages;

/**
  @file rewriter.cc
  Implementation of the Rewriter class's member functions.
*/


/** Functions used in the hash */
uchar *get_rule_hash_code(const uchar *entry, size_t *length,
                          my_bool MY_ATTRIBUTE((unused)))
{
  const Rule *rule= pointer_cast<const Rule*>(entry);
  *length= PARSER_SERVICE_DIGEST_LENGTH;
  const uchar *digest= pointer_cast<const uchar*>(rule->digest_buffer());
  /*
    What on earth is the hash table going to do with a non-const hash key?
    Maybe I don't want to know...
  */
  return const_cast<uchar*>(digest);
}


void free_rule(void *entry) { delete pointer_cast<Rule*>(entry); }


Rewriter::Rewriter()
{
  my_hash_init(&m_digests, &my_charset_bin, 10, 0,
               PARSER_SERVICE_DIGEST_LENGTH,
               get_rule_hash_code,
               free_rule, 0,
               PSI_INSTRUMENT_ME);
}


Rewriter::~Rewriter() { my_hash_free(&m_digests); }


bool Rewriter::load_rule(MYSQL_THD thd, Persisted_rule *diskrule)
{
  std::auto_ptr<Rule> memrule_ptr(new Rule);
  Rule *memrule= memrule_ptr.get();
  Rule::Load_status load_status= memrule->load(thd, diskrule);

  switch (load_status)
  {
  case Rule::OK:
    my_hash_insert(&m_digests, pointer_cast<uchar*>(memrule_ptr.release()));
    diskrule->message= Nullable<string>();
    diskrule->pattern_digest= services::print_digest(memrule->digest_buffer());
    diskrule->normalized_pattern= memrule->normalized_pattern();
    return false;
    break;
  case Rule::PATTERN_GOT_NO_DIGEST:
    diskrule->set_message(messages::PATTERN_GOT_NO_DIGEST);
    break;
  case Rule::PATTERN_PARSE_ERROR:
    diskrule->set_message(string(messages::PATTERN_PARSE_ERROR) + ": "
                          ">>" +
                          memrule->pattern_parse_error_message() +
                          "<<");
    break;
  case Rule::PATTERN_NOT_A_SELECT_STATEMENT:
    diskrule->set_message(messages::PATTERN_NOT_A_SELECT_STATEMENT);
    break;
  case Rule::REPLACEMENT_PARSE_ERROR:
    diskrule->set_message(string(messages::REPLACEMENT_PARSE_ERROR) + ": "
                          ">>" +
                          memrule->replacement_parse_error_message() +
                          "<<");
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
static void do_debug_sync(MYSQL_THD thd)
{
  DBUG_ASSERT(opt_debug_sync_timeout > 0);
  const char act[]= "now signal parked wait_for go";
  DBUG_ASSERT(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
}
#endif

void Rewriter::do_refresh(MYSQL_THD session_thd)
{
  bool saw_rule_error= false;

  DBUG_ENTER("Rewriter::do_refresh");
  Cursor c(session_thd);

  DBUG_PRINT("info", ("Rewriter::do_refresh cursor opened"));
  DBUG_EXECUTE_IF("dbug.block_do_refresh", { do_debug_sync(session_thd); });

  if (c.table_is_malformed())
  {
    m_refresh_status= REWRITER_ERROR_TABLE_MALFORMED;
    DBUG_VOID_RETURN;
  }
  my_hash_reset(&m_digests);

  for (; c != rules_table_service::end(); ++c)
  {
    Persisted_rule diskrule(&c);
    if (diskrule.is_enabled)
    {
      if (!diskrule.pattern.has_value())
      {
        diskrule.set_message("Pattern is NULL.");
        saw_rule_error= true;
      }
      else if (!diskrule.replacement.has_value())
      {
        diskrule.set_message("Replacement is NULL.");
        saw_rule_error= true;
      }
      else
        saw_rule_error|= load_rule(session_thd, &diskrule);
      diskrule.write_to(&c);
    }
  }
  if (c.had_serious_read_error())
    m_refresh_status= REWRITER_ERROR_READ_FAILED;
  else if (saw_rule_error)
    m_refresh_status= REWRITER_ERROR_LOAD_FAILED;
  else
    m_refresh_status= REWRITER_OK;
  DBUG_VOID_RETURN;
}


namespace {

struct Refresh_callback_args
{
  Rewriter *me;
  MYSQL_THD session_thd;
};

extern "C"
void *refresh_callback(void *p_args)
{
  Refresh_callback_args *args= pointer_cast<Refresh_callback_args*>(p_args);
  (args->me->do_refresh)(args->session_thd);
  return NULL;
}

} // namespace


Rewriter::Load_status Rewriter::refresh(MYSQL_THD thd)
{
  services::Session session(thd);

  Refresh_callback_args args= { this, session.thd() };

  m_refresh_status= REWRITER_OK;

  my_thread_handle handle;
  mysql_parser_start_thread(session.thd(), refresh_callback, &args, &handle);

  mysql_parser_join_thread(&handle);

  return m_refresh_status;
}


Rewrite_result Rewriter::rewrite_query(MYSQL_THD thd, const uchar *key)
{
  HASH_SEARCH_STATE state;
  Rewrite_result result;

  Rule *rule= pointer_cast<Rule*>(my_hash_first(&m_digests, key,
                                                PARSER_SERVICE_DIGEST_LENGTH,
                                                &state));
  while (rule != NULL)
  {
    result.digest_matched= true;
    if (rule->matches(thd))
    {
      result= rule->create_new_query(thd);
      if (result.was_rewritten)
        return result;
    }
    rule= pointer_cast<Rule*>(my_hash_next(&m_digests, key,
                                           PARSER_SERVICE_DIGEST_LENGTH,
                                           &state));
  }

  result.was_rewritten= false;
  return result;
}
