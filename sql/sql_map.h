/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* interface for memory mapped files */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class mapped_files :public ilink {
  byte *map;
  ha_rows size;
  char *name;					// name of mapped file
  File file;					// >= 0 if open
  int  error;					// If not mapped
  uint use_count;

public:
  mapped_files(const my_string name,byte *magic,uint magic_length);
  ~mapped_files();

  friend class mapped_file;
  friend mapped_files *map_file(const my_string name,byte *magic,
				uint magic_length);
  friend void unmap_file(mapped_files *map);
};


class mapped_file
{
  mapped_files *file;
public:
  mapped_file(const my_string name,byte *magic,uint magic_length)
  {
    file=map_file(name,magic,magic_length);	/* old or new map */
  }
  ~mapped_file()
  {
    unmap_file(file);				/* free map */
  }
  byte *map()
  {
    return file->map;
  }
};
