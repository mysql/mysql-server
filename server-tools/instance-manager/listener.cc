/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#ifdef __GNUC__
#pragma implementation
#endif

#include "listener.h"

#include <m_string.h>
#include <mysql.h>
#include <violite.h>
#include <sys/un.h>

#include "thread_registry.h"
#include "options.h"
#include "instance_map.h"
#include "log.h"
#include "mysql_connection.h"


/*
  Listener_thread - incapsulates listening functionality
*/

class Listener_thread: public Listener_thread_args
{
public:
  Listener_thread(const Listener_thread_args &args);
  ~Listener_thread();
  void run();
private:
  ulong total_connection_count;
  Thread_info thread_info;
private:
  void handle_new_mysql_connection(Vio *vio);
};


Listener_thread::Listener_thread(const Listener_thread_args &args) :
  Listener_thread_args(args.thread_registry, args.options, args.user_map,
                       args.instance_map)
  ,total_connection_count(0)
  ,thread_info(pthread_self())
{
  thread_registry.register_thread(&thread_info);
}


Listener_thread::~Listener_thread()
{
  thread_registry.unregister_thread(&thread_info);
}


/*
  Listener_thread::run() - listen all supported sockets and spawn a thread
  to handle incoming connection.
  Using 'die' in case of syscall failure is OK now - we don't hold any
  resources and 'die' kills the signal thread automatically. To be rewritten
  one day.
  See also comments in mysqlmanager.cc to picture general Instance Manager
  architecture.
*/

void Listener_thread::run()
{
  enum { LISTEN_BACK_LOG_SIZE = 5 };            // standard backlog size
  int flags;
  int arg= 1;                             /* value to be set by setsockopt */
  /* I. prepare 'listen' sockets */

  int ip_socket= socket(AF_INET, SOCK_STREAM, 0);
  if (ip_socket == INVALID_SOCKET)
  {
    log_error("Listener_thead::run(): socket(AF_INET) failed, %s",
              strerror(errno));
    thread_registry.request_shutdown();
    return;
  }

  struct sockaddr_in ip_socket_address;
  bzero(&ip_socket_address, sizeof(ip_socket_address));

  ulong im_bind_addr;
  if (options.bind_address != 0)
  {
    if ((im_bind_addr= (ulong) inet_addr(options.bind_address)) == INADDR_NONE)
      im_bind_addr= htonl(INADDR_ANY);
  }
  else
    im_bind_addr= htonl(INADDR_ANY);
  uint im_port= options.port_number;

  ip_socket_address.sin_family= AF_INET;
  ip_socket_address.sin_addr.s_addr = im_bind_addr;


  ip_socket_address.sin_port= (unsigned short)
                              htons((unsigned short) im_port);

  setsockopt(ip_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &arg, sizeof(arg));
  if (bind(ip_socket, (struct sockaddr *) &ip_socket_address,
           sizeof(ip_socket_address)))
  {
    log_error("Listener_thread::run(): bind(ip socket) failed, '%s'",
              strerror(errno));
    thread_registry.request_shutdown();
    return;
  }

  if (listen(ip_socket, LISTEN_BACK_LOG_SIZE))
  {
    log_error("Listener_thread::run(): listen(ip socket) failed, %s",
              strerror(errno));
    thread_registry.request_shutdown();
    return;
  }
      /* set the socket nonblocking */
  flags= fcntl(ip_socket, F_GETFL, 0);
  fcntl(ip_socket, F_SETFL, flags | O_NONBLOCK);
    /* make sure that instances won't be listening our sockets */
  flags= fcntl(ip_socket, F_GETFD, 0);
  fcntl(ip_socket, F_SETFD, flags | FD_CLOEXEC);

  log_info("accepting connections on ip socket");

  /*--------------------------------------------------------------*/
  int unix_socket= socket(AF_UNIX, SOCK_STREAM, 0);
  if (unix_socket == INVALID_SOCKET)
  {
    log_error("Listener_thead::run(): socket(AF_UNIX) failed, %s",
              strerror(errno));
    thread_registry.request_shutdown();
    return;
  }

  struct sockaddr_un unix_socket_address;
  bzero(&unix_socket_address, sizeof(unix_socket_address));

  unix_socket_address.sun_family= AF_UNIX;
  strmake(unix_socket_address.sun_path, options.socket_file_name,
          sizeof(unix_socket_address.sun_path));
  unlink(unix_socket_address.sun_path); // in case we have stale socket file

  {
    /*
      POSIX specifies default permissions for a pathname created by bind
      to be 0777. We need everybody to have access to the socket.
    */
    mode_t old_mask= umask(0);
    if (bind(unix_socket, (struct sockaddr *) &unix_socket_address,
             sizeof(unix_socket_address)))
    {
      log_error("Listener_thread::run(): bind(unix socket) failed, "
                "socket file name is '%s', error '%s'",
                unix_socket_address.sun_path, strerror(errno));
      thread_registry.request_shutdown();
      return;
    }
    umask(old_mask);

    if (listen(unix_socket, LISTEN_BACK_LOG_SIZE))
    {
      log_error("Listener_thread::run(): listen(unix socket) failed, %s",
                strerror(errno));
      thread_registry.request_shutdown();
      return;
    }

      /* set the socket nonblocking */
    flags= fcntl(unix_socket, F_GETFL, 0);
    fcntl(unix_socket, F_SETFL, flags | O_NONBLOCK);
      /* make sure that instances won't be listening our sockets */
    flags= fcntl(unix_socket, F_GETFD, 0);
    fcntl(unix_socket, F_SETFD, flags | FD_CLOEXEC);
  }
  log_info("accepting connections on unix socket %s",
           unix_socket_address.sun_path);

  /* II. Listen sockets and spawn childs */

  {
    int n= max(unix_socket, ip_socket) + 1;
    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(unix_socket, &read_fds);
    FD_SET(ip_socket, &read_fds);

    while (thread_registry.is_shutdown() == false)
    {
      fd_set read_fds_arg= read_fds;
      int rc= select(n, &read_fds_arg, 0, 0, 0);
      if (rc == -1 && errno != EINTR)
        log_error("Listener_thread::run(): select() failed, %s",
                  strerror(errno));
      else
      {
        /* Assuming that rc > 0 as we asked to wait forever */
        if (FD_ISSET(unix_socket, &read_fds_arg))
        {
          int client_fd= accept(unix_socket, 0, 0);
          /* accept may return -1 (failure or spurious wakeup) */
          if (client_fd >= 0)                    // connection established
          {
            if (Vio *vio= vio_new(client_fd, VIO_TYPE_SOCKET, 1))
              handle_new_mysql_connection(vio);
            else
            {
              shutdown(client_fd, SHUT_RDWR);
              close(client_fd);
            }
          }
        }
        else
          if (FD_ISSET(ip_socket, &read_fds_arg))
          {
            int client_fd= accept(ip_socket, 0, 0);
            /* accept may return -1 (failure or spurious wakeup) */
            if (client_fd >= 0)                    // connection established
            {
              if (Vio *vio= vio_new(client_fd, VIO_TYPE_TCPIP, 0))
              {
                handle_new_mysql_connection(vio);
              }
              else
              {
                shutdown(client_fd, SHUT_RDWR);
                close(client_fd);
              }
            }
        }
      }
    }
  }

  /* III. Release all resources and exit */

  log_info("Listener_thread::run(): shutdown requested, exiting...");

  close(unix_socket);
  close(ip_socket);
  unlink(unix_socket_address.sun_path);
}


/*
  Create new mysql connection. Created thread is responsible for deletion of
  the Mysql_connection_thread_args and Vio instances passed to it.
  SYNOPSYS
    handle_new_mysql_connection()
*/

void Listener_thread::handle_new_mysql_connection(Vio *vio)
{
  if (Mysql_connection_thread_args *mysql_thread_args=
      new Mysql_connection_thread_args(vio, thread_registry, user_map,
                                       ++total_connection_count,
                                       instance_map)
      )
  {
    /*
      Initialize thread attributes to create detached thread; it seems
      easier to do it ad-hoc than have a global variable for attributes.
    */
    pthread_t mysql_thd_id;
    pthread_attr_t mysql_thd_attr;
    pthread_attr_init(&mysql_thd_attr);
    pthread_attr_setdetachstate(&mysql_thd_attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&mysql_thd_id, &mysql_thd_attr, mysql_connection,
                       mysql_thread_args))
    {
      delete mysql_thread_args;
      vio_delete(vio);
      log_error("handle_one_mysql_connection(): pthread_create(mysql) failed");
    }
    pthread_attr_destroy(&mysql_thd_attr);
  }
  else
    vio_delete(vio);
}


C_MODE_START


pthread_handler_decl(listener, arg)
{
  Listener_thread_args *args= (Listener_thread_args *) arg;
  Listener_thread listener(*args);
  listener.run();
  /*
    args is a stack variable because listener thread lives as long as the
    manager process itself
  */
  return 0;
}


C_MODE_END

