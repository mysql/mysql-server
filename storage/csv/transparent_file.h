/* Copyright (c) 2003, 2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

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

#include <sys/types.h>
#include <sys/stat.h>
#include <my_dir.h>


class Transparent_file
{
  File filedes;
  uchar *buff;  /* in-memory window to the file or mmaped area */
  /* current window sizes */
  my_off_t lower_bound;
  my_off_t upper_bound;
  uint buff_size;

public:

  Transparent_file();
  ~Transparent_file();

  void init_buff(File filedes_arg);
  uchar *ptr();
  my_off_t start();
  my_off_t end();
  char get_value (my_off_t offset);
  my_off_t read_next();
};
