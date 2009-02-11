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

#include <errno.h>
#include <stdio.h>
#include "ndbinfo.c"

#define DEFINE_NDBINFO_TABLE(var, name, ncols, flags, num) \
DECLARE_NDBINFO_TABLE(var, num) = { { name, ncols, flags } }


DECLARE_NDBINFO_TABLE(t1,4) = {{"t1",4,0},
                               {
                                 {"col1",NDBINFO_TYPE_STRING},
                                 {"col2",NDBINFO_TYPE_STRING},
                                 {"col3",NDBINFO_TYPE_STRING},
                                 {"col4",NDBINFO_TYPE_STRING}
                               }};

int main(int argc, char* argv[])
{
  char b[1024];

  if(ndbinfo_create_sql(&t1.t,b,sizeof(b))!=0)
  {
    printf("Failed\n");
    return -1;
  }

  printf("%s\n\n",b);

  return 0;
}
