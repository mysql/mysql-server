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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

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

/*
  Reset all warnings for the thread

  SYNOPSIS
    mysql_reset_errors()
    thd			Thread handle
*/  

void mysql_reset_errors(THD *thd)
{
  free_root(&thd->warn_root,MYF(0));
  bzero((char*) thd->warn_count, sizeof(thd->warn_count));
  thd->warn_list.empty();
}


/* 
  Push the warning/error to error list if there is still room in the list

  SYNOPSIS
    push_warning()
    thd			Thread handle
    level		Severity of warning (note, warning, error ...)
    code		Error number
    msg			Clear error message
*/

void push_warning(THD *thd, MYSQL_ERROR::enum_warning_level level, uint code,
		  const char *msg)
{
  if (thd->warn_list.elements < thd->variables.max_error_count)
  {
    /*
      The following code is here to change the allocation to not
      use the thd->mem_root, which is freed after each query
    */
    MEM_ROOT *old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
    my_pthread_setspecific_ptr(THR_MALLOC, &thd->warn_root);
    MYSQL_ERROR *err= new MYSQL_ERROR(code, level, msg);
    if (err)
      thd->warn_list.push_back(err);
    my_pthread_setspecific_ptr(THR_MALLOC, old_root);
  }
  thd->warn_count[(uint) level]++;
  thd->total_warn_count++;
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


my_bool mysqld_show_warnings(THD *thd, ulong levels_to_show)
{  
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_errors");

  field_list.push_back(new Item_empty_string("Level", 7));
  field_list.push_back(new Item_int("Code",0,4));
  field_list.push_back(new Item_empty_string("Message",MYSQL_ERRMSG_SIZE));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  MYSQL_ERROR *err;
  SELECT_LEX *sel= &thd->lex.select_lex;
  ha_rows offset= sel->offset_limit, limit= sel->select_limit;
  
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
    thd->packet.length(0);
    net_store_data(&thd->packet,warning_level_names[err->level]);
    net_store_data(&thd->packet,(uint32) err->code);
    net_store_data(&thd->packet,err->msg);
    if (my_net_write(&thd->net,(char*)thd->packet.ptr(),thd->packet.length()))
      DBUG_RETURN(1);
    if (!--limit)
      break;
  }
  send_eof(thd);  
  DBUG_RETURN(0);
}
