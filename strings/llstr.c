/* Copyright (C) 2000 MySQL AB

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

/*
  Defines: llstr();

  llstr(value, buff);

  This function saves a longlong value in a buffer and returns the pointer to
  the buffer.  This is useful when trying to portable print longlong
  variables with printf() as there is no usable printf() standard one can use.
*/


#include <my_global.h>
#include "m_string.h"

char *llstr(longlong value,char *buff)
{
  longlong10_to_str(value,buff,-10);
  return buff;
}
