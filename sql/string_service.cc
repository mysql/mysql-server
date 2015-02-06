/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

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
  This file provide mysql_string service to plugins.
  operations on mysql_string can be performed by plugins via these service
  functions.
*/

#include <my_sys.h>
#include "string_service.h"
#include "mysql/service_mysql_string.h"
/* key_memory_string_iterator */
#include "mysqld.h"
PSI_memory_key key_memory_string_iterator;

/*  
  This service function converts the mysql_string to the character set
  specified by charset_name parameter.
*/
extern "C"
int mysql_string_convert_to_char_ptr(mysql_string_handle string_handle,
                                     const char *charset_name,
                                     char *buffer,
                                     unsigned int buffer_size,
                                     int *error)
{
  String *str= (String *) string_handle;
  int len= (int)my_convert(buffer, buffer_size - 1, &my_charset_utf8_general_ci,
                           str->ptr(), str->length(), str->charset(),
                           (uint*) error);
  buffer[len]= '\0';
  return (len);
}

/*  
  This service function deallocates the mysql_string_handle allocated on
  server and used in plugins.
*/
extern "C"
void mysql_string_free(mysql_string_handle string_handle)
{
  String *str= (String *) string_handle;
  str->mem_free();
  delete [] str;
}

/*  
  This service function deallocates the mysql_string_iterator_handle
  allocated on server and used in plugins.
*/
extern "C"
void mysql_string_iterator_free(mysql_string_iterator_handle iterator_handle)
{
  my_free((string_iterator *) iterator_handle);
}

/* This service function allocate mysql_string_iterator_handle and return it */
extern "C"
mysql_string_iterator_handle mysql_string_get_iterator(mysql_string_handle
                                                       string_handle)
{
  String *str= (String *) string_handle;
  string_iterator *iterator= (string_iterator *) my_malloc(key_memory_string_iterator,
                                                           sizeof
                                           (struct st_string_iterator), MYF(0));
  iterator->iterator_str= str;
  iterator->iterator_ptr= str->ptr();
  iterator->ctype= 0;
  return (iterator);
}

/* Provide service which returns the next mysql_string_iterator_handle */
extern "C"
int mysql_string_iterator_next(mysql_string_iterator_handle iterator_handle)
{
  int char_len, char_type, tmp_len;
  string_iterator *iterator= (string_iterator *) iterator_handle;
  String *str= iterator->iterator_str;
  const CHARSET_INFO *cs= str->charset();
  char *end= (char*) str->ptr() + str->length();
  if (iterator->iterator_ptr >= (const char*) end)
    return (0);
  char_len= (cs->cset->ctype(cs, &char_type, (uchar*) iterator->iterator_ptr,
                             (uchar*) end));
  iterator->ctype= char_type;
  tmp_len= (char_len > 0 ? char_len : (char_len < 0 ? -char_len : 1));
  if(iterator->iterator_ptr+tmp_len > end)
    return (0);
  else
    iterator->iterator_ptr+= tmp_len;
  return (1);
}

/*  
  Provide service which calculate weather the current iterator_ptr points to
  upper case character or not
*/
extern "C"
int mysql_string_iterator_isupper(mysql_string_iterator_handle iterator_handle)
{
  string_iterator *iterator= (string_iterator *) iterator_handle;
  return (iterator->ctype & _MY_U);
}

/*  
  Provide service which calculate weather the current iterator_ptr points to
  lower case character or not
*/
extern "C"
int mysql_string_iterator_islower(mysql_string_iterator_handle iterator_handle)
{
  string_iterator *iterator= (string_iterator *) iterator_handle;
  return (iterator->ctype & _MY_L);
}

/*  
  Provide service which calculate weather the current iterator_ptr points to
  digit or not
*/
extern "C"
int mysql_string_iterator_isdigit(mysql_string_iterator_handle iterator_handle)
{
  string_iterator *iterator= (string_iterator *) iterator_handle;
  return (iterator->ctype & _MY_NMR);
}

/*  
  This function provide plugin service to convert a String pointed by handle to
  lower case. Conversion depends on the client character set info
*/
extern "C"
mysql_string_handle mysql_string_to_lowercase(mysql_string_handle string_handle)
{
  String *str= (String *) string_handle;
  String *res= new String[1];
  const CHARSET_INFO *cs= str->charset();
  if (cs->casedn_multiply == 1)
  {
    res->copy(*str);
    my_casedn_str(cs, res->c_ptr_quick());
  }
  else
  {
    size_t len= str->length() * cs->casedn_multiply;
    res->set_charset(cs);
    res->alloc(len);
    len= cs->cset->casedn(cs, (char*) str->ptr(), str->length(), (char *) res->ptr(), len);
    res->length(len);
  }
  return (res);
}
