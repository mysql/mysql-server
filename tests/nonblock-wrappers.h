/*
  Copyright 2011 Kristian Nielsen and Monty Program Ab

  This file is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  Wrappers that re-implement the normal blocking libmysql API calls in terms
  of the non-blocking API calls and explicit waiting.

  Used to test the non-blocking calls using mysql_client_test.
*/

#ifndef __WIN__
#include <poll.h>
#else
#include <WinSock2.h>
#endif

/*
  Run the appropriate poll() syscall to wait for the event that libmysql
  requested. Return which event(s) occured.
*/
static int
wait_for_mysql(MYSQL *mysql, int status)
{
#ifdef __WIN__
  fd_set rs, ws, es;
  int res;
  struct timeval tv, *timeout;
  my_socket s= mysql_get_socket(mysql);
  FD_ZERO(&rs);
  FD_ZERO(&ws);
  FD_ZERO(&es);
  if (status & MYSQL_WAIT_READ)
    FD_SET(s, &rs);
  if (status & MYSQL_WAIT_WRITE)
    FD_SET(s, &ws);
  if (status & MYSQL_WAIT_EXCEPT)
    FD_SET(s, &es);
  if (status & MYSQL_WAIT_TIMEOUT)
  {
    tv.tv_sec= mysql_get_timeout_value(mysql);
    tv.tv_usec= 0;
    timeout= &tv;
  }
  else
    timeout= NULL;
  res= select(1, &rs, &ws, &es, timeout);
  if (res == 0)
    return MYSQL_WAIT_TIMEOUT;
  else if (res == SOCKET_ERROR)
    return MYSQL_WAIT_TIMEOUT;
  else
  {
    int status= 0;
    if (FD_ISSET(s, &rs))
      status|= MYSQL_WAIT_READ;
    if (FD_ISSET(s, &ws))
      status|= MYSQL_WAIT_WRITE;
    if (FD_ISSET(s, &es))
      status|= MYSQL_WAIT_EXCEPT;
    return status;
  }
#else
  struct pollfd pfd;
  int timeout;
  int res;

  pfd.fd= mysql_get_socket(mysql);
  pfd.events=
    (status & MYSQL_WAIT_READ ? POLLIN : 0) |
    (status & MYSQL_WAIT_WRITE ? POLLOUT : 0) |
    (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);
  if (status & MYSQL_WAIT_TIMEOUT)
    timeout= 1000*mysql_get_timeout_value(mysql);
  else
    timeout= -1;
  do {
    res= poll(&pfd, 1, timeout);
    /*
      In a real event framework, we should re-compute the timeout on getting
      EINTR to account for the time elapsed before the interruption.
    */
  } while (res < 0 && errno == EINTR);
  if (res == 0)
    return MYSQL_WAIT_TIMEOUT;
  else if (res < 0)
    return MYSQL_WAIT_TIMEOUT;
  else
  {
    int status= 0;
    if (pfd.revents & POLLIN)
      status|= MYSQL_WAIT_READ;
    if (pfd.revents & POLLOUT)
      status|= MYSQL_WAIT_WRITE;
    if (pfd.revents & POLLPRI)
      status|= MYSQL_WAIT_EXCEPT;
    return status;
  }
#endif
}


/*
  If WRAP_NONBLOCK_ENABLED is defined, it is a variable that can be used to
  enable or disable the use of non-blocking API wrappers. If true the
  non-blocking API will be used, if false the normal blocking API will be
  called directly.
*/
#ifdef WRAP_NONBLOCK_ENABLED
#define USE_BLOCKING(name__, invoke_blocking__) \
  if (!(WRAP_NONBLOCK_ENABLED)) return name__ invoke_blocking__;
#define USE_BLOCKING_VOID_RETURN(name__, invoke__) \
  if (!(WRAP_NONBLOCK_ENABLED)) { name__ invoke__; return; }
#else
#define USE_BLOCKING(name__, invoke_blocking__)
#define USE_BLOCKING_VOID_RETURN(name__, invoke__)
#endif

/*
  I would preferably have declared the wrappers static.
  However, if we do so, compilers will warn about definitions not used, and
  with -Werror this breaks compilation :-(
*/
#define MK_WRAPPER(ret_type__, name__, decl__, invoke__, invoke_blocking__, cont_arg__, mysql__) \
ret_type__ wrap_ ## name__ decl__                                             \
{                                                                             \
  ret_type__ res;                                                             \
  int status;                                                                 \
  USE_BLOCKING(name__, invoke_blocking__)                                     \
  status= name__ ## _start invoke__;                                          \
  while (status)                                                              \
  {                                                                           \
    status= wait_for_mysql(mysql__, status);                                  \
    status= name__ ## _cont(&res, cont_arg__, status);                        \
  }                                                                           \
  return res;                                                                 \
}

#define MK_WRAPPER_VOID_RETURN(name__, decl__, invoke__, cont_arg__, mysql__) \
void wrap_ ## name__ decl__                                                   \
{                                                                             \
  int status;                                                                 \
  USE_BLOCKING_VOID_RETURN(name__, invoke__)                                  \
  status= name__ ## _start invoke__;                                          \
  while (status)                                                              \
  {                                                                           \
    status= wait_for_mysql(mysql__, status);                                  \
    status= name__ ## _cont(cont_arg__, status);                              \
  }                                                                           \
}

MK_WRAPPER(
  MYSQL *,
  mysql_real_connect,
  (MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long clientflag),
  (&res, mysql, host, user, passwd, db, port, unix_socket, clientflag),
  (mysql, host, user, passwd, db, port, unix_socket, clientflag),
  mysql,
  mysql)


MK_WRAPPER(
  int,
  mysql_real_query,
  (MYSQL *mysql, const char *stmt_str, unsigned long length),
  (&res, mysql, stmt_str, length),
  (mysql, stmt_str, length),
  mysql,
  mysql)

MK_WRAPPER(
  MYSQL_ROW,
  mysql_fetch_row,
  (MYSQL_RES *result),
  (&res, result),
  (result),
  result,
  result->handle)

MK_WRAPPER(
  int,
  mysql_set_character_set,
  (MYSQL *mysql, const char *csname),
  (&res, mysql, csname),
  (mysql, csname),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_select_db,
  (MYSQL *mysql, const char *db),
  (&res, mysql, db),
  (mysql, db),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_send_query,
  (MYSQL *mysql, const char *q, unsigned long length),
  (&res, mysql, q, length),
  (mysql, q, length),
  mysql,
  mysql)

MK_WRAPPER(
  MYSQL_RES *,
  mysql_store_result,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER_VOID_RETURN(
  mysql_free_result,
  (MYSQL_RES *result),
  (result),
  result,
  result->handle)

MK_WRAPPER_VOID_RETURN(
  mysql_close,
  (MYSQL *sock),
  (sock),
  sock,
  sock)

MK_WRAPPER(
  my_bool,
  mysql_change_user,
  (MYSQL *mysql, const char *user, const char *passwd, const char *db),
  (&res, mysql, user, passwd, db),
  (mysql, user, passwd, db),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_query,
  (MYSQL *mysql, const char *q),
  (&res, mysql, q),
  (mysql, q),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_shutdown,
  (MYSQL *mysql, enum mysql_enum_shutdown_level shutdown_level),
  (&res, mysql, shutdown_level),
  (mysql, shutdown_level),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_dump_debug_info,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_refresh,
  (MYSQL *mysql, unsigned int refresh_options),
  (&res, mysql, refresh_options),
  (mysql, refresh_options),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_kill,
  (MYSQL *mysql, unsigned long pid),
  (&res, mysql, pid),
  (mysql, pid),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_set_server_option,
  (MYSQL *mysql, enum enum_mysql_set_option option),
  (&res, mysql, option),
  (mysql, option),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_ping,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  const char *,
  mysql_stat,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  MYSQL_RES *,
  mysql_list_dbs,
  (MYSQL *mysql, const char *wild),
  (&res, mysql, wild),
  (mysql, wild),
  mysql,
  mysql)

MK_WRAPPER(
  MYSQL_RES *,
  mysql_list_tables,
  (MYSQL *mysql, const char *wild),
  (&res, mysql, wild),
  (mysql, wild),
  mysql,
  mysql)

MK_WRAPPER(
  MYSQL_RES *,
  mysql_list_processes,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  MYSQL_RES *,
  mysql_list_fields,
  (MYSQL *mysql, const char *table, const char *wild),
  (&res, mysql, table, wild),
  (mysql, table, wild),
  mysql,
  mysql)

MK_WRAPPER(
  my_bool,
  mysql_read_query_result,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_stmt_prepare,
  (MYSQL_STMT *stmt, const char *query, unsigned long length),
  (&res, stmt, query, length),
  (stmt, query, length),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  int,
  mysql_stmt_execute,
  (MYSQL_STMT *stmt),
  (&res, stmt),
  (stmt),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  int,
  mysql_stmt_fetch,
  (MYSQL_STMT *stmt),
  (&res, stmt),
  (stmt),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  int,
  mysql_stmt_store_result,
  (MYSQL_STMT *stmt),
  (&res, stmt),
  (stmt),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  my_bool,
  mysql_stmt_close,
  (MYSQL_STMT *stmt),
  (&res, stmt),
  (stmt),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  my_bool,
  mysql_stmt_reset,
  (MYSQL_STMT *stmt),
  (&res, stmt),
  (stmt),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  my_bool,
  mysql_stmt_free_result,
  (MYSQL_STMT *stmt),
  (&res, stmt),
  (stmt),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  my_bool,
  mysql_stmt_send_long_data,
  (MYSQL_STMT *stmt, unsigned int param_number, const char *data, unsigned long length),
  (&res, stmt, param_number, data, length),
  (stmt, param_number, data, length),
  stmt,
  stmt->mysql)

MK_WRAPPER(
  my_bool,
  mysql_commit,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  my_bool,
  mysql_rollback,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

MK_WRAPPER(
  my_bool,
  mysql_autocommit,
  (MYSQL *mysql, my_bool auto_mode),
  (&res, mysql, auto_mode),
  (mysql, auto_mode),
  mysql,
  mysql)

MK_WRAPPER(
  int,
  mysql_next_result,
  (MYSQL *mysql),
  (&res, mysql),
  (mysql),
  mysql,
  mysql)

#undef USE_BLOCKING
#undef MK_WRAPPER
#undef MK_WRAPPER_VOID_RETURN


#define mysql_real_connect wrap_mysql_real_connect
#define mysql_real_query wrap_mysql_real_query
#define mysql_fetch_row wrap_mysql_fetch_row
#define mysql_set_character_set wrap_mysql_set_character_set
#define mysql_select_db wrap_mysql_select_db
#define mysql_send_query wrap_mysql_send_query
#define mysql_store_result wrap_mysql_store_result
#define mysql_free_result wrap_mysql_free_result
#define mysql_close wrap_mysql_close
#define mysql_change_user wrap_mysql_change_user
#define mysql_query wrap_mysql_query
#define mysql_shutdown wrap_mysql_shutdown
#define mysql_dump_debug_info wrap_mysql_dump_debug_info
#define mysql_refresh wrap_mysql_refresh
#define mysql_kill wrap_mysql_kill
#define mysql_set_server_option wrap_mysql_set_server_option
#define mysql_ping wrap_mysql_ping
#define mysql_stat wrap_mysql_stat
#define mysql_list_dbs wrap_mysql_list_dbs
#define mysql_list_tables wrap_mysql_list_tables
#define mysql_list_processes wrap_mysql_list_processes
#define mysql_list_fields wrap_mysql_list_fields
#define mysql_read_query_result wrap_mysql_read_query_result
#define mysql_stmt_prepare wrap_mysql_stmt_prepare
#define mysql_stmt_execute wrap_mysql_stmt_execute
#define mysql_stmt_fetch wrap_mysql_stmt_fetch
#define mysql_stmt_store_result wrap_mysql_stmt_store_result
#define mysql_stmt_close wrap_mysql_stmt_close
#define mysql_stmt_reset wrap_mysql_stmt_reset
#define mysql_stmt_free_result wrap_mysql_stmt_free_result
#define mysql_stmt_send_long_data wrap_mysql_stmt_send_long_data
#define mysql_commit wrap_mysql_commit
#define mysql_rollback wrap_mysql_rollback
#define mysql_autocommit wrap_mysql_autocommit
#define mysql_next_result wrap_mysql_next_result
