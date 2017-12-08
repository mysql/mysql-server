/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <stdio.h>
#include <NdbDictionary.hpp>
#include "ndberror.c"

#include <NdbTap.hpp>

TAPTEST(ndberror_check)
{
  int ok= 1;
  /* check for duplicate error codes */
  for(int i = 0; i < NbErrorCodes; i++)
  {
    for(int j = i + 1; j < NbErrorCodes; j++)
    {
      if (ErrorCodes[i].code == ErrorCodes[j].code)
      {
        fprintf(stderr, "Duplicate error code %u\n", ErrorCodes[i].code);
        ok = 0;
      }
    }
  }
  return ok;
}

