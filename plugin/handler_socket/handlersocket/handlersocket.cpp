
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <memory>
#include <string>
#include <stdio.h>

#include "config.hpp"
#include "hstcpsvr.hpp"
#include "string_util.hpp"
#include "mysql_incl.hpp"

#define DBG_LOG \
  if (dena::verbose_level >= 100) { \
    fprintf(stderr, "%s %p\n", __PRETTY_FUNCTION__, this); \
  }
#define DBG_DO(x) if (dena::verbose_level >= 100) { x; }

#define DBG_DIR(x)

using namespace dena;

static char *handlersocket_address = 0;
static char *handlersocket_port = 0;
static char *handlersocket_port_wr = 0;
static unsigned int handlersocket_epoll = 1;
static unsigned int handlersocket_threads = 32;
static unsigned int handlersocket_threads_wr = 1;
static unsigned int handlersocket_timeout = 30;
static unsigned int handlersocket_backlog = 32768;
static unsigned int handlersocket_sndbuf = 0;
static unsigned int handlersocket_rcvbuf = 0;
static unsigned int handlersocket_readsize = 0;
static unsigned int handlersocket_accept_balance = 0;
static unsigned int handlersocket_wrlock_timeout = 0;
static char *handlersocket_plain_secret = 0;
static char *handlersocket_plain_secret_wr = 0;

struct daemon_handlersocket_data {
  hstcpsvr_ptr hssvr_rd;
  hstcpsvr_ptr hssvr_wr;
};

static int
daemon_handlersocket_init(void *p)
{
  DENA_VERBOSE(10, fprintf(stderr, "handlersocket: initialized\n"));
  config conf;
  conf["use_epoll"] = handlersocket_epoll ? "1" : "0";
  if (handlersocket_address) {
    conf["host"] = handlersocket_address;
  }
  if (handlersocket_port) {
    conf["port"] = handlersocket_port;
  }
  /*
   * unix domain socket
   * conf["host"] = "/";
   * conf["port"] = "/tmp/handlersocket";
   */
  if (handlersocket_threads > 0) {
    conf["num_threads"] = to_stdstring(handlersocket_threads);
  } else {
    conf["num_threads"] = "1";
  }
  conf["timeout"] = to_stdstring(handlersocket_timeout);
  conf["listen_backlog"] = to_stdstring(handlersocket_backlog);
  conf["sndbuf"] = to_stdstring(handlersocket_sndbuf);
  conf["rcvbuf"] = to_stdstring(handlersocket_rcvbuf);
  conf["readsize"] = to_stdstring(handlersocket_readsize);
  conf["accept_balance"] = to_stdstring(handlersocket_accept_balance);
  conf["wrlock_timeout"] = to_stdstring(handlersocket_wrlock_timeout);
  std::auto_ptr<daemon_handlersocket_data> ap(new daemon_handlersocket_data);
  if (handlersocket_port != 0 && handlersocket_port_wr != handlersocket_port) {
    conf["port"] = handlersocket_port;
    if (handlersocket_plain_secret) {
      conf["plain_secret"] = handlersocket_plain_secret;
    }
    ap->hssvr_rd = hstcpsvr_i::create(conf);
    ap->hssvr_rd->start_listen();
  }
  if (handlersocket_port_wr != 0) {
    if (handlersocket_threads_wr > 0) {
      conf["num_threads"] = to_stdstring(handlersocket_threads_wr);
    }
    conf["port"] = handlersocket_port_wr;
    conf["for_write"] = "1";
    conf["plain_secret"] = "";
    if (handlersocket_plain_secret_wr) {
      conf["plain_secret"] = handlersocket_plain_secret_wr;
    }
    ap->hssvr_wr = hstcpsvr_i::create(conf);
    ap->hssvr_wr->start_listen();
  }
  st_plugin_int *const plugin = static_cast<st_plugin_int *>(p);
  plugin->data = ap.release();
  return 0;
}

static int
daemon_handlersocket_deinit(void *p)
{
  DENA_VERBOSE(10, fprintf(stderr, "handlersocket: terminated\n"));
  st_plugin_int *const plugin = static_cast<st_plugin_int *>(p);
  daemon_handlersocket_data *ptr =
    static_cast<daemon_handlersocket_data *>(plugin->data);
  delete ptr;
  return 0;
}

static struct st_mysql_daemon daemon_handlersocket_plugin = {
  MYSQL_DAEMON_INTERFACE_VERSION
};

static MYSQL_SYSVAR_UINT(verbose, dena::verbose_level, 0,
  "0..10000", 0, 0, 10 /* default */, 0, 10000, 0);
static MYSQL_SYSVAR_UINT(epoll, handlersocket_epoll, PLUGIN_VAR_READONLY,
  "0..1", 0, 0, 1 /* default */, 0, 1, 0);
static MYSQL_SYSVAR_STR(address, handlersocket_address,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC, "", NULL, NULL, NULL);
static MYSQL_SYSVAR_STR(port, handlersocket_port,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC, "", NULL, NULL, NULL);
static MYSQL_SYSVAR_STR(port_wr, handlersocket_port_wr,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC, "", NULL, NULL, NULL);
static MYSQL_SYSVAR_UINT(threads, handlersocket_threads, PLUGIN_VAR_READONLY,
  "1..3000", 0, 0, 16 /* default */, 1, 3000, 0);
static MYSQL_SYSVAR_UINT(threads_wr, handlersocket_threads_wr,
  PLUGIN_VAR_READONLY, "1..3000", 0, 0, 1 /* default */, 1, 3000, 0);
static MYSQL_SYSVAR_UINT(timeout, handlersocket_timeout, PLUGIN_VAR_READONLY,
  "30..3600", 0, 0, 300 /* default */, 30, 3600, 0);
static MYSQL_SYSVAR_UINT(backlog, handlersocket_backlog, PLUGIN_VAR_READONLY,
  "5..1000000", 0, 0, 32768 /* default */, 5, 1000000, 0);
static MYSQL_SYSVAR_UINT(sndbuf, handlersocket_sndbuf, PLUGIN_VAR_READONLY,
  "0..16777216", 0, 0, 0 /* default */, 0, 16777216, 0);
static MYSQL_SYSVAR_UINT(rcvbuf, handlersocket_rcvbuf, PLUGIN_VAR_READONLY,
  "0..16777216", 0, 0, 0 /* default */, 0, 16777216, 0);
static MYSQL_SYSVAR_UINT(readsize, handlersocket_readsize, PLUGIN_VAR_READONLY,
  "0..16777216", 0, 0, 0 /* default */, 0, 16777216, 0);
static MYSQL_SYSVAR_UINT(accept_balance, handlersocket_accept_balance,
  PLUGIN_VAR_READONLY, "0..10000", 0, 0, 0 /* default */, 0, 10000, 0);
static MYSQL_SYSVAR_UINT(wrlock_timeout, handlersocket_wrlock_timeout,
  PLUGIN_VAR_READONLY, "0..3600", 0, 0, 12 /* default */, 0, 3600, 0);
static MYSQL_SYSVAR_STR(plain_secret, handlersocket_plain_secret,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC, "", NULL, NULL, NULL);
static MYSQL_SYSVAR_STR(plain_secret_wr, handlersocket_plain_secret_wr,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC, "", NULL, NULL, NULL);


/* warning: type-punning to incomplete type might break strict-aliasing
 * rules */
static struct st_mysql_sys_var *daemon_handlersocket_system_variables[] = {
  MYSQL_SYSVAR(verbose),
  MYSQL_SYSVAR(address),
  MYSQL_SYSVAR(port),
  MYSQL_SYSVAR(port_wr),
  MYSQL_SYSVAR(epoll),
  MYSQL_SYSVAR(threads),
  MYSQL_SYSVAR(threads_wr),
  MYSQL_SYSVAR(timeout),
  MYSQL_SYSVAR(backlog),
  MYSQL_SYSVAR(sndbuf),
  MYSQL_SYSVAR(rcvbuf),
  MYSQL_SYSVAR(readsize),
  MYSQL_SYSVAR(accept_balance),
  MYSQL_SYSVAR(wrlock_timeout),
  MYSQL_SYSVAR(plain_secret),
  MYSQL_SYSVAR(plain_secret_wr),
  0
};

static SHOW_VAR hs_status_variables[] = {
  {"table_open", (char*) &open_tables_count, SHOW_LONGLONG},
  {"table_close", (char*) &close_tables_count, SHOW_LONGLONG},
  {"table_lock", (char*) &lock_tables_count, SHOW_LONGLONG},
  {"table_unlock", (char*) &unlock_tables_count, SHOW_LONGLONG},
  #if 0
  {"index_exec", (char*) &index_exec_count, SHOW_LONGLONG},
  #endif
  {NullS, NullS, SHOW_LONG}
};

static int show_hs_vars(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_ARRAY;
  var->value= (char *) &hs_status_variables;
  return 0;
}

static SHOW_VAR daemon_handlersocket_status_variables[] = {
  {"Hs", (char*) show_hs_vars, SHOW_FUNC},
  {NullS, NullS, SHOW_LONG}
};


maria_declare_plugin(handlersocket)
{
  MYSQL_DAEMON_PLUGIN,
  &daemon_handlersocket_plugin,
  "handlersocket",
  "higuchi dot akira at dena dot jp",
  "Direct access into InnoDB",
  PLUGIN_LICENSE_BSD,
  daemon_handlersocket_init,
  daemon_handlersocket_deinit,
  0x0100 /* 1.0 */,
  daemon_handlersocket_status_variables,
  daemon_handlersocket_system_variables,
  "1.0",
  MariaDB_PLUGIN_MATURITY_BETA
}
maria_declare_plugin_end;
