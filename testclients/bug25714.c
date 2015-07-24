/* Copyright (c) 2007, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <m_string.h>
#include <assert.h>

int main (int argc, char **argv)
{
  MYSQL conn;
  int OK;

  const char* query4= "INSERT INTO federated.t1 SET Value=54";
  const char* query5= "INSERT INTO federated.t1 SET Value=55";

  MY_INIT(argv[0]);

  if (argc != 2 || !strcmp(argv[1], "--help"))
  {
    fprintf(stderr, "This program is a part of the MySQL test suite. "
            "It is not intended to be executed directly by a user.\n");
    return -1;
  }

  mysql_init(&conn);
  if (!mysql_real_connect(
                          &conn,
                          "127.0.0.1",
                          "root",
                          "",
                          "test",
                          atoi(argv[1]),
                          NULL,
                          CLIENT_FOUND_ROWS))
  {
    fprintf(stderr, "Failed to connect to database: Error: %s\n",
            mysql_error(&conn));
    return 1;
  } else {
    printf("%s\n", mysql_error(&conn));
  }

  OK = mysql_real_query (&conn, query4, (ulong) strlen(query4));

  assert(0 == OK);

  printf("%ld inserted\n",
         (long) mysql_insert_id(&conn));

  OK = mysql_real_query (&conn, query5, (ulong) strlen(query5));

  assert(0 == OK);

  printf("%ld inserted\n",
         (long) mysql_insert_id(&conn));

  mysql_close(&conn);
  my_end(0);

  return 0;
}

