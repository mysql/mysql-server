/* Copyright (C) 2000 MySQL AB

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

#ifdef __WIN__
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "mysql.h"

#define SELECT_QUERY "select name from test where num = %d"


int main(int argc, char **argv)
{
  int	count, num;
  MYSQL mysql,*sock;
  MYSQL_RES *res;
  char	qbuf[160];

  if (argc != 2)
  {
    fprintf(stderr,"usage : select_test <dbname>\n\n");
    exit(1);
  }

  if (!(sock = mysql_connect(&mysql,NULL,0,0)))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n\n",mysql_error(&mysql));
    perror("");
    exit(1);
  }
  mysql.reconnect= 1;

  if (mysql_select_db(sock,argv[1]) < 0)
  {
    fprintf(stderr,"Couldn't select database %s!\n%s\n",argv[1],
	    mysql_error(sock));
    exit(1);
  }

  if (!(res=mysql_list_dbs(sock,NULL)))
  {
    fprintf(stderr,"Couldn't list dbs!\n%s\n",mysql_error(sock));
    exit(1);
  }
  mysql_free_result(res);
  if (!(res=mysql_list_tables(sock,NULL)))
  {
    fprintf(stderr,"Couldn't list tables!\n%s\n",mysql_error(sock));
    exit(1);
  }
  mysql_free_result(res);

  mysql_close(sock);
  exit(0);
  return 0;
}
