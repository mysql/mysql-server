/*
  Copyright 2011 Kristian Nielsen

  Experiments with non-blocking libmysql.

  This is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  MySQL non-blocking client library functions.
*/

#include "my_global.h"
#include "my_sys.h"
#include "mysql.h"
#include "errmsg.h"
#include "sql_common.h"
#include "my_context.h"
#include "violite.h"


#ifdef __WIN__
/*
  Windows does not support MSG_DONTWAIT for send()/recv(). So we need to ensure
  that the socket is non-blocking at the start of every operation.
*/
#define WIN_SET_NONBLOCKING(mysql) { \
    my_bool old_mode__; \
    if ((mysql)->net.vio) vio_blocking((mysql)->net.vio, FALSE, &old_mode__); \
  }
#else
#define WIN_SET_NONBLOCKING(mysql)
#endif

extern struct mysql_async_context *mysql_get_async_context(MYSQL *mysql);


void
my_context_install_suspend_resume_hook(struct mysql_async_context *b,
                                       void (*hook)(my_bool, void *),
                                       void *user_data)
{
  b->suspend_resume_hook= hook;
  b->suspend_resume_hook_user_data= user_data;
}


/* Asynchronous connect(); socket must already be set non-blocking. */
int
my_connect_async(struct mysql_async_context *b, my_socket fd,
                 const struct sockaddr *name, uint namelen, uint timeout)
{
  int res;
#ifdef __WIN__
  int s_err_size;
#else
  socklen_t s_err_size;
#endif

  /*
    Start to connect asynchronously.
    If this will block, we suspend the call and return control to the
    application context. The application will then resume us when the socket
    polls ready for write, indicating that the connection attempt completed.
  */
  res= connect(fd, name, namelen);
#ifdef __WIN__
  if (res != 0)
  {
    int wsa_err= WSAGetLastError();
    if (wsa_err != WSAEWOULDBLOCK)
      return res;
#else
  if (res < 0)
  {
    if (errno != EINPROGRESS && errno != EALREADY && errno != EAGAIN)
      return res;
#endif
    b->timeout_value= timeout;
    b->ret_status= MYSQL_WAIT_WRITE |
      (timeout ? MYSQL_WAIT_TIMEOUT : 0);
#ifdef __WIN__
    b->ret_status|= MYSQL_WAIT_EXCEPT;
#endif
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(TRUE, b->suspend_resume_hook_user_data);
    my_context_yield(&b->async_context);
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(FALSE, b->suspend_resume_hook_user_data);
    if (b->ret_status & MYSQL_WAIT_TIMEOUT)
      return -1;

    s_err_size= sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*) &res, &s_err_size) != 0)
      return -1;
    if (res)
    {
      errno= res;
      return -1;
    }
  }
  return res;
}

ssize_t
my_recv_async(struct mysql_async_context *b, int fd,
              unsigned char *buf, size_t size, uint timeout)
{
  ssize_t res;

  for (;;)
  {
    res= recv(fd, buf, size,
#ifdef __WIN__
              0
#else
              MSG_DONTWAIT
#endif
              );
    if (res >= 0 ||
#ifdef __WIN__
        WSAGetLastError() != WSAEWOULDBLOCK
#else
        (errno != EAGAIN && errno != EINTR)
#endif
        )
      return res;
    b->ret_status= MYSQL_WAIT_READ;
    if (timeout)
    {
      b->ret_status|= MYSQL_WAIT_TIMEOUT;
      b->timeout_value= timeout;
    }
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(TRUE, b->suspend_resume_hook_user_data);
    my_context_yield(&b->async_context);
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(FALSE, b->suspend_resume_hook_user_data);
    if (b->ret_status & MYSQL_WAIT_TIMEOUT)
      return -1;
  }
}

ssize_t
my_send_async(struct mysql_async_context *b, int fd,
              const unsigned char *buf, size_t size, uint timeout)
{
  ssize_t res;

  for (;;)
  {
    res= send(fd, buf, size,
#ifdef __WIN__
              0
#else
              MSG_DONTWAIT
#endif
              );
    if (res >= 0 ||
#ifdef __WIN__
        WSAGetLastError() != WSAEWOULDBLOCK
#else
        (errno != EAGAIN && errno != EINTR)
#endif
        )
      return res;
    b->ret_status= MYSQL_WAIT_WRITE;
    if (timeout)
    {
      b->ret_status|= MYSQL_WAIT_TIMEOUT;
      b->timeout_value= timeout;
    }
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(TRUE, b->suspend_resume_hook_user_data);
    my_context_yield(&b->async_context);
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(FALSE, b->suspend_resume_hook_user_data);
    if (b->ret_status & MYSQL_WAIT_TIMEOUT)
      return -1;
  }
}


my_bool
my_poll_read_async(struct mysql_async_context *b, uint timeout)
{
  b->ret_status= MYSQL_WAIT_READ | MYSQL_WAIT_TIMEOUT;
  b->timeout_value= timeout;
  if (b->suspend_resume_hook)
    (*b->suspend_resume_hook)(TRUE, b->suspend_resume_hook_user_data);
  my_context_yield(&b->async_context);
  if (b->suspend_resume_hook)
    (*b->suspend_resume_hook)(FALSE, b->suspend_resume_hook_user_data);
  return (b->ret_status & MYSQL_WAIT_READ) ? 0 : 1;
}


#ifdef HAVE_OPENSSL
int
my_ssl_read_async(struct mysql_async_context *b, SSL *ssl,
                  void *buf, int size)
{
  int res, ssl_err;

  for (;;)
  {
    res= SSL_read(ssl, buf, size);
    if (res >= 0)
      return res;
    ssl_err= SSL_get_error(ssl, res);
    if (ssl_err == SSL_ERROR_WANT_READ)
      b->ret_status= MYSQL_WAIT_READ;
    else if (ssl_err == SSL_ERROR_WANT_WRITE)
      b->ret_status= MYSQL_WAIT_WRITE;
    else
      return res;
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(TRUE, b->suspend_resume_hook_user_data);
    my_context_yield(&b->async_context);
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(FALSE, b->suspend_resume_hook_user_data);
  }
}

int
my_ssl_write_async(struct mysql_async_context *b, SSL *ssl,
                   const void *buf, int size)
{
  int res, ssl_err;

  for (;;)
  {
    res= SSL_write(ssl, buf, size);
    if (res >= 0)
      return res;
    ssl_err= SSL_get_error(ssl, res);
    if (ssl_err == SSL_ERROR_WANT_READ)
      b->ret_status= MYSQL_WAIT_READ;
    else if (ssl_err == SSL_ERROR_WANT_WRITE)
      b->ret_status= MYSQL_WAIT_WRITE;
    else
      return res;
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(TRUE, b->suspend_resume_hook_user_data);
    my_context_yield(&b->async_context);
    if (b->suspend_resume_hook)
      (*b->suspend_resume_hook)(FALSE, b->suspend_resume_hook_user_data);
  }
}
#endif  /* HAVE_OPENSSL */

unsigned int STDCALL
mysql_get_timeout_value(const MYSQL *mysql)
{
  if (mysql->extension && mysql->extension->async_context)
    return mysql->extension->async_context->timeout_value;
  else
    return 0;
}

/*
  Now create non-blocking definitions for all the calls that may block.

  Each call FOO gives rise to FOO_start() that prepares the MYSQL object for
  doing non-blocking calls that can suspend operation mid-way, and then starts
  the call itself. And a FOO_start_internal trampoline to assist with running
  the real call in a co-routine that can be suspended. And a FOO_cont() that
  can continue a suspended operation.
*/

#define MK_ASYNC_CALLS(call__, decl_args__, invoke_args__, cont_arg__, mysql_val__, parms_mysql_val__, parms_assign__, ret_type__, err_val__, ok_val__, extra1__) \
static void                                                                   \
call__ ## _start_internal(void *d)                                            \
{                                                                             \
  struct call__ ## _params *parms;                                            \
  ret_type__ ret;                                                             \
  struct mysql_async_context *b;                                              \
                                                                              \
  parms= (struct call__ ## _params *)d;                                       \
  b= (parms_mysql_val__)->extension->async_context;                           \
                                                                              \
  ret= call__ invoke_args__;                                                  \
  b->ret_result. ok_val__ = ret;                                              \
  b->ret_status= 0;                                                           \
}                                                                             \
int STDCALL                                                                   \
call__ ## _start decl_args__                                                  \
{                                                                             \
  int res;                                                                    \
  struct mysql_async_context *b;                                              \
  struct call__ ## _params parms;                                             \
                                                                              \
  extra1__                                                                    \
  if (!(b= mysql_get_async_context((mysql_val__))))                           \
  {                                                                           \
    *ret= err_val__;                                                          \
    return 0;                                                                 \
  }                                                                           \
  parms_assign__                                                              \
                                                                              \
  b->active= 1;                                                               \
  res= my_context_spawn(&b->async_context, call__ ## _start_internal, &parms);\
  b->active= 0;                                                               \
  if (res < 0)                                                                \
  {                                                                           \
    set_mysql_error((mysql_val__), CR_OUT_OF_MEMORY, unknown_sqlstate);       \
    b->suspended= 0;                                                          \
    *ret= err_val__;                                                          \
    return 0;                                                                 \
  }                                                                           \
  else if (res > 0)                                                           \
  {                                                                           \
    /* Suspended. */                                                          \
    b->suspended= 1;                                                          \
    return b->ret_status;                                                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Finished. */                                                           \
    b->suspended= 0;                                                          \
    *ret= b->ret_result. ok_val__;                                            \
    return 0;                                                                 \
  }                                                                           \
}                                                                             \
int STDCALL                                                                   \
call__ ## _cont(ret_type__ *ret, cont_arg__, int ready_status)                \
{                                                                             \
  int res;                                                                    \
  struct mysql_async_context *b;                                              \
                                                                              \
  b= (mysql_val__)->extension->async_context;                                 \
  if (!b || !b->suspended)                                                    \
  {                                                                           \
    set_mysql_error((mysql_val__), CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);\
    *ret= err_val__;                                                          \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
  b->active= 1;                                                               \
  b->ret_status= ready_status;                                                \
  res= my_context_continue(&b->async_context);                                \
  b->active= 0;                                                               \
  if (res < 0)                                                                \
  {                                                                           \
    set_mysql_error((mysql_val__), CR_OUT_OF_MEMORY, unknown_sqlstate);       \
    b->suspended= 0;                                                          \
    *ret= err_val__;                                                          \
    return 0;                                                                 \
  }                                                                           \
  else if (res > 0)                                                           \
  {                                                                           \
    /* Suspended. */                                                          \
    return b->ret_status;                                                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Finished. */                                                           \
    b->suspended= 0;                                                          \
    *ret= b->ret_result. ok_val__;                                            \
    return 0;                                                                 \
  }                                                                           \
}

#define MK_ASYNC_CALLS_VOID_RETURN(call__, decl_args__, invoke_args__, cont_arg__, mysql_val__, parms_mysql_val__, parms_assign__, extra1__) \
static void                                                                   \
call__ ## _start_internal(void *d)                                            \
{                                                                             \
  struct call__ ## _params *parms;                                            \
  struct mysql_async_context *b;                                              \
                                                                              \
  parms= (struct call__ ## _params *)d;                                       \
  b= (parms_mysql_val__)->extension->async_context;                           \
                                                                              \
  call__ invoke_args__;                                                       \
  b->ret_status= 0;                                                           \
}                                                                             \
int STDCALL                                                                   \
call__ ## _start decl_args__                                                  \
{                                                                             \
  int res;                                                                    \
  struct mysql_async_context *b;                                              \
  struct call__ ## _params parms;                                             \
                                                                              \
  extra1__                                                                    \
  if (!(b= mysql_get_async_context((mysql_val__))))                           \
  {                                                                           \
    return 0;                                                                 \
  }                                                                           \
  parms_assign__                                                              \
                                                                              \
  b->active= 1;                                                               \
  res= my_context_spawn(&b->async_context, call__ ## _start_internal, &parms);\
  b->active= 0;                                                               \
  if (res < 0)                                                                \
  {                                                                           \
    set_mysql_error((mysql_val__), CR_OUT_OF_MEMORY, unknown_sqlstate);       \
    b->suspended= 0;                                                          \
    return 0;                                                                 \
  }                                                                           \
  else if (res > 0)                                                           \
  {                                                                           \
    /* Suspended. */                                                          \
    b->suspended= 1;                                                          \
    return b->ret_status;                                                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Finished. */                                                           \
    b->suspended= 0;                                                          \
    return 0;                                                                 \
  }                                                                           \
}                                                                             \
int STDCALL                                                                   \
call__ ## _cont(cont_arg__, int ready_status)                                 \
{                                                                             \
  int res;                                                                    \
  struct mysql_async_context *b;                                              \
                                                                              \
  b= (mysql_val__)->extension->async_context;                                 \
  if (!b || !b->suspended)                                                    \
  {                                                                           \
    set_mysql_error((mysql_val__), CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);\
    return 0;                                                                 \
  }                                                                           \
                                                                              \
  b->active= 1;                                                               \
  b->ret_status= ready_status;                                                \
  res= my_context_continue(&b->async_context);                                \
  b->active= 0;                                                               \
  if (res < 0)                                                                \
  {                                                                           \
    set_mysql_error((mysql_val__), CR_OUT_OF_MEMORY, unknown_sqlstate);       \
    b->suspended= 0;                                                          \
    return 0;                                                                 \
  }                                                                           \
  else if (res > 0)                                                           \
  {                                                                           \
    /* Suspended. */                                                          \
    return b->ret_status;                                                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Finished. */                                                           \
    b->suspended= 0;                                                          \
    return 0;                                                                 \
  }                                                                           \
}

struct mysql_real_connect_params {
  MYSQL *mysql;
  const char *host;
  const char *user;
  const char *passwd;
  const char *db;
  unsigned int port;
  const char *unix_socket;
  unsigned long client_flags;
};
MK_ASYNC_CALLS(
  mysql_real_connect,
  (MYSQL **ret, MYSQL *mysql, const char *host, const char *user,
   const char *passwd, const char *db, unsigned int port,
   const char *unix_socket, unsigned long client_flags),
  (parms->mysql, parms->host, parms->user, parms->passwd, parms->db,
   parms->port, parms->unix_socket, parms->client_flags),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    parms.mysql= mysql;
    parms.host= host;
    parms.user= user;
    parms.passwd= passwd;
    parms.db= db;
    parms.port= port;
    parms.unix_socket= unix_socket;
    parms.client_flags= client_flags;
  },
  MYSQL *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_real_query_params {
  MYSQL *mysql;
  const char *stmt_str;
  unsigned long length;
};
MK_ASYNC_CALLS(
  mysql_real_query,
  (int *ret, MYSQL *mysql, const char *stmt_str, unsigned long length),
  (parms->mysql, parms->stmt_str, parms->length),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.stmt_str= stmt_str;
    parms.length= length;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_fetch_row_params {
  MYSQL_RES *result;
};
MK_ASYNC_CALLS(
  mysql_fetch_row,
  (MYSQL_ROW *ret, MYSQL_RES *result),
  (parms->result),
  MYSQL_RES *result,
  result->handle,
  parms->result->handle,
  {
    WIN_SET_NONBLOCKING(result->handle)
    parms.result= result;
  },
  MYSQL_ROW,
  NULL,
  r_ptr,
  /*
    If we already fetched all rows from server (eg. mysql_store_result()),
    then result->handle will be NULL and we cannot suspend. But that is fine,
    since in this case mysql_fetch_row cannot block anyway. Just return
    directly.
  */
  if (!result->handle)
  {
    *ret= mysql_fetch_row(result);
    return 0;
  }
)

struct mysql_set_character_set_params {
  MYSQL *mysql;
  const char *csname;
};
MK_ASYNC_CALLS(
  mysql_set_character_set,
  (int *ret, MYSQL *mysql, const char *csname),
  (parms->mysql, parms->csname),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.csname= csname;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_select_db_params {
  MYSQL *mysql;
  const char *db;
};
MK_ASYNC_CALLS(
  mysql_select_db,
  (int *ret, MYSQL *mysql, const char *db),
  (parms->mysql, parms->db),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.db= db;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_send_query_params {
  MYSQL *mysql;
  const char *q;
  unsigned long length;
};
MK_ASYNC_CALLS(
  mysql_send_query,
  (int *ret, MYSQL *mysql, const char *q, unsigned long length),
  (parms->mysql, parms->q, parms->length),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.q= q;
    parms.length= length;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_store_result_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_store_result,
  (MYSQL_RES **ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  MYSQL_RES *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_free_result_params {
  MYSQL_RES *result;
};
MK_ASYNC_CALLS_VOID_RETURN(
  mysql_free_result,
  (MYSQL_RES *result),
  (parms->result),
  MYSQL_RES *result,
  result->handle,
  parms->result->handle,
  {
    WIN_SET_NONBLOCKING(result->handle)
    parms.result= result;
  },
  /*
    mysql_free_result() can have NULL in result->handle (this happens when all
    rows have been fetched and mysql_fetch_row() returned NULL.)
    So we cannot suspend, but it does not matter, as in this case
    mysql_free_result() cannot block.
    It is also legitimate to have NULL result, which will do nothing.
  */
  if (!result || !result->handle)
  {
    mysql_free_result(result);
    return 0;
  })

struct mysql_pre_close_params {
  MYSQL *sock;
};
/*
  We need special handling for mysql_close(), as the first part may block,
  while the last part needs to free our extra library context stack.

  So we do the first part (mysql_pre_close()) non-blocking, but the last part
  blocking.
*/
extern void mysql_pre_close(MYSQL *mysql);
MK_ASYNC_CALLS_VOID_RETURN(
  mysql_pre_close,
  (MYSQL *sock),
  (parms->sock),
  MYSQL *sock,
  sock,
  parms->sock,
  {
    WIN_SET_NONBLOCKING(sock)
    parms.sock= sock;
  },
  /* Nothing */)
int STDCALL
mysql_close_start(MYSQL *sock)
{
  int res;

  /* It is legitimate to have NULL sock argument, which will do nothing. */
  if (sock)
  {
    res= mysql_pre_close_start(sock);
    /* If we need to block, return now and do the rest in mysql_close_cont(). */
    if (res)
      return res;
  }
  mysql_close(sock);
  return 0;
}
int STDCALL
mysql_close_cont(MYSQL *sock, int ready_status)
{
  int res;

  res= mysql_pre_close_cont(sock, ready_status);
  if (res)
    return res;
  mysql_close(sock);
  return 0;
}

#ifdef USE_OLD_FUNCTIONS
struct mysql_connect_params {
  MYSQL *mysql;
  const char *host;
  const char *user;
  const char *passwd;
};
MK_ASYNC_CALLS(
  mysql_connect,
  (MYSQL **ret, MYSQL *mysql, const char *host, const char *user, const char *passwd),
  (parms->mysql, parms->host, parms->user, parms->passwd),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.host= host;
    parms.user= user;
    parms.passwd= passwd;
  },
  MYSQL *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_create_db_params {
  MYSQL *mysql;
  const char *DB;
};
MK_ASYNC_CALLS(
  mysql_create_db,
  (int *ret, MYSQL *mysql, const char *DB),
  (parms->mysql, parms->DB),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.DB= DB;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_drop_db_params {
  MYSQL *mysql;
  const char *DB;
};
MK_ASYNC_CALLS(
  mysql_drop_db,
  (int *ret, MYSQL *mysql, const char *DB),
  (parms->mysql, parms->DB),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.DB= DB;
  },
  int,
  1,
  r_int,
  /* Nothing */)

#endif

/*
  These following are not available inside the server (neither blocking or
  non-blocking).
*/
#ifndef MYSQL_SERVER
struct mysql_change_user_params {
  MYSQL *mysql;
  const char *user;
  const char *passwd;
  const char *db;
};
MK_ASYNC_CALLS(
  mysql_change_user,
  (my_bool *ret, MYSQL *mysql, const char *user, const char *passwd, const char *db),
  (parms->mysql, parms->user, parms->passwd, parms->db),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.user= user;
    parms.passwd= passwd;
    parms.db= db;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* Nothing */)

struct mysql_query_params {
  MYSQL *mysql;
  const char *q;
};
MK_ASYNC_CALLS(
  mysql_query,
  (int *ret, MYSQL *mysql, const char *q),
  (parms->mysql, parms->q),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.q= q;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_shutdown_params {
  MYSQL *mysql;
  enum mysql_enum_shutdown_level shutdown_level;
};
MK_ASYNC_CALLS(
  mysql_shutdown,
  (int *ret, MYSQL *mysql, enum mysql_enum_shutdown_level shutdown_level),
  (parms->mysql, parms->shutdown_level),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.shutdown_level= shutdown_level;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_dump_debug_info_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_dump_debug_info,
  (int *ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_refresh_params {
  MYSQL *mysql;
  unsigned int refresh_options;
};
MK_ASYNC_CALLS(
  mysql_refresh,
  (int *ret, MYSQL *mysql, unsigned int refresh_options),
  (parms->mysql, parms->refresh_options),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.refresh_options= refresh_options;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_kill_params {
  MYSQL *mysql;
  unsigned long pid;
};
MK_ASYNC_CALLS(
  mysql_kill,
  (int *ret, MYSQL *mysql, unsigned long pid),
  (parms->mysql, parms->pid),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.pid= pid;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_set_server_option_params {
  MYSQL *mysql;
  enum enum_mysql_set_option option;
};
MK_ASYNC_CALLS(
  mysql_set_server_option,
  (int *ret, MYSQL *mysql, enum enum_mysql_set_option option),
  (parms->mysql, parms->option),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.option= option;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_ping_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_ping,
  (int *ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  int,
  1,
  r_int,
  /* Nothing */)

struct mysql_stat_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_stat,
  (const char **ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  const char *,
  NULL,
  r_const_ptr,
  /* Nothing */)

struct mysql_list_dbs_params {
  MYSQL *mysql;
  const char *wild;
};
MK_ASYNC_CALLS(
  mysql_list_dbs,
  (MYSQL_RES **ret, MYSQL *mysql, const char *wild),
  (parms->mysql, parms->wild),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.wild= wild;
  },
  MYSQL_RES *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_list_tables_params {
  MYSQL *mysql;
  const char *wild;
};
MK_ASYNC_CALLS(
  mysql_list_tables,
  (MYSQL_RES **ret, MYSQL *mysql, const char *wild),
  (parms->mysql, parms->wild),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.wild= wild;
  },
  MYSQL_RES *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_list_processes_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_list_processes,
  (MYSQL_RES **ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  MYSQL_RES *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_list_fields_params {
  MYSQL *mysql;
  const char *table;
  const char *wild;
};
MK_ASYNC_CALLS(
  mysql_list_fields,
  (MYSQL_RES **ret, MYSQL *mysql, const char *table, const char *wild),
  (parms->mysql, parms->table, parms->wild),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.table= table;
    parms.wild= wild;
  },
  MYSQL_RES *,
  NULL,
  r_ptr,
  /* Nothing */)

struct mysql_read_query_result_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_read_query_result,
  (my_bool *ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* Nothing */)

struct mysql_stmt_prepare_params {
  MYSQL_STMT *stmt;
  const char *query;
  unsigned long length;
};
MK_ASYNC_CALLS(
  mysql_stmt_prepare,
  (int *ret, MYSQL_STMT *stmt, const char *query, unsigned long length),
  (parms->stmt, parms->query, parms->length),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
    parms.query= query;
    parms.length= length;
  },
  int,
  1,
  r_int,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_prepare(stmt, query, length);
    return 0;
  })

struct mysql_stmt_execute_params {
  MYSQL_STMT *stmt;
};
MK_ASYNC_CALLS(
  mysql_stmt_execute,
  (int *ret, MYSQL_STMT *stmt),
  (parms->stmt),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
  },
  int,
  1,
  r_int,
  /*
    If eg. mysql_change_user(), stmt->mysql will be NULL.
    In this case, we cannot block.
  */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_execute(stmt);
    return 0;
  })

struct mysql_stmt_fetch_params {
  MYSQL_STMT *stmt;
};
MK_ASYNC_CALLS(
  mysql_stmt_fetch,
  (int *ret, MYSQL_STMT *stmt),
  (parms->stmt),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
  },
  int,
  1,
  r_int,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_fetch(stmt);
    return 0;
  })

struct mysql_stmt_store_result_params {
  MYSQL_STMT *stmt;
};
MK_ASYNC_CALLS(
  mysql_stmt_store_result,
  (int *ret, MYSQL_STMT *stmt),
  (parms->stmt),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
  },
  int,
  1,
  r_int,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_store_result(stmt);
    return 0;
  })

struct mysql_stmt_close_params {
  MYSQL_STMT *stmt;
};
MK_ASYNC_CALLS(
  mysql_stmt_close,
  (my_bool *ret, MYSQL_STMT *stmt),
  (parms->stmt),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_close(stmt);
    return 0;
  })

struct mysql_stmt_reset_params {
  MYSQL_STMT *stmt;
};
MK_ASYNC_CALLS(
  mysql_stmt_reset,
  (my_bool *ret, MYSQL_STMT *stmt),
  (parms->stmt),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_reset(stmt);
    return 0;
  })

struct mysql_stmt_free_result_params {
  MYSQL_STMT *stmt;
};
MK_ASYNC_CALLS(
  mysql_stmt_free_result,
  (my_bool *ret, MYSQL_STMT *stmt),
  (parms->stmt),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_free_result(stmt);
    return 0;
  })

struct mysql_stmt_send_long_data_params {
  MYSQL_STMT *stmt;
  unsigned int param_number;
  const char *data;
  unsigned long length;
};
MK_ASYNC_CALLS(
  mysql_stmt_send_long_data,
  (my_bool *ret, MYSQL_STMT *stmt, unsigned int param_number, const char *data, unsigned long length),
  (parms->stmt, parms->param_number, parms->data, parms->length),
  MYSQL_STMT *stmt,
  stmt->mysql,
  parms->stmt->mysql,
  {
    WIN_SET_NONBLOCKING(stmt->mysql)
    parms.stmt= stmt;
    parms.param_number= param_number;
    parms.data= data;
    parms.length= length;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* If stmt->mysql==NULL then we will not block so can call directly. */
  if (!stmt->mysql)
  {
    *ret= mysql_stmt_send_long_data(stmt, param_number, data, length);
    return 0;
  })

struct mysql_commit_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_commit,
  (my_bool *ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* Nothing */)

struct mysql_rollback_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_rollback,
  (my_bool *ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* Nothing */)

struct mysql_autocommit_params {
  MYSQL *mysql;
  my_bool auto_mode;
};
MK_ASYNC_CALLS(
  mysql_autocommit,
  (my_bool *ret, MYSQL *mysql, my_bool auto_mode),
  (parms->mysql, parms->auto_mode),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
    parms.auto_mode= auto_mode;
  },
  my_bool,
  TRUE,
  r_my_bool,
  /* Nothing */)

struct mysql_next_result_params {
  MYSQL *mysql;
};
MK_ASYNC_CALLS(
  mysql_next_result,
  (int *ret, MYSQL *mysql),
  (parms->mysql),
  MYSQL *mysql,
  mysql,
  parms->mysql,
  {
    WIN_SET_NONBLOCKING(mysql)
    parms.mysql= mysql;
  },
  int,
  1,
  r_int,
  /* Nothing */)
#endif


/*
  The following functions can newer block, and so do not have special
  non-blocking versions:

    mysql_num_rows()
    mysql_num_fields()
    mysql_eof()
    mysql_fetch_field_direct()
    mysql_fetch_fields()
    mysql_row_tell()
    mysql_field_tell()
    mysql_field_count()
    mysql_affected_rows()
    mysql_insert_id()
    mysql_errno()
    mysql_error()
    mysql_sqlstate()
    mysql_warning_count()
    mysql_info()
    mysql_thread_id()
    mysql_character_set_name()
    mysql_init()
    mysql_ssl_set()
    mysql_get_ssl_cipher()
    mysql_use_result()
    mysql_get_character_set_info()
    mysql_set_local_infile_handler()
    mysql_set_local_infile_default()
    mysql_get_server_info()
    mysql_get_server_name()
    mysql_get_client_info()
    mysql_get_client_version()
    mysql_get_host_info()
    mysql_get_server_version()
    mysql_get_proto_info()
    mysql_options()
    mysql_data_seek()
    mysql_row_seek()
    mysql_field_seek()
    mysql_fetch_lengths()
    mysql_fetch_field()
    mysql_escape_string()
    mysql_hex_string()
    mysql_real_escape_string()
    mysql_debug()
    myodbc_remove_escape()
    mysql_thread_safe()
    mysql_embedded()
    mariadb_connection()
    mysql_stmt_init()
    mysql_stmt_fetch_column()
    mysql_stmt_param_count()
    mysql_stmt_attr_set()
    mysql_stmt_attr_get()
    mysql_stmt_bind_param()
    mysql_stmt_bind_result()
    mysql_stmt_result_metadata()
    mysql_stmt_param_metadata()
    mysql_stmt_errno()
    mysql_stmt_error()
    mysql_stmt_sqlstate()
    mysql_stmt_row_seek()
    mysql_stmt_row_tell()
    mysql_stmt_data_seek()
    mysql_stmt_num_rows()
    mysql_stmt_affected_rows()
    mysql_stmt_insert_id()
    mysql_stmt_field_count()
    mysql_more_results()
    mysql_get_socket()
    mysql_get_timeout_value()
*/
