/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "mysql/plugin_query_rewrite.h"
#include "mysql/service_rules_table.h"
#include "sql_cache.h"
#include "sql_error.h"
#include "sql_parse.h"
#include "sql_plugin.h"
#include "sql_query_rewrite.h"
#include "log.h"
#include "sql_base.h"
#include "sql_class.h"

class THD;

#ifndef EMBEDDED_LIBRARY
static void raise_query_rewritten_note(THD *thd,
                                       const char *original_query,
                                       const char *rewritten_query,
                                       const char *plugin_name)
{
  Sql_condition::enum_severity_level sl= Sql_condition::SL_NOTE;
  const char *message= "Query '%s' rewritten to '%s' by plugin: %s.";
  push_warning_printf(thd, sl, ER_UNKNOWN_ERROR, message,
                      original_query, rewritten_query,
                      plugin_name);
};


/**
  Rewrites a text query by calling the plugin's rewrite method.

  @param[in] thd
  @param[in] plugin
  @param[in] arg Not used here, but this function needs to conform to the
  plugin_foreach_func typedef.

  @retval FALSE always
*/
static my_bool rewrite_query_pre_parse(THD *thd, plugin_ref ref, void *)
{
  st_mysql_rewrite_pre_parse *plugin=
    plugin_data<st_mysql_rewrite_pre_parse*>(ref);

  st_mysql_plugin *plug= plugin_decl(ref);

  Mysql_rewrite_pre_parse_param param= {
    0, // flags
    thd,
    NULL, // data
    thd->query().str,
    thd->query().length,
    NULL, // rewritten_query
    0     // rewritten_query_length
  };
  plugin->rewrite(&param);
  if (param.flags & FLAG_REWRITE_PLUGIN_QUERY_REWRITTEN)
  {
    // It is a rewrite fulltext plugin and we need a rewrite we must have
    // generated a new query then.
    DBUG_ASSERT(param.rewritten_query != NULL &&
                param.rewritten_query_length > 0);
    raise_query_rewritten_note(thd, thd->query().str, param.rewritten_query,
                               plug->name);
    alloc_query(thd, param.rewritten_query, param.rewritten_query_length);
    thd->m_parser_state->init(thd, thd->query().str, thd->query().length);
  }
  if (plugin->deinit)
    plugin->deinit(&param);
  return FALSE;
}


/**
  Asks this post-parse query rewrite plugin if it needs digest to be
  calculated during parsing.

  @param plugin_ref The plugin, assumed to be a post-parse query rewrite
  plugin.

  @param parser_state_void Assumed to be a Parser_state*.
*/
static my_bool enable_digest_if_plugin_needs_it(THD *, plugin_ref plugin_ref,
                                                void *parser_state_void)
{
  const st_mysql_rewrite_post_parse *plugin=
    plugin_data<st_mysql_rewrite_post_parse*>(plugin_ref);

  Parser_state *ps= static_cast<Parser_state*>(parser_state_void);

  if (plugin->needs_statement_digest)
    ps->m_input.m_compute_digest= true;

  return 0;
}


/**
  Rewrites a parsed query by calling the plugin's rewrite function.

  @param[in] thd
  @param[in] plugin
  @param[in] is_prepared_ptr Expected to be a bool* saying whether the query
  is a prepared statement.

  @retval false Success.
  @retval true Error.
*/
static my_bool rewrite_query_post_parse(THD *thd, plugin_ref ref,
                                        void* is_prepared_ptr)
{
  bool is_prepared= *static_cast<bool*>(is_prepared_ptr);
  st_mysql_rewrite_post_parse *plugin=
    plugin_data<st_mysql_rewrite_post_parse*>(ref);

  st_mysql_plugin *descriptor= plugin_decl(ref);
  Mysql_rewrite_post_parse_param param= {
    is_prepared ? FLAG_REWRITE_PLUGIN_IS_PREPARED_STATEMENT : 0,
    thd,
    NULL, // data
  };

  /*
    The actual query string is allocated in a mem_root, so we can trust that
    it will be available even after query rewrite. We just copy the struct
    pointing to it.
  */
  const LEX_CSTRING original_query= thd->query();
  my_bool err= plugin->rewrite(&param);

  if (param.flags & FLAG_REWRITE_PLUGIN_QUERY_REWRITTEN)
  {
    raise_query_rewritten_note(thd, original_query.str, thd->query().str,
                               descriptor->name);
    thd->lex->safe_to_cache_query= false;
  }

  return err;
}


void invoke_pre_parse_rewrite_plugins(THD *thd)
{
  Diagnostics_area *plugin_da= thd->get_query_rewrite_plugin_da();
  if (plugin_da == NULL)
    return;
  plugin_da->reset_condition_info(thd);
  Diagnostics_area *da= thd->get_parser_da();
  thd->push_diagnostics_area(plugin_da, false);

  plugin_foreach(thd,
                 rewrite_query_pre_parse,
                 MYSQL_REWRITE_PRE_PARSE_PLUGIN,
                 NULL);

  da->copy_sql_conditions_from_da(thd, plugin_da);

  thd->pop_diagnostics_area();
}


void enable_digest_if_any_plugin_needs_it(THD *thd, Parser_state *ps)
{
  plugin_foreach(thd, enable_digest_if_plugin_needs_it,
                 MYSQL_REWRITE_POST_PARSE_PLUGIN, ps);
}


bool invoke_post_parse_rewrite_plugins(THD *thd, my_bool is_prepared)
{
  Diagnostics_area *plugin_da= thd->get_query_rewrite_plugin_da();
  plugin_da->reset_diagnostics_area();
  plugin_da->reset_condition_info(thd);

  Diagnostics_area *stmt_da= thd->get_stmt_da();

  /*
    We save the value of keep_diagnostics here as it gets reset by
    push_diagnostics_area(), see below for use.
  */
  bool keeping_diagnostics= thd->lex->keep_diagnostics == DA_KEEP_PARSE_ERROR;

  thd->push_diagnostics_area(plugin_da, false);

  {
    /*
      We have to call a function in rules_table_service.cc, or the service
      won't be visible to plugins.
    */
#ifndef DBUG_OFF
    int dummy=
#endif
      rules_table_service::
      dummy_function_to_ensure_we_are_linked_into_the_server();
    DBUG_ASSERT(dummy == 1);
  }

  bool err= plugin_foreach(thd,
                           rewrite_query_post_parse,
                           MYSQL_REWRITE_POST_PARSE_PLUGIN,
                           &is_prepared);

  if (plugin_da->current_statement_cond_count() != 0)
  {
    /*
      A plugin raised at least one condition. At this point these are in the
      plugin DA, and we should copy them to the statement DA. But before we do
      that, we may have to clear it as this DA may contain conditions from the
      previous statement. We have to clear it *unless* the statement is a
      diagnostics statement, in which case we keep everything: conditions from
      previous statements, parser conditions and plugin conditions. If this is
      not a diagnostics statement, parse_sql() has already cleared the
      statement DA, copied the parser condtitions to the statement DA and set
      DA_KEEP_PARSE_ERROR. So we arrive at the below condition for telling us
      when to clear the statement DA.
    */
    if (thd->lex->sql_command != SQLCOM_SHOW_WARNS && !keeping_diagnostics)
      stmt_da->reset_condition_info(thd);

    /* We need to put any errors in the DA as well as the condition list. */
    if (plugin_da->is_error())
      stmt_da->set_error_status(plugin_da->mysql_errno(),
                                plugin_da->message_text(),
                                plugin_da->returned_sqlstate());

    stmt_da->copy_sql_conditions_from_da(thd, plugin_da);

    /*
      Do not clear the condition list when starting execution as it now
      contains not the results of the previous executions, but a non-zero
      number of errors/warnings thrown during parsing or plugin execution.
    */
    thd->lex->keep_diagnostics= DA_KEEP_PARSE_ERROR;
  }

  thd->pop_diagnostics_area();

  return err;
}


/**
  Initializes a query rewrite plugin.

  @tparam Plugin_type The type of the plugin struct.

  @param[in] plugin

  @retval FALSE OK
  @retval TRUE There was an error.
*/
template <typename Plugin_type>
int initialize_rewrite_plugin(st_plugin_int *plugin_handle)
{
  DBUG_ENTER("initialize_rewrite_plugin");

  Plugin_type *plugin= static_cast<Plugin_type*>(plugin_handle->plugin->info);

  if (plugin->rewrite == NULL)
  {
    sql_print_error("Plugin: '%s' can't create a query rewrite plugin "
                    "without a rewrite function.",
                    plugin_handle->name.str);
    DBUG_RETURN(1);
  }

  /* Launch the plugin's init function */
  int err= 0;
  if (plugin_handle->plugin->init != NULL)
  {
    DBUG_PRINT("info", ("Initializing plugin: '%s'", plugin_handle->name.str));
    if ((err= plugin_handle->plugin->init(plugin_handle)))
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin_handle->name.str);

  }

  /* Make the plugin interface easy to access */
  plugin_handle->data= plugin;

  DBUG_RETURN(err);
}

int initialize_rewrite_pre_parse_plugin(st_plugin_int *plugin)
{
  return initialize_rewrite_plugin<st_mysql_rewrite_pre_parse>(plugin);
}

int initialize_rewrite_post_parse_plugin(st_plugin_int *plugin)
{
  return initialize_rewrite_plugin<st_mysql_rewrite_post_parse>(plugin);
}


/**
  Finalizes a query rewrite plugin.

  @param[in] The plugin.

  @retval 0   OK
  @retval !=0 There was an error.
*/
int finalize_rewrite_plugin(st_plugin_int *plugin)
{
  DBUG_ENTER("finalize_rewrite_pre_parse_plugin");
  int err= 0;

  /* Launch the plugins' deinit function */
  if (plugin->plugin->deinit)
  {
    DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
    if ((err= plugin->plugin->deinit && plugin->plugin->deinit(plugin)))
      DBUG_PRINT("warning", ("Plugin '%s' deinit function returned an error.",
                             plugin->name.str));
  }
  plugin->data= NULL;
  DBUG_RETURN(err);
}


#else /* EMBEDDED_LIBRARY */

void invoke_pre_parse_rewrite_plugins(THD *thd) {}

void enable_digest_if_any_plugin_needs_it(THD *thd, Parser_state *ps) {}

bool invoke_post_parse_rewrite_plugins(THD *thd, my_bool is_prepared)
{
  return false;
}

int initialize_rewrite_pre_parse_plugin(st_plugin_int *plugin)
{
  return 1;
}

int initialize_rewrite_post_parse_plugin(st_plugin_int *plugin)
{
  return 1;
}

int finalize_rewrite_plugin(st_plugin_int *plugin)
{
  return 1;
}

#endif /* EMBEDDED_LIBRARY */
