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


#include "mysys_err.h"


enum_return_status Appender::truncate(my_off_t new_position)
{
  DBUG_ENTER("Appender::truncate");
  my_off_t old_position;
  PROPAGATE_REPORTED_ERROR(tell(&old_position));
  if (new_position >= old_position)
  {
    if (new_position > old_position)
    {
      my_error(EE_CANT_SEEK, MYF(0), get_source_name(), 0);
      RETURN_REPORTED_ERROR;
    }
    RETURN_OK;
  }
  PROPAGATE_REPORTED_ERROR(do_truncate(new_position));
  RETURN_OK;
}


#endif
