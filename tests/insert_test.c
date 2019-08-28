/* Copyright (c) 2000-2004 MySQL AB
   Use is subject to license terms

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <stdio.h>
#include <stdlib.h>
#include "mysql.h"

#define INSERT_QUERY "insert into test (name,num) values ('item %d', %d)"


int main(int argc, char **argv)
{
  int	count,num;
  MYSQL *sock,mysql;
  char	qbuf[160];

  if (argc != 3)
  {
    fprintf(stderr,"usage : insert_test <dbname> <Num>\n\n");
    exit(1);
  }

  mysql_init(&mysql);
  if (!(sock = mysql_real_connect(&mysql,NULL,NULL,NULL,argv[1],0,NULL,0)))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n",mysql_error(&mysql));
    perror("");
    exit(1);
  }
  mysql.reconnect= 1;

  num = atoi(argv[2]);
  count = 0;
  while (count < num)
  {
    sprintf(qbuf,INSERT_QUERY,count,count);
    if(mysql_query(sock,qbuf))
    {
      fprintf(stderr,"Query failed (%s)\n",mysql_error(sock));
      exit(1);
    }
    count++;
  }
  mysql_close(sock);
  exit(0);
  return 0;
}
