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


enum_group_status Rot_file::open(const char *_filename, bool _writable)
{
  DBUG_ENTER("Rot_file::open");

  writable= _writable;
  DBUG_ASSERT(strlen(_filename) < sizeof(filename));
  strcpy(filename, _filename);

  DBUG_PRINT("info", ("filename=%s writable=%d", filename, writable));

  fd= my_open(filename, (writable ? O_RDWR | O_CREAT : O_RDONLY) | O_BINARY,
              MYF(MY_WME));
  if (fd < 0)
    // @todo: my_error /sven
    DBUG_RETURN(GS_ERROR_IO);

  ulonglong offset;
  header_length= Compact_encoding::read_unsigned(fd, &offset, MYF(0));
  if (header_length == 0)
  {
    // file was empty: write the header
    if (Compact_encoding::write_unsigned(fd, 1, MYF(MY_WME)) <= 0)
      DBUG_RETURN(GS_ERROR_IO);
    header_length= 1;
  }
  else if (offset != 0)
  {
    // @todo: my_error("unknown format") /sven
    close();
    DBUG_RETURN(GS_ERROR_PARSE);
  }

  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Rot_file::close()
{
  DBUG_ENTER("Rot_file::close");
  if (my_close(fd, MYF(MY_WME)) == 0)
  {
    fd= 0;
    DBUG_RETURN(GS_SUCCESS);
  }
  DBUG_RETURN(GS_ERROR_IO);
}


enum_group_status Rot_file::purge(my_off_t offset)
{
  DBUG_ASSERT(0);  // @todo: implement /sven
  DBUG_ENTER("Rot_file::purge");
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Rot_file::truncate(my_off_t offset)
{
  DBUG_ASSERT(0); // @todo: implement /sven
  DBUG_ENTER("Rot_file::truncate");
  DBUG_RETURN(GS_SUCCESS);
}


Rot_file::~Rot_file()
{
  DBUG_ASSERT(!is_open());
}


#endif
