/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/*
  Code for handling strings with can grow dynamicly.
  Copyright Monty Program KB.
  By monty.
*/

#include "mysys_priv.h"
#include <m_string.h>

my_bool init_dynamic_string(DYNAMIC_STRING *str, const char *init_str,
			    size_t init_alloc, size_t alloc_increment)
{
  size_t length;
  DBUG_ENTER("init_dynamic_string");

  if (!alloc_increment)
    alloc_increment=128;
  length=1;
  if (init_str && (length= strlen(init_str)+1) < init_alloc)
    init_alloc=((length+alloc_increment-1)/alloc_increment)*alloc_increment;
  if (!init_alloc)
    init_alloc=alloc_increment;

  if (!(str->str=(char*) my_malloc(init_alloc,MYF(MY_WME))))
    DBUG_RETURN(TRUE);
  str->length=length-1;
  if (init_str)
    memcpy(str->str,init_str,length);
  str->max_length=init_alloc;
  str->alloc_increment=alloc_increment;
  DBUG_RETURN(FALSE);
}


my_bool dynstr_set(DYNAMIC_STRING *str, const char *init_str)
{
  uint length=0;
  DBUG_ENTER("dynstr_set");

  if (init_str && (length= (uint) strlen(init_str)+1) > str->max_length)
  {
    str->max_length=((length+str->alloc_increment-1)/str->alloc_increment)*
      str->alloc_increment;
    if (!str->max_length)
      str->max_length=str->alloc_increment;
    if (!(str->str=(char*) my_realloc(str->str,str->max_length,MYF(MY_WME))))
      DBUG_RETURN(TRUE);
  }
  if (init_str)
  {
    str->length=length-1;
    memcpy(str->str,init_str,length);
  }
  else
    str->length=0;
  DBUG_RETURN(FALSE);
}


my_bool dynstr_realloc(DYNAMIC_STRING *str, size_t additional_size)
{
  DBUG_ENTER("dynstr_realloc");

  if (!additional_size) DBUG_RETURN(FALSE);
  if (str->length + additional_size > str->max_length)
  {
    str->max_length=((str->length + additional_size+str->alloc_increment-1)/
		     str->alloc_increment)*str->alloc_increment;
    if (!(str->str=(char*) my_realloc(str->str,str->max_length,MYF(MY_WME))))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


my_bool dynstr_append(DYNAMIC_STRING *str, const char *append)
{
  return dynstr_append_mem(str,append,(uint) strlen(append));
}


my_bool dynstr_append_mem(DYNAMIC_STRING *str, const char *append,
			  size_t length)
{
  char *new_ptr;
  DBUG_ENTER("dynstr_append_mem");
  if (str->length+length >= str->max_length)
  {
    size_t new_length=(str->length+length+str->alloc_increment)/
      str->alloc_increment;
    new_length*=str->alloc_increment;
    if (!(new_ptr=(char*) my_realloc(str->str,new_length,MYF(MY_WME))))
      DBUG_RETURN(TRUE);
    str->str=new_ptr;
    str->max_length=new_length;
  }
  memcpy(str->str + str->length,append,length);
  str->length+=length;
  str->str[str->length]=0;			/* Safety for C programs */
  DBUG_RETURN(FALSE);
}


my_bool dynstr_trunc(DYNAMIC_STRING *str, size_t n)
{
  str->length-=n;
  str->str[str->length]= '\0';
  return FALSE;
}

/*
  Concatenates any number of strings, escapes any OS quote in the result then
  surround the whole affair in another set of quotes which is finally appended
  to specified DYNAMIC_STRING.  This function is especially useful when 
  building strings to be executed with the system() function.

  @param str Dynamic String which will have addtional strings appended.
  @param append String to be appended.
  @param ... Optional. Additional string(s) to be appended.

  @note The final argument in the list must be NullS even if no additional
  options are passed.

  @return True = Success.
*/

my_bool dynstr_append_os_quoted(DYNAMIC_STRING *str, const char *append, ...)
{
#ifdef __WIN__
  LEX_CSTRING quote= { C_STRING_WITH_LEN("\"") };
  LEX_CSTRING replace= { C_STRING_WITH_LEN("\\\"") };
#else
  LEX_CSTRING quote= { C_STRING_WITH_LEN("\'") };
  LEX_CSTRING replace= { C_STRING_WITH_LEN("'\"'\"'") };
#endif /* __WIN__ */
  my_bool ret= TRUE;
  va_list dirty_text;

  ret&= dynstr_append_mem(str, quote.str, quote.length); /* Leading quote */
  va_start(dirty_text, append);
  while (append != NullS)
  {
    const char  *cur_pos= append;
    const char *next_pos= cur_pos;

    /* Search for quote in each string and replace with escaped quote */
    while(*(next_pos= strcend(cur_pos, quote.str[0])) != '\0')
    {
      ret&= dynstr_append_mem(str, cur_pos, (uint) (next_pos - cur_pos));
      ret&= dynstr_append_mem(str, replace.str, replace.length);
      cur_pos= next_pos + 1;
    }
    ret&= dynstr_append_mem(str, cur_pos, (uint) (next_pos - cur_pos));
    append= va_arg(dirty_text, char *);
  }
  va_end(dirty_text);
  ret&= dynstr_append_mem(str, quote.str, quote.length); /* Trailing quote */

  return ret;
}


void dynstr_free(DYNAMIC_STRING *str)
{
  my_free(str->str);
  str->str= NULL;
}


/* Give over the control of the dynamic string to caller */

void dynstr_reassociate(DYNAMIC_STRING *str, char **ptr, size_t *length,
                        size_t *alloc_length)
{
  *ptr=          str->str;
  *length=       str->length;
  *alloc_length= str->max_length;
  str->str=0;
}
