/* Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "storage/csv/transparent_file.h"

#include <mysql/psi/mysql_file.h>

#include "my_sys.h"          // MY_WME, MY_ALLOW_ZERO_PTR, MY_SEEK_SET

PSI_memory_key csv_key_memory_Transparent_file;

Transparent_file::Transparent_file() : lower_bound(0), buff_size(IO_SIZE)
{ 
  buff= (uchar *) my_malloc(csv_key_memory_Transparent_file,
                            buff_size*sizeof(uchar),  MYF(MY_WME)); 
}

Transparent_file::~Transparent_file()
{ 
  my_free(buff);
}

void Transparent_file::init_buff(File filedes_arg)
{
  filedes= filedes_arg;
  /* read the beginning of the file */
  lower_bound= 0;
  mysql_file_seek(filedes, 0, MY_SEEK_SET, MYF(0));
  if (filedes && buff)
    upper_bound= mysql_file_read(filedes, buff, buff_size, MYF(0));
}

uchar *Transparent_file::ptr()
{ 
  return buff; 
}

my_off_t Transparent_file::start()
{ 
  return lower_bound; 
}

my_off_t Transparent_file::end()
{ 
  return upper_bound; 
}

my_off_t Transparent_file::read_next()
{
  size_t bytes_read;

  /*
     No need to seek here, as the file managed by Transparent_file class
     always points to upper_bound byte
  */
  if ((bytes_read= mysql_file_read(filedes, buff, buff_size, MYF(0)))
      == MY_FILE_ERROR)
    return (my_off_t) -1;

  /* end of file */
  if (!bytes_read)
    return (my_off_t) -1;

  lower_bound= upper_bound;
  upper_bound+= bytes_read;

  return lower_bound;
}


char Transparent_file::get_value(my_off_t offset)
{
  size_t bytes_read;

  /* check boundaries */
  if ((lower_bound <= offset) && (((my_off_t) offset) < upper_bound))
    return buff[offset - lower_bound];

  mysql_file_seek(filedes, offset, MY_SEEK_SET, MYF(0));
  /* read appropriate portion of the file */
  if ((bytes_read= mysql_file_read(filedes, buff, buff_size,
                                   MYF(0))) == MY_FILE_ERROR)
    return 0;

  lower_bound= offset;
  upper_bound= lower_bound + bytes_read;

  /* end of file */
  if (upper_bound == (my_off_t) offset)
    return 0;

  return buff[0];
}
