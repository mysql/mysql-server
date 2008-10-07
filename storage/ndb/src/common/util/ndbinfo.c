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
#include <string.h>
#include <errno.h>

#include <ndbinfo.h>

int ndbinfo_create_sql(struct ndbinfo_table *t, char* sql, int len)
{
  int i;

  snprintf(sql,len,"CREATE TABLE `%s` (", t->name);

  len-=strlen(sql);
  sql+=strlen(sql);
  if(len<0)
    return ENOMEM;

  for(i=0;i<t->ncols;i++)
  {
    snprintf(sql,len,"\n\t`%s` %s,",
             t->col[i].name, ndbinfo_coltype_to_string(t->col[i].coltype));
    len-=strlen(sql);
    sql+=strlen(sql);
    if(len<0)
      return ENOMEM;
  }
  *(--sql)='\0';
  snprintf(sql,len,"\n) ENGINE=NDBINFO;");
  len-=strlen(sql);
  sql+=strlen(sql);
  if(len<0)
    return ENOMEM;

  return 0;
}
