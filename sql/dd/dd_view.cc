/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd_view.h"

#include "dd_table_share.h"                   // dd_get_mysql_charset
#include "log.h"                              // sql_print_error, sql_print_..
#include "parse_file.h"                       // PARSE_FILE_TIMESTAMPLENGTH
#include "sql_class.h"                        // THD
#include "transaction.h"                      // trans_commit

#include "dd/dd.h"                            // dd::get_dictionary
#include "dd/dictionary.h"                    // dd::Dictionary
#include "dd/properties.h"                    // dd::Properties
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/types/schema.h"                  // dd::Schema
#include "dd/types/view.h"                    // dd::View

namespace dd {

static ulonglong dd_get_old_view_check_type(dd::View::enum_check_option type)
{
  switch (type)
  {
  case dd::View::CO_NONE:
    return VIEW_CHECK_NONE;

  case dd::View::CO_LOCAL:
    return VIEW_CHECK_LOCAL;

  case dd::View::CO_CASCADED:
    return VIEW_CHECK_CASCADED;

  }

/* purecov: begin deadcode */
  sql_print_error("Error: Invalid view check option.");
  DBUG_ASSERT(false);

  return VIEW_CHECK_NONE;
/* purecov: end */
}


/** For enum in dd::View */
static dd::View::enum_check_option dd_get_new_view_check_type(ulonglong type)
{
  switch (type)
  {
  case VIEW_CHECK_NONE:
    return dd::View::CO_NONE;

  case VIEW_CHECK_LOCAL:
    return dd::View::CO_LOCAL;

  case VIEW_CHECK_CASCADED:
    return dd::View::CO_CASCADED;

  }

/* purecov: begin deadcode */
  sql_print_error("Error: Invalid view check option.");
  DBUG_ASSERT(false);

  return dd::View::CO_NONE;
/* purecov: end */
}


static enum enum_view_algorithm
dd_get_old_view_algorithm_type(dd::View::enum_algorithm type)
{
  switch (type)
  {
  case dd::View::VA_UNDEFINED:
    return VIEW_ALGORITHM_UNDEFINED;

  case dd::View::VA_TEMPORARY_TABLE:
    return VIEW_ALGORITHM_TEMPTABLE;

  case dd::View::VA_MERGE:
    return VIEW_ALGORITHM_MERGE;

  }

/* purecov: begin deadcode */
  sql_print_error("Error: Invalid view algorithm.");
  DBUG_ASSERT(false);

  return VIEW_ALGORITHM_UNDEFINED;
/* purecov: end */
}


static dd::View::enum_algorithm
dd_get_new_view_algorithm_type(enum enum_view_algorithm type)
{
  switch (type)
  {
  case VIEW_ALGORITHM_UNDEFINED:
    return dd::View::VA_UNDEFINED;

  case VIEW_ALGORITHM_TEMPTABLE:
    return dd::View::VA_TEMPORARY_TABLE;

  case VIEW_ALGORITHM_MERGE:
    return dd::View::VA_MERGE;

  }

/* purecov: begin deadcode */
  sql_print_error("Error: Invalid view algorithm.");
  DBUG_ASSERT(false);

  return dd::View::VA_UNDEFINED;
/* purecov: end */
}


static ulonglong
dd_get_old_view_security_type(dd::View::enum_security_type type)
{
  switch (type)
  {
  case dd::View::ST_DEFAULT:
    return VIEW_SUID_DEFAULT;

  case dd::View::ST_INVOKER:
    return VIEW_SUID_INVOKER;

  case dd::View::ST_DEFINER:
    return VIEW_SUID_DEFINER;

  }

/* purecov: begin deadcode */
  sql_print_error("Error: Invalid view security type.");
  DBUG_ASSERT(false);

  return VIEW_SUID_DEFAULT;
/* purecov: end */
}


static dd::View::enum_security_type
dd_get_new_view_security_type(ulonglong type)
{
  switch (type)
  {
  case VIEW_SUID_DEFAULT:
    return dd::View::ST_DEFAULT;

  case VIEW_SUID_INVOKER:
    return dd::View::ST_INVOKER;

  case VIEW_SUID_DEFINER:
    return dd::View::ST_DEFINER;

  }

/* purecov: begin deadcode */
  sql_print_error("Error: Invalid view security type.");
  DBUG_ASSERT(false);

  return dd::View::ST_DEFAULT;
/* purecov: end */
}


bool create_view(THD *thd,
                 TABLE_LIST *view,
                 const char *schema_name,
                 const char *view_name)
{
  dd::cache::Dictionary_client *client= thd->dd_client();

  // Check if the schema exists.
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  const dd::Schema *sch_obj= NULL;
  if (client->acquire<dd::Schema>(schema_name, &sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (!sch_obj)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name);
    return true;
  }

  // Create dd::View object.
  std::unique_ptr<dd::View> view_obj;
  if (dd::get_dictionary()->is_system_view_name(schema_name, view_name))
  {
    view_obj.reset(sch_obj->create_system_view(thd));
  }
  else
  {
    view_obj.reset(sch_obj->create_view(thd));
  }

  // View name.
  view_obj->set_name(view_name);

  // Set definer.
  view_obj->set_definer(view->definer.user.str, view->definer.host.str);

  // View definition.
  view_obj->set_definition(std::string(view->select_stmt.str,
                                       view->select_stmt.length));

  view_obj->set_definition_utf8(std::string(view->view_body_utf8.str,
                                            view->view_body_utf8.length));

  // Set updatable.
  view_obj->set_updatable(view->updatable_view);

  // Set check option.
  view_obj->set_check_option(dd_get_new_view_check_type(view->with_check));

  // Set algorithm.
  view_obj->set_algorithm(
              dd_get_new_view_algorithm_type(
                (enum enum_view_algorithm) view->algorithm));

  // Set security type.
  view_obj->set_security_type(
              dd_get_new_view_security_type(view->view_suid));

  // Assign client collation ID. The create option specifies character
  // set name, and we store the default collation id for this character set
  // name, which implicitly identifies the character set.
  const CHARSET_INFO *collation= NULL;
  if (resolve_charset(view->view_client_cs_name.str, system_charset_info,
                      &collation))
  {
    // resolve_charset will not cause an error to be reported if the
    // character set was not found, so we must report error here.
    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), view->view_client_cs_name.str);
    return true;
  }

  view_obj->set_client_collation_id(collation->number);

  // Assign connection collation ID.
  if (resolve_collation(view->view_connection_cl_name.str, system_charset_info,
                        &collation))
  {
    // resolve_charset will not cause an error to be reported if the
    // collation was not found, so we must report error here.
    my_error(ER_UNKNOWN_COLLATION, MYF(0), view->view_connection_cl_name.str);
    return true;
  }

  view_obj->set_connection_collation_id(collation->number);

  time_t tm= my_time(0);
  get_date(view->timestamp.str,
           GETDATE_DATE_TIME|GETDATE_GMT|GETDATE_FIXEDLENGTH,
           tm);
  view->timestamp.length= PARSE_FILE_TIMESTAMPLENGTH;

  dd::Properties *view_options= &view_obj->options();
  view_options->set("timestamp", std::string(view->timestamp.str,
                                             view->timestamp.length));

  Disable_gtid_state_update_guard disabler(thd);

  // Store info in DD views table.
  if (client->store(view_obj.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


void read_view(TABLE_LIST *view,
               const dd::View &view_obj,
               MEM_ROOT *mem_root)
{
  // Fill TABLE_LIST 'view' with view details.
  std::string definer_user= view_obj.definer_user();
  view->definer.user.length= definer_user.length();
  view->definer.user.str= (char*) strmake_root(mem_root,
                                               definer_user.c_str(),
                                               definer_user.length());

  std::string definer_host= view_obj.definer_host();
  view->definer.host.length= definer_host.length();
  view->definer.host.str= (char*) strmake_root(mem_root,
                                               definer_host.c_str(),
                                               definer_host.length());

  // View definition body.
  std::string vd_utf8= view_obj.definition_utf8();
  view->view_body_utf8.length= vd_utf8.length();
  view->view_body_utf8.str= (char*) strmake_root(mem_root,
                                                 vd_utf8.c_str(),
                                                 vd_utf8.length());

  // Get updatable.
  view->updatable_view= view_obj.is_updatable();

  // Get check option.
  view->with_check= dd_get_old_view_check_type(view_obj.check_option());

  // Get algorithm.
  view->algorithm= dd_get_old_view_algorithm_type(view_obj.algorithm());

  // Get security type.
  view->view_suid= dd_get_old_view_security_type(view_obj.security_type());

  // Get definition.
  std::string view_definition= view_obj.definition();
  view->select_stmt.length= view_definition.length();
  view->select_stmt.str= (char*) strmake_root(mem_root,
                                         view_definition.c_str(),
                                         view_definition.length());

  // Get view_client_cs_name. Note that this is the character set name.
  CHARSET_INFO *collation= dd_get_mysql_charset(view_obj.client_collation_id());
  DBUG_ASSERT(collation);
  view->view_client_cs_name.length= strlen(collation->csname);
  view->view_client_cs_name.str= strdup_root(mem_root, collation->csname);

  // Get view_connection_cl_name. Note that this is the collation name.
  collation= dd_get_mysql_charset(view_obj.connection_collation_id());
  DBUG_ASSERT(collation);
  view->view_connection_cl_name.length= strlen(collation->name);
  view->view_connection_cl_name.str= strdup_root(mem_root, collation->name);
}

} // namespace dd
