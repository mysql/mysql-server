/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include <my_global.h>
#include "log.h"

//#include "mysql_connection.h"

void manager(const char *socket_file_name)
{
  while (true)
  {
    log_info("manager is alive");
    sleep(2);
  }
#if 0
  /*
    Dummy manager implementation: listens on a UNIX socket and
    starts echo server in a dedicated thread for each accepted connection.
    Enough to test startup/shutdown/options/logging of the instance manager.
  */

  int fd= socket(AF_UNIX, SOCK_STREAM, 0);
  
  if (!fd)
    die("socket(): failed");

  struct sockaddr_un address;
  bzero(&address, sizeof(address));
  address.sun_family= AF_UNIX;
  strcpy(address.sun_path, socket_path);
  int opt= 1;

  if (unlink(socket_path) ||
      bind(fd, (struct sockaddr *) &address, sizeof(address)) ||
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    die("unlink | bind | setsockopt failed");
  
  if (listen(fd, 5))
    die("listen() failed");
  
  int client_fd;
  while ((client_fd= accept(fd, 0, 0)) != -1);
  {
    printf("accepted\n");
    const char *message= "\n10hel";
    send(client_fd, message, strlen(message), 0);

    int sleep_seconds= argc > 1 && atoi(argv[1]) ? atoi(argv[1]) : 1;
    printf("sleeping %d seconds\n", sleep_seconds);
    sleep(sleep_seconds);
    close(client_fd);
  }
  printf("accept(): failed\n");
  close(fd);
#endif
}
