/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "zgroups.h"


#ifdef HAVE_UGID


#include "sql_string.h"


enum_return_status String_appender::do_append(const uchar *buf, size_t length)
{
  DBUG_ENTER("String_appender::append");
  if (str->append((const char *)buf, length))
  {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


enum_return_status String_appender::do_truncate(my_off_t new_position)
{
  DBUG_ENTER("String_appender::truncate");
  str->length(new_position);
  RETURN_OK;
}


enum_return_status String_appender::do_tell(my_off_t *position) const
{
  DBUG_ENTER("String_appender::tell");
  *position= str->length();
  RETURN_OK;
}


#endif
