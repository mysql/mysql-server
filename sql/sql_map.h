#ifndef SQL_MAP_INCLUDED
#define SQL_MAP_INCLUDED

/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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


/* interface for memory mapped files */

#ifdef USE_PRAGMA_INTERFACE 
#pragma interface			/* gcc class implementation */
#endif

#include "my_base.h"                            /* ha_rows */
#include "sql_list.h"                           /* ilink */

class mapped_files;
mapped_files *map_file(const char * name,uchar *magic,uint magic_length);
void unmap_file(mapped_files *map);

class mapped_files :public ilink {
  uchar *map;
  ha_rows size;
  char *name;					// name of mapped file
  File file;					// >= 0 if open
  int  error;					// If not mapped
  uint use_count;

public:
  mapped_files(const char * name,uchar *magic,uint magic_length);
  ~mapped_files();

  friend class mapped_file;
  friend mapped_files *map_file(const char * name,uchar *magic,
				uint magic_length);
  friend void unmap_file(mapped_files *map);
};


class mapped_file
{
  mapped_files *file;
public:
  mapped_file(const char * name,uchar *magic,uint magic_length)
  {
    file=map_file(name,magic,magic_length);	/* old or new map */
  }
  ~mapped_file()
  {
    unmap_file(file);				/* free map */
  }
  uchar *map()
  {
    return file->map;
  }
};

#endif /* SQL_MAP_INCLUDED */
