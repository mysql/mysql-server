/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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
#ifdef __APPLE__
#include <stdio.h>
#endif

#ifndef _WIN32
#include <netdb.h>
#endif

#include <memory>

#include "xcom/network/include/network_management_interface.h"
#include "xcom/network/network_provider_manager.h"
#include "xcom/site_struct.h"
#include "xcom/task_arg.h"
#include "xcom/task_debug.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_cache.h"
#include "xcom/xcom_input_request.h"
#include "xcom/xcom_os_layer.h"
#include "xcom/xdr_utils.h"

#define XCOM_THREAD_DEBUG 1

typedef unsigned long long synode_cnt;

#define SET_EXECUTED_MSG(x) \
  do {                      \
    IFDBG(D_NONE, FN);      \
    set_executed_msg(x);    \
  } while (0)

/* Constants */

#ifdef XCOM_TRANSACTIONS
static trans_id const last_trans = {0xffffffff, 0xffffffff};

#endif

extern int ARBITRATOR_HACK;
extern task_arg null_arg;

void *xcom_thread_main(void *cp);

synode_no incr_synode(synode_no synode);

synode_no decr_synode(synode_no synode);

char *dbg_pax_msg(pax_msg const *p);
pax_msg *dispatch_op(site_def const *site, pax_msg *p, linkage *reply_queue);
synode_no set_executed_msg(synode_no msgno);
void request_values(synode_no find, synode_no end);
void set_group(uint32_t id);
void check_tasks();
int xcom_booted();
int iamthegreatest(site_def const *s);
void xcom_send(app_data_ptr a, pax_msg *msg);
int reply_handler_task(task_arg arg);
int acceptor_learner_task(task_arg arg);
synode_no get_max_synode();
void xcom_thread_deinit();
int taskmain(xcom_port listen_port);
void xcom_thread_init();
site_def *install_node_group(app_data_ptr a);
int xcom_taskmain2(xcom_port listen_port);
void set_max_synode(synode_no synode);
synode_no set_current_message(synode_no msgno);

int is_real_recover(app_data_ptr a);

void init_xcom_base();
uint32_t new_id();
synode_no get_boot_key();
site_def const *get_executor_site();
site_def *get_executor_site_rw();
site_def const *get_proposer_site();
synode_no get_current_message();
void start_run_tasks();

int is_node_v4_reachable(char *node_address);
int is_node_v4_reachable_with_info(struct addrinfo *retrieved_addr_info);
int are_we_allowed_to_upgrade_to_v6(app_data_ptr a);

/**
 * @brief Process incoming are_you_alive (i.e.: ping) messages and act
 * accordingly
 *
 * GCS/XCom has a full mesh of connections between all nodes. A connects to B
 * and B connects back to A.
 *
 * If we cut out B with, for instance, a firewall, we have the A->B connection
 * silently dead, but we have the B->A connection alive. Since we only do
 * monitoring on one half of the connection (the incoming messages), we will
 * consider that B is alive, although we can't contact it. In the same way, B
 * will consider that A is dead, since it does not receive any message from it.
 *
 * We must be able to break the outgoing connection if we detect that something
 * is wrong, in order to make the bi-directional connection state consistent and
 * report that node as unreachable. That can be done if we start receiving
 * pings from a node that we consider that it is alive. After some pings,
 * we just kill the outgoing connection, thus creating a consistent state.
 *
 * Breaking this connection should only occur if the node has already booted,
 * meaning that the whole joining process is complete and the node is up and
 * running. This is due to the fact that we receive pings as part of the
 * process of joining a group.
 *
 * @param site current site definitions
 * @param pm a possible ping message:
 * @param has_client_already_booted check if this node has already booted
 * @param current_time current XCom time
 *
 * @return int 1 if the node connection is closed. 0, otherwise.
 */
int pre_process_incoming_ping(site_def const *site, pax_msg const *pm,
                              int has_client_already_booted,
                              double current_time);

#define RESET_CLIENT_MSG              \
  if (ep->client_msg) {               \
    msg_link_delete(&ep->client_msg); \
  }

#define APP ep->client_msg->p->a

#define FIND_MAX (MIN_LENGTH / 10)

#define x_actions                                                              \
  X(x_fsm_wait)                                                                \
  , X(x_fsm_poll), X(x_fsm_init), X(x_fsm_u_boot), X(x_fsm_add),               \
      X(x_fsm_net_boot), X(x_fsm_force_config), X(x_fsm_snapshot),             \
      X(x_fsm_local_snapshot), X(x_fsm_snapshot_wait), X(x_fsm_need_snapshot), \
      X(x_fsm_complete), X(x_fsm_terminate), X(x_fsm_exit), X(x_fsm_timeout)
#define X(a) a

enum xcom_actions { x_actions };
typedef enum xcom_actions xcom_actions;
#undef X

extern const char *xcom_actions_name[];

struct add_args {
  char *addr;
  xcom_port port;
  node_list *nl;
};
typedef struct add_args add_args;

synode_no xcom_get_last_removed_from_cache();

char const *xcom_fsm(xcom_actions action, task_arg fsmargs);
void site_post_install_action(site_def *site);

void site_install_action(site_def *site, cargo_type operation);
void send_client_add_node(char *srv, xcom_port port, node_list *nl);
void send_client_remove_node(char *srv, xcom_port port, node_list *nl);

typedef void (*xcom_full_data_receiver)(site_def const *site, pax_machine *pma,
                                        app_data_ptr app,
                                        delivery_status app_status);
void set_xcom_full_data_receiver(xcom_full_data_receiver x);

typedef void (*xcom_full_local_view_receiver)(site_def const *site,
                                              node_set nodes);
void set_xcom_full_local_view_receiver(xcom_full_local_view_receiver x);

typedef void (*xcom_full_global_view_receiver)(site_def const *site,
                                               synode_no message_id,
                                               node_set nodes);
void set_xcom_full_global_view_receiver(xcom_full_global_view_receiver x);

typedef void (*xcom_data_receiver)(synode_no message_id, synode_no origin,
                                   site_def const *site, node_set nodes,
                                   u_int size, synode_no last_removed,
                                   char *data);
void set_xcom_data_receiver(xcom_data_receiver x);

typedef void (*xcom_local_view_receiver)(synode_no message_id, node_set nodes);
void set_xcom_local_view_receiver(xcom_local_view_receiver x);

typedef void (*xcom_global_view_receiver)(synode_no config_id,
                                          synode_no message_id, node_set nodes,
                                          xcom_event_horizon);
void set_xcom_global_view_receiver(xcom_global_view_receiver x);

typedef void (*xcom_config_receiver)(app_data *a);

void set_xcom_config_receiver(xcom_config_receiver x);

void set_xcom_logger(xcom_logger x);
void set_xcom_debugger(xcom_debugger x);
void set_xcom_debugger_check(xcom_debugger_check x);

typedef void (*app_snap_handler)(blob *gcs_snap, synode_no log_start,
                                 synode_no log_end);
void set_app_snap_handler(app_snap_handler x);

typedef synode_no (*app_snap_getter)(blob *gcs_snap);
void set_app_snap_getter(app_snap_getter x);

typedef void (*xcom_state_change_cb)(int status);
void set_xcom_run_cb(xcom_state_change_cb x);
void set_xcom_terminate_cb(xcom_state_change_cb x);
void set_xcom_exit_cb(xcom_state_change_cb x);
void set_xcom_expel_cb(xcom_state_change_cb x);

typedef int (*should_exit_getter)();
void set_should_exit_getter(should_exit_getter x);

typedef void (*xcom_recovery_cb)();

void set_xcom_recovery_init_cb(xcom_recovery_cb x);

void set_xcom_recovery_restart_cb(xcom_recovery_cb x);

void set_xcom_recovery_begin_cb(xcom_recovery_cb x);

void set_xcom_recovery_end_cb(xcom_recovery_cb x);

app_data_ptr init_config_with_group(app_data *a, node_list *nl, cargo_type type,
                                    uint32_t group_id);
app_data_ptr init_set_event_horizon_msg(app_data *a, uint32_t group_id,
                                        xcom_event_horizon event_horizon);
app_data_ptr init_get_leaders_msg(app_data *a, uint32_t group_id);
app_data_ptr init_set_cache_size_msg(app_data *a, uint64_t cache_limit);
app_data_ptr init_get_event_horizon_msg(app_data *a, uint32_t group_id);
app_data_ptr init_app_msg(app_data *a, char *payload, u_int payload_size);
app_data_ptr init_terminate_command(app_data *a);

void terminate_and_exit();

/* Hook the logic to pop from the input channel. */
typedef xcom_input_request_ptr (*xcom_input_try_pop_cb)(void);
void set_xcom_input_try_pop_cb(xcom_input_try_pop_cb pop);
/* Create a connection to the input channel's signalling socket. */
bool_t xcom_input_new_signal_connection(char const *address, xcom_port port);
/* Signal that the input channel has commands. */
bool_t xcom_input_signal(void);
/* Destroy the connection to the input channel's signalling socket. */
void xcom_input_free_signal_connection(void);

/*
 Registers a callback that is called right after
 the accept routine returns.
 */
typedef int (*xcom_socket_accept_cb)(int fd, site_def const *config);
int set_xcom_socket_accept_cb(xcom_socket_accept_cb x);

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
int64_t xcom_client_send_die(connection_descriptor *fd);
int64_t xcom_client_send_data(uint32_t size, char *data,
                              connection_descriptor *fd);
xcom_event_horizon xcom_get_minimum_event_horizon();
xcom_event_horizon xcom_get_maximum_event_horizon();
int xcom_client_get_event_horizon(connection_descriptor *fd, uint32_t group_id,
                                  xcom_event_horizon *event_horizon);
int xcom_client_set_event_horizon(connection_descriptor *fd, uint32_t group_id,
                                  xcom_event_horizon event_horizon);
int xcom_client_terminate_and_exit(connection_descriptor *fd);
int xcom_client_set_cache_limit(connection_descriptor *fd,
                                uint64_t cache_limit);
int xcom_client_get_synode_app_data(connection_descriptor *const fd,
                                    uint32_t group_id,
                                    synode_no_array *const synodes,
                                    synode_app_data_array *const reply);
int xcom_client_convert_into_local_server(connection_descriptor *const fd);
int64_t xcom_send_client_app_data(connection_descriptor *fd, app_data_ptr a,
                                  int force);

std::unique_ptr<Network_provider_management_interface>
get_network_management_interface();

std::unique_ptr<Network_provider_operations_interface>
get_network_operations_interface();

/**
  Copies app data @c source into @c target and checks if the copy
  succeeded. Sets *target to NULL if the copy fails.

  @param[in, out] target The pax_msg to which the app_data will be copied.
  @param source The app data that will be copied.
  @retval TRUE if the copy was successful.
  @retval FALSE if the copy failed, in which case *target is set to NULL;
          a failed copy means that there was an error allocating memory for
          the copy.
*/
bool_t safe_app_data_copy(pax_msg **target, app_data_ptr source);

/**
 * Initializes the message @c msg to go through a 3-phase, regular Paxos.
 * Executed by Proposers.
 *
 * @param site XCom configuration
 * @param p Paxos instance
 * @param msg Message to send
 * @param msgno Synode where @c msg will be proposed
 * @param msg_type The type of the message, e.g. normal or no_op
 */
void prepare_push_3p(site_def const *site, pax_machine *p, pax_msg *msg,
                     synode_no msgno, pax_msg_type msg_type);
/**
 * Initializes the message @c p as a Prepare message, as in the message for
 * Phase 1 (a) of the Paxos protocol.
 * Executed by Proposers.
 *
 * @param p The message to send
 */
void init_prepare_msg(pax_msg *p);
/**
 * Initializes the message @c p as a Prepare message for a no-op, as in the
 * message for Phase 1 (a) of the Paxos protocol.
 * Executed by Proposers.
 *
 * @param p The no-op message to send
 * @retval created paxos message of type no_op
 */
pax_msg *create_noop(pax_msg *p);
/**
 * Process the incoming Prepare message from a Proposer, as in the message for
 * Phase 1 (a) of the Paxos protocol.
 * Executed by Acceptors.
 *
 * @param p Paxos instance
 * @param pm Incoming Prepare message
 * @param synode Synode of the Paxos instance/Accept message
 * @retval pax_msg* the reply to send to the Proposer (as in the Phase 1 (b)
 * message of the Paxos protocol) if the Acceptor accepts the Prepare
 * @retval NULL otherwise
 */
pax_msg *handle_simple_prepare(pax_machine *p, pax_msg *pm, synode_no synode);
/**
 * Process the incoming acknowledge from an Acceptor to a sent Prepare, as in
 * the message for Phase 1 (b) of the Paxos protocol.
 * Executed by Proposers.
 *
 * @param site XCom configuration
 * @param p Paxos instance
 * @param m Incoming message
 * @retval TRUE if a majority of Acceptors replied to the Proposer's Prepare
 * @retval FALSE otherwise
 */
bool_t handle_simple_ack_prepare(site_def const *site, pax_machine *p,
                                 pax_msg *m);
/**
 * Initializes the proposer's message to go through a 2-phase Paxos on the
 * proposer's reserved ballot (0,_).
 * Executed by Proposers.
 *
 * @param site XCom configuration
 * @param p Paxos instance
 */
void prepare_push_2p(site_def const *site, pax_machine *p);
/**
 * Initializes the message @c p as an Accept message, as in the message for
 * Phase 2 (a) of the Paxos protocol.
 * Executed by Proposers.
 *
 * @param p The message to send
 */
void init_propose_msg(pax_msg *p);
/**
 * Process the incoming Accept from a Proposer, as in the message for
 * Phase 2 (a) of the Paxos protocol.
 * Executed by Acceptors.
 *
 * @param p Paxos instance
 * @param m Incoming Accept message
 * @param synode Synode of the Paxos instance/Accept message
 * @retval pax_msg* the reply to send to the Proposer (as in the Phase 2 (b)
 * message of the Paxos protocol) if the Acceptor accepts the Accept
 * @retval NULL otherwise
 */
pax_msg *handle_simple_accept(pax_machine *p, pax_msg *m, synode_no synode);
/**
 * Process the incoming acknowledge from an Acceptor to a sent Accept, as in
 * the message for Phase 2 (b) of the Paxos protocol. Executed by Proposers.
 *
 * @param site XCom configuration
 * @param p Paxos instance
 * @param m Incoming message
 * @retval pax_msg* the Learn message to send to Leaners if a majority of
 * Acceptors replied to the Proposer's Accept
 * @retval NULL otherwise
 */
pax_msg *handle_simple_ack_accept(site_def const *site, pax_machine *p,
                                  pax_msg *m);
/**
 * Process the incoming tiny, i.e. without the learned value, Learn message.
 * Executed by Learners.
 *
 * @param site XCom configuration
 * @param pm Paxos instance
 * @param p Incoming message
 */
void handle_tiny_learn(site_def const *site, pax_machine *pm, pax_msg *p);
/**
 * Process the incoming Learn message.
 * Executed by Learners.
 *
 * @param site XCom configuration
 * @param p Paxos instance
 * @param m Incoming message
 */
void handle_learn(site_def const *site, pax_machine *p, pax_msg *m);
/**
 * @retval 1 if the value for the Paxos instance @c *p has been learned
 * @retval 0 otherwise
 */
int pm_finished(pax_machine *p);
/** @return true if we should process the incoming need_boot_op message passed
 * in parameter p. */
bool_t should_handle_need_boot(site_def const *site, pax_msg *p);
/**
 * Initializes the message @c p as a need_boot_op message.
 *
 * @param p The message to send
 * @param identity The unique incarnation identifier of this XCom instance
 */
void init_need_boot_op(pax_msg *p, node_address *identity);

int xcom_client_set_max_leaders(connection_descriptor *fd, node_no max_leaders,
                                uint32_t group_id);

void init_set_max_leaders(uint32_t group_id, app_data *a, node_no max_leaders);
void init_set_leaders(uint32_t group_id, app_data *a,
                      leader_array const leaders);
void init_set_leaders(uint32_t group_id, app_data *a, u_int n,
                      char const *names[]);
void init_set_leaders(uint32_t group_id, app_data *leader_app,
                      leader_array const leaders, app_data *max_app,
                      node_no max_leaders);
void init_set_leaders(uint32_t group_id, app_data *leader_app, u_int n,
                      char const *names[], app_data *max_app,
                      node_no max_leaders);

int xcom_client_set_leaders(connection_descriptor *fd,
                            leader_array const leaders, uint32_t group_id);
int xcom_client_set_leaders(connection_descriptor *fd, u_int n,
                            char const *names[], uint32_t group_id);
int xcom_client_set_leaders(connection_descriptor *fd,
                            leader_array const leaders, node_no max_leaders,
                            uint32_t group_id);
int xcom_client_set_leaders(connection_descriptor *fd, u_int n,
                            char const *names[], node_no max_leaders,
                            uint32_t group_id);

int xcom_client_get_leaders(connection_descriptor *fd, uint32_t group_id,
                            leader_info_data *leaders);

typedef void (*xcom_election_cb)(leader_array leaders);
void set_xcom_election_cb(xcom_election_cb x);

static inline char *strerr_msg(char *buf, size_t len, int nr) {
#if defined(_WIN32)
  strerror_s(buf, len, nr);
#else
  snprintf(buf, len, "%s", strerror(nr));
#endif
  return buf;
}

void xcom_sleep(unsigned int seconds);

#define XCOM_COMMS_ERROR 1
#define XCOM_COMMS_OTHER 2
#define XCOM_COMMS_OK 0
void set_xcom_comms_cb(xcom_state_change_cb x);

extern "C" synode_no get_delivered_msg();
void set_max_synode_from_unified_boot(synode_no unified_boot_synode);
void send_x_fsm_complete();
synode_no get_default_start(app_data_ptr a);
synode_no get_last_delivered_msg();
void set_log_end(gcs_snapshot *gcs);

extern "C" void synthesize_leaders(leader_array *leaders);

#define XCOM_FSM(action, arg)                           \
  do {                                                  \
    const char *s = xcom_fsm(action, arg);              \
    G_TRACE("%f %s:%d", seconds(), __FILE__, __LINE__); \
    G_DEBUG("new state %s", s);                         \
  } while (0)

int pm_finished(pax_machine *p);
bool_t handle_max_leaders(app_data_ptr a);

#endif
