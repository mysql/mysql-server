/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_BASE_H
#define XCOM_BASE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_os_layer.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_os_layer.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xdr_utils.h"

#define XCOM_THREAD_DEBUG 1

typedef unsigned long long synode_cnt;

#define SET_EXECUTED_MSG(x) \
  do {                      \
    MAY_DBG(FN);            \
    set_executed_msg(x);    \
  } while (0)

/* {{{ Constants */

#ifdef XCOM_TRANSACTIONS
static trans_id const last_trans = {0xffffffff, 0xffffffff};

#endif

/* }}} */

extern int ARBITRATOR_HACK;
extern task_arg null_arg;

void *xcom_thread_main(void *cp);

synode_no vp_count_to_synode(u_long high, u_long low, node_no nodeid,
                             uint32_t groupid);

synode_no incr_synode(synode_no synode);

synode_no decr_synode(synode_no synode);

void broadcast_recover_end();
char *dbg_pax_msg(pax_msg const *p);
pax_msg *dispatch_op(site_def const *site, pax_msg *p, linkage *reply_queue);
synode_no set_executed_msg(synode_no msgno);
void request_values(synode_no find, synode_no end);
void set_group(uint32_t id);
void check_tasks();
int xcom_booted();
int iamthegreatest(site_def const *s);
void xcom_send(app_data_ptr a, pax_msg *msg);
void deliver_view_msg(site_def const *site);
int reply_handler_task(task_arg arg);
int acceptor_learner_task(task_arg arg);
synode_no get_max_synode();
void xcom_thread_deinit();
int taskmain(xcom_port listen_port);
void xcom_thread_init();
site_def *install_node_group(app_data_ptr a);
int xcom_taskmain(xcom_port listen_port);
int xcom_taskmain2(xcom_port listen_port);
void set_max_synode(synode_no synode);
synode_no set_current_message(synode_no msgno);

void xcom_send_data(uint32_t size, char *data);

bool_t must_force_recover();
channel *get_prop_input_queue();
int is_real_recover(app_data_ptr a);

void init_xcom_base();
void set_force_recover(bool_t const x);
void add_to_cache(app_data_ptr a, synode_no synode);
uint32_t new_id();
synode_no get_boot_key();
site_def const *get_executor_site();
site_def const *get_proposer_site();
synode_no get_current_message();
void start_run_tasks();

#define RESET_CLIENT_MSG              \
  if (ep->client_msg) {               \
    msg_link_delete(&ep->client_msg); \
  }

#define APP ep->client_msg->p->a

#define XAPP ep->p->learner.msg->a

#define FIND_MAX (CACHED / 10)

/* Set type and object pointer */
#define PLP msg->payload.manager_message_payload_u
#define SET_REP_TYPE(quark, ptr) \
  q = (quark);                   \
  object = PLP.ptr

#define x_state_list \
  X(x_start)         \
  X(x_boot) X(x_recover) X(x_run) X(x_done) X(x_snapshot_wait) X(x_recover_wait)
#define x_actions                                                       \
  X(xa_wait)                                                            \
  X(xa_poll)                                                            \
  X(xa_init)                                                            \
  X(xa_u_boot)                                                          \
  X(xa_add)                                                             \
  X(xa_net_boot)                                                        \
  X(xa_force_config)                                                    \
  X(xa_snapshot) X(xa_snapshot_wait) X(xa_need_snapshot) X(xa_complete) \
      X(xa_terminate) X(xa_exit) X(xa_timeout)
#define X(a) a,
enum xcom_state { x_state_list };
typedef enum xcom_state xcom_state;

enum xcom_actions { x_actions };
typedef enum xcom_actions xcom_actions;
#undef X

extern const char *xcom_state_name[];

extern const char *xcom_actions_name[];

struct add_args {
  char *addr;
  xcom_port port;
  node_list *nl;
};
typedef struct add_args add_args;

void xcom_add_node(char *addr, xcom_port port, node_list *nl);

xcom_state xcom_fsm(xcom_actions action, task_arg fsmargs);
void site_post_install_action(site_def *site);

void site_install_action(site_def *site, cargo_type operation);
void send_client_add_node(char *srv, xcom_port port, node_list *nl);
void send_client_remove_node(char *srv, xcom_port port, node_list *nl);

typedef void (*xcom_data_receiver)(synode_no message_id, node_set nodes,
                                   u_int size, char *data);
void set_xcom_data_receiver(xcom_data_receiver x);

typedef void (*xcom_local_view_receiver)(synode_no message_id, node_set nodes);
void set_xcom_local_view_receiver(xcom_local_view_receiver x);

typedef void (*xcom_global_view_receiver)(synode_no config_id,
                                          synode_no message_id, node_set nodes);
void set_xcom_global_view_receiver(xcom_global_view_receiver x);

void set_xcom_logger(xcom_logger x);
void set_xcom_debugger(xcom_debugger x);
void set_xcom_debugger_check(xcom_debugger_check x);

typedef void (*app_snap_handler)(blob *gcs_snap);
void set_app_snap_handler(app_snap_handler x);

typedef synode_no (*app_snap_getter)(blob *gcs_snap);
void set_app_snap_getter(app_snap_getter x);

typedef void (*xcom_state_change_cb)(int status);
void set_xcom_run_cb(xcom_state_change_cb x);
void set_xcom_terminate_cb(xcom_state_change_cb x);
void set_xcom_exit_cb(xcom_state_change_cb x);

typedef int (*should_exit_getter)();
void set_should_exit_getter(should_exit_getter x);

app_data_ptr init_config_with_group(app_data *a, node_list *nl, cargo_type type,
                                    uint32_t group_id);

/*
 Registers a callback that is called right after
 the accept routine returns.
 */
typedef int (*xcom_socket_accept_cb)(int fd);
int set_xcom_socket_accept_cb(xcom_socket_accept_cb x);

connection_descriptor *xcom_open_client_connection(char *server,
                                                   xcom_port port);
int xcom_close_client_connection(connection_descriptor *connection);

int xcom_client_disable_arbitrator(connection_descriptor *fd);
int xcom_client_enable_arbitrator(connection_descriptor *fd);
int xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                         uint32_t group_id);
int xcom_client_boot(connection_descriptor *fd, node_list *nl,
                     uint32_t group_id);
int xcom_client_force_add_node(connection_descriptor *fd, node_list *nl,
                               uint32_t group_id);
int xcom_client_force_config(connection_descriptor *fd, node_list *nl,
                             uint32_t group_id);
int xcom_client_force_remove_node(connection_descriptor *fd, node_list *nl,
                                  uint32_t group_id);
int xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                            uint32_t group_id);
int64_t xcom_client_send_data(uint32_t size, char *data,
                              connection_descriptor *fd);
int xcom_client_terminate_and_exit(connection_descriptor *fd);
int xcom_client_set_cache_limit(connection_descriptor *fd,
                                uint64_t cache_limit);

static inline char *strerr_msg(char *buf, size_t len, int nr)
{
#if defined (_WIN32)
  strerror_s(buf, len, nr);
#else
  snprintf(buf, len, "%s", strerror(nr));
#endif
  return buf;
}

#define XCOM_COMMS_ERROR 1
#define XCOM_COMMS_OTHER 2
#define XCOM_COMMS_OK 0
void set_xcom_comms_cb(xcom_state_change_cb x);

synode_no get_delivered_msg();
void set_max_synode_from_unified_boot(synode_no unified_boot_synode);
void server_push_log(server *srv, synode_no push, node_no node);

#define XCOM_FSM(action, arg)                               \
  do {                                                      \
    const char *s = xcom_state_name[xcom_fsm(action, arg)]; \
    G_TRACE("%f %s:%d", seconds(), __FILE__, __LINE__);     \
    G_MESSAGE("new state %s", s);                           \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif
