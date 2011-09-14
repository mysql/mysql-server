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


Rot_file::Rot_file(const char *_filename)
  : fd(-1)
{
  DBUG_ASSERT(strlen(_filename) < sizeof(filename));
  strcpy(filename, _filename);
}


int Rot_file::open(bool write)
{
  DBUG_ENTER("Rot_file::open");

  fd= my_open(filename, (writable ? O_RDWR | O_CREAT : O_RDONLY) | O_BINARY,
              MYF(MY_WME));
  if (fd < 0)
    // @todo: my_error /sven
    DBUG_RETURN(1);

  ulonglong offset;
  header_length= Compact_encoding::read_unsigned(fd, &offset, MYF(0));
  if (header_length == 0)
  {
    // file was empty: write the header
    if (Compact_encoding::write_unsigned(fd, 1, MYF(MY_WME)) <= 0)
      DBUG_RETURN(1);
    header_length= 1;
  }
  else if (header_length == 0 || offset != 0)
  {
    // @todo: my_error /sven
    close();
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


int Rot_file::close()
{
  DBUG_ENTER("Rot_file::close");
  if (my_close(fd, MYF(MY_WME)) == 0)
  {
    fd= 0;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


my_off_t Rot_file::pread(my_off_t offset, my_off_t length, uchar *buffer) const
{
  return my_pread(fd, buffer, length, offset, MYF(MY_WME));
}


int Rot_file::purge(my_off_t offset)
{
  DBUG_ASSERT(0);  // @todo: implement /sven
  DBUG_ENTER("Rot_file::purge");
  DBUG_RETURN(0);
}


int Rot_file::truncate(my_off_t offset)
{
  DBUG_ASSERT(0); // @todo: implement /sven
  DBUG_ENTER("Rot_file::truncate");
  DBUG_RETURN(0);
}


Rot_file::~Rot_file()
{
  DBUG_ASSERT(!is_open());
}


#endif
