/*  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "rewriter_plugin.h"
#include "rule.h" // Rewrite_result
#include "rewriter.h"

#include <my_global.h>
#include <mysql/plugin_audit.h>
#include <mysql/psi/mysql_thread.h>
#include <mysql/service_my_plugin_log.h>
#include <my_atomic.h>
#include <my_sys.h>
#include "services.h"
#include "mysqld_error.h"
#include "template_utils.h"
#include <algorithm>

using std::string;

/**
  @file rewriter_plugin.cc

  The plugin declaration part of the Rewriter plugin. This file (alone)
  creates a post-parse query rewrite plugin. All of the functionality is
  brought in by including rewriter.h.
*/

/**
  We handle recovery in a kind of lazy manner. If the server was shut down
  with this plugin installed, we should ideally load the rules into memory
  right on the spot in rewriter_plugin_init() as the server comes to. But this
  plugin is relying on transactions to do that and unfortunately the
  transaction log is not set up at this point. (It's set up about 200 lines
  further down in init_server_components() at the time of writing.) That's why
  we simply set this flag in rewriter_plugin_init() and reload in
  rewrite_query_post_parse() if it is set.
*/
static bool needs_initial_load;

static const char *MESSAGE_OOM= "Out of memory.";

static const size_t MAX_QUERY_LENGTH_IN_LOG= 100;

static MYSQL_PLUGIN plugin_info;

static mysql_rwlock_t LOCK_table;
static Rewriter* rewriter;

/// @name Status variables for the plugin.
///@{

/// Number of queries that were rewritten.
static long long status_var_number_rewritten_queries;

/// Indicates if there was an error during the last reload.
static bool status_var_reload_error;

/// Number of rules loaded in the in-memory table.
static unsigned status_var_number_loaded_rules;

/// Number of times the in-memory table was reloaded.
static long long status_var_number_reloads;

#define PLUGIN_NAME "Rewriter"

static st_mysql_show_var rewriter_plugin_status_vars[]=
{
  {PLUGIN_NAME "_number_rewritten_queries",
     pointer_cast<char*>(&status_var_number_rewritten_queries), SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {PLUGIN_NAME "_reload_error",
     pointer_cast<char*>(&status_var_reload_error), SHOW_BOOL, SHOW_SCOPE_GLOBAL},
  {PLUGIN_NAME "_number_loaded_rules",
     pointer_cast<char*>(&status_var_number_loaded_rules), SHOW_INT, SHOW_SCOPE_GLOBAL},
  {PLUGIN_NAME "_number_reloads",
     pointer_cast<char*>(&status_var_number_reloads), SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
  {0, 0, SHOW_BOOL, SHOW_SCOPE_GLOBAL}
};


///@}


/// Verbose level.
static int sys_var_verbose;

/// Enabled.
static my_bool sys_var_enabled;

/// Updater function for the status variable ..._verbose.
static void update_verbose(MYSQL_THD, struct st_mysql_sys_var *, void *,
                           const void *save)
{
  sys_var_verbose= *static_cast<const int*>(save);
}

/// Updater function for the status variable ..._enabled.
static void update_enabled(MYSQL_THD, struct st_mysql_sys_var *, void *,
                           const void *value)
{
  sys_var_enabled= *static_cast<const int*>(value);
}


static MYSQL_SYSVAR_INT(verbose,              // Name.
                        sys_var_verbose,      // Variable.
                        PLUGIN_VAR_NOCMDARG,  // Not a command-line argument.
                        "Tells " PLUGIN_NAME " how verbose it should be.",
                        NULL,                 // Check function.
                        update_verbose,       // Update function.
                        1,                    // Default value.
                        0,                    // Min value.
                        2,                    // Max value.
                        1                     // Block size.
);

static MYSQL_SYSVAR_BOOL(enabled,              // Name.
                         sys_var_enabled,      // Variable.
                         PLUGIN_VAR_NOCMDARG,  // Not a command-line argument.
                         "Whether queries should actually be rewritten.",
                         NULL,                 // Check function.
                         update_enabled,       // Update function.
                         1                     // Default value.
);

struct st_mysql_sys_var* rewriter_plugin_sys_vars[]=
{
  MYSQL_SYSVAR(verbose),
  MYSQL_SYSVAR(enabled),
  NULL
};

MYSQL_PLUGIN get_rewriter_plugin_info() { return plugin_info; }

/// @name Plugin declaration.
///@{

static int rewrite_query_notify(MYSQL_THD thd,
                                mysql_event_class_t event_class,
                                const void *event);
static int rewriter_plugin_init(MYSQL_PLUGIN plugin_ref);
static int rewriter_plugin_deinit(void*);

/* Audit plugin descriptor */
static struct st_mysql_audit rewrite_query_descriptor=
{
  MYSQL_AUDIT_INTERFACE_VERSION,                    /* interface version */
  NULL,                                             /* release_thd()     */
  rewrite_query_notify,                             /* event_notify()    */
  { 0,
    0,
    (unsigned long) MYSQL_AUDIT_PARSE_ALL, }        /* class mask        */
};

/* Plugin descriptor */
mysql_declare_plugin(audit_log)
{
    MYSQL_AUDIT_PLUGIN,             /* plugin type                   */
    &rewrite_query_descriptor,      /* type specific descriptor      */
    PLUGIN_NAME,                    /* plugin name                   */
    "Oracle",                       /* author                        */
    "A query rewrite plugin that"
    " rewrites queries using the"
    " parse tree.",                 /* description                   */
    PLUGIN_LICENSE_GPL,             /* license                       */
    rewriter_plugin_init,           /* plugin initializer            */
    rewriter_plugin_deinit,         /* plugin deinitializer          */
    0x0002,                         /* version                       */
    rewriter_plugin_status_vars,    /* status variables              */
    rewriter_plugin_sys_vars,       /* system variables              */
    NULL,                           /* reserverd                     */
    0                               /* flags                         */
}
mysql_declare_plugin_end;

///@}

#ifdef HAVE_PSI_INTERFACE
PSI_rwlock_key key_rwlock_LOCK_table_;

static PSI_rwlock_info all_rewrite_rwlocks[]=
{
  { &key_rwlock_LOCK_table_, "LOCK_plugin_rewriter_table_", 0 }
};


static void init_rewriter_psi_keys()
{
  const char* category= "rewriter";
  int count;

  count= array_elements(all_rewrite_rwlocks);
  mysql_rwlock_register(category, all_rewrite_rwlocks, count);
}
#endif


static int rewriter_plugin_init(MYSQL_PLUGIN plugin_ref)
{
#ifdef HAVE_PSI_INTERFACE
  init_rewriter_psi_keys();
#endif
  mysql_rwlock_init(key_rwlock_LOCK_table_, &LOCK_table);
  plugin_info= plugin_ref;
  status_var_number_rewritten_queries= 0;
  status_var_reload_error= false;
  status_var_number_loaded_rules= 0;
  status_var_number_reloads= 0;

  rewriter= new Rewriter();
  /*
    We don't need to protect this with a mutex here. This function is called
    by a single thread when loading the plugin or starting up the server.
  */
  needs_initial_load= true;
  return 0;
}

static int rewriter_plugin_deinit(void*)
{
  plugin_info= NULL;
  delete rewriter;
  mysql_rwlock_destroy(&LOCK_table);
  return 0;
}


/**
  Reloads the rules into the in-memory table. This function assumes that the
  appropriate lock is already taken and doesn't concern itself with locks.
*/
bool reload(MYSQL_THD thd)
{
  const char *message= NULL;
  try {
    switch (rewriter->refresh(thd))
    {
    case Rewriter::REWRITER_OK:
      return false;
    case Rewriter::REWRITER_ERROR_TABLE_MALFORMED:
      message= "Wrong column count or names when loading rules.";
      break;
    case Rewriter::REWRITER_ERROR_LOAD_FAILED:
      message= "Some rules failed to load.";
      break;
    case Rewriter::REWRITER_ERROR_READ_FAILED:
      message= "Got error from storage engine while refreshing rewrite rules.";
      break;
    }
  }
  catch (const std::bad_alloc &ba)
  {
    message= MESSAGE_OOM;
  }
  DBUG_ASSERT(message != NULL);
  my_plugin_log_message(&plugin_info, MY_ERROR_LEVEL, "%s", message);
  return true;
}


bool lock_and_reload(MYSQL_THD thd)
{
  mysql_rwlock_wrlock(&LOCK_table);
  status_var_reload_error= reload(thd);
  status_var_number_loaded_rules= rewriter->get_number_loaded_rules();
  ++status_var_number_reloads;
  needs_initial_load= false;
  mysql_rwlock_unlock(&LOCK_table);

  return status_var_reload_error;
}


bool refresh_rules_table()
{
  MYSQL_THD thd= mysql_parser_current_session();
  return lock_and_reload(thd);
}


static string shorten_query(MYSQL_LEX_STRING query)
{
  static const string ellipsis = "...";
  string shortened_query(query.str,
                         std::min(query.length, MAX_QUERY_LENGTH_IN_LOG));
  if (query.length > MAX_QUERY_LENGTH_IN_LOG)
    shortened_query+= ellipsis;
  return shortened_query;
}


/**
  Writes a message in the log saying that the query did not get
  rewritten. (Only if verbose level is high enough.)
*/
static void log_nonrewritten_query(MYSQL_THD thd, const uchar *digest_buf,
                                   Rewrite_result result)
{
  if (sys_var_verbose >= 2)
  {
    string query= shorten_query(mysql_parser_get_query(thd));
    string digest= services::print_digest(digest_buf);
    string message;
    message.append("Statement \"");
    message.append(query);
    message.append("\" with digest \"");
    message.append(digest);
    message.append("\" ");
    if (result.digest_matched)
      message.append("matched some rule but had different parse tree and/or "
                     "literals.");
    else
      message.append("did not match any rule.");
    my_plugin_log_message(&plugin_info, MY_INFORMATION_LEVEL, "%s", message.c_str());
  }
}


/**
  Entry point to the plugin. The server calls this function after each parsed
  query when the plugin is active. The function extracts the digest of the
  query. If the digest matches an existing rewrite rule, it is executed.
*/
static int rewrite_query_notify(MYSQL_THD thd, mysql_event_class_t event_class,
                                 const void *event)
{
  DBUG_ASSERT(event_class == MYSQL_AUDIT_PARSE_CLASS);

  const struct mysql_event_parse *event_parse=
    static_cast<const struct mysql_event_parse *>(event);

  if (event_parse->event_subclass != MYSQL_AUDIT_PARSE_POSTPARSE ||
      !sys_var_enabled)
    return 0;

  uchar digest[PARSER_SERVICE_DIGEST_LENGTH];

  if (mysql_parser_get_statement_digest(thd, digest))
    return 0;

  if (needs_initial_load)
    lock_and_reload(thd);

  mysql_rwlock_rdlock(&LOCK_table);

  Rewrite_result rewrite_result;
  try
  {
    rewrite_result= rewriter->rewrite_query(thd, digest);
  }
  catch (std::bad_alloc &ba)
  {
    my_plugin_log_message(&plugin_info, MY_ERROR_LEVEL, "%s", MESSAGE_OOM);
  }

  mysql_rwlock_unlock(&LOCK_table);

  int parse_error= 0;
  if (!rewrite_result.was_rewritten)
    log_nonrewritten_query(thd, digest, rewrite_result);
  else
  {
    *((int *)event_parse->flags)|=
                        (int)MYSQL_AUDIT_PARSE_REWRITE_PLUGIN_QUERY_REWRITTEN;
    bool is_prepared=
      (*(event_parse->flags) & MYSQL_AUDIT_PARSE_REWRITE_PLUGIN_IS_PREPARED_STATEMENT) != 0;

    parse_error= services::parse(thd, rewrite_result.new_query, is_prepared);
    if (parse_error != 0)
      my_plugin_log_message(&plugin_info, MY_ERROR_LEVEL,
                            "Rewritten query failed to parse:%s\n",
                            mysql_parser_get_query(thd).str);

    my_atomic_add64(&status_var_number_rewritten_queries, 1);
  }

  return 0;
}
