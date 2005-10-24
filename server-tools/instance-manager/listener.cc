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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "listener.h"
#include "priv.h"
#include <m_string.h>
#include <mysql.h>
#include <violite.h>
#ifndef __WIN__
#include <sys/un.h>
#endif
#include <sys/stat.h>

#include "thread_registry.h"
#include "options.h"
#include "instance_map.h"
#include "log.h"
#include "mysql_connection.h"
#include "portability.h"


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
  static const int LISTEN_BACK_LOG_SIZE= 5;     /* standard backlog size */
  ulong total_connection_count;
  Thread_info thread_info;

  int     sockets[2];
  int     num_sockets;
  fd_set  read_fds;
private:
  void handle_new_mysql_connection(Vio *vio);
  int   create_tcp_socket();
  int   create_unix_socket(struct sockaddr_un &unix_socket_address);
};


Listener_thread::Listener_thread(const Listener_thread_args &args) :
  Listener_thread_args(args.thread_registry, args.options, args.user_map,
                       args.instance_map)
  ,total_connection_count(0)
  ,thread_info(pthread_self())
  ,num_sockets(0)
{
}


Listener_thread::~Listener_thread()
{
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
  int n= 0;

#ifndef __WIN__
  /* we use this var to check whether we are running on LinuxThreads */
  pid_t thread_pid;

  thread_pid= getpid();

  struct sockaddr_un unix_socket_address;
  /* set global variable */
  linuxthreads= (thread_pid != manager_pid);
#endif

  thread_registry.register_thread(&thread_info);

  my_thread_init();

  FD_ZERO(&read_fds);

  /* I. prepare 'listen' sockets */
  if (create_tcp_socket())
    goto err;

#ifndef __WIN__
  if (create_unix_socket(unix_socket_address))
    goto err;
#endif

  /* II. Listen sockets and spawn childs */
  for (int i= 0; i < num_sockets; i++)
    n= max(n, sockets[i]);
  n++;

  timeval tv;
  tv.tv_sec= 0;
  tv.tv_usec= 100000;
  while (!thread_registry.is_shutdown())
  {
    fd_set read_fds_arg= read_fds;

    /*
      When using valgrind 2.0 this syscall doesn't get kicked off by a
      signal during shutdown. This results in failing assert
      (Thread_registry::~Thread_registry). Valgrind 2.2 works fine.
    */
    int rc= select(n, &read_fds_arg, 0, 0, &tv);

    if (rc == 0 || rc == -1)
    {
      if (rc == -1 && errno != EINTR)
        log_error("Listener_thread::run(): select() failed, %s",
                  strerror(errno));
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
          Vio *vio= vio_new(client_fd, socket_index == 0 ?
                            VIO_TYPE_SOCKET : VIO_TYPE_TCPIP,
                            socket_index == 0 ? 1 : 0);
          if (vio != 0)
            handle_new_mysql_connection(vio);
          else
          {
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
          }
        }
      }
    }
  }

  /* III. Release all resources and exit */

  log_info("Listener_thread::run(): shutdown requested, exiting...");

  for (int i= 0; i < num_sockets; i++)
    close(sockets[i]);

#ifndef __WIN__
  unlink(unix_socket_address.sun_path);
#endif

  thread_registry.unregister_thread(&thread_info);
  my_thread_end();
  return;

err:
  // we have to close the ip sockets in case of error
  for (int i= 0; i < num_sockets; i++)
    close(sockets[i]);

  thread_registry.unregister_thread(&thread_info);
  thread_registry.request_shutdown();
  my_thread_end();
  return;
}

void set_non_blocking(int socket)
{
#ifndef __WIN__
  int flags= fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#else
  u_long arg= 1;
  ioctlsocket(socket, FIONBIO, &arg);
#endif
}

void set_no_inherit(int socket)
{
#ifndef __WIN__
  int flags= fcntl(socket, F_GETFD, 0);
  fcntl(socket, F_SETFD, flags | FD_CLOEXEC);
#endif
}

int Listener_thread::create_tcp_socket()
{
  /* value to be set by setsockopt */
  int arg= 1;

  int ip_socket= socket(AF_INET, SOCK_STREAM, 0);
  if (ip_socket == INVALID_SOCKET)
  {
    log_error("Listener_thead::run(): socket(AF_INET) failed, %s",
              strerror(errno));
    return -1;
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
  ip_socket_address.sin_addr.s_addr= im_bind_addr;


  ip_socket_address.sin_port= (unsigned short)
    htons((unsigned short) im_port);

  setsockopt(ip_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &arg, sizeof(arg));
  if (bind(ip_socket, (struct sockaddr *) &ip_socket_address,
           sizeof(ip_socket_address)))
  {
    log_error("Listener_thread::run(): bind(ip socket) failed, '%s'",
              strerror(errno));
    close(ip_socket);
    return -1;
  }

  if (listen(ip_socket, LISTEN_BACK_LOG_SIZE))
  {
    log_error("Listener_thread::run(): listen(ip socket) failed, %s",
              strerror(errno));
    close(ip_socket);
    return -1;
  }

  /* set the socket nonblocking */
  set_non_blocking(ip_socket);

  /* make sure that instances won't be listening our sockets */
  set_no_inherit(ip_socket);

  FD_SET(ip_socket, &read_fds);
  sockets[num_sockets++]= ip_socket;
  log_info("accepting connections on ip socket");
  return 0;
}

#ifndef __WIN__
int Listener_thread::
create_unix_socket(struct sockaddr_un &unix_socket_address)
{
  int unix_socket= socket(AF_UNIX, SOCK_STREAM, 0);
  if (unix_socket == INVALID_SOCKET)
  {
    log_error("Listener_thead::run(): socket(AF_UNIX) failed, %s",
              strerror(errno));
    return -1;
  }

  bzero(&unix_socket_address, sizeof(unix_socket_address));

  unix_socket_address.sun_family= AF_UNIX;
  strmake(unix_socket_address.sun_path, options.socket_file_name,
          sizeof(unix_socket_address.sun_path));
  unlink(unix_socket_address.sun_path); // in case we have stale socket file

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
    close(unix_socket);
    return -1;
  }

  umask(old_mask);

  if (listen(unix_socket, LISTEN_BACK_LOG_SIZE))
  {
    log_error("Listener_thread::run(): listen(unix socket) failed, %s",
              strerror(errno));
    close(unix_socket);
    return -1;
  }

  /* set the socket nonblocking */
  set_non_blocking(unix_socket);

  /* make sure that instances won't be listening our sockets */
  set_no_inherit(unix_socket);

  log_info("accepting connections on unix socket %s",
           unix_socket_address.sun_path);
  sockets[num_sockets++]= unix_socket;
  FD_SET(unix_socket, &read_fds);
  return 0;
}
#endif


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


pthread_handler_t listener(void *arg)
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

