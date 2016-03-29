/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "dd_routine.h"                        // Routine methods

#include "dd_table.h"                          // dd_get_new_field_type
#include "sp_head.h"                           // sp_head
#include "sp_pcontext.h"                       // sp_variable
#include "sql_db.h"                            // get_default_db_collation
#include "transaction.h"                       // trans_commit
#include "tztime.h"                            // Time_zone

#include "dd/properties.h"                     // dd::Properties
#include "dd/cache/dictionary_client.h"        // dd::cache::Dictionary_client
#include "dd/types/function.h"                 // dd::Function
#include "dd/types/parameter.h"                // dd::Parameter
#include "dd/types/parameter_type_element.h"   // dd::Parameter_type_element
#include "dd/types/procedure.h"                // dd::Procedure
#include "dd/types/schema.h"                   // dd::Schema

namespace dd {

////////////////////////////////////////////////////////////////////////////////

enum_sp_return_code find_routine(cache::Dictionary_client *dd_client,
                                 sp_name *name, enum_sp_type type,
                                 const Routine **routine)
{
  DBUG_ENTER("dd::find_routine");

  if (type == enum_sp_type::FUNCTION)
  {
    if (dd_client->acquire<dd::Function>(name->m_db.str, name->m_name.str,
                                         routine))
      DBUG_RETURN(SP_INTERNAL_ERROR);
  }
  else
  {
    if (dd_client->acquire<dd::Procedure>(name->m_db.str, name->m_name.str,
                                          routine))
      DBUG_RETURN(SP_INTERNAL_ERROR);
  }

  if (*routine == NULL)
    DBUG_RETURN(SP_DOES_NOT_EXISTS);

  DBUG_RETURN(SP_OK);
}

////////////////////////////////////////////////////////////////////////////////

/**
  Helper method to get numeric scale for types using Create_field type object.

  @param[in]  field      Field object.
  @param[out] scale      numeric scale value for types.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool get_field_numeric_scale(Create_field *field, uint *scale)
{
  bool error= false;

  DBUG_ASSERT(*scale == 0);

  switch (field->sql_type)
  {
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    /* For these types we show NULL in I_S if scale was not given. */
    if (field->decimals != NOT_FIXED_DEC)
      *scale= field->decimals;
    else
      error= true;
    break;
  case MYSQL_TYPE_NEWDECIMAL:
  case MYSQL_TYPE_DECIMAL:
    *scale= field->decimals;
    break;
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONGLONG:
    DBUG_ASSERT(field->decimals == 0);
    break;
  default:
    error= true;
  }

  return error;
}

////////////////////////////////////////////////////////////////////////////////

/**
  Helper method for create_routine() to fill return type information of stored
  routine from the sp_head.
  from the sp_head.

  @param[in]  thd        Thread handle.
  @param[in]  sp         Stored routine object.
  @param[out] sf         dd::Function object.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool fill_dd_function_return_type(THD *thd, sp_head *sp, Function *sf)
{
  DBUG_ENTER("fill_dd_function_return_type");

  Create_field *return_field= &sp->m_return_field_def;
  DBUG_ASSERT(return_field != NULL);

  // Set result data type.
  sf->set_result_data_type(dd_get_new_field_type(return_field->sql_type));

  // Set result is_zerofill flag.
  sf->set_result_zerofill(return_field->is_zerofill);

  // Set result is_unsigned flag.
  sf->set_result_unsigned(return_field->is_unsigned);

  // set result char length.
  sf->set_result_char_length(return_field->length);

  // Set result numric scale.
  uint scale= 0;
  if (!get_field_numeric_scale(return_field, &scale))
    sf->set_result_numeric_scale(scale);
  else
    DBUG_ASSERT(sf->is_result_numeric_scale_null());

  // Set result collation id.
  sf->set_result_collation_id(return_field->charset->number);

  DBUG_RETURN(false);
}

////////////////////////////////////////////////////////////////////////////////

/**
  Helper method for create_routine() to fill parameter information
  from the object of type Create_field.
  Method is called by the fill_routine_parameters_info().

  @param[in]  field    Object of type Create_field.
  @param[out] param    Parameter object to be filled using the state of field
                       object.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool fill_parameter_info_from_field(Create_field *field,
                                           dd::Parameter *param)
{
  DBUG_ENTER("fill_parameter_info_from_field");

  // Set data type.
  param->set_data_type(dd_get_new_field_type(field->sql_type));

  // Set is_zerofill flag.
  param->set_zerofill(field->is_zerofill);

  // Set is_unsigned flag.
  param->set_unsigned(field->is_unsigned);

  // Set char length.
  param->set_char_length(field->length);

  // Set numeric scale.
  uint scale= 0;
  if (!get_field_numeric_scale(field, &scale))
    param->set_numeric_scale(scale);
  else
    DBUG_ASSERT(param->is_numeric_scale_null());

  // Set geometry sub type
  if (field->sql_type == MYSQL_TYPE_GEOMETRY)
  {
    Properties *param_options= &param->options();
    param_options->set_uint32("geom_type", field->geom_type);
  }

  // Set elements of enum or set data type.
  if (field->interval)
  {
    DBUG_ASSERT(field->sql_type == MYSQL_TYPE_ENUM ||
                field->sql_type == MYSQL_TYPE_SET);

    const char **pos= field->interval->type_names;
    for (uint i=0; *pos != NULL; pos++, i++)
    {
      // Create enum/set object.
      Parameter_type_element  *elem_obj= NULL;

      if (field->sql_type == MYSQL_TYPE_ENUM)
        elem_obj= param->add_enum_element();
      else if (field->sql_type == MYSQL_TYPE_SET)
        elem_obj= param->add_set_element();

      std::string interval_name(*pos, field->interval->type_lengths[i]);

      elem_obj->set_name(interval_name);
    }
  }

  // Set collation id.
  param->set_collation_id(field->charset->number);

  DBUG_RETURN(false);
}

////////////////////////////////////////////////////////////////////////////////

/**
  Helper method for create_routine() to fill parameters of routine to
  dd::Routine object from the sp_head.
  Method is called from the fill_dd_routine_info().

  @param[in]  thd        Thread handle.
  @param[in]  sp         Stored routine object.
  @param[out] routine    dd::Routine object prepared from sp_head.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool fill_routine_parameters_info(THD *thd, sp_head *sp,
                                         Routine *routine)
{
  DBUG_ENTER("fill_routine_parameters_info");

  /*
    The return type of the stored function is listed as first parameter from
    the Information_schema.parameters. Storing return type as first parameter
    for the stored functions.
  */
  if (sp->m_type == enum_sp_type::FUNCTION)
  {
    // Add parameter.
    dd::Parameter *param= routine->add_parameter();

    // Fill return type information.
    fill_parameter_info_from_field(&sp->m_return_field_def, param);
  }

  // Fill parameter information of the stored routine.
  sp_pcontext *sp_root_parsing_ctx= sp->get_root_parsing_context();
  DBUG_ASSERT(sp_root_parsing_ctx != NULL);
  for (uint i= 0; i < sp_root_parsing_ctx->context_var_count(); i++)
  {
    sp_variable *sp_var= sp_root_parsing_ctx->find_variable(i);
    Create_field *field_def= &sp_var->field_def;

    // Add parameter.
    dd::Parameter *param= routine->add_parameter();

    // Set parameter name.
    param->set_name(sp_var->name.str);

    // Set parameter mode.
    Parameter::enum_parameter_mode mode;
    switch (sp_var->mode)
    {
    case sp_variable::MODE_IN:
      mode= Parameter::PM_IN;
      break;
    case sp_variable::MODE_OUT:
      mode= Parameter::PM_OUT;
      break;
    case sp_variable::MODE_INOUT:
      mode= Parameter::PM_INOUT;
      break;
    default:
      DBUG_ASSERT(false); /* purecov: deadcode */
      DBUG_RETURN(true);  /* purecov: deadcode */
    }
    param->set_mode(mode);

    // Fill return type information.
    fill_parameter_info_from_field(field_def, param);
  }

  DBUG_RETURN(false);
}

////////////////////////////////////////////////////////////////////////////////

/**
  Helper method for create_routine() to prepare dd::Routine object
  from the sp_head.

  @param[in]  thd        Thread handle.
  @param[in]  sp         Stored routine object.
  @param[out] routine    dd::Routine object to be prepared from the sp_head.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool fill_dd_routine_info(THD *thd, sp_head *sp, Routine *routine)
{
  DBUG_ENTER("fill_dd_routine_info");

  // Set name.
  routine->set_name(sp->m_name.str);

  // Set definition.
  routine->set_definition(sp->m_body.str);

  // Set definition_utf8.
  routine->set_definition_utf8(sp->m_body_utf8.str);

  // Set parameter str for show routine operations.
  routine->set_parameter_str(sp->m_params.str);

  // Set is_deterministic.
  routine->set_deterministic(sp->m_chistics->detistic);

  // Set SQL data access.
  Routine::enum_sql_data_access daccess;
  enum_sp_data_access sp_daccess=
    (sp->m_chistics->daccess == SP_DEFAULT_ACCESS) ?
      SP_DEFAULT_ACCESS_MAPPING : sp->m_chistics->daccess;
  switch (sp_daccess)
  {
  case SP_NO_SQL:
    daccess= Routine::SDA_NO_SQL;
    break;
  case SP_CONTAINS_SQL:
    daccess= Routine::SDA_CONTAINS_SQL;
    break;
  case SP_READS_SQL_DATA:
    daccess= Routine::SDA_READS_SQL_DATA;
    break;
  case SP_MODIFIES_SQL_DATA:
    daccess= Routine::SDA_MODIFIES_SQL_DATA;
    break;
  default:
    DBUG_ASSERT(false); /* purecov: deadcode */
    DBUG_RETURN(true);  /* purecov: deadcode */
  }
  routine->set_sql_data_access(daccess);

  // Set security type.
  View::enum_security_type sec_type;
  enum_sp_suid_behaviour sp_suid=
    (sp->m_chistics->suid == SP_IS_DEFAULT_SUID) ?
      SP_DEFAULT_SUID_MAPPING : sp->m_chistics->suid;
  switch (sp_suid)
  {
  case SP_IS_SUID:
    sec_type= View::ST_DEFINER;
    break;
  case SP_IS_NOT_SUID:
    sec_type= View::ST_INVOKER;
    break;
  default:
    DBUG_ASSERT(false); /* purecov: deadcode */
    DBUG_RETURN(true);  /* purecov: deadcode */
  }
  routine->set_security_type(sec_type);

  // Set definer.
  routine->set_definer(thd->lex->definer->user.str,
                       thd->lex->definer->host.str);

  // Set sql_mode.
  routine->set_sql_mode(thd->variables.sql_mode);

  // Set client collation id.
  routine->set_client_collation_id(thd->charset()->number);

  // Set connection collation id.
  routine->set_connection_collation_id(
    thd->variables.collation_connection->number);

  // Set schema collation id.
  const CHARSET_INFO *db_cs= NULL;
  if (get_default_db_collation(thd, sp->m_db.str, &db_cs))
  {
    DBUG_ASSERT(thd->is_error());
    DBUG_ASSERT(true);
  }
  if (db_cs == NULL)
    db_cs= thd->collation();

  routine->set_schema_collation_id(db_cs->number);

  // Set comment.
  routine->set_comment(
    sp->m_chistics->comment.str ? sp->m_chistics->comment.str : "");

  // Fill routine parameters
  DBUG_RETURN(fill_routine_parameters_info(thd, sp, routine));
}

////////////////////////////////////////////////////////////////////////////////

enum_sp_return_code create_routine(THD *thd, const Schema *schema, sp_head *sp)
{
  DBUG_ENTER("dd::create_routine");

  bool error= false;
  // Create Function or Procedure object.
  if (sp->m_type == enum_sp_type::FUNCTION)
  {
    std::unique_ptr<Function> func(schema->create_function(thd));

    // Fill stored function return type.
    if (fill_dd_function_return_type(thd, sp, func.get()))
      DBUG_RETURN(SP_STORE_FAILED);

    // Fill routine object.
    if (fill_dd_routine_info(thd, sp, func.get()))
      DBUG_RETURN(SP_STORE_FAILED);

    // Store routine metadata in DD table.
    enum_check_fields saved_count_cuted_fields= thd->count_cuted_fields;
    thd->count_cuted_fields= CHECK_FIELD_WARN;
    error= thd->dd_client()->store(func.get());
    thd->count_cuted_fields= saved_count_cuted_fields;
  }
  else
  {
    std::unique_ptr<Procedure> proc(schema->create_procedure(thd));

    // Fill routine object.
    if (fill_dd_routine_info(thd, sp, proc.get()))
      DBUG_RETURN(SP_STORE_FAILED);

    // Store routine metadata in DD table.
    enum_check_fields saved_count_cuted_fields= thd->count_cuted_fields;
    thd->count_cuted_fields= CHECK_FIELD_WARN;
    error= thd->dd_client()->store(proc.get());
    thd->count_cuted_fields= saved_count_cuted_fields;
  }

  if (error)
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(SP_STORE_FAILED);
  }

  error= (trans_commit_stmt(thd) || trans_commit(thd));
  DBUG_RETURN(error ? SP_INTERNAL_ERROR : SP_OK);
}

////////////////////////////////////////////////////////////////////////////////

enum_sp_return_code remove_routine(THD *thd, const Routine *routine)
{
  DBUG_ENTER("dd::remove_routine");

  // Drop routine.
  if(thd->dd_client()->drop(routine))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(SP_DROP_FAILED);
  }

  bool error= (trans_commit_stmt(thd) || trans_commit(thd));
  DBUG_RETURN(error ? SP_INTERNAL_ERROR : SP_OK);
}

////////////////////////////////////////////////////////////////////////////////

enum_sp_return_code alter_routine(THD *thd, const Routine *routine,
                                  st_sp_chistics *chistics)
{
  DBUG_ENTER("dd::alter_routine");

  cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  std::unique_ptr<Routine> new_routine(routine->clone());
  // Set last altered time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);
  new_routine->set_last_altered(ull_curtime);

  // Set security type.
  if (chistics->suid != SP_IS_DEFAULT_SUID)
  {
    View::enum_security_type sec_type;

    switch (chistics->suid)
    {
    case SP_IS_SUID:
      sec_type= View::ST_DEFINER;
      break;
    case SP_IS_NOT_SUID:
      sec_type= View::ST_INVOKER;
      break;
    default:
      DBUG_ASSERT(false);            /* purecov: deadcode */
      DBUG_RETURN(SP_ALTER_FAILED);  /* purecov: deadcode */
    }

    new_routine->set_security_type(sec_type);
  }

  // Set sql data access.
  if (chistics->daccess != SP_DEFAULT_ACCESS)
  {
    Routine::enum_sql_data_access daccess;
    switch (chistics->daccess)
    {
    case SP_NO_SQL:
      daccess= Routine::SDA_NO_SQL;
      break;
    case SP_CONTAINS_SQL:
      daccess= Routine::SDA_CONTAINS_SQL;
      break;
    case SP_READS_SQL_DATA:
      daccess= Routine::SDA_READS_SQL_DATA;
      break;
    case SP_MODIFIES_SQL_DATA:
      daccess= Routine::SDA_MODIFIES_SQL_DATA;
      break;
    default:
      DBUG_ASSERT(false);           /* purecov: deadcode */
      DBUG_RETURN(SP_ALTER_FAILED); /* purecov: deadcode */
    }
    new_routine->set_sql_data_access(daccess);
  }

  // Set comment.
  if (chistics->comment.str)
    new_routine->set_comment(chistics->comment.str);

  // Update routine.
  if (thd->dd_client()->update(&routine, new_routine.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(SP_ALTER_FAILED);
  }

  bool error= (trans_commit_stmt(thd) || trans_commit(thd));
  DBUG_RETURN(error ? SP_INTERNAL_ERROR : SP_OK);
}

////////////////////////////////////////////////////////////////////////////////
} //namespace dd
