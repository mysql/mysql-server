/* Copyright (C) 1995-2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

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

#include "mysql_priv.h"
#include "sp_rcontext.h"

/*
  Store a new message in an error object

  This is used to in group_concat() to register how many warnings we actually
  got after the query has been executed.
*/

void MYSQL_ERROR::set_msg(THD *thd, const char *msg_arg)
{
  msg= strdup_root(&thd->warn_root, msg_arg);
}


/*
  Reset all warnings for the thread

  SYNOPSIS
    mysql_reset_errors()
    thd			Thread handle

  IMPLEMENTATION
    Don't reset warnings if this has already been called for this query.
    This may happen if one gets a warning during the parsing stage,
    in which case push_warnings() has already called this function.
*/  

void mysql_reset_errors(THD *thd)
{
  DBUG_ENTER("mysql_reset_errors");
  if (thd->query_id != thd->warn_id)
  {
    thd->warn_id= thd->query_id;
    free_root(&thd->warn_root,MYF(0));
    bzero((char*) thd->warn_count, sizeof(thd->warn_count));
    thd->warn_list.empty();
    thd->row_count= 1; // by default point to row 1
  }
  DBUG_VOID_RETURN;
}


/* 
  Push the warning/error to error list if there is still room in the list

  SYNOPSIS
    push_warning()
    thd			Thread handle
    level		Severity of warning (note, warning, error ...)
    code		Error number
    msg			Clear error message
    
  RETURN
    pointer on MYSQL_ERROR object
*/

MYSQL_ERROR *push_warning(THD *thd, MYSQL_ERROR::enum_warning_level level, 
                          uint code, const char *msg)
{
  MYSQL_ERROR *err= 0;
  DBUG_ENTER("push_warning");

  if (thd->query_id != thd->warn_id)
    mysql_reset_errors(thd);
  if (thd->spcont &&
      thd->spcont->find_handler(code,
                                ((int) level >=
                                 (int) MYSQL_ERROR::WARN_LEVEL_WARN &&
                                 thd->really_abort_on_warning()) ?
                                MYSQL_ERROR::WARN_LEVEL_ERROR : level))
  {
    DBUG_RETURN(NULL);
  }

  /* Abort if we are using strict mode and we are not using IGNORE */
  if ((int) level >= (int) MYSQL_ERROR::WARN_LEVEL_WARN &&
      thd->really_abort_on_warning())
  {
    thd->killed= THD::KILL_BAD_DATA;
    my_message(code, msg, MYF(0));
    DBUG_RETURN(NULL);
  }

  if (thd->warn_list.elements < thd->variables.max_error_count)
  {
    /*
      The following code is here to change the allocation to not
      use the thd->mem_root, which is freed after each query
    */
    MEM_ROOT *old_root= thd->mem_root;
    thd->mem_root= &thd->warn_root;
    if ((err= new MYSQL_ERROR(thd, code, level, msg)))
      thd->warn_list.push_back(err);
    thd->mem_root= old_root;
  }
  thd->warn_count[(uint) level]++;
  thd->total_warn_count++;
  DBUG_RETURN(err);
}

/*
  Push the warning/error to error list if there is still room in the list

  SYNOPSIS
    push_warning_printf()
    thd			Thread handle
    level		Severity of warning (note, warning, error ...)
    code		Error number
    msg			Clear error message
*/

void push_warning_printf(THD *thd, MYSQL_ERROR::enum_warning_level level,
			 uint code, const char *format, ...)
{
  va_list args;
  char    warning[ERRMSGSIZE+20];
  DBUG_ENTER("push_warning_printf");
  DBUG_PRINT("enter",("warning: %u", code));
  
  va_start(args,format);
  my_vsnprintf(warning, sizeof(warning), format, args);
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
    0	ok
    1	Error sending data to client
*/

static const char *warning_level_names[]= {"Note", "Warning", "Error", "?"};
static int warning_level_length[]= { 4, 7, 5, 1 };

my_bool mysqld_show_warnings(THD *thd, ulong levels_to_show)
{  
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_warnings");

  field_list.push_back(new Item_empty_string("Level", 7));
  field_list.push_back(new Item_return_int("Code",4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Message",MYSQL_ERRMSG_SIZE));

  if (thd->protocol->send_fields(&field_list,
                                 Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  MYSQL_ERROR *err;
  SELECT_LEX *sel= &thd->lex->select_lex;
  ha_rows offset= sel->offset_limit, limit= sel->select_limit;
  Protocol *protocol=thd->protocol;
  
  List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
  while ((err= it++))
  {
    /* Skip levels that the user is not interested in */
    if (!(levels_to_show & ((ulong) 1 << err->level)))
      continue;
    if (offset)
    {
      offset--;
      continue;
    }
    protocol->prepare_for_resend();
    protocol->store(warning_level_names[err->level],
		    warning_level_length[err->level], system_charset_info);
    protocol->store((uint32) err->code);
    protocol->store(err->msg, strlen(err->msg), system_charset_info);
    if (protocol->write())
      DBUG_RETURN(1);
    if (!--limit)
      break;
  }
  send_eof(thd);  
  DBUG_RETURN(0);
}
