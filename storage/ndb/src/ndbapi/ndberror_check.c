/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <stdio.h>
#include "ndberror.c"

int main()
{
  int i, j, error = 0;

  /* check for duplicate error codes */
  for(i = 0; i < NbErrorCodes; i++)
  {
    for(j = i + 1; j < NbErrorCodes; j++)
    {
      if (ErrorCodes[i].code == ErrorCodes[j].code)
      {
        fprintf(stderr, "Duplicate error code %u\n", ErrorCodes[i].code);
        error = 1;
      }
    }
  }
  if (error)
    return -1;
  return 0;
}
