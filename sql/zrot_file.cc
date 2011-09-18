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
#include "mysqld_error.h"


#ifdef HAVE_UGID


enum_return_status Rot_file::open(const char *_filename, bool writable)
{
  DBUG_ENTER("Rot_file::open");

  _is_writable= writable;

  size_t filename_length= strlen(_filename);
  DBUG_ASSERT(filename_length < sizeof(base_filename));
  memcpy(base_filename, _filename, filename_length + 1);
  memcpy(sub_file.filename, _filename, filename_length);
  memcpy(sub_file.filename + filename_length, "-00.0", 6);

  DBUG_PRINT("info", ("base_filename='%s' sub_file.filename='%s' writable=%d",
                      base_filename, sub_file.filename, writable));

  sub_file.fd= my_open(sub_file.filename,
                       (writable ? O_RDWR | O_CREAT : O_RDONLY) | O_BINARY,
                       MYF(MY_WME));
  if (sub_file.fd < 0)
  {
    my_error(ER_FILE_NOT_FOUND, MYF(0), sub_file.filename, errno);
    RETURN_REPORTED_ERROR;
  }

  ulonglong offset;
  int ret= Compact_encoding::read_unsigned(sub_file.fd, &offset, MYF(0));
  DBUG_PRINT("info", ("rotfile %s header length %d offset %lld",
                      sub_file.filename, ret, offset));
  if (ret <= 0)
  {
    if (offset == 0)
    {
      // IO error: generate message
      my_error(ER_ERROR_ON_READ, MYF(0), sub_file.filename, errno);
      RETURN_REPORTED_ERROR;
    }
    else if (ret == 0 && offset == 1)
    {
      // file was empty: write the header
      offset= 0;
      if (Compact_encoding::write_unsigned(sub_file.fd, offset,
                                           MYF(MY_WME)) <= 0)
        RETURN_REPORTED_ERROR;
      sub_file.header_length= 1;
    }
    else 
    {
      // file format error
      my_error(ER_FILE_FORMAT, MYF(0), sub_file.filename);
      close();
      RETURN_REPORTED_ERROR;
    }
  }
  else
    sub_file.header_length= ret;

  _is_open= true;
  RETURN_OK;
}


enum_return_status Rot_file::close()
{
  DBUG_ENTER("Rot_file::close");
  if (my_close(sub_file.fd, MYF(MY_WME)) != 0)
    RETURN_REPORTED_ERROR;
  sub_file.fd= -1;
  RETURN_OK;
}


enum_return_status Rot_file::purge(my_off_t offset)
{
  DBUG_ASSERT(0);  // @todo: implement /sven
  DBUG_ENTER("Rot_file::purge");
  RETURN_OK;
}


enum_return_status Rot_file::truncate(my_off_t offset)
{
  DBUG_ASSERT(0); // @todo: implement /sven
  DBUG_ENTER("Rot_file::truncate");
  RETURN_OK;
}


Rot_file::~Rot_file()
{
  //DBUG_ASSERT(!is_open());
}


#endif
