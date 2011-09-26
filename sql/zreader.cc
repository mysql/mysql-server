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


#include "mysqld_error.h"
#include "sql_class.h"


enum_read_status Reader::file_pread(File fd, uchar *buffer,
                                    size_t length, my_off_t offset)
{
  DBUG_ENTER("Reader::my_pread");
  size_t read_bytes= my_pread(fd, buffer, length, offset, MYF(0));
  if (read_bytes != length)
  {
    /*
    if (abort_loop || current_thd->killed)
    {
      /// @todo: report other error?
      my_error(ER_ERROR_ON_READ, MYF(0), get_source_name(), errno);
      DBUG_RETURN(READ_ERROR);
    }
    */
    if (read_bytes == MY_FILE_ERROR)
    {
      my_error(ER_ERROR_ON_READ, MYF(0), get_source_name(), errno);
      DBUG_RETURN(READ_ERROR);
    }
    if (read_bytes == 0)
      DBUG_RETURN(READ_EOF);
    DBUG_ASSERT(read_bytes < length);
    DBUG_RETURN(READ_TRUNCATED);
  }
  DBUG_RETURN(READ_OK);
}


enum_read_status Reader::file_seek(File fd,
                                   my_off_t old_position, my_off_t new_position)
{
  DBUG_ENTER("Reader::file_seek");
  MY_STAT stat;
  if (my_fstat(fd, &stat, MYF(MY_WME)) != 0)
    DBUG_RETURN(READ_ERROR);
  if (old_position > (my_off_t)stat.st_size)
    // should not happen unless user truncated file under our feet
    DBUG_RETURN(READ_ERROR);
  if (new_position > (my_off_t)stat.st_size)
    DBUG_RETURN(old_position == (my_off_t)stat.st_size ?
                READ_EOF : READ_TRUNCATED);
  DBUG_RETURN(READ_OK);
}


#endif
