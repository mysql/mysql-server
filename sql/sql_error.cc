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

  - Whenever an error or warning occured, it pushes the same to
    the respective list along with sending it to client.

  - When client requests the information using SHOW command, then 
    server processes from this list and returns back in the form of 
    resultset.

    syntax : SHOW [COUNT(*)] ERRORS [LIMIT [offset,] rows]
             SHOW [COUNT(*)] WARNINGS [LIMIT [offset,] rows]

***********************************************************************/
/* Handles errors and warnings .. */

#include "mysql_priv.h"

/* 
  Push the error to error list
*/

void push_error(uint code, const char *msg)
{
  THD *thd=current_thd;

  mysql_st_error *err = new mysql_st_error(code,msg,(const char*)thd->query);  
  
  if (thd->err_list.elements >= thd->max_error_count)
  {
    /* Remove the old elements and always maintain the max size 
       equal to sql_error_count.
       
       If one max_error_count using sets sql_error_count less than 
       the current list size, then make sure it always grows upto 
       sql_error_count size only

       ** BUG ** : Doesn't work in removing the old one 
       from the list, and thus SET SQL_ERROR_COUNT=x doesn't work
    */
    for (uint count=thd->err_list.elements-1; 
         count <= thd->max_error_count-1; count++)
    {      
      thd->err_list.remove_last();
    }
  }  
  thd->err_list.push_front(err);  
}

/* 
  Push the warning to warning list
*/

void push_warning(uint code, const char *msg)
{  
  THD *thd=current_thd;

  mysql_st_error *warn = new mysql_st_error(code,msg,(const char *)thd->query);   
  
  if (thd->warn_list.elements >= thd->max_warning_count)
  {
    /* Remove the old elements and always maintian the max size 
       equal to sql_error_count 
    */
    for (uint count=thd->warn_list.elements; 
         count <= thd->max_warning_count-1; count++)
    {
      thd->warn_list.remove_last();
    }
  }    
  thd->warn_list.push_front(warn);
}

/*
  List all errors 
*/

int mysqld_show_errors(THD *thd)
{  
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_errors");

  field_list.push_back(new Item_int("CODE",0,4));
  field_list.push_back(new Item_empty_string("MESSAGE",MYSQL_ERRMSG_SIZE)); 
  field_list.push_back(new Item_empty_string("QUERY",NAME_LEN));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  mysql_st_error *err;
  SELECT_LEX *sel=&thd->lex.select_lex;
  ha_rows offset = sel->offset_limit,limit = sel->select_limit;
  uint num_rows = 0;
  
  Error_iterator<mysql_st_error> it(thd->err_list);

  while(offset-- && (err = it++));/* Should be fixed with overloaded
				     operator '+' or with new funtion 
				     goto() in list ?
				  */

  while((num_rows++ < limit) && (err = it++))
  {
    thd->packet.length(0);
    net_store_data(&thd->packet,(uint32)err->code);
    net_store_data(&thd->packet,err->msg);
    net_store_data(&thd->packet,err->query);
    
    if (my_net_write(&thd->net,(char*)thd->packet.ptr(),thd->packet.length()))
      DBUG_RETURN(-1);
  }
  send_eof(&thd->net);  
  DBUG_RETURN(0);
}

/*
  Return errors count 
*/

int mysqld_show_errors_count(THD *thd)
{
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_errors_count");

  field_list.push_back(new Item_int("COUNT(*)",0,4));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  thd->packet.length(0);
  net_store_data(&thd->packet,(uint32)thd->err_list.elements);

  if (my_net_write(&thd->net,(char*) thd->packet.ptr(),thd->packet.length()))
    DBUG_RETURN(-1);  

  send_eof(&thd->net);
  DBUG_RETURN(0);
}

/*
  List all warnings 
*/

int mysqld_show_warnings(THD *thd)
{
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_warnings");

  field_list.push_back(new Item_int("CODE",0,21));
  field_list.push_back(new Item_empty_string("MESSAGE",MYSQL_ERRMSG_SIZE));   
  field_list.push_back(new Item_empty_string("QUERY",NAME_LEN));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  mysql_st_error *warn;

  
  SELECT_LEX *sel=&thd->lex.select_lex;
  ha_rows offset = sel->offset_limit,limit = sel->select_limit;
  uint num_rows = 0;
  
  Error_iterator<mysql_st_error> it(thd->warn_list);
  while(offset-- && (warn = it++));
  while((num_rows++ < limit) && (warn = it++))
  {
    thd->packet.length(0);
    net_store_data(&thd->packet,(uint32)warn->code);
    net_store_data(&thd->packet,warn->msg);
    net_store_data(&thd->packet,warn->query);
    
    if (my_net_write(&thd->net,(char*) thd->packet.ptr(),thd->packet.length()))
      DBUG_RETURN(-1);
  }
  send_eof(&thd->net);  
  DBUG_RETURN(0);
}

/*
  Return warnings count 
*/

int mysqld_show_warnings_count(THD *thd)
{
  List<Item> field_list;
  DBUG_ENTER("mysqld_show_warnings_count");

  field_list.push_back(new Item_int("COUNT(*)",0,21));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  thd->packet.length(0);
  net_store_data(&thd->packet,(uint32)thd->warn_list.elements);

  if (my_net_write(&thd->net,(char*)thd->packet.ptr(),thd->packet.length()))
    DBUG_RETURN(-1);
   
  send_eof(&thd->net);
  DBUG_RETURN(0);
}

