/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include <stdio.h>
#include <stdlib.h>
#include "mysql.h"

static void change_user(MYSQL *sock,const char *user, const char *password,
			const char *db,my_bool warning)
{
  if (mysql_change_user(sock,user,password,db) != warning)
  {
    fprintf(stderr,"Couldn't change user to: user: '%s', password: '%s', db: '%s':  Error: %s\n",
	    user, password ? password : "", db ? db : "",
	    mysql_error(sock));
  }
}


int main(int argc, char **argv)
{
  MYSQL *sock;

  if (!(sock=mysql_init(0)))
  {
    fprintf(stderr,"Couldn't initialize mysql struct\n");
    exit(1);
  }
  mysql_options(sock,MYSQL_READ_DEFAULT_GROUP,"connect");
  if (!mysql_real_connect(sock,NULL,NULL,NULL,NULL,0,NULL,0))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n",mysql_error(sock));
    perror("");
    exit(1);
  }

  if (mysql_select_db(sock,"test"))
  {
    fprintf(stderr,"Couldn't select database test: Error: %s\n",
	    mysql_error(sock));
  }

  change_user(sock,"test_user","test_user","test",0);
  change_user(sock,"test",NULL,"test",0);
  change_user(sock,"test_user",NULL,"test",1);
  change_user(sock,"test_user",NULL,NULL,1);
  change_user(sock,"test_user","test_user","mysql",1);

  mysql_close(sock);
  exit(0);
  return 0;
}
