/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

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

/**********************************************************************
This file contains the implementation of error and warnings related

  - Whenever an error or warning occurred, it pushes it to a warning list
    that the user can retrieve with SHOW WARNINGS or SHOW ERRORS.

  - For each statement, we return the number of warnings generated from this
    command.  Note that this can be different from @@warning_count as
    we reset the warning list only for questions that uses a table.
    This is done to allow on to do:
    INSERT ...;
    SELECT @@warning_count;
    SHOW WARNINGS;
    (If we would reset after each command, we could not retrieve the number
     of warnings)

  - When client requests the information using SHOW command, then 
    server processes from this list and returns back in the form of 
    resultset.

    Supported syntaxes:

    SHOW [COUNT(*)] ERRORS [LIMIT [offset,] rows]
    SHOW [COUNT(*)] WARNINGS [LIMIT [offset,] rows]
    SELECT @@warning_count, @@error_count;

***********************************************************************/

#include "sql_priv.h"
#include "unireg.h"
#include "sql_error.h"
#include "sp_rcontext.h"

using std::min;
using std::max;

/*
  Design notes about Sql_condition::m_message_text.

  The member Sql_condition::m_message_text contains the text associated with
  an error, warning or note (which are all SQL 'conditions')

  Producer of Sql_condition::m_message_text:
  ----------------------------------------

  (#1) the server implementation itself, when invoking functions like
  my_error() or push_warning()

  (#2) user code in stored programs, when using the SIGNAL statement.

  (#3) user code in stored programs, when using the RESIGNAL statement.

  When invoking my_error(), the error number and message is typically
  provided like this:
  - my_error(ER_WRONG_DB_NAME, MYF(0), ...);
  - my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));

  In both cases, the message is retrieved from ER(ER_XXX), which in turn
  is read from the resource file errmsg.sys at server startup.
  The strings stored in the errmsg.sys file are expressed in the character set
  that corresponds to the server --language start option
  (see error_message_charset_info).

  When executing:
  - a SIGNAL statement,
  - a RESIGNAL statement,
  the message text is provided by the user logic, and is expressed in UTF8.

  Storage of Sql_condition::m_message_text:
  ---------------------------------------

  (#4) The class Sql_condition is used to hold the message text member.
  This class represents a single SQL condition.

  (#5) The class Warning_info represents a SQL condition area, and contains
  a collection of SQL conditions in the Warning_info::m_warn_list

  Consumer of Sql_condition::m_message_text:
  ----------------------------------------

  (#6) The statements SHOW WARNINGS and SHOW ERRORS display the content of
  the warning list.

  (#7) The GET DIAGNOSTICS statement reads the content of:
  - the top level statement condition area (when executed in a query),
  - a sub statement (when executed in a stored program)
  and return the data stored in a Sql_condition.

  (#8) The RESIGNAL statement reads the Sql_condition caught by an exception
  handler, to raise a new or modified condition (in #3).

  The big picture
  ---------------
                                                              --------------
                                                              |            ^
                                                              V            |
  my_error(#1)                 SIGNAL(#2)                 RESIGNAL(#3)     |
      |(#A)                       |(#B)                       |(#C)        |
      |                           |                           |            |
      ----------------------------|----------------------------            |
                                  |                                        |
                                  V                                        |
                           Sql_condition(#4)                                 |
                                  |                                        |
                                  |                                        |
                                  V                                        |
                           Warning_info(#5)                                |
                                  |                                        |
          -----------------------------------------------------            |
          |                       |                           |            |
          |                       |                           |            |
          |                       |                           |            |
          V                       V                           V            |
   SHOW WARNINGS(#6)      GET DIAGNOSTICS(#7)              RESIGNAL(#8)    |
          |  |                    |                           |            |
          |  --------             |                           V            |
          |         |             |                           --------------
          V         |             |
      Connectors    |             |
          |         |             |
          -------------------------
                    |
                    V
             Client application

  Current implementation status
  -----------------------------

  (#1) (my_error) produces data in the 'error_message_charset_info' CHARSET

  (#2) and (#3) (SIGNAL, RESIGNAL) produces data internally in UTF8

  (#6) (SHOW WARNINGS) produces data in the 'error_message_charset_info' CHARSET

  (#7) (GET DIAGNOSTICS) is implemented.

  (#8) (RESIGNAL) produces data internally in UTF8 (see #3)

  As a result, the design choice for (#4) and (#5) is to store data in
  the 'error_message_charset_info' CHARSET, to minimize impact on the code base.
  This is implemented by using 'String Sql_condition::m_message_text'.

  The UTF8 -> error_message_charset_info conversion is implemented in
  Sql_cmd_common_signal::eval_signal_informations() (for path #B and #C).

  Future work
  -----------

  - Change (#1) (my_error) to generate errors in UTF8.
    See WL#751 (Recoding of error messages)

  - Change (#4 and #5) to store message text in UTF8 natively.
    In practice, this means changing the type of the message text to
    '<UTF8 String 128 class> Sql_condition::m_message_text', and is a direct
    consequence of WL#751.
*/

Sql_condition::Sql_condition()
 : Sql_alloc(),
   m_class_origin((const char*) NULL, 0, & my_charset_utf8_bin),
   m_subclass_origin((const char*) NULL, 0, & my_charset_utf8_bin),
   m_constraint_catalog((const char*) NULL, 0, & my_charset_utf8_bin),
   m_constraint_schema((const char*) NULL, 0, & my_charset_utf8_bin),
   m_constraint_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_catalog_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_schema_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_table_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_column_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_cursor_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_message_text(),
   m_sql_errno(0),
   m_level(Sql_condition::WARN_LEVEL_ERROR),
   m_mem_root(NULL)
{
  memset(m_returned_sqlstate, 0, sizeof(m_returned_sqlstate));
}

void Sql_condition::init(MEM_ROOT *mem_root)
{
  DBUG_ASSERT(mem_root != NULL);
  DBUG_ASSERT(m_mem_root == NULL);
  m_mem_root= mem_root;
}

void Sql_condition::clear()
{
  m_class_origin.length(0);
  m_subclass_origin.length(0);
  m_constraint_catalog.length(0);
  m_constraint_schema.length(0);
  m_constraint_name.length(0);
  m_catalog_name.length(0);
  m_schema_name.length(0);
  m_table_name.length(0);
  m_column_name.length(0);
  m_cursor_name.length(0);
  m_message_text.length(0);
  m_sql_errno= 0;
  m_level= Sql_condition::WARN_LEVEL_ERROR;
}

Sql_condition::Sql_condition(MEM_ROOT *mem_root)
 : Sql_alloc(),
   m_class_origin((const char*) NULL, 0, & my_charset_utf8_bin),
   m_subclass_origin((const char*) NULL, 0, & my_charset_utf8_bin),
   m_constraint_catalog((const char*) NULL, 0, & my_charset_utf8_bin),
   m_constraint_schema((const char*) NULL, 0, & my_charset_utf8_bin),
   m_constraint_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_catalog_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_schema_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_table_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_column_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_cursor_name((const char*) NULL, 0, & my_charset_utf8_bin),
   m_message_text(),
   m_sql_errno(0),
   m_level(Sql_condition::WARN_LEVEL_ERROR),
   m_mem_root(mem_root)
{
  DBUG_ASSERT(mem_root != NULL);
  memset(m_returned_sqlstate, 0, sizeof(m_returned_sqlstate));
}

static void copy_string(MEM_ROOT *mem_root, String* dst, const String* src)
{
  size_t len= src->length();
  if (len)
  {
    char* copy= (char*) alloc_root(mem_root, len + 1);
    if (copy)
    {
      memcpy(copy, src->ptr(), len);
      copy[len]= '\0';
      dst->set(copy, len, src->charset());
    }
  }
  else
    dst->length(0);
}

void
Sql_condition::copy_opt_attributes(const Sql_condition *cond)
{
  DBUG_ASSERT(this != cond);
  copy_string(m_mem_root, & m_class_origin, & cond->m_class_origin);
  copy_string(m_mem_root, & m_subclass_origin, & cond->m_subclass_origin);
  copy_string(m_mem_root, & m_constraint_catalog, & cond->m_constraint_catalog);
  copy_string(m_mem_root, & m_constraint_schema, & cond->m_constraint_schema);
  copy_string(m_mem_root, & m_constraint_name, & cond->m_constraint_name);
  copy_string(m_mem_root, & m_catalog_name, & cond->m_catalog_name);
  copy_string(m_mem_root, & m_schema_name, & cond->m_schema_name);
  copy_string(m_mem_root, & m_table_name, & cond->m_table_name);
  copy_string(m_mem_root, & m_column_name, & cond->m_column_name);
  copy_string(m_mem_root, & m_cursor_name, & cond->m_cursor_name);
}

void
Sql_condition::set(uint sql_errno, const char* sqlstate,
                   Sql_condition::enum_warning_level level, const char* msg)
{
  DBUG_ASSERT(sql_errno != 0);
  DBUG_ASSERT(sqlstate != NULL);
  DBUG_ASSERT(msg != NULL);

  m_sql_errno= sql_errno;
  memcpy(m_returned_sqlstate, sqlstate, SQLSTATE_LENGTH);
  m_returned_sqlstate[SQLSTATE_LENGTH]= '\0';

  set_class_origin();
  set_subclass_origin();
  set_builtin_message_text(msg);
  m_level= level;
}

void
Sql_condition::set_builtin_message_text(const char* str)
{
  /*
    See the comments
     "Design notes about Sql_condition::m_message_text."
  */
  const char* copy;

  copy= strdup_root(m_mem_root, str);
  m_message_text.set(copy, strlen(copy), error_message_charset_info);
  DBUG_ASSERT(! m_message_text.is_alloced());
}

const char*
Sql_condition::get_message_text() const
{
  return m_message_text.ptr();
}

int
Sql_condition::get_message_octet_length() const
{
  return m_message_text.length();
}

void
Sql_condition::set_sqlstate(const char* sqlstate)
{
  memcpy(m_returned_sqlstate, sqlstate, SQLSTATE_LENGTH);
  m_returned_sqlstate[SQLSTATE_LENGTH]= '\0';
}

static LEX_CSTRING sqlstate_origin[]= {
  { STRING_WITH_LEN("ISO 9075") },
  { STRING_WITH_LEN("MySQL") }
};

void
Sql_condition::set_class_origin()
{
  char cls[2];
  LEX_CSTRING *origin;

  /* Let CLASS = the first two letters of RETURNED_SQLSTATE. */
  cls[0]= m_returned_sqlstate[0];
  cls[1]= m_returned_sqlstate[1];

  /* Only digits and upper case latin letter are allowed. */
  DBUG_ASSERT(my_isdigit(&my_charset_latin1, cls[0]) ||
              my_isupper(&my_charset_latin1, cls[0]));

  DBUG_ASSERT(my_isdigit(&my_charset_latin1, cls[1]) ||
              my_isupper(&my_charset_latin1, cls[1]));

  /*
    If CLASS[1] is any of: 0 1 2 3 4 A B C D E F G H
    and CLASS[2] is any of: 0-9 A-Z
  */
  if (((cls[0] >= '0' && cls[0] <= '4') || (cls[0] >= 'A' && cls[0] <= 'H')) &&
      ((cls[1] >= '0' && cls[1] <= '9') || (cls[1] >= 'A' && cls[1] <= 'Z')))
    /* then let CLASS_ORIGIN = 'ISO 9075'. */
    origin= &sqlstate_origin[0];
  else
    /* let CLASS_ORIGIN = 'MySQL'. */
    origin= &sqlstate_origin[1];

  m_class_origin.set_ascii(origin->str, origin->length);
}

void
Sql_condition::set_subclass_origin()
{
  LEX_CSTRING *origin;

  DBUG_ASSERT(! m_class_origin.is_empty());

  /*
    Let SUBCLASS = the next three letters of RETURNED_SQLSTATE.
    If CLASS_ORIGIN = 'ISO 9075' or SUBCLASS = '000'
  */
  if (! memcmp(m_class_origin.ptr(), STRING_WITH_LEN("ISO 9075")) ||
      ! memcmp(m_returned_sqlstate+2, STRING_WITH_LEN("000")))
    /* then let SUBCLASS_ORIGIN = 'ISO 9075'. */
    origin= &sqlstate_origin[0];
  else
    /* let SUBCLASS_ORIGIN = 'MySQL'. */
    origin= &sqlstate_origin[1];

  m_subclass_origin.set_ascii(origin->str, origin->length);
}

Diagnostics_area::Diagnostics_area()
 : m_main_wi(0, false)
{
  push_warning_info(&m_main_wi);

  reset_diagnostics_area();
}

Diagnostics_area::Diagnostics_area(ulonglong warning_info_id,
                                   bool allow_unlimited_warnings)
 : m_main_wi(warning_info_id, allow_unlimited_warnings)
{
  push_warning_info(&m_main_wi);

  reset_diagnostics_area();
}

/**
  Clear this diagnostics area.

  Normally called at the end of a statement.
*/

void
Diagnostics_area::reset_diagnostics_area()
{
  DBUG_ENTER("reset_diagnostics_area");
#ifdef DBUG_OFF
  set_overwrite_status(false);
  /** Don't take chances in production */
  m_message[0]= '\0';
  m_sql_errno= 0;
  m_affected_rows= 0;
  m_last_insert_id= 0;
  m_statement_warn_count= 0;
#endif
  get_warning_info()->clear_error_condition();
  set_is_sent(false);
  /** Tiny reset in debug mode to see garbage right away */
  m_status= DA_EMPTY;
  DBUG_VOID_RETURN;
}


/**
  Set OK status -- ends commands that do not return a
  result set, e.g. INSERT/UPDATE/DELETE.
*/

void
Diagnostics_area::set_ok_status(ulonglong affected_rows,
                                ulonglong last_insert_id,
                                const char *message)
{
  DBUG_ENTER("set_ok_status");
  DBUG_ASSERT(! is_set());
  /*
    In production, refuse to overwrite an error or a custom response
    with an OK packet.
  */
  if (is_error() || is_disabled())
    return;

  m_statement_warn_count= current_statement_warn_count();
  m_affected_rows= affected_rows;
  m_last_insert_id= last_insert_id;
  if (message)
    strmake(m_message, message, sizeof(m_message) - 1);
  else
    m_message[0]= '\0';
  m_status= DA_OK;
  DBUG_VOID_RETURN;
}


/**
  Set EOF status.
*/

void
Diagnostics_area::set_eof_status(THD *thd)
{
  DBUG_ENTER("set_eof_status");
  /* Only allowed to report eof if has not yet reported an error */
  DBUG_ASSERT(! is_set());
  /*
    In production, refuse to overwrite an error or a custom response
    with an EOF packet.
  */
  if (is_error() || is_disabled())
    return;

  /*
    If inside a stored procedure, do not return the total
    number of warnings, since they are not available to the client
    anyway.
  */
  m_statement_warn_count= (thd->sp_runtime_ctx ?
                           0 :
                           current_statement_warn_count());

  m_status= DA_EOF;
  DBUG_VOID_RETURN;
}

/**
  Set ERROR status in the Diagnostics Area. This function should be used to
  report fatal errors (such as out-of-memory errors) when no further
  processing is possible.

  @param sql_errno        SQL-condition error number
*/

void
Diagnostics_area::set_error_status(uint sql_errno)
{
  set_error_status(sql_errno,
                   ER(sql_errno),
                   mysql_errno_to_sqlstate(sql_errno),
                   NULL);
}

/**
  Set ERROR status in the Diagnostics Area.

  @note error_condition may be NULL. It happens if a) OOM error is being
  reported; or b) when Warning_info is full.

  @param sql_errno        SQL-condition error number
  @param message          SQL-condition message
  @param sqlstate         SQL-condition state
  @param error_condition  SQL-condition object representing the error state
*/

void
Diagnostics_area::set_error_status(uint sql_errno,
                                   const char *message,
                                   const char *sqlstate,
                                   const Sql_condition *error_condition)
{
  DBUG_ENTER("set_error_status");
  /*
    Only allowed to report error if has not yet reported a success
    The only exception is when we flush the message to the client,
    an error can happen during the flush.
  */
  DBUG_ASSERT(! is_set() || m_can_overwrite_status);

  // message must be set properly by the caller.
  DBUG_ASSERT(message);

  // sqlstate must be set properly by the caller.
  DBUG_ASSERT(sqlstate);

#ifdef DBUG_OFF
  /*
    In production, refuse to overwrite a custom response with an
    ERROR packet.
  */
  if (is_disabled())
    return;
#endif

  m_sql_errno= sql_errno;
  memcpy(m_sqlstate, sqlstate, SQLSTATE_LENGTH);
  m_sqlstate[SQLSTATE_LENGTH]= '\0';
  strmake(m_message, message, sizeof(m_message)-1);

  get_warning_info()->set_error_condition(error_condition);

  m_status= DA_ERROR;
  DBUG_VOID_RETURN;
}


/**
  Mark the diagnostics area as 'DISABLED'.

  This is used in rare cases when the COM_ command at hand sends a response
  in a custom format. One example is the query cache, another is
  COM_STMT_PREPARE.
*/

void
Diagnostics_area::disable_status()
{
  DBUG_ASSERT(! is_set());
  m_status= DA_DISABLED;
}

Warning_info::Warning_info(ulonglong warn_id_arg, bool allow_unlimited_warnings)
  :m_current_statement_warn_count(0),
  m_current_row_for_warning(1),
  m_warn_id(warn_id_arg),
  m_error_condition(NULL),
  m_allow_unlimited_warnings(allow_unlimited_warnings),
  m_read_only(FALSE)
{
  /* Initialize sub structures */
  init_sql_alloc(&m_warn_root, WARN_ALLOC_BLOCK_SIZE, WARN_ALLOC_PREALLOC_SIZE);
  m_warn_list.empty();
  memset(m_warn_count, 0, sizeof(m_warn_count));
}

Warning_info::~Warning_info()
{
  free_root(&m_warn_root,MYF(0));
}


bool Warning_info::has_sql_condition(const char *message_str,
                                     ulong message_length) const
{
  Diagnostics_area::Sql_condition_iterator it(m_warn_list);
  const Sql_condition *err;

  while ((err= it++))
  {
    if (strncmp(message_str, err->get_message_text(), message_length) == 0)
      return true;
  }

  return false;
}


void Warning_info::clear(ulonglong new_id)
{
  id(new_id);
  m_warn_list.empty();
  m_marked_sql_conditions.empty();
  free_root(&m_warn_root, MYF(0));
  memset(m_warn_count, 0, sizeof(m_warn_count));
  m_current_statement_warn_count= 0;
  m_current_row_for_warning= 1; /* Start counting from the first row */
  clear_error_condition();
}


void Warning_info::append_warning_info(THD *thd, const Warning_info *source)
{
  const Sql_condition *err;
  Diagnostics_area::Sql_condition_iterator it(source->m_warn_list);
  const Sql_condition *src_error_condition = source->get_error_condition();

  while ((err= it++))
  {
    // Do not use ::push_warning() to avoid invocation of THD-internal-handlers.
    Sql_condition *new_error= Warning_info::push_warning(thd, err);

    if (src_error_condition && src_error_condition == err)
      set_error_condition(new_error);

    if (source->is_marked_for_removal(err))
      mark_condition_for_removal(new_error);
  }
}


/**
  Copy Sql_conditions that are not WARN_LEVEL_ERROR from the source
  Warning_info to the current Warning_info.

  @param thd    Thread context.
  @param sp_wi  Stored-program Warning_info
  @param thd     Thread context.
  @param src_wi  Warning_info to copy from.
*/
void Diagnostics_area::copy_non_errors_from_wi(THD *thd,
                                               const Warning_info *src_wi)
{
  Sql_condition_iterator it(src_wi->m_warn_list);
  const Sql_condition *cond;
  Warning_info *wi= get_warning_info();

  while ((cond= it++))
  {
    if (cond->get_level() == Sql_condition::WARN_LEVEL_ERROR)
      continue;

    Sql_condition *new_condition= wi->push_warning(thd, cond);

    if (src_wi->is_marked_for_removal(cond))
      wi->mark_condition_for_removal(new_condition);
  }
}


void Warning_info::mark_sql_conditions_for_removal()
{
  Sql_condition_list::Iterator it(m_warn_list);
  Sql_condition *cond;

  while ((cond= it++))
    mark_condition_for_removal(cond);
}


void Warning_info::remove_marked_sql_conditions()
{
  List_iterator_fast<Sql_condition> it(m_marked_sql_conditions);
  Sql_condition *cond;

  while ((cond= it++))
  {
    m_warn_list.remove(cond);
    m_warn_count[cond->get_level()]--;
    m_current_statement_warn_count--;
    if (cond == m_error_condition)
      m_error_condition= NULL;
  }

  m_marked_sql_conditions.empty();
}


bool Warning_info::is_marked_for_removal(const Sql_condition *cond) const
{
  List_iterator_fast<Sql_condition> it(
    const_cast<List<Sql_condition>&> (m_marked_sql_conditions));
  Sql_condition *c;

  while ((c= it++))
  {
    if (c == cond)
      return true;
  }

  return false;
}


void Warning_info::reserve_space(THD *thd, uint count)
{
  while (m_warn_list.elements() &&
         (m_warn_list.elements() + count) > thd->variables.max_error_count)
    m_warn_list.remove(m_warn_list.front());
}

Sql_condition *Warning_info::push_warning(THD *thd,
                                          uint sql_errno, const char* sqlstate,
                                          Sql_condition::enum_warning_level level,
                                          const char *msg)
{
  Sql_condition *cond= NULL;

  if (! m_read_only)
  {
    if (m_allow_unlimited_warnings ||
        m_warn_list.elements() < thd->variables.max_error_count)
    {
      cond= new (& m_warn_root) Sql_condition(& m_warn_root);
      if (cond)
      {
        cond->set(sql_errno, sqlstate, level, msg);
        m_warn_list.push_back(cond);
      }
    }
    m_warn_count[(uint) level]++;
  }

  m_current_statement_warn_count++;
  return cond;
}

Sql_condition *Warning_info::push_warning(THD *thd, const Sql_condition *sql_condition)
{
  Sql_condition *new_condition= push_warning(thd,
                                           sql_condition->get_sql_errno(),
                                           sql_condition->get_sqlstate(),
                                           sql_condition->get_level(),
                                           sql_condition->get_message_text());

  if (new_condition)
    new_condition->copy_opt_attributes(sql_condition);

  return new_condition;
}

/*
  Push the warning to error list if there is still room in the list

  SYNOPSIS
    push_warning()
    thd			Thread handle
    level		Severity of warning (note, warning)
    code		Error number
    msg			Clear error message
*/

void push_warning(THD *thd, Sql_condition::enum_warning_level level,
                  uint code, const char *msg)
{
  DBUG_ENTER("push_warning");
  DBUG_PRINT("enter", ("code: %d, msg: %s", code, msg));

  /*
    Calling push_warning/push_warning_printf with a level of
    WARN_LEVEL_ERROR *is* a bug.  Either use my_printf_error(),
    my_error(), or WARN_LEVEL_WARN.
  */
  DBUG_ASSERT(level != Sql_condition::WARN_LEVEL_ERROR);

  if (level == Sql_condition::WARN_LEVEL_ERROR)
    level= Sql_condition::WARN_LEVEL_WARN;

  (void) thd->raise_condition(code, NULL, level, msg);

  DBUG_VOID_RETURN;
}


/*
  Push the warning to error list if there is still room in the list

  SYNOPSIS
    push_warning_printf()
    thd			Thread handle
    level		Severity of warning (note, warning)
    code		Error number
    msg			Clear error message
*/

void push_warning_printf(THD *thd, Sql_condition::enum_warning_level level,
			 uint code, const char *format, ...)
{
  va_list args;
  char    warning[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("push_warning_printf");
  DBUG_PRINT("enter",("warning: %u", code));

  DBUG_ASSERT(code != 0);
  DBUG_ASSERT(format != NULL);

  va_start(args,format);
  my_vsnprintf_ex(&my_charset_utf8_general_ci, warning,
                  sizeof(warning), format, args);
  va_end(args);
  push_warning(thd, level, code, warning);
  DBUG_VOID_RETURN;
}


/*
  Send all notes, errors or warnings to the client in a result set

  SYNOPSIS
    mysqld_show_warnings()
    thd			Thread handler
    levels_to_show	Bitmap for which levels to show

  DESCRIPTION
    Takes into account the current LIMIT

  RETURN VALUES
    FALSE ok
    TRUE  Error sending data to client
*/

const LEX_STRING warning_level_names[]=
{
  { C_STRING_WITH_LEN("Note") },
  { C_STRING_WITH_LEN("Warning") },
  { C_STRING_WITH_LEN("Error") },
  { C_STRING_WITH_LEN("?") }
};

bool mysqld_show_warnings(THD *thd, ulong levels_to_show)
{
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_warnings");

  DBUG_ASSERT(thd->get_stmt_da()->is_warning_info_read_only());

  field_list.push_back(new Item_empty_string("Level", 7));
  field_list.push_back(new Item_return_int("Code",4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Message",MYSQL_ERRMSG_SIZE));

  if (thd->protocol->send_result_set_metadata(&field_list,
                                 Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  const Sql_condition *err;
  SELECT_LEX *sel= &thd->lex->select_lex;
  SELECT_LEX_UNIT *unit= &thd->lex->unit;
  ulonglong idx= 0;
  Protocol *protocol=thd->protocol;

  unit->set_limit(sel);

  Diagnostics_area::Sql_condition_iterator it=
    thd->get_stmt_da()->sql_conditions();
  while ((err= it++))
  {
    /* Skip levels that the user is not interested in */
    if (!(levels_to_show & ((ulong) 1 << err->get_level())))
      continue;
    if (++idx <= unit->offset_limit_cnt)
      continue;
    if (idx > unit->select_limit_cnt)
      break;
    protocol->prepare_for_resend();
    protocol->store(warning_level_names[err->get_level()].str,
		    warning_level_names[err->get_level()].length,
                    system_charset_info);
    protocol->store((uint32) err->get_sql_errno());
    protocol->store(err->get_message_text(),
                    err->get_message_octet_length(),
                    system_charset_info);
    if (protocol->write())
      DBUG_RETURN(TRUE);
  }
  my_eof(thd);

  thd->get_stmt_da()->set_warning_info_read_only(FALSE);

  DBUG_RETURN(FALSE);
}


ErrConvString::ErrConvString(double nr)
{
  // enough to print '-[digits].E+###'
  DBUG_ASSERT(sizeof(err_buffer) > DBL_DIG + 8);
  buf_length= my_gcvt(nr, MY_GCVT_ARG_DOUBLE,
                      sizeof(err_buffer) - 1, err_buffer, NULL);  
}



ErrConvString::ErrConvString(const my_decimal *nr)
{
  int len= sizeof(err_buffer);
  (void) decimal2string((decimal_t *) nr, err_buffer, &len, 0, 0, 0);
  buf_length= (uint) len;
}


ErrConvString::ErrConvString(const struct st_mysql_time *ltime, uint dec)
{
  buf_length= my_TIME_to_str(ltime, err_buffer,
                             MY_MIN(dec, DATETIME_MAX_DECIMALS));
}


/**
   Convert value for dispatch to error message(see WL#751).

   @param to          buffer for converted string
   @param to_length   size of the buffer
   @param from        string which should be converted
   @param from_length string length
   @param from_cs     charset from convert
 
   @retval
   number of bytes written to "to"
*/

uint err_conv(char *buff, size_t to_length, const char *from,
              size_t from_length, const CHARSET_INFO *from_cs)
{
  char *to= buff;
  const char *from_start= from;
  size_t res;

  DBUG_ASSERT(to_length > 0);
  to_length--;
  if (from_cs == &my_charset_bin)
  {
    uchar char_code;
    res= 0;
    while (1)
    {
      if ((uint)(from - from_start) >= from_length ||
          res >= to_length)
      {
        *to= 0;
        break;
      }

      char_code= ((uchar) *from);
      if (char_code >= 0x20 && char_code <= 0x7E)
      {
        *to++= char_code;
        from++;
        res++;
      }
      else
      {
        if (res + 4 >= to_length)
        {
          *to= 0;
          break;
        }
        res+= my_snprintf(to, 5, "\\x%02X", (uint) char_code);
        to+=4;
        from++;
      }
    }
  }
  else
  {
    uint errors;
    res= copy_and_convert(to, to_length, system_charset_info,
                          from, from_length, from_cs, &errors);
    to+= res;
    *to= 0;
  }
  return to - buff;
}


/**
   Convert string for dispatch to client(see WL#751).

   @param to          buffer to convert
   @param to_length   buffer length
   @param to_cs       chraset to convert
   @param from        string from convert
   @param from_length string length
   @param from_cs     charset from convert
   @param errors      count of errors during convertion

   @retval
   length of converted string
*/

uint32 convert_error_message(char *to, uint32 to_length,
                             const CHARSET_INFO *to_cs,
                             const char *from, uint32 from_length,
                             const CHARSET_INFO *from_cs, uint *errors)
{
  int         cnvres;
  my_wc_t     wc;
  const uchar *from_end= (const uchar*) from+from_length;
  char *to_start= to;
  uchar *to_end;
  my_charset_conv_mb_wc mb_wc= from_cs->cset->mb_wc;
  my_charset_conv_wc_mb wc_mb;
  uint error_count= 0;
  uint length;

  DBUG_ASSERT(to_length > 0);
  /* Make room for the null terminator. */
  to_length--;
  to_end= (uchar*) (to + to_length);

  if (!to_cs || from_cs == to_cs || to_cs == &my_charset_bin)
  {
    length= MY_MIN(to_length, from_length);
    memmove(to, from, length);
    to[length]= 0;
    return length;
  }

  wc_mb= to_cs->cset->wc_mb;
  while (1)
  {
    if ((cnvres= (*mb_wc)(from_cs, &wc, (uchar*) from, from_end)) > 0)
    {
      if (!wc)
        break;
      from+= cnvres;
    }
    else if (cnvres == MY_CS_ILSEQ)
    {
      wc= (ulong) (uchar) *from;
      from+=1;
    }
    else
      break;

    if ((cnvres= (*wc_mb)(to_cs, wc, (uchar*) to, to_end)) > 0)
      to+= cnvres;
    else if (cnvres == MY_CS_ILUNI)
    {
      length= (wc <= 0xFFFF) ? 6/* '\1234' format*/ : 9 /* '\+123456' format*/;
      if ((uchar*)(to + length) >= to_end)
        break;
      cnvres= my_snprintf(to, 9,
                          (wc <= 0xFFFF) ? "\\%04X" : "\\+%06X", (uint) wc);
      to+= cnvres;
    }
    else
      break;
  }

  *to= 0;
  *errors= error_count;
  return (uint32) (to - to_start);
}


/**
  Sanity check for SQLSTATEs. The function does not check if it's really an
  existing SQL-state (there are just too many), it just checks string length and
  looks for bad characters.

  @param sqlstate the condition SQLSTATE.

  @retval true if it's ok.
  @retval false if it's bad.
*/

bool is_sqlstate_valid(const LEX_STRING *sqlstate)
{
  if (sqlstate->length != 5)
    return false;

  for (int i= 0 ; i < 5 ; ++i)
  {
    char c = sqlstate->str[i];

    if ((c < '0' || '9' < c) &&
	(c < 'A' || 'Z' < c))
      return false;
  }

  return true;
}
