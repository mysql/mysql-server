/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "sp_head.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sql_signal.h"

/*
  The parser accepts any error code (desired)
  The runtime internally supports any error code (desired)
  The client server protocol is limited to 16 bits error codes (restriction)
  Enforcing the 65535 limit in the runtime until the protocol can change.
*/
#define MAX_MYSQL_ERRNO UINT_MAX16

const LEX_STRING Diag_condition_item_names[]=
{
  { C_STRING_WITH_LEN("CLASS_ORIGIN") },
  { C_STRING_WITH_LEN("SUBCLASS_ORIGIN") },
  { C_STRING_WITH_LEN("CONSTRAINT_CATALOG") },
  { C_STRING_WITH_LEN("CONSTRAINT_SCHEMA") },
  { C_STRING_WITH_LEN("CONSTRAINT_NAME") },
  { C_STRING_WITH_LEN("CATALOG_NAME") },
  { C_STRING_WITH_LEN("SCHEMA_NAME") },
  { C_STRING_WITH_LEN("TABLE_NAME") },
  { C_STRING_WITH_LEN("COLUMN_NAME") },
  { C_STRING_WITH_LEN("CURSOR_NAME") },
  { C_STRING_WITH_LEN("MESSAGE_TEXT") },
  { C_STRING_WITH_LEN("MYSQL_ERRNO") },

  { C_STRING_WITH_LEN("CONDITION_IDENTIFIER") },
  { C_STRING_WITH_LEN("CONDITION_NUMBER") },
  { C_STRING_WITH_LEN("CONNECTION_NAME") },
  { C_STRING_WITH_LEN("MESSAGE_LENGTH") },
  { C_STRING_WITH_LEN("MESSAGE_OCTET_LENGTH") },
  { C_STRING_WITH_LEN("PARAMETER_MODE") },
  { C_STRING_WITH_LEN("PARAMETER_NAME") },
  { C_STRING_WITH_LEN("PARAMETER_ORDINAL_POSITION") },
  { C_STRING_WITH_LEN("RETURNED_SQLSTATE") },
  { C_STRING_WITH_LEN("ROUTINE_CATALOG") },
  { C_STRING_WITH_LEN("ROUTINE_NAME") },
  { C_STRING_WITH_LEN("ROUTINE_SCHEMA") },
  { C_STRING_WITH_LEN("SERVER_NAME") },
  { C_STRING_WITH_LEN("SPECIFIC_NAME") },
  { C_STRING_WITH_LEN("TRIGGER_CATALOG") },
  { C_STRING_WITH_LEN("TRIGGER_NAME") },
  { C_STRING_WITH_LEN("TRIGGER_SCHEMA") }
};

const LEX_STRING Diag_statement_item_names[]=
{
  { C_STRING_WITH_LEN("NUMBER") },
  { C_STRING_WITH_LEN("MORE") },
  { C_STRING_WITH_LEN("COMMAND_FUNCTION") },
  { C_STRING_WITH_LEN("COMMAND_FUNCTION_CODE") },
  { C_STRING_WITH_LEN("DYNAMIC_FUNCTION") },
  { C_STRING_WITH_LEN("DYNAMIC_FUNCTION_CODE") },
  { C_STRING_WITH_LEN("ROW_COUNT") },
  { C_STRING_WITH_LEN("TRANSACTIONS_COMMITTED") },
  { C_STRING_WITH_LEN("TRANSACTIONS_ROLLED_BACK") },
  { C_STRING_WITH_LEN("TRANSACTION_ACTIVE") }
};


Set_signal_information::Set_signal_information(
  const Set_signal_information& set)
{
  memcpy(m_item, set.m_item, sizeof(m_item));
}

void Set_signal_information::clear()
{
  memset(m_item, 0, sizeof(m_item));
}

void Sql_cmd_common_signal::assign_defaults(
                                    Sql_condition *cond,
                                    bool set_level_code,
                                    Sql_condition::enum_warning_level level,
                                    int sqlcode)
{
  if (set_level_code)
  {
    cond->m_level= level;
    cond->m_sql_errno= sqlcode;
  }
  if (! cond->get_message_text())
    cond->set_builtin_message_text(ER(sqlcode));
}

void Sql_cmd_common_signal::eval_defaults(THD *thd, Sql_condition *cond)
{
  DBUG_ASSERT(cond);

  const char* sqlstate;
  bool set_defaults= (m_cond != 0);

  if (set_defaults)
  {
    /*
      SIGNAL is restricted in sql_yacc.yy to only signal SQLSTATE conditions.
    */
    DBUG_ASSERT(m_cond->type == sp_condition_value::SQLSTATE);
    sqlstate= m_cond->sql_state;
    cond->set_sqlstate(sqlstate);
  }
  else
    sqlstate= cond->get_sqlstate();

  DBUG_ASSERT(sqlstate);
  /* SQLSTATE class "00": illegal, rejected in the parser. */
  DBUG_ASSERT((sqlstate[0] != '0') || (sqlstate[1] != '0'));

  if ((sqlstate[0] == '0') && (sqlstate[1] == '1'))
  {
    /* SQLSTATE class "01": warning. */
    assign_defaults(cond, set_defaults,
                    Sql_condition::WARN_LEVEL_WARN, ER_SIGNAL_WARN);
  }
  else if ((sqlstate[0] == '0') && (sqlstate[1] == '2'))
  {
    /* SQLSTATE class "02": not found. */
    assign_defaults(cond, set_defaults,
                    Sql_condition::WARN_LEVEL_ERROR, ER_SIGNAL_NOT_FOUND);
  }
  else
  {
    /* other SQLSTATE classes : error. */
    assign_defaults(cond, set_defaults,
                    Sql_condition::WARN_LEVEL_ERROR, ER_SIGNAL_EXCEPTION);
  }
}

static bool assign_fixed_string(MEM_ROOT *mem_root,
                                CHARSET_INFO *dst_cs,
                                size_t max_char,
                                String *dst,
                                const String* src)
{
  bool truncated;
  size_t numchars;
  const CHARSET_INFO *src_cs;
  const char* src_str;
  const char* src_end;
  size_t src_len;
  size_t to_copy;
  char* dst_str;
  size_t dst_len;
  size_t dst_copied;
  uint32 dummy_offset;

  src_str= src->ptr();
  if (src_str == NULL)
  {
    dst->set((const char*) NULL, 0, dst_cs);
    return false;
  }

  src_cs= src->charset();
  src_len= src->length();
  src_end= src_str + src_len;
  numchars= src_cs->cset->numchars(src_cs, src_str, src_end);

  if (numchars <= max_char)
  {
    to_copy= src->length();
    truncated= false;
  }
  else
  {
    numchars= max_char;
    to_copy= dst_cs->cset->charpos(dst_cs, src_str, src_end, numchars);
    truncated= true;
  }

  if (String::needs_conversion(to_copy, src_cs, dst_cs, & dummy_offset))
  {
    dst_len= numchars * dst_cs->mbmaxlen;
    dst_str= (char*) alloc_root(mem_root, dst_len + 1);
    if (dst_str)
    {
      const char* well_formed_error_pos;
      const char* cannot_convert_error_pos;
      const char* from_end_pos;

      dst_copied= well_formed_copy_nchars(dst_cs, dst_str, dst_len,
                                          src_cs, src_str, src_len,
                                          numchars,
                                          & well_formed_error_pos,
                                          & cannot_convert_error_pos,
                                          & from_end_pos);
      DBUG_ASSERT(dst_copied <= dst_len);
      dst_len= dst_copied; /* In case the copy truncated the data */
      dst_str[dst_copied]= '\0';
    }
  }
  else
  {
    dst_len= to_copy;
    dst_str= (char*) alloc_root(mem_root, dst_len + 1);
    if (dst_str)
    {
      memcpy(dst_str, src_str, to_copy);
      dst_str[to_copy]= '\0';
    }
  }
  dst->set(dst_str, dst_len, dst_cs);

  return truncated;
}

static int assign_condition_item(MEM_ROOT *mem_root, const char* name, THD *thd,
                                 Item *set, String *ci)
{
  char str_buff[(64+1)*4]; /* Room for a null terminated UTF8 String 64 */
  String str_value(str_buff, sizeof(str_buff), & my_charset_utf8_bin);
  String *str;
  bool truncated;

  DBUG_ENTER("assign_condition_item");

  if (set->is_null())
  {
    thd->raise_error_printf(ER_WRONG_VALUE_FOR_VAR, name, "NULL");
    DBUG_RETURN(1);
  }

  str= set->val_str(& str_value);
  truncated= assign_fixed_string(mem_root, & my_charset_utf8_bin, 64, ci, str);
  if (truncated)
  {
    if (thd->is_strict_mode())
    {
      thd->raise_error_printf(ER_COND_ITEM_TOO_LONG, name);
      DBUG_RETURN(1);
    }

    thd->raise_warning_printf(WARN_COND_ITEM_TRUNCATED, name);
  }

  DBUG_RETURN(0);
}


int Sql_cmd_common_signal::eval_signal_informations(THD *thd, Sql_condition *cond)
{
  struct cond_item_map
  {
    enum enum_diag_condition_item_name m_item;
    String Sql_condition::*m_member;
  };

  static cond_item_map map[]=
  {
    { DIAG_CLASS_ORIGIN, & Sql_condition::m_class_origin },
    { DIAG_SUBCLASS_ORIGIN, & Sql_condition::m_subclass_origin },
    { DIAG_CONSTRAINT_CATALOG, & Sql_condition::m_constraint_catalog },
    { DIAG_CONSTRAINT_SCHEMA, & Sql_condition::m_constraint_schema },
    { DIAG_CONSTRAINT_NAME, & Sql_condition::m_constraint_name },
    { DIAG_CATALOG_NAME, & Sql_condition::m_catalog_name },
    { DIAG_SCHEMA_NAME, & Sql_condition::m_schema_name },
    { DIAG_TABLE_NAME, & Sql_condition::m_table_name },
    { DIAG_COLUMN_NAME, & Sql_condition::m_column_name },
    { DIAG_CURSOR_NAME, & Sql_condition::m_cursor_name }
  };

  Item *set;
  String str_value;
  String *str;
  int i;
  uint j;
  int result= 1;
  enum enum_diag_condition_item_name item_enum;
  String *member;
  const LEX_STRING *name;

  DBUG_ENTER("Sql_cmd_common_signal::eval_signal_informations");

  for (i= FIRST_DIAG_SET_PROPERTY;
       i <= LAST_DIAG_SET_PROPERTY;
       i++)
  {
    set= m_set_signal_information.m_item[i];
    if (set)
    {
      if (! set->fixed)
      {
        if (set->fix_fields(thd, & set))
          goto end;
        m_set_signal_information.m_item[i]= set;
      }
    }
  }

  /*
    Generically assign all the UTF8 String 64 condition items
    described in the map.
  */
  for (j= 0; j < array_elements(map); j++)
  {
    item_enum= map[j].m_item;
    set= m_set_signal_information.m_item[item_enum];
    if (set != NULL)
    {
      member= & (cond->* map[j].m_member);
      name= & Diag_condition_item_names[item_enum];
      if (assign_condition_item(cond->m_mem_root, name->str, thd, set, member))
        goto end;
    }
  }

  /*
    Assign the remaining attributes.
  */

  set= m_set_signal_information.m_item[DIAG_MESSAGE_TEXT];
  if (set != NULL)
  {
    if (set->is_null())
    {
      thd->raise_error_printf(ER_WRONG_VALUE_FOR_VAR,
                              "MESSAGE_TEXT", "NULL");
      goto end;
    }
    /*
      Enforce that SET MESSAGE_TEXT = <value> evaluates the value
      as VARCHAR(128) CHARACTER SET UTF8.
    */
    bool truncated;
    String utf8_text;
    str= set->val_str(& str_value);
    truncated= assign_fixed_string(thd->mem_root, & my_charset_utf8_bin, 128,
                                   & utf8_text, str);
    if (truncated)
    {
      if (thd->is_strict_mode())
      {
        thd->raise_error_printf(ER_COND_ITEM_TOO_LONG,
                                "MESSAGE_TEXT");
        goto end;
      }

      thd->raise_warning_printf(WARN_COND_ITEM_TRUNCATED,
                                "MESSAGE_TEXT");
    }

    /*
      See the comments
       "Design notes about Sql_condition::m_message_text."
      in file sql_error.cc
    */
    String converted_text;
    converted_text.set_charset(error_message_charset_info);
    converted_text.append(utf8_text.ptr(), utf8_text.length(),
                          utf8_text.charset());
    cond->set_builtin_message_text(converted_text.c_ptr_safe());
  }

  set= m_set_signal_information.m_item[DIAG_MYSQL_ERRNO];
  if (set != NULL)
  {
    if (set->is_null())
    {
      thd->raise_error_printf(ER_WRONG_VALUE_FOR_VAR,
                              "MYSQL_ERRNO", "NULL");
      goto end;
    }
    longlong code= set->val_int();
    if ((code <= 0) || (code > MAX_MYSQL_ERRNO))
    {
      str= set->val_str(& str_value);
      thd->raise_error_printf(ER_WRONG_VALUE_FOR_VAR,
                              "MYSQL_ERRNO", str->c_ptr_safe());
      goto end;
    }
    cond->m_sql_errno= (int) code;
  }

  /*
    The various item->val_xxx() methods don't return an error code,
    but flag thd in case of failure.
  */
  if (! thd->is_error())
    result= 0;

end:
  for (i= FIRST_DIAG_SET_PROPERTY;
       i <= LAST_DIAG_SET_PROPERTY;
       i++)
  {
    set= m_set_signal_information.m_item[i];
    if (set)
    {
      if (set->fixed)
        set->cleanup();
    }
  }

  DBUG_RETURN(result);
}

bool Sql_cmd_common_signal::raise_condition(THD *thd, Sql_condition *cond)
{
  bool result= TRUE;

  DBUG_ENTER("Sql_cmd_common_signal::raise_condition");

  DBUG_ASSERT(thd->lex->query_tables == NULL);

  eval_defaults(thd, cond);
  if (eval_signal_informations(thd, cond))
    DBUG_RETURN(result);

  /* SIGNAL should not signal WARN_LEVEL_NOTE */
  DBUG_ASSERT((cond->m_level == Sql_condition::WARN_LEVEL_WARN) ||
              (cond->m_level == Sql_condition::WARN_LEVEL_ERROR));

  Sql_condition *raised= NULL;
  raised= thd->raise_condition(cond->get_sql_errno(),
                               cond->get_sqlstate(),
                               cond->get_level(),
                               cond->get_message_text());
  if (raised)
    raised->copy_opt_attributes(cond);

  if (cond->m_level == Sql_condition::WARN_LEVEL_WARN)
  {
    my_ok(thd);
    result= FALSE;
  }

  DBUG_RETURN(result);
}

bool Sql_cmd_signal::execute(THD *thd)
{
  bool result= TRUE;
  Sql_condition cond(thd->mem_root);

  DBUG_ENTER("Sql_cmd_signal::execute");

  /*
    WL#2110 SIGNAL specification says:

      When SIGNAL is executed, it has five effects, in the following order:

        (1) First, the diagnostics area is completely cleared. So if the
        SIGNAL is in a DECLARE HANDLER then any pending errors or warnings
        are gone. So is 'row count'.

    This has roots in the SQL standard specification for SIGNAL.
  */

  thd->get_stmt_da()->reset_diagnostics_area();
  thd->set_row_count_func(0);
  thd->get_stmt_da()->clear_warning_info(thd->query_id);

  result= raise_condition(thd, &cond);

  DBUG_RETURN(result);
}


/**
  Execute RESIGNAL SQL-statement.

  @param thd Thread context.

  @return Error status
  @retval true  in case of error
  @retval false on success
*/

bool Sql_cmd_resignal::execute(THD *thd)
{
  Diagnostics_area *da= thd->get_stmt_da();
  const sp_rcontext::Sql_condition_info *signaled;

  DBUG_ENTER("Sql_cmd_resignal::execute");

  // This is a way to force sql_conditions from the current Warning_info to be
  // passed to the caller's Warning_info.
  da->set_warning_info_id(thd->query_id);

  if (! thd->sp_runtime_ctx ||
      ! (signaled= thd->sp_runtime_ctx->raised_condition()))
  {
    thd->raise_error(ER_RESIGNAL_WITHOUT_ACTIVE_HANDLER);
    DBUG_RETURN(true);
  }

  Sql_condition signaled_err(thd->mem_root);
  signaled_err.set(signaled->sql_errno,
                   signaled->sql_state,
                   signaled->level,
                   signaled->message);


  if (m_cond) // RESIGNAL with signal_value.
  {
    query_cache_abort(&thd->query_cache_tls);

    /* Keep handled conditions. */
    da->unmark_sql_conditions_from_removal();

    /* Check if the old condition still exists. */
    if (da->has_sql_condition(signaled->message, strlen(signaled->message)))
    {
      /* Make room for the new RESIGNAL condition. */
      da->reserve_space(thd, 1);
    }
    else
    {
      /* Make room for old condition + the new RESIGNAL condition. */
      da->reserve_space(thd, 2);

      da->push_warning(thd, &signaled_err);
    }
  }

  DBUG_RETURN(raise_condition(thd, &signaled_err));
}
