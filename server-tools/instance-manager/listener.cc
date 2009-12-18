/* Copyright (C) 2003-2006 MySQL AB

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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "listener.h"

#include <my_global.h>
#include <mysql.h>
#include <violite.h>

#include <sys/stat.h>
#ifndef __WIN__
#include <sys/un.h>
#endif

#include "log.h"
#include "mysql_connection.h"
#include "options.h"
#include "portability.h"
#include "priv.h"
#include "thread_registry.h"


static void set_non_blocking(int socket)
{
#ifndef __WIN__
  int flags= fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#else
  u_long arg= 1;
  ioctlsocket(socket, FIONBIO, &arg);
#endif
}


static void set_no_inherit(int socket)
{
#ifndef __WIN__
  int flags= fcntl(socket, F_GETFD, 0);
  fcntl(socket, F_SETFD, flags | FD_CLOEXEC);
#endif
}

const int Listener::LISTEN_BACK_LOG_SIZE= 5;     /* standard backlog size */

Listener::Listener(Thread_registry *thread_registry_arg,
                   User_map *user_map_arg)
  :thread_registry(thread_registry_arg),
  user_map(user_map_arg),
  total_connection_count(0),
  num_sockets(0)
{
}


/*
  Listener::run() - listen all supported sockets and spawn a thread
  to handle incoming connection.
  Using 'die' in case of syscall failure is OK now - we don't hold any
  resources and 'die' kills the signal thread automatically. To be rewritten
  one day.
  See also comments in mysqlmanager.cc to picture general Instance Manager
  architecture.
*/

void Listener::run()
{
  int i, n= 0;

#ifndef __WIN__
  struct sockaddr_un unix_socket_address;
#endif

  log_info("Listener: started.");

  thread_registry->register_thread(&thread_info);

  FD_ZERO(&read_fds);

  /* I. prepare 'listen' sockets */
  if (create_tcp_socket())
    goto err;

#ifndef __WIN__
  if (create_unix_socket(unix_socket_address))
    goto err;
#endif

  /* II. Listen sockets and spawn childs */
  for (i= 0; i < num_sockets; i++)
    n= max(n, sockets[i]);
  n++;

  timeval tv;
  while (!thread_registry->is_shutdown())
  {
    fd_set read_fds_arg= read_fds;
    /*
      We should reintialize timer as on linux it is modified
      to reflect amount of time not slept.
    */
    tv.tv_sec= 0;
    tv.tv_usec= 100000;

    /*
      When using valgrind 2.0 this syscall doesn't get kicked off by a
      signal during shutdown. This results in failing assert
      (Thread_registry::~Thread_registry). Valgrind 2.2 works fine.
    */
    int rc= select(n, &read_fds_arg, 0, 0, &tv);

    if (rc == 0 || rc == -1)
    {
      if (rc == -1 && errno != EINTR)
        log_error("Listener: select() failed: %s.",
                  (const char *) strerror(errno));
      continue;
    }


    for (int socket_index= 0; socket_index < num_sockets; socket_index++)
    {
      /* Assuming that rc > 0 as we asked to wait forever */
      if (FD_ISSET(sockets[socket_index], &read_fds_arg))
      {
        int client_fd= accept(sockets[socket_index], 0, 0);
        /* accept may return -1 (failure or spurious wakeup) */
        if (client_fd >= 0)                    // connection established
        {
          set_no_inherit(client_fd);

          struct st_vio *vio=
            vio_new(client_fd,
                    socket_index == 0 ?  VIO_TYPE_SOCKET : VIO_TYPE_TCPIP,
                    socket_index == 0 ? 1 : 0);

          if (vio != NULL)
            handle_new_mysql_connection(vio);
          else
          {
            shutdown(client_fd, SHUT_RDWR);
            closesocket(client_fd);
          }
        }
      }
    }
  }

  /* III. Release all resources and exit */

  log_info("Listener: shutdown requested, exiting...");

  for (i= 0; i < num_sockets; i++)
    closesocket(sockets[i]);

#ifndef __WIN__
  unlink(unix_socket_address.sun_path);
#endif

  thread_registry->unregister_thread(&thread_info);

  log_info("Listener: finished.");
  return;

err:
  log_error("Listener: failed to initialize. Initiate shutdown...");

  // we have to close the ip sockets in case of error
  for (i= 0; i < num_sockets; i++)
    closesocket(sockets[i]);

  thread_registry->set_error_status();
  thread_registry->unregister_thread(&thread_info);
  thread_registry->request_shutdown();
  return;
}

int Listener::create_tcp_socket()
{
  /* value to be set by setsockopt */
  int arg= 1;

  int ip_socket= socket(AF_INET, SOCK_STREAM, 0);
  if (ip_socket == INVALID_SOCKET)
  {
    log_error("Listener: socket(AF_INET) failed: %s.",
              (const char *) strerror(errno));
    return -1;
  }

  struct sockaddr_in ip_socket_address;
  bzero(&ip_socket_address, sizeof(ip_socket_address));

  ulong im_bind_addr;
  if (Options::Main::bind_address != 0)
  {
    im_bind_addr= (ulong) inet_addr(Options::Main::bind_address);

    if (im_bind_addr == (ulong) INADDR_NONE)
      im_bind_addr= htonl(INADDR_ANY);
  }
  else
    im_bind_addr= htonl(INADDR_ANY);
  uint im_port= Options::Main::port_number;

  ip_socket_address.sin_family= AF_INET;
  ip_socket_address.sin_addr.s_addr= im_bind_addr;


  ip_socket_address.sin_port= (unsigned short)
    htons((unsigned short) im_port);

  setsockopt(ip_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &arg, sizeof(arg));
  if (bind(ip_socket, (struct sockaddr *) &ip_socket_address,
           sizeof(ip_socket_address)))
  {
    log_error("Listener: bind(ip socket) failed: %s.",
              (const char *) strerror(errno));
    closesocket(ip_socket);
    return -1;
  }

  if (listen(ip_socket, LISTEN_BACK_LOG_SIZE))
  {
    log_error("Listener: listen(ip socket) failed: %s.",
              (const char *) strerror(errno));
    closesocket(ip_socket);
    return -1;
  }

  /* set the socket nonblocking */
  set_non_blocking(ip_socket);

  /* make sure that instances won't be listening our sockets */
  set_no_inherit(ip_socket);

  FD_SET(ip_socket, &read_fds);
  sockets[num_sockets++]= ip_socket;
  log_info("Listener: accepting connections on ip socket (port: %d)...",
           (int) im_port);
  return 0;
}

#ifndef __WIN__
int Listener::
create_unix_socket(struct sockaddr_un &unix_socket_address)
{
  int unix_socket= socket(AF_UNIX, SOCK_STREAM, 0);
  if (unix_socket == INVALID_SOCKET)
  {
    log_error("Listener: socket(AF_UNIX) failed: %s.",
              (const char *) strerror(errno));
    return -1;
  }

  bzero(&unix_socket_address, sizeof(unix_socket_address));

  unix_socket_address.sun_family= AF_UNIX;
  strmake(unix_socket_address.sun_path, Options::Main::socket_file_name,
          sizeof(unix_socket_address.sun_path) - 1);
  unlink(unix_socket_address.sun_path); // in case we have stale socket file

  /*
    POSIX specifies default permissions for a pathname created by bind
    to be 0777. We need everybody to have access to the socket.
  */
  mode_t old_mask= umask(0);
  if (bind(unix_socket, (struct sockaddr *) &unix_socket_address,
           sizeof(unix_socket_address)))
  {
    log_error("Listener: bind(unix socket) failed for '%s': %s.",
              (const char *) unix_socket_address.sun_path,
              (const char *) strerror(errno));
    close(unix_socket);
    return -1;
  }

  umask(old_mask);

  if (listen(unix_socket, LISTEN_BACK_LOG_SIZE))
  {
    log_error("Listener: listen(unix socket) failed: %s.",
              (const char *) strerror(errno));
    close(unix_socket);
    return -1;
  }

  /* set the socket nonblocking */
  set_non_blocking(unix_socket);

  /* make sure that instances won't be listening our sockets */
  set_no_inherit(unix_socket);

  log_info("Listener: accepting connections on unix socket '%s'...",
           (const char *) unix_socket_address.sun_path);
  sockets[num_sockets++]= unix_socket;
  FD_SET(unix_socket, &read_fds);
  return 0;
}
#endif


/*
  Create new mysql connection. Created thread is responsible for deletion of
  the Mysql_connection and Vio instances passed to it.
  SYNOPSIS
    handle_new_mysql_connection()
*/

void Listener::handle_new_mysql_connection(struct st_vio *vio)
{
  Mysql_connection *mysql_connection=
    new Mysql_connection(thread_registry, user_map,
                         vio, ++total_connection_count);
  if (mysql_connection == NULL || mysql_connection->start(Thread::DETACHED))
  {
    log_error("Listener: can not start connection handler.");
    delete mysql_connection;
    vio_delete(vio);
  }
  /* The connection will delete itself when the thread is finished */
}
