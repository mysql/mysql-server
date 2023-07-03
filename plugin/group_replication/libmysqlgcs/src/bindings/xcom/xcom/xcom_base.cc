/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#include <assert.h>
#include <errno.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#ifndef _WIN32
#include <inttypes.h>
#endif
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif

#ifndef _WIN32
#include <poll.h>
#endif

/**
  @file
  xcom/xcom_base.c
    The new version of xcom is a major rewrite to allow
    transmission of multiple messages from several sources
    simultaneously without collision. The interface to xcom is largely
    intact, one notable change is that xcom will consider the message
    delivered as soon as it has got a majority. Consequently, the VP
    set will not necessarily show all nodes which will actually
    receive the message.

    OHKFIX Add wait for complete last known node set to mimic the old
    semantics.


    IMPORTANT: What xcom does and what it does not do:

    xcom messages are received in the same order on all nodes.

    xcom guarantees that if a message is delivered to one node, it will
    eventually be seen on all other nodes as well.

    xcom messages are available to a crashed node when it comes up
    again if at least one node which knows the value of the message
    has not crashed. The size of the message cache is configurable.

    OHKFIX Add logging to disk to make messages durable across system
    crash and to increase the number of messages which may be cached.

    There is no guarantee whatsoever about the order of messages from
    different nodes, not even the order of multiple messages from the
    same node. It is up to the client to impose such an order by
    waiting on a message before it sends the next.

    xcom can notify the client that a message has timed out, and in
    that case will try to cancel the message, but it cannot guarantee
    that a message which has timed out will not be delivered.

    xcom attaches a node set to each message as it is delivered to the
    client. This node set reflects the current node set that xcom
    believes is active, it does not mean that the message has been
    delivered yet to all nodes in the set. Neither does it mean that
    the message has not been delivered to the nodes not in the set.

    A cache of Paxos state machines is central to the new design. The
    purpose of the cache is both to store a window of messages, and to
    decouple the different parts of xcom, like message proposal,
    message delivery and execution, and recovery.  The old cache was
    limited to caching messages, and a single state machine ran the
    combined VP and Paxos algorithm. This constrained xcom to deliver
    only a single message at a time.

    Each instance of the Paxos state machine implements the basic
    Paxos protocol.  Unlike the cache in the old system, it is not
    cleared when a site is deleted.  This removes some problems
    related to message delivery during site deletion.  The cache is a
    classic fixed size LRU with a hash index.

    Some extensions to the basic Paxos algorithm has been implemented:

    A node has ownership to all synodes with its own node number. Only
    a node with node number N can propose a value for synode {X N},
    where X is the sequence number, and N is the node number. Other
    nodes can only propose the special value no_op for synode {X N}.
    The reason for this is to retain the leaderless Paxos algorithm,
    but to avoid collisions between nodes which are competing for the
    same synode number. With this scheme, each node has its own unique
    number series during normal operation. The scheme has the
    following implications:

    1. If a node N has not already proposed a value for the synode {X N},
    it may at any time send a LEARN message to the other nodes with
    the reserved value no_op, without going through phase 1 and 2 of
    Paxos. This is because the other nodes are constrained to propose
    no_op for this synode, so the final outcome will always be no_op.
    To avoid unnecessary message transmission, a node will try to
    broadcast the no_op LEARN messages by piggybacking the information
    on the messages of the basic Paxos protocol.

    2. Other nodes which want to find the value of synode {X N} may do
    so by trying to get the value no_op accepted by following the
    basic Paxos algorithm. The result will be the actual value
    proposed by node N if it has done so, otherwise no_op. This will
    typically only be necessary when a node is down, and the other
    nodes need to find the values from the missing node in order to be
    able to continue execution.

    Messages are delivered in order to the client, and the order is
    determined by the sequence number and the node number, with the
    sequence number as the most significant part.

    The xcom network interface has been redesigned and is now
    implemented directly on top of TCP, and has so far been completely
    trouble free. We use poll() or select() to implement non-blocking
    send and receive, but libev could equally well have been used.

    Multicast is implemented on top of unicast as before, but the
    implementation is prepared to use real multicast with relatively
    minor changes.

    The roles of proposer, acceptor/learner, and executor are now
    directly mapped to unique task types which interact with the Paxos
    state machines, whereas the previous implementation folded all the
    roles into a single event driven state machine.

    The following terminology will be used:

    A node is an instance of the xcom thread. There is only one instance
    of the xcom thread in the agent.
    A client is the application which is using xcom to send messages.
    A thread is a real OS thread.
    A task is a logical process. It is implemented by coroutines and
    an explicit stack.

    The implementation of tasks and non-blocking socket operations is
    isolated in task.h and task.c.

    A node will open a tcp connection to each of the other nodes. This
    connection is used for all communication initiated by the node,
    and replies to messages will arrive on the connection on which it
    was sent.

    static int tcp_server(task_arg);

    The tcp_server listens on the xcom port and starts an
    acceptor_learner_task whenever a new connection is detected.

    static int tcp_reaper_task(task_arg);

    Closes tcp connection which have been unused for too long.

    static int sender_task(task_arg);

    The sender_task waits for tcp messages on its input queue and
    sends it on the tcp socket. If the socket is closed for any
    reason, the sender_task will reconnect the socket. There is one
    sender_task for each socket. The sender task exists mainly to
    simplify the logic in the other tasks, but it could have been
    replaced with a coroutine which handles the connection logic after
    having reserved the socket for its client task.

    static int generator_task(task_arg);

    The generator_task reads messages from the client queue and moves
    them into the input queue of the proposer_task.

    OHKFIX Use a tcp socket instead of the client queue. We can then
    remove the generator_task and let the acceptor_learner_task do the
    dispatching.

    static int proposer_task(task_arg);

    Assign a message number to an outgoing message and try to get it
    accepted. There may be several proposer tasks on each node
    working in parallel. If there are multiple proposer tasks, xcom can
    not guarantee that the messages will be sent in the same order as
    received from the client.

    static int acceptor_learner_task(task_arg);

    This is the server part of the xcom thread. There is one
    acceptor_learner_task for each node in the system. The acceptor
    learner_task reads messages from the socket, finds the correct
    Paxos state machine, and dispatches to the correct message handler
    with the state machine and message as arguments.

    static int reply_handler_task(task_arg);

    The reply_handler_task does the same job as the
    acceptor_learner_task, but listens on the socket which the node
    uses to send messages, so it will handle only replies on that
    socket.

    static int executor_task(task_arg);

    The ececutor_task waits for a Paxos message to be accpeted. When
    the message is accepted, it is delivered to the client,
    unless it is a no-op. In either case, the executor_task steps to
    the next message and repeats the wait. If it times out waiting for
    a message, it will try to get a no-op accepted.

    static int alive_task(task_arg);

    Sends i-am-alive to other nodes if there has been no normal traffic
    for a while. It also pings nodes which seem to be inactive.

    static int detector_task(task_arg);

    The detector_task periodically scans the set of connections from
    other nodes and sees if there has been any activity. If there has
    been no activity for some time, it will assume that the node is
    dead, and send a view message to the client.


    Reconfiguration:

    The xcom reconfiguration process is essentially the one described in
    "Reconfiguring a State Machine" by Lamport et al. as the R-alpha
    algorithm.
    We execute the reconfiguration command immediately, but the config is
    only valid after a delay of alpha messages.
    The parameter alpha is the same as
    EVENT_HORIZON in this implementation. :/static.*too_far
    All tcp messages from beyond the event horizon will be ignored.

*/
#include "xcom/xcom_profile.h"

#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "xcom/x_platform.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef __linux__
#include <sys/sockio.h>
#endif
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include <memory>

#include "xcom/app_data.h"
#include "xcom/get_synode_app_data.h"
#include "xcom/node_no.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_struct.h"
#include "xcom/task.h"
#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_transport.h"
#include "xcom/xdr_utils.h"
#include "xdr_gen/xcom_vp.h"

#include "xcom/bitset.h"
#include "xcom/leader_info_data.h"
#include "xcom/node_list.h"
#include "xcom/node_set.h"
#include "xcom/pax_msg.h"
#include "xcom/site_def.h"
#include "xcom/sock_probe.h"
#include "xcom/synode_no.h"
#include "xcom/task_debug.h"
#include "xcom/task_net.h"
#include "xcom/xcom_cache.h"
#include "xcom/xcom_cfg.h"
#include "xcom/xcom_interface.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_msg_queue.h"
#include "xcom/xcom_recover.h"
#include "xcom/xcom_statistics.h"
#include "xcom/xcom_vp_str.h"

#include "xcom/network/xcom_network_provider.h"

#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif /* _WIN32 */

#include <openssl/ssl.h>

#endif

#include <chrono>
#include <future>
#include <queue>
#include <tuple>

/* Defines and constants */

#define SYS_STRERROR_SIZE 512
#define XCOM_SEND_APP_WAIT_TIMEOUT 20

/* Avoid printing the warning of protocol version mismatch too often */
#define PROTOVERSION_WARNING_TIMEOUT 600.0 /** Every 10 minutes */
static double protoversion_warning_time =
    0.0; /** Timestamp of previous protoversion warning */

/* Skip prepare for first ballot */
#ifdef ALWAYS_THREEPHASE
int const threephase = 1;
#else
int const threephase = 0;
#endif

#include "xcom/retry.h"

#ifdef NODE_0_IS_ARBITRATOR
int ARBITRATOR_HACK = 1;
#else
int ARBITRATOR_HACK = 0;
#endif

static int const no_duplicate_payload = 1;

/* Use buffered read when reading messages from the network */
static int use_buffered_read = 1;

/* Used to handle OOM errors */
int oom_abort = 0;

/* Forward declarations */
long xcom_unique_long(void);
static int64_t socket_write(
    connection_descriptor *wfd, void *_buf, uint32_t n,
    connnection_write_method write_function = con_write);

static double wakeup_delay(double old);
static void note_snapshot(node_no node);

/* Task types */
static int proposer_task(task_arg arg);
static int executor_task(task_arg arg);
static int sweeper_task(task_arg arg);
extern int alive_task(task_arg arg);
extern int cache_manager_task(task_arg arg);
extern int detector_task(task_arg arg);

static int finished(pax_machine *p);
static int accepted(pax_machine *p);
static int started(pax_machine *p);
static synode_no first_free_synode_local(synode_no msgno);
static void free_forced_config_site_def();
static void activate_sweeper();
static void force_pax_machine(pax_machine *p, int enforcer);
static void propose_noop_2p(synode_no find, pax_machine *p);
static void handle_need_snapshot(linkage *reply_queue, pax_msg *pm);
static void handle_skip(site_def const *site, pax_machine *p, pax_msg *m);
static void paxos_fsm(pax_machine *paxos, site_def const *site,
                      paxos_event event, pax_msg *mess);
static inline bool is_leader(site_def *site);
int is_active_leader(node_no x, site_def *site);
static msg_handler *primary_dispatch_table();
static msg_handler *secondary_dispatch_table();
static void recompute_node_sets(site_def const *old_site, site_def *new_site);
void recompute_timestamps(detector_state const old_timestamp,
                          node_list const *old_nodes,
                          detector_state new_timestamp,
                          node_list const *new_nodes);
void analyze_leaders(site_def *site);
static inline int ignore_message(synode_no x, site_def *site, char const *dbg);

/* Global variables */

int xcom_shutdown = 0;  /* Xcom_Shutdown flag */
synode_no executed_msg; /* The message we are waiting to execute */
synode_no max_synode;   /* Max message number seen so far */
task_env *boot = nullptr;
task_env *detector = nullptr;
task_env *killer = nullptr;
task_env *net_boot = nullptr;
task_env *net_recover = nullptr;
void *xcom_thread_input = nullptr;

long xcom_debug_mask =
    /* D_DETECT | */ D_FSM /* | D_FILEOP | D_CONS | D_BASE */ | D_TRANSPORT;
long xcom_dbg_stack[DBG_STACK_SIZE];
int xcom_dbg_stack_top = 0;

static void init_proposers();
void initialize_lsn(uint64_t n);

void init_base_vars() {
  xcom_shutdown = 0;          /* Xcom_Shutdown flag */
  executed_msg = null_synode; /* The message we are waiting to execute */
  max_synode = null_synode;   /* Max message number seen so far */
  boot = nullptr;
  detector = nullptr;
  killer = nullptr;
  net_boot = nullptr;
  net_recover = nullptr;
  xcom_thread_input = nullptr;
}

static task_env *executor = nullptr;
static task_env *sweeper = nullptr;
static task_env *retry = nullptr;
static task_env *proposer[PROPOSERS];
static task_env *alive_t = nullptr;
static task_env *cache_task = nullptr;

static uint32_t my_id = 0; /* Unique id of this instance */
uint32_t get_my_xcom_id() { return my_id; }
static synode_no current_message; /* Current message number */
static synode_no
    last_config_modification_id; /*Last configuration change proposal*/
static uint64_t lsn = 0;         /* Current log sequence number */

synode_no get_current_message() { return current_message; }

static channel prop_input_queue; /* Proposer task input queue */

enum class synode_allocation_type { todo = 0, local, remote, global };
enum class synode_reservation_status : int {
  number_ok,
  no_nodes,
  delivery_timeout
};

#if 0
static char const *synode_allocation_type_to_str(synode_allocation_type x) {
  switch (x) {
    case synode_allocation_type::todo:
      return "todo";
    case synode_allocation_type::local:
      return "local";
    case synode_allocation_type::remote:
      return "remote";
    case synode_allocation_type::global:
      return "global";
    default:
      return "";
  }
}
#endif

// A pool of synode numbers implemented as a queue
struct synode_pool {
  std::queue<std::pair<synode_no, synode_allocation_type>> data;
  linkage queue;

  synode_pool() { link_init(&queue, TYPE_HASH("task_env")); }

  void put(synode_no synode, synode_allocation_type allocation) {
    data.push({synode, allocation});
    task_wakeup(&queue);
  }

  auto get() {
    auto retval = data.front();
    data.pop();
    return retval;
  }

  bool empty() { return data.empty(); }
};

synode_pool synode_number_pool;

extern int client_boot_done;
extern int netboot_ok;

static linkage exec_wait = {
    0, &exec_wait, &exec_wait}; /* Executor will wake up tasks sleeping here */

linkage detector_wait = {0, &detector_wait,
                         &detector_wait}; /* Detector sleeps here */

static struct {
  int n;
  unsigned long id[MAX_DEAD];
} dead_sites;

synode_no get_max_synode() { return max_synode; }

static bool_t is_latest_config(site_def const *const config) {
  site_def const *const latest_config = get_site_def();
  assert(latest_config != nullptr);
  return config == latest_config;
}

/**
 * Get the first pending configuration that reconfigures the event horizon.
 *
 * Retrieve the first pending site_def, i.e. with the smallest start synod that
 * is greater than executed_msg, that reconfigures the event horizon.
 */
static site_def const *first_event_horizon_reconfig() {
  site_def const *active_config = find_site_def(executed_msg);
  xcom_event_horizon active_event_horizon = active_config->event_horizon;
  site_def const *first_event_horizon_reconfig = nullptr;
  site_def const *next_config = nullptr;
  for (next_config = find_next_site_def(active_config->start);
       next_config != nullptr && first_event_horizon_reconfig == nullptr;
       next_config = find_next_site_def(next_config->start)) {
    if (active_event_horizon != next_config->event_horizon) {
      first_event_horizon_reconfig = next_config;
    }
  }
  return first_event_horizon_reconfig;
}

/**
 * Get the latest pending configuration that reconfigures the event horizon.
 *
 * Retrieve the last pending site_def, i.e. with the greatest start synod that
 * is greater than executed_msg, that reconfigures the event horizon.
 */
static site_def const *latest_event_horizon_reconfig() {
  site_def const *active_config = find_site_def(executed_msg);
  xcom_event_horizon previous_event_horizon = active_config->event_horizon;
  site_def const *last_event_horizon_reconfig = nullptr;
  site_def const *next_config = nullptr;
  for (next_config = find_next_site_def(active_config->start);
       next_config != nullptr;
       next_config = find_next_site_def(next_config->start)) {
    if (previous_event_horizon != next_config->event_horizon) {
      previous_event_horizon = next_config->event_horizon;
      last_event_horizon_reconfig = next_config;
    }
  }
  return last_event_horizon_reconfig;
}

/**
 * Add the event horizon to the given base synod s.
 *
 * We are assuming right now that this function is used solely in the context of
 * "we have received a reconfiguration command at synod s, when should it be
 * scheduled to take effect?"
 * The result of this function is *when* it should take effect.
 *
 * Common case: there are no configurations pending, or if there are, none of
 * them reconfigure the event horizon. The common case result is:
 *
 *   s + event_horizon(active_config) + 1
 *
 *
 * If an event horizon reconfiguration R is pending, it means that the command C
 * proposed for synod s is concurrent with R, i.e., s falls in the interval
 * ]proposed(R), start(R)[.
 *
 * In this situation we apply the command C proposed for synod s *after* taking
 * into account R's event horizon.
 *
 * This means that the result is:
 *
 *   start(R) + event_horizon(R) + 1
 */
#ifdef PERMISSIVE_EH_ACTIVE_CONFIG
/* purecov: begin deadcode */
static synode_no add_default_event_horizon(synode_no s) {
  s.msgno += EVENT_HORIZON_MIN + 1;
  return s;
}
/* purecov: end */
#endif

static synode_no add_event_horizon(synode_no s) {
  site_def const *active_config = find_site_def(executed_msg);
  if (active_config) {
    site_def const *pending_config = latest_event_horizon_reconfig();
    bool_t const no_event_horizon_reconfig_pending =
        (pending_config == nullptr);
    if (is_latest_config(active_config) || no_event_horizon_reconfig_pending) {
      s.msgno = s.msgno + active_config->event_horizon + 1;
    } else {
      s.msgno = pending_config->start.msgno + pending_config->event_horizon + 1;
    }
    return s;
  } else { /* This is initial boot or recovery, we have no config */
#ifdef PERMISSIVE_EH_ACTIVE_CONFIG
    return add_default_event_horizon(s);
#else
    /* We should always have an active config */
    /* purecov: begin deadcode */
    assert(active_config != nullptr);
    return null_synode;
/* purecov: end */
#endif
  }
}

/**
   Set node group
*/
void set_group(uint32_t id) {
  IFDBG(D_NONE, FN; STRLIT("changing group id of global variables ");
        NDBG((unsigned long)id, lu););
  /*	set_group_id(id); */
  current_message.group_id = id;
  executed_msg.group_id = id;
  max_synode.group_id = id;
}

static void bury_site(uint32_t id) {
  if (id != 0) {
    dead_sites.id[dead_sites.n % MAX_DEAD] = id;
    dead_sites.n = (dead_sites.n + 1) % MAX_DEAD;
  }
}

static bool_t is_dead_site(uint32_t id) {
  int i = 0;
  for (i = 0; i < MAX_DEAD; i++) {
    if (dead_sites.id[i] == id)
      return TRUE;
    else if (dead_sites.id[i] == 0)
      return FALSE;
  }
  return FALSE;
}

extern node_set *init_node_set(node_set *set, u_int n);
extern node_set *alloc_node_set(node_set *set, u_int n);

#if 0
/* Find our previous message number. */
static synode_no decr_msgno(synode_no msgno)
{
	synode_no ret = msgno;
	ret.msgno--;
	ret.node = get_nodeno(find_site_def(ret)); /* In case site and node number has changed */
	return ret;
}
#endif

/* Find our next message number. */
static synode_no incr_msgno(synode_no msgno) {
  synode_no ret = msgno;
  ret.msgno++;
  ret.node = get_nodeno(
      find_site_def(ret)); /* In case site and node number has changed */
  return ret;
}

synode_no incr_synode(synode_no synode) {
  synode_no ret = synode;
  ret.node++;
  if (ret.node >= get_maxnodes(find_site_def(synode))) {
    ret.node = 0;
    ret.msgno++;
  }
  /* 	IFDBG(D_NONE, FN; SYCEXP(synode); SYCEXP(ret)); */
  return ret; /* Change this if we change message number type */
}

#if 0
synode_no decr_synode(synode_no synode) {
  synode_no ret = synode;
  if (ret.node == 0) {
    ret.msgno--;
    ret.node = get_maxnodes(find_site_def(ret));
  }
  ret.node--;
  return ret; /* Change this if we change message number type */
}
#endif

static void skip_value(pax_msg *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode));
  p->op = learn_op;
  p->msg_type = no_op;
}

/* Utilities and debug */

#ifndef _WIN32
/* Ignore this signal */
static int ignoresig(int signum) {
  struct sigaction act;
  struct sigaction oldact;

  memset(&act, 0, sizeof(act));
  act.sa_handler = SIG_IGN;
  memset(&oldact, 0, sizeof(oldact));

  return sigaction(signum, &act, &oldact);
}
#else
#define SIGPIPE 0
static int ignoresig(int) { return 0; }
#endif

static int recently_active(pax_machine *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p);
        STRLIT(p->learner.msg ? pax_op_to_str(p->learner.msg->op) : "NULL");
        NDBG(p->last_modified, f); NDBG(task_now(), f));
  return p->last_modified != 0.0 &&
         (p->last_modified + BUILD_TIMEOUT + median_time()) > task_now();
}

static inline int finished(pax_machine *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p);
        STRLIT(p->learner.msg ? pax_op_to_str(p->learner.msg->op) : "NULL"););
  return p->learner.msg && (p->learner.msg->op == learn_op ||
                            p->learner.msg->op == tiny_learn_op);
}

int pm_finished(pax_machine *p) { return finished(p); }

static inline int accepted(pax_machine *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p);
        STRLIT(p->acceptor.msg ? pax_op_to_str(p->acceptor.msg->op) : "NULL"););
  return p->acceptor.msg && p->acceptor.msg->op != initial_op;
}

static inline int accepted_noop(pax_machine *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p);
        STRLIT(p->acceptor.msg ? pax_op_to_str(p->acceptor.msg->op) : "NULL"););
  return accepted(p) && p->acceptor.msg->msg_type == no_op;
}

static inline int noop_match(pax_machine *p, pax_msg *pm) {
  return pm->msg_type == no_op && accepted_noop(p);
}

static inline int started(pax_machine *p) {
  return p->op != initial_op || (p->acceptor.promise.cnt > 0) ||
         (p->proposer.msg && (p->proposer.msg->op != initial_op)) ||
         accepted(p) || finished(p);
}

void set_last_received_config(synode_no received_config_change) {
  last_config_modification_id = received_config_change;
}

/* Definition of majority */
static inline node_no max_check(site_def const *site) {
#ifdef MAXACCEPT
  return MIN(get_maxnodes(site), MAXACCEPT);
#else
  return get_maxnodes(site);
#endif
}

static site_def *forced_config = nullptr;
static int is_forcing_node(pax_machine const *p) { return p->enforcer; }
static int wait_forced_config = 0;

/* Definition of majority */
static inline int majority(bit_set const *nodeset, site_def const *s, int all,
                           int delay [[maybe_unused]], int force) {
  node_no ok = 0;
  node_no i = 0;
  int retval = 0;
#ifdef WAIT_FOR_ALL_FIRST
  double sec = task_now();
#endif
  node_no max = max_check(s);

  /* IFDBG(D_NONE, FN; NDBG(max,lu); NDBG(all,d); NDBG(delay,d); NDBG(force,d));
   */

  /* Count nodes that has answered */
  for (i = 0; i < max; i++) {
    if (BIT_ISSET(i, nodeset)) {
      ok++;
    }
#ifdef WAIT_FOR_ALL_FIRST
    else {
      if (all) return 0; /* Delay until all nodes have answered */
      if (delay && !may_be_dead(s->detected, i, sec)) {
        return 0; /* Delay until all live nodes have answered */
      }
    }
#endif
  }

  /* If we are forcing messages, attempt to ensure consistency by
     requiring all remaining nodes to agree. Forced_config points to
     the config that should be used as acceptors in this
     case. Another possibility is to use the original config and
     count the number of live nodes, but since the force flag is
     being used only to force a new config, it seems safer to use
     the new config and no time-dependent info. Note that we are
     counting the answers based on the normal config, but use the
     number of nodes from forced_config. This is safe, since we can
     assume that the nodes that are not in forced_config will never
     answer. */

  if (force) {
    IFDBG(D_NONE, FN; STRLIT("force majority"); NDBG(ok, u); NDBG(max, u);
          NDBG(get_maxnodes(forced_config), u));
    return ok == get_maxnodes(forced_config);
  } else {
/* Have now seen answer from all live nodes */
#ifdef NODE_0_IS_ARBITRATOR
    retval = all ? ok == max
                 : ok > max / 2 ||
                       (ARBITRATOR_HACK && (get_nodeno(s) == 0) && (2 == max));
#else
    retval = all ? ok == max : ok > max / 2 || (ARBITRATOR_HACK && (2 == max));
#endif
    /* 	IFDBG(D_NONE, FN; NDBG(max,lu); NDBG(all,d); NDBG(delay,d);
     * NDBG(retval,d)); */
    return retval;
  }
}

#define IS_CONS_ALL(p) \
  ((p)->proposer.msg->a ? (p)->proposer.msg->a->consensus == cons_all : 0)

/* See if a majority of acceptors have answered our prepare */
static int prep_majority(site_def const *site, pax_machine const *p) {
  int ok = 0;

  assert(p);
  assert(p->proposer.prep_nodeset);
  assert(p->proposer.msg);
  /* IFDBG(D_NONE, FN; BALCEXP(p->proposer.bal)); */
  ok = majority(p->proposer.prep_nodeset, site, IS_CONS_ALL(p),
                p->proposer.bal.cnt <= 1,
                p->proposer.msg->force_delivery || p->force_delivery);
  return ok;
}

/* See if a majority of acceptors have answered our propose */
static int prop_majority(site_def const *site, pax_machine const *p) {
  int ok = 0;

  assert(p);
  assert(p->proposer.prop_nodeset);
  assert(p->proposer.msg);
  /* IFDBG(D_NONE, FN; BALCEXP(p->proposer.bal)); */
  ok = majority(p->proposer.prop_nodeset, site, IS_CONS_ALL(p),
                p->proposer.bal.cnt <= 1,
                p->proposer.msg->force_delivery || p->force_delivery);
  return ok;
}

/* Xcom thread */

static site_def *executor_site = nullptr;

site_def const *get_executor_site() { return executor_site; }
site_def *get_executor_site_rw() { return executor_site; }

static site_def *proposer_site = nullptr;

site_def const *get_proposer_site() { return proposer_site; }

/* delivered_msg may point to a no_op message, which will not actually be
 * delivered */
static synode_no delivered_msg = NULL_SYNODE;

synode_no get_delivered_msg() { return delivered_msg; }

/* last_delivered_msg is the last synode we actually delivered */
static synode_no last_delivered_msg = NULL_SYNODE;
synode_no get_last_delivered_msg() { return last_delivered_msg; }

void init_xcom_base() {
  IFDBG(D_NONE, FN);
  xcom_shutdown = 0;
  current_message = null_synode;
  executed_msg = null_synode;
  delivered_msg = null_synode;
  last_delivered_msg = null_synode;
  max_synode = null_synode;
  client_boot_done = 0;
  netboot_ok = 0;

  xcom_recover_init();
  my_id = new_id();
  push_site_def(nullptr);
  /*	update_servers(NULL); */
  xcom_cache_var_init();
  median_filter_init();
  link_init(&exec_wait, TYPE_HASH("task_env"));
  link_init(&detector_wait, TYPE_HASH("task_env"));
  link_init(&connect_wait, TYPE_HASH("task_env"));
  executor_site = nullptr;
  proposer_site = nullptr;

  /** Reset lsn */
  initialize_lsn(0);
  IFDBG(D_NONE, FN);
}

static void init_tasks() {
  IFDBG(D_NONE, FN);
  set_task(&boot, nullptr);
  set_task(&net_boot, nullptr);
  set_task(&net_recover, nullptr);
  set_task(&killer, nullptr);
  set_task(&executor, nullptr);
  set_task(&retry, nullptr);
  set_task(&detector, nullptr);
  init_proposers();
  set_task(&alive_t, nullptr);
  set_task(&sweeper, nullptr);
  set_task(&cache_task, nullptr);
  IFDBG(D_NONE, FN);
}

/* Initialize the xcom thread */
void xcom_thread_init() {
#ifndef NO_SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  init_base_vars();
  init_site_vars();
  init_crc32c();
  xcom_srand48((long int)task_now());

  init_xcom_base();
  init_tasks();

  /* Initialize input queue */
  channel_init(&prop_input_queue, TYPE_HASH("msg_link"));
  init_link_list();
  task_sys_init();

  init_cache();
}

/* Empty the proposer input queue */
static void empty_prop_input_queue() {
  empty_msg_channel(&prop_input_queue);
  IFDBG(D_NONE, FN; STRLIT("prop_input_queue empty"));
}

static void empty_synode_number_pool() {
  while (!synode_number_pool.data.empty()) {
    synode_number_pool.data.pop();
  }
}

/* De-initialize the xcom thread */
void xcom_thread_deinit() {
  IFDBG(D_BUG, FN; STRLIT("Empty proposer input queue"));
  empty_prop_input_queue();
  IFDBG(D_BUG, FN; STRLIT("Empty synode number pool"));
  empty_synode_number_pool();
  IFDBG(D_BUG, FN; STRLIT("Empty link free list"));
  empty_link_free_list();
  IFDBG(D_BUG, FN; STRLIT("De-initialize cache"));
  deinit_cache();
  garbage_collect_servers();
  IFDBG(D_BUG, FN; STRLIT("De-initialize network cache"));
  deinit_network_cache();
  IFDBG(D_BUG, FN; STRLIT("De-initialize xcom_interface"));
  deinit_xcom_interface();
}

#define PROP_ITER \
  int i;          \
  for (i = 0; i < PROPOSERS; i++)

static void init_proposers() {
  PROP_ITER { set_task(&proposer[i], nullptr); }
}

static void create_proposers() {
  PROP_ITER {
    set_task(&proposer[i], task_new(proposer_task, int_arg(i), "proposer_task",
                                    XCOM_THREAD_DEBUG));
  }
}

static synode_no *proposer_synodes[PROPOSERS];

static void add_proposer_synode(int i, synode_no *syn_ptr) {
  if (i >= 0 && i < PROPOSERS) {
    proposer_synodes[i] = syn_ptr;
  }
}

static void remove_proposer_synode(int i) { add_proposer_synode(i, nullptr); }

static synode_no get_proposer_synode(int i) {
  if (i >= 0 && i < PROPOSERS && proposer_synodes[i]) {
    return *proposer_synodes[i];
  } else {
    return null_synode;
  }
}

static synode_no min_proposer_synode() {
  synode_no s_min;
  int i;
  for (i = 0; i < PROPOSERS; i++) {
    s_min = get_proposer_synode(i);
    if (!synode_eq(null_synode, s_min)) break;  // Initial value
  }
  for (; i < PROPOSERS; i++) {
    if (synode_lt(get_proposer_synode(i), s_min))
      s_min = get_proposer_synode(i);
  }
  return s_min;
}

static void terminate_proposers() {
  PROP_ITER { task_terminate(proposer[i]); }
}

static void free_forced_config_site_def() {
  free_site_def(forced_config);
  forced_config = nullptr;
}

#if TASK_DBUG_ON
[[maybe_unused]] static void dbg_proposers();
static void dbg_proposers() {
  GET_GOUT;
  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;
  NDBG(PROPOSERS, d);
  {
    PROP_ITER { PPUT(proposer[i]); }
  }
  PRINT_GOUT;
  FREE_GOUT;
}
#endif

static void set_proposer_startpoint() {
  IFDBG(D_NONE, FN; STRLIT("changing current message"));
  if (synode_gt(max_synode, get_current_message())) {
    if (max_synode.msgno <= 1)
      set_current_message(first_free_synode_local(max_synode));
    else
      set_current_message(incr_msgno(first_free_synode_local(max_synode)));
  }
  if (synode_gt(executed_msg, get_current_message())) {
    set_current_message(first_free_synode_local(executed_msg));
  }
}

/* Task functions */

static xcom_state_change_cb xcom_run_cb = nullptr;
static xcom_state_change_cb xcom_terminate_cb = nullptr;
static xcom_state_change_cb xcom_comms_cb = nullptr;
static xcom_state_change_cb xcom_exit_cb = nullptr;
static xcom_state_change_cb xcom_expel_cb = nullptr;
static xcom_input_try_pop_cb xcom_try_pop_from_input_cb = nullptr;
[[maybe_unused]] static xcom_recovery_cb recovery_begin_cb = nullptr;
[[maybe_unused]] static xcom_recovery_cb recovery_restart_cb = nullptr;
[[maybe_unused]] static xcom_recovery_cb recovery_init_cb = nullptr;
[[maybe_unused]] static xcom_recovery_cb recovery_end_cb = nullptr;

void set_xcom_run_cb(xcom_state_change_cb x) { xcom_run_cb = x; }
void set_xcom_exit_cb(xcom_state_change_cb x) { xcom_exit_cb = x; }
void set_xcom_comms_cb(xcom_state_change_cb x) { xcom_comms_cb = x; }
void set_xcom_expel_cb(xcom_state_change_cb x) { xcom_expel_cb = x; }

void set_xcom_input_try_pop_cb(xcom_input_try_pop_cb pop) {
  xcom_try_pop_from_input_cb = pop;
}

#ifdef XCOM_STANDALONE
/* purecov: begin deadcode */
void set_xcom_terminate_cb(xcom_state_change_cb x) { xcom_terminate_cb = x; }
/* purecov: end */

/* purecov: begin deadcode */
void set_xcom_recovery_begin_cb(xcom_recovery_cb x) { recovery_begin_cb = x; }
/* purecov: end */

/* purecov: begin deadcode */
void set_xcom_recovery_restart_cb(xcom_recovery_cb x) {
  recovery_restart_cb = x;
}
/* purecov: end */

/* purecov: begin deadcode */
void set_xcom_recovery_init_cb(xcom_recovery_cb x) { recovery_init_cb = x; }
/* purecov: end */

/* purecov: begin deadcode */
void set_xcom_recovery_end_cb(xcom_recovery_cb x) { recovery_end_cb = x; }
/* purecov: end */
#endif

/**
 * These fields are used to signal XCom's request queue. After a request
 * is added, one will write 1 byte to warn local_server_task that it has work to
 * do.
 *
 * We use two types of signalling connection:
 * - An anonymous pipe, when possible, in POSIX compatible systems
 * - A regular socket connection, in Windows
 *
 * input_signal_connection is the connection_descriptor returned when one opens
 * a local signalling connection. It will contain either:
 * - The write side of a connection, in case of using a pipe OR;
 * - A bidirectional connection, when using a regular socket connection;
 *
 * input_signal_connection_pipe is the connection_descriptor that holds the read
 * side of a pipe connection. It is only allocated when we are able to have
 * a pipe connection.
 */

static connection_descriptor *input_signal_connection{nullptr};

connection_descriptor *input_signal_connection_pipe{nullptr};
int pipe_signal_connections[2] = {-1, -1};

#ifndef XCOM_WITHOUT_OPENSSL
static bool_t xcom_input_signal_connection_shutdown_ssl_wait_for_peer() {
  int ssl_error_code = 0;
  do {
    char buf[1024];
    ssl_error_code = SSL_read(input_signal_connection->ssl_fd, buf, 1024);
  } while (ssl_error_code > 0);

  {
    bool_t const successful =
        (SSL_get_error(input_signal_connection->ssl_fd, ssl_error_code) ==
         SSL_ERROR_ZERO_RETURN);
    return successful;
  }
}

static bool_t xcom_input_signal_connection_shutdown_ssl() {
  bool_t successful = FALSE;

  int ssl_error_code = SSL_shutdown(input_signal_connection->ssl_fd);

  bool_t const need_to_wait_for_peer_shutdown = (ssl_error_code == 0);
  bool_t const something_went_wrong = (ssl_error_code < 0);
  if (need_to_wait_for_peer_shutdown) {
    successful = xcom_input_signal_connection_shutdown_ssl_wait_for_peer();
    if (!successful) goto end;
  } else if (something_went_wrong) {
    goto end;
  }

  ssl_free_con(input_signal_connection);
  successful = TRUE;

end:
  return successful;
}
#endif

bool_t xcom_input_new_signal_connection(char const *address, xcom_port port) {
  bool_t const SUCCESSFUL = TRUE;
  bool_t const UNSUCCESSFUL = FALSE;
  assert(input_signal_connection == nullptr);

  if (input_signal_connection_pipe != nullptr) {
    input_signal_connection =
        (connection_descriptor *)malloc(sizeof(connection_descriptor));
    input_signal_connection->fd = pipe_signal_connections[1];
#ifndef XCOM_WITHOUT_OPENSSL
    input_signal_connection->ssl_fd = nullptr;
#endif
    set_connected(input_signal_connection, CON_FD);

    G_INFO("Successfully connected to the local XCom via anonymous pipe");

    return SUCCESSFUL;
  } else {
    /* purecov: begin deadcode */
    /* Try to connect. */
    input_signal_connection = open_new_local_connection(address, port);
    if (input_signal_connection->fd == -1) {
      return UNSUCCESSFUL;
    }

    /* Have the server handle the rest of this connection using a local_server
       task. */
    if (xcom_client_convert_into_local_server(input_signal_connection) == 1) {
      G_TRACE(
          "Converted the signalling connection handler into a local_server "
          "task on the client side.");

#ifndef XCOM_WITHOUT_OPENSSL
      /* No more SSL in this connection. */
      if (Network_provider_manager::getInstance().get_running_protocol() ==
          XCOM_PROTOCOL) {
        bool_t const using_ssl = (input_signal_connection->ssl_fd != nullptr);
        if (using_ssl) {
          bool_t successful = xcom_input_signal_connection_shutdown_ssl();
          if (!successful) {
            G_ERROR(
                "Error shutting down SSL on XCom's signalling connection on "
                "the "
                "client side.");
            xcom_input_free_signal_connection();
            return UNSUCCESSFUL;
          }
        }
      }
#endif
      G_INFO("Successfully connected to the local XCom via socket connection");
      return SUCCESSFUL;
    } else {
      G_INFO(
          "Error converting the signalling connection handler into a "
          "local_server task on the client side. This will result on a failure "
          "to join this node to a configuration");
      xcom_input_free_signal_connection();
      return UNSUCCESSFUL;
    }
    /* purecov: end */
  }
}

bool_t xcom_input_signal() {
  bool_t successful = FALSE;
  if (input_signal_connection != nullptr) {
    unsigned char tiny_buf[1] = {0};
    int64_t error_code;
    connnection_write_method to_write_function =
        input_signal_connection_pipe != nullptr ? con_pipe_write : con_write;

    error_code =
        socket_write(input_signal_connection, tiny_buf, 1, to_write_function);

    successful = (error_code == 1);
  }
  return successful;
}

void xcom_input_free_signal_connection() {
  if (input_signal_connection != nullptr) {
    if (input_signal_connection_pipe != nullptr) {
      close(input_signal_connection->fd);
    } else {
      /* purecov: begin deadcode */
      close_open_connection(input_signal_connection);
      /* purecov: end */
    }

    free(input_signal_connection);
    input_signal_connection = nullptr;
  }
}

#ifndef XCOM_WITHOUT_OPENSSL
static int local_server_shutdown_ssl(connection_descriptor *con, void *buf,
                                     int n, int *ret) {
  DECL_ENV
  int ssl_error_code;
  bool_t need_to_wait_for_peer_shutdown;
  bool_t something_went_wrong;
  int64_t nr_read;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  *ret = 0;
  TASK_BEGIN
  ep->ssl_error_code = SSL_shutdown(con->ssl_fd);
  ep->need_to_wait_for_peer_shutdown = (ep->ssl_error_code == 0);
  ep->something_went_wrong = (ep->ssl_error_code < 0);
  if (ep->need_to_wait_for_peer_shutdown) {
    do {
      TASK_CALL(task_read(con, buf, n, &ep->nr_read));
    } while (ep->nr_read > 0);
    ep->ssl_error_code =
        SSL_get_error(con->ssl_fd, static_cast<int>(ep->nr_read));
    ep->something_went_wrong = (ep->ssl_error_code != SSL_ERROR_ZERO_RETURN);
  }
  if (ep->something_went_wrong) TERMINATE;
  ssl_free_con(con);
  *ret = 1;
  FINALLY
  TASK_END;
}
#endif

int local_server(task_arg arg) {
  DECL_ENV
  connection_descriptor rfd;
  int ssl_shutdown_ret;
  unsigned char buf[1024]; /* arbitrary size */
  int64_t nr_read;
  xcom_input_request_ptr request;
  xcom_input_request_ptr next_request;
  pax_msg *request_pax_msg;
  pax_msg *reply_payload;
  linkage internal_reply_queue;
  msg_link *internal_reply;
  bool signaling_connection_error;
  connnection_read_method signal_read;
  ENV_INIT
  rfd.fd = -1;
  ssl_shutdown_ret = 0;
  memset(buf, 0, 1024);
  nr_read = 0;
  request = nullptr;
  link_init(&internal_reply_queue, TYPE_HASH("msg_link"));
  next_request = nullptr;
  request_pax_msg = nullptr;
  reply_payload = nullptr;
  internal_reply = nullptr;
  signaling_connection_error = false;
  END_ENV_INIT
  END_ENV;
  TASK_BEGIN
  assert(xcom_try_pop_from_input_cb != nullptr);
  {
    connection_descriptor *arg_rfd = (connection_descriptor *)get_void_arg(arg);
    ep->rfd = *arg_rfd;
    if (input_signal_connection_pipe == nullptr) free(arg_rfd);
  }

  // We will check if we have a pipe open or if we use a classic signalling
  // connection.
  ep->signal_read =
      input_signal_connection_pipe != nullptr ? con_pipe_read : con_read;

#ifndef XCOM_WITHOUT_OPENSSL
  /* No more SSL in this connection. */
  if (Network_provider_manager::getInstance().get_running_protocol() ==
          XCOM_PROTOCOL &&
      ep->rfd.ssl_fd) {
    TASK_CALL(local_server_shutdown_ssl(&ep->rfd, ep->buf, 1024,
                                        &ep->ssl_shutdown_ret));
    if (ep->ssl_shutdown_ret != 1) {
      G_ERROR(
          "Error shutting down SSL on XCom's signalling connection on the "
          "server side.");
      TERMINATE;
    }
  }
#endif

  while (!xcom_shutdown) {
    /* Wait for signal that there is work to consume from the queue. */
    if (!ep->signaling_connection_error) {
      TASK_CALL(
          task_read(&ep->rfd, ep->buf, 1024, &ep->nr_read, ep->signal_read));
      if (ep->nr_read == 0) {
        /* purecov: begin inspected */
        G_WARNING("local_server: client closed the signalling connection?");
        ep->signaling_connection_error = true;
        /* purecov: end */
      } else if (ep->nr_read < 0) {
        /* purecov: begin inspected */
        IFDBG(D_NONE, FN; NDBG64(ep->nr_read));
        G_WARNING(
            "local_server: error reading from the signalling connection?");
        ep->signaling_connection_error = true;
        /* purecov: end */
      }
    }

    /**
     * If an error occurs or if the client connection for the local server is
     * forcefully shutdown, we continue processing the queue until the end
     * resorting to time-based waits.
     */
    if (ep->signaling_connection_error) {
      TASK_DELAY(0.1);
    }

    /* Pop, dispatch, and reply. */
    ep->request = xcom_try_pop_from_input_cb();
    while (ep->request != nullptr) {
      /* Take ownership of the tail of the list, otherwise we lose it when we
         free ep->request. */
      ep->next_request = xcom_input_request_extract_next(ep->request);
      unchecked_replace_pax_msg(&ep->request_pax_msg,
                                pax_msg_new_0(null_synode));
      assert(ep->request_pax_msg->refcnt == 1);
      ep->request_pax_msg->op = client_msg;

      /* Take ownership of the request's app_data, otherwise the app_data is
         freed with ep->request. */
      ep->request_pax_msg->a = xcom_input_request_extract_app_data(ep->request);
      ep->request_pax_msg->to = VOID_NODE_NO;
      ep->request_pax_msg->force_delivery =
          (ep->request_pax_msg->a->body.c_t == force_config_type);
      dispatch_op(nullptr, ep->request_pax_msg, &ep->internal_reply_queue);
      if (!link_empty(&ep->internal_reply_queue)) {
        ep->internal_reply =
            (msg_link *)(link_extract_first(&ep->internal_reply_queue));
        assert(ep->internal_reply->p);
        assert(ep->internal_reply->p->refcnt == 1);
        /* We are going to take ownership of the pax_msg which has the reply
           payload, so we bump its reference count so that it is not freed by
           msg_link_delete. */
        ep->reply_payload = ep->internal_reply->p;
        ep->reply_payload->refcnt++;
        msg_link_delete(&ep->internal_reply);
        /* There should only have been one reply. */
        assert(link_empty(&ep->internal_reply_queue));
      } else {
        ep->reply_payload = nullptr;
      }
      /* Reply to the request. */
      xcom_input_request_reply(ep->request, ep->reply_payload);
      xcom_input_request_free(ep->request);
      ep->request = ep->next_request;
    }
  }
  FINALLY
  IFDBG(D_BUG, FN; STRLIT(" shutdown "); NDBG(ep->rfd.fd, d);
        NDBG(task_now(), f));
  /* Close the signalling connection. */
  if (!ep->signaling_connection_error) {
    if (input_signal_connection_pipe != nullptr &&
        ep->rfd.fd != -1) {  // We add -1 here, because in rare cases, the task
                             // might have not been activated. Thus, it might
                             // not have a reference to the socket to close.
      close(ep->rfd.fd);
      remove_and_wakeup(ep->rfd.fd);
    } else {
      shutdown_connection(&ep->rfd);
    }
  }

  unchecked_replace_pax_msg(&ep->request_pax_msg, nullptr);
  IFDBG(D_NONE, FN; NDBG(xcom_shutdown, d));
  TASK_END;
}

static bool_t local_server_is_setup() {
  return xcom_try_pop_from_input_cb != nullptr;
}

static void init_time_queue();
static int paxos_timer_task(task_arg arg [[maybe_unused]]);

int xcom_taskmain2(xcom_port listen_port) {
  init_xcom_transport(listen_port);

  IFDBG(D_BUG, FN; STRLIT("enter taskmain"));
  ignoresig(SIGPIPE);

  {
    result tcp_fd = {0, 0};

    /*
    Setup networking
    */
    auto &net_manager = Network_provider_manager::getInstance();
    bool error_starting_network_provider =
        net_manager.start_active_network_provider();
    if (error_starting_network_provider) {
      /* purecov: begin inspected */
      g_critical("Unable to start %s Network Provider",
                 Communication_stack_to_string::to_string(
                     net_manager.get_running_protocol()));
      if (xcom_comms_cb) {
        xcom_comms_cb(XCOM_COMMS_ERROR);
      }
      if (xcom_terminate_cb) {
        xcom_terminate_cb(0);
      }
      goto cleanup;
      /* purecov: end */
    }

// We will use POSIX pipes for local queue signaling if we are not using WIN32
#if !defined(_WIN32)
    if (local_server_is_setup()) {
      /* Launch local_server task to handle this connection. */
      {
        if (pipe(pipe_signal_connections) == -1) {
          /* purecov: begin inspected */
          g_critical("Unable to start local signaling mechanism");
          if (xcom_comms_cb) {
            xcom_comms_cb(XCOM_COMMS_ERROR);
          }
          if (xcom_terminate_cb) {
            xcom_terminate_cb(0);
          }
          goto cleanup;
          /* purecov: end */
        }
        unblock_fd(pipe_signal_connections[0]);

        /*
         Create the read side of input_signal_connection_pipe and create the
         local_server.

         If one would to use regular sockets, this code is not executed and
         the local_server is created in the dispatch_op function.
        */
        input_signal_connection_pipe =
            (connection_descriptor *)malloc(sizeof(connection_descriptor));
        input_signal_connection_pipe->fd = pipe_signal_connections[0];
#ifndef XCOM_WITHOUT_OPENSSL
        input_signal_connection_pipe->ssl_fd = nullptr;
#endif
        set_connected(input_signal_connection_pipe, CON_FD);
        task_new(local_server, void_arg(input_signal_connection_pipe),
                 "local_server", XCOM_THREAD_DEBUG);
      }
    }
#endif

    if (xcom_comms_cb) {
      xcom_comms_cb(XCOM_COMMS_OK);
    }

    IFDBG(D_NONE, FN; STRLIT("Creating tasks"));

    task_new(incoming_connection_task, int_arg(tcp_fd.val), "tcp_server",
             XCOM_THREAD_DEBUG);
    task_new(tcp_reaper_task, null_arg, "tcp_reaper_task", XCOM_THREAD_DEBUG);
#if defined(_WIN32)
    task_new(tcp_reconnection_task, null_arg, "tcp_reconnection_task",
             XCOM_THREAD_DEBUG);
#endif

    init_time_queue();
    task_new(paxos_timer_task, null_arg, "paxos_timer_task", XCOM_THREAD_DEBUG);
    IFDBG(D_BUG, FN; STRLIT("XCOM is listening on "); NPUT(listen_port, d));
  }

#ifdef XCOM_STANDALONE
  if (recovery_init_cb) recovery_init_cb();

  if (recovery_begin_cb) recovery_begin_cb();
#endif

  task_loop();
cleanup:

#ifdef TASK_EVENT_TRACE
  dump_task_events();
#endif
  // STOP NETWORK PROVIDERS
  Network_provider_manager::getInstance().stop_all_network_providers();

  xcom_thread_deinit();

  IFDBG(D_BUG, FN; STRLIT(" exit "); NDBG(xcom_dbg_stack_top, d);
        NDBG((unsigned)xcom_debug_mask, x));
  xcom_debug_mask = 0;
  xcom_dbg_stack_top = 0;
  if (input_signal_connection_pipe != nullptr) {
    ::xcom_input_free_signal_connection();

    free(input_signal_connection_pipe);
    input_signal_connection_pipe = nullptr;

    pipe_signal_connections[0] = -1;
    pipe_signal_connections[1] = -1;
  }

  if (xcom_exit_cb) {
    xcom_exit_cb(0);
  }

  return 1;
}

/* Paxos message construction and sending */

/* Initialize a message for sending */
static void prepare(pax_msg *p, pax_op op) {
  p->op = op;
  p->reply_to = p->proposal;
}

/* Initialize a prepare_msg */
void init_prepare_msg(pax_msg *p) { prepare(p, prepare_op); }

static int prepare_msg(pax_msg *p) {
  init_prepare_msg(p);
  /* p->msg_type = normal; */
  return send_to_acceptors(p, "prepare_msg");
}

/* Initialize a noop_msg */
pax_msg *create_noop(pax_msg *p) {
  init_prepare_msg(p);
  p->msg_type = no_op;
  return p;
}

/* Initialize a read_msg */
static pax_msg *create_read(site_def const *site, pax_msg *p) {
  p->msg_type = normal;
  p->proposal.node = get_nodeno(site);
  prepare(p, read_op);
  return p;
}

static int skip_msg(pax_msg *p) {
  prepare(p, skip_op);
  IFDBG(D_NONE, FN; STRLIT("skipping message "); SYCEXP(p->synode));
  p->msg_type = no_op;
  return send_to_all(p, "skip_msg");
}

static void brand_app_data(pax_msg *p) {
  app_data_ptr a = p->a;
  while (a) {
    a->app_key = p->synode;
    a->group_id = p->synode.group_id;
    IFDBG(D_NONE, FN; PTREXP(a); SYCEXP(p->synode); SYCEXP(a->app_key));
    a = a->next;
  }
}

static synode_no my_unique_id(synode_no synode) {
  assert(my_id != 0);
  site_def const *site = find_site_def(synode);
  /* Random number derived from node number and timestamp which uniquely defines
   * this instance */
  synode.group_id = my_id;
  synode.node = get_nodeno(site);
  return synode;
}

static void set_unique_id(pax_msg *msg, synode_no synode) {
  app_data_ptr a = msg->a;
  while (a) {
    a->unique_id = synode;
    a = a->next;
  }
}

void init_propose_msg(pax_msg *p) {
  p->op = accept_op;
  p->reply_to = p->proposal;
  brand_app_data(p);
  /* set_unique_id(p, my_unique_id(synode)); */
}

static int send_propose_msg(pax_msg *p) {
  return send_to_acceptors(p, "propose_msg");
}

static int propose_msg(pax_msg *p) {
  init_propose_msg(p);
  return send_propose_msg(p);
}

static void set_learn_type(pax_msg *p) {
  p->op = learn_op;
  p->msg_type = p->a ? normal : no_op;
}

/* purecov: begin deadcode */
static void init_learn_msg(pax_msg *p) {
  set_learn_type(p);
  p->reply_to = p->proposal;
}

static int send_learn_msg(site_def const *site, pax_msg *p) {
  IFDBG(D_NONE, FN; dbg_bitset(p->receivers, get_maxnodes(site)););
  return send_to_all_site(site, p, "learn_msg");
}
/* purecov: end */

static pax_msg *create_tiny_learn_msg(pax_machine *pm, pax_msg *p) {
  pax_msg *tiny_learn_msg = clone_pax_msg_no_app(p);

  ref_msg(tiny_learn_msg);
  tiny_learn_msg->msg_type = p->a ? normal : no_op;
  tiny_learn_msg->op = tiny_learn_op;
  tiny_learn_msg->reply_to = pm->proposer.bal;

  return tiny_learn_msg;
}

static int send_tiny_learn_msg(site_def const *site, pax_msg *p) {
  int retval = send_to_all_site(site, p, "tiny_learn_msg");
  unref_msg(&p);
  return retval;
}

/* Proposer task */

void prepare_push_3p(site_def const *site, pax_machine *p, pax_msg *msg,
                     synode_no msgno, pax_msg_type msg_type) {
  IFDBG(D_NONE, FN; SYCEXP(msgno); NDBG(p->proposer.bal.cnt, d);
        NDBG(p->acceptor.promise.cnt, d));
  BIT_ZERO(p->proposer.prep_nodeset);
  p->proposer.bal.node = get_nodeno(site);
  {
    int maxcnt = MAX(p->proposer.bal.cnt, p->acceptor.promise.cnt);
    p->proposer.bal.cnt = ++maxcnt;
  }
  msg->synode = msgno;
  msg->proposal = p->proposer.bal;
  msg->msg_type = msg_type;
  msg->force_delivery = p->force_delivery;
}

void prepare_push_2p(site_def const *site, pax_machine *p) {
  assert(p->proposer.msg);

  BIT_ZERO(p->proposer.prop_nodeset);
  IFDBG(D_NONE, FN; SYCEXP(p->synode));
  p->proposer.bal.cnt = 0;
  p->proposer.bal.node = get_nodeno(site);
  p->proposer.msg->proposal = p->proposer.bal;
  p->proposer.msg->synode = p->synode;
  p->proposer.msg->force_delivery = p->force_delivery;
}

static void push_msg_2p(site_def const *site, pax_machine *p) {
  prepare_push_2p(site, p);
  propose_msg(p->proposer.msg);
}

static void push_msg_3p(site_def const *site, pax_machine *p, pax_msg *msg,
                        synode_no msgno, pax_msg_type msg_type) {
  if (wait_forced_config) {
    force_pax_machine(p, 1);
  }

  assert(msgno.msgno != 0);
  prepare_push_3p(site, p, msg, msgno, msg_type);
  assert(p->proposer.msg);
  prepare_msg(msg);
  IFDBG(D_NONE, FN; BALCEXP(msg->proposal); SYCEXP(msgno); STRLIT(" op ");
        STRLIT(pax_op_to_str(msg->op)));
}

/* Brand client message with unique ID */
static void brand_client_msg(pax_msg *msg, synode_no msgno) {
  assert(!synode_eq(msgno, null_synode));
  set_unique_id(msg, my_unique_id(msgno));
}

void xcom_send(app_data_ptr a, pax_msg *msg) {
  IFDBG(D_NONE, FN; PTREXP(a); SYCEXP(a->app_key); SYCEXP(msg->synode));
  msg->a = a;
  msg->op = client_msg;
  {
    msg_link *link = msg_link_new(msg, VOID_NODE_NO);
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_pax_msg(msg)));
    channel_put(&prop_input_queue, &link->l);
  }
}

#define FNVSTART 0x811c9dc5

/* Fowler-Noll-Vo type multiplicative hash */
static uint32_t fnv_hash(unsigned char *buf, size_t length, uint32_t sum) {
  size_t i = 0;
  for (i = 0; i < length; i++) {
    sum = sum * (uint32_t)0x01000193 ^ (uint32_t)buf[i];
  }
  return sum;
}

/**
   Create a new (hopefully unique) ID. The basic idea is to create a hash from
   the host ID and a timestamp.
*/
uint32_t new_id() {
  long id = xcom_unique_long();
  double timestamp = task_now();
  uint32_t retval = 0;
  while (retval == 0 ||
         is_dead_site(retval)) { /* Avoid returning 0 or already used site id */
    retval = fnv_hash((unsigned char *)&id, sizeof(id), 0);
    retval = fnv_hash((unsigned char *)&timestamp, sizeof(timestamp), retval);
  }
  return retval;
}

static synode_no getstart(app_data_ptr a) {
  synode_no retval = null_synode;
  /* If a->group_id is null_id, we set the group id  from app_key.group_id,
   * which is hopefully not null_id. If it is, we're out of luck. */
  if (a && a->group_id == null_id) {
    /* purecov: begin deadcode */
    a->group_id = a->app_key.group_id; /* app_key may have valid group */
                                       /* purecov: end */
  }
  G_DEBUG("pid %d getstart group_id %x", xpid(), a->group_id);
  if (!a || a->group_id == null_id) {
    retval.group_id = new_id();
  } else {
    a->app_key.group_id = a->group_id;
    retval = a->app_key;
    if (get_site_def() &&
        retval.msgno > 1) { /* Special case for initial boot of site */
      /* Not valid until after event horizon has been passed */
      retval = add_event_horizon(retval);
    }
  }
  return retval;
}

#ifdef PERMISSIVE_EH_ACTIVE_CONFIG
/* purecov: begin deadcode */
synode_no get_default_start(app_data_ptr a) {
  synode_no retval = null_synode;
  /* If a->group_id is null_id, we set the group id  from app_key.group_id,
   * which is hopefully not null_id. If it is, we're out of luck. */
  if (a && a->group_id == null_id) {
    a->group_id = a->app_key.group_id; /* app_key may have valid group */
  }
  G_DEBUG("pid %d getstart group_id %x", xpid(), a ? a->group_id : 0);
  if (!a || a->group_id == null_id) {
    retval.group_id = new_id();
  } else {
    a->app_key.group_id = a->group_id;
    retval = a->app_key;
    if (retval.msgno > 1) { /* Special case for initial boot of site */
      /* Not valid until after event horizon has been passed */
      retval = add_default_event_horizon(retval);
    }
  }
  return retval;
}
/* purecov: end */
#endif

#if TASK_DBUG_ON
/* purecov: begin deadcode */
static void dump_xcom_node_names(site_def const *site) {
  u_int i;
  constexpr const size_t bufsize = NSERVERS * 256;
  char buf[bufsize]; /* Big enough */
  char *p = buf;
  if (!site) {
    G_INFO("pid %d no site", xpid());
    return;
  }
  *p = 0;
  for (i = 0; i < site->nodes.node_list_len; i++) {
    p = strncat(p, site->nodes.node_list_val[i].address, bufsize - 1);
    p = strncat(p, " ", bufsize - 1);
  }
  buf[bufsize - 1] = 0;
  G_INFO("pid %d node names %s", xpid(), buf);
}
/* purecov: end */
#endif

void site_install_action(site_def *site, cargo_type operation) {
  IFDBG(D_NONE, FN; NDBG(get_nodeno(get_site_def()), u));
  assert(site->event_horizon);
  if (group_mismatch(site->start, max_synode) ||
      synode_gt(site->start, max_synode))
    set_max_synode(site->start);
  site->nodeno = xcom_find_node_index(&site->nodes);
  push_site_def(site);
  IFDBG(D_NONE, dump_xcom_node_names(site));
  IFDBG(D_BUG, FN; SYCEXP(site->start); SYCEXP(site->boot_key);
        NUMEXP(site->max_active_leaders));
  IFDBG(D_BUG, FN; COPY_AND_FREE_GOUT(dbg_site_def(site)));
  set_group(get_group_id(site));
  if (get_maxnodes(get_site_def())) {
    update_servers(site, operation);
  }
  site->install_time = task_now();
  G_INFO(
      "Sucessfully installed new site definition. Start synode for this "
      "configuration is " SY_FMT ", boot key synode is " SY_FMT
      ", configured event horizon=%" PRIu32 ", my node identifier is %u",
      SY_MEM(site->start), SY_MEM(site->boot_key), site->event_horizon,
      get_nodeno(site));
  IFDBG(D_NONE, FN; NDBG(get_nodeno(site), u));
  IFDBG(D_NONE, FN; SYCEXP(site->start); SYCEXP(site->boot_key);
        NDBG(site->install_time, f));
  IFDBG(D_NONE, FN; NDBG(get_nodeno(site), u));
  ADD_DBG(
      D_BASE, add_event(EVENT_DUMP_PAD, string_arg("nodeno"));
      add_event(EVENT_DUMP_PAD, uint_arg(get_nodeno(site)));
      add_event(EVENT_DUMP_PAD, string_arg("site->boot_key"));
      add_synode_event(site->boot_key);
      /* add_event(EVENT_DUMP_PAD, uint_arg(chksum_node_list(&site->nodes))); */
  );
}

static void active_leaders(site_def *site, leader_array *leaders) {
  u_int i;
  u_int n;
  /* Synthesize leaders by copying all node names of active leaders */
  for (i = 0, n = 0; i < site->nodes.node_list_len; i++) {
    if (is_active_leader(i, site)) n++;
  }
  leaders->leader_array_len = n;
  if (n) {
    leaders->leader_array_val = static_cast<leader *>(
        xcom_calloc((size_t)leaders->leader_array_len, sizeof(leader)));
    for (i = 0, n = 0; i < site->nodes.node_list_len; i++) {
      if (is_active_leader(i, site)) {
        leaders->leader_array_val[n++].address =
            strdup(site->nodes.node_list_val[i].address);
      }
    }
  } else {
    leaders->leader_array_val = nullptr;
  }
}

extern "C" void synthesize_leaders(leader_array *leaders) {
  // Default value meaning 'not set by client '
  leaders->leader_array_len = 0;
  leaders->leader_array_val = nullptr;
}

static bool leaders_set_by_client(site_def const *site) {
  return site->leaders.leader_array_len != 0;
}

static site_def *create_site_def_with_start(app_data_ptr a, synode_no start) {
  site_def *site = new_site_def();
  IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)););
  init_site_def(a->body.app_u_u.nodes.node_list_len,
                a->body.app_u_u.nodes.node_list_val, site);
  site->start = start;
  site->boot_key = a->app_key;

// If SINGLE_WRITER_ONLY is defined, ALL configs will be single writer. Used for
// running all tests in single writer mode
#ifdef SINGLE_WRITER_ONLY
  site->max_active_leaders = 1; /*Single writer */
#else
  site->max_active_leaders = active_leaders_all; /* Set to all nodes*/
#endif

  return site;
}

static xcom_proto constexpr single_writer_support = x_1_9;

static site_def *install_ng_with_start(app_data_ptr a, synode_no start) {
  if (a) {
    site_def *site = create_site_def_with_start(a, start);
    site_def const *old_site = get_site_def();

    // The reason why we need to recompute node sets and time stamps, is that
    // node sets and time stamps are stored in the site_def indexed by node
    // number, but they really are related to a specific node, not a specific
    // node number. When the site_def changes, the node number of a node may
    // change, thus invalidating the mapping from node numbers to node sets and
    // timestamps. But given the old and new definition, it is possible to
    // remap.
    if (old_site && old_site->x_proto >= single_writer_support) {
      recompute_node_sets(old_site, site);
      recompute_timestamps(old_site->detected, &old_site->nodes, site->detected,
                           &site->nodes);
    }
    site_install_action(site, a->body.c_t);
    return site;
  }
  return nullptr;
}

site_def *install_node_group(app_data_ptr a) {
  ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("a->app_key"));
          add_synode_event(a->app_key););
  if (a)
    return install_ng_with_start(a, getstart(a));
  else
    return nullptr;
}

void set_max_synode(synode_no synode) {
  max_synode = synode; /* Track max synode number */
  IFDBG(D_BASE, FN; STRLIT("new "); SYCEXP(max_synode));
  activate_sweeper();
}

static int is_busy(synode_no s) {
  pax_machine *p = hash_get(s);
  if (!p) {
    return 0;
  } else {
    return started(p);
  }
}

bool_t match_my_msg(pax_msg *learned, pax_msg *mine) {
  IFDBG(D_NONE, FN; PTREXP(learned->a);
        if (learned->a) SYCEXP(learned->a->unique_id); PTREXP(mine->a);
        if (mine->a) SYCEXP(mine->a->unique_id););
  if (learned->a && mine->a) { /* Both have app data, see if data is mine */
    return synode_eq(learned->a->unique_id, mine->a->unique_id);
  } else if (!(learned->a || mine->a)) { /* None have app data, anything goes */
    return TRUE;
  } else { /* Definitely mismatch */
    return FALSE;
  }
}

/*
 * Initialize the log sequence number (lsn).
 */
void initialize_lsn(uint64_t n) { lsn = n; }

/**
 * Assign the next log sequence number (lsn) for a message.
 *
 * Initial propose sets lsn to msgno of the max message number as safe starting
 * point, otherwise lsn shall be ever increasing. lsn ensures sender order known
 * on receiver side, as messages may arrive "out of order" due to
 * retransmission. We use max_synode instead of current_message to avoid any
 * conflict with lsn allocated by a previous instance of the node.
 */
static uint64_t assign_lsn() {
  if (lsn == 0) {
    initialize_lsn(max_synode.msgno);
  }
  lsn++;
  IFDBG(D_EXEC, NDBG64(lsn));
  return lsn;
}

#if TASK_DBUG_ON
/* purecov: begin deadcode */
static int check_lsn(app_data_ptr a) {
  while (a) {
    if (!a->lsn) return 0;
    a = a->next;
  }
  return 1;
}
/* purecov: end */
#endif

static void propose_noop(synode_no find, pax_machine *p);

/**
 * Checks if the given synod s is outside the event horizon.
 *
 * Common case: there are no configurations pending, or if there are, none of
 * them reconfigure the event horizon. The common case threshold is:
 *
 *   last_executed_synod + event_horizon(active_config)
 *
 *
 * If an event horizon reconfiguration R is pending, it is possible that it
 * reduces the event horizon. In that case, it is possible that the threshold
 * above falls outside the new event horizon.
 *
 * For example, consider last_executed_synod = 42 and
 * event_horizon(active_config) = 10.
 * At this point this member participates in synods up to 52.
 * Now consider an event horizon reconfiguration that takes effect at synod 45,
 * which modifies the event horizon to 2. This means that when
 * last_executed_synod = 45, event_horizon(active_config) = 2. At this point
 * this member should only be able to participate in synods up to 47. The member
 * may have previously started processing messages directed to synods between 47
 * and 52, but will now ignore messages directed to those same synods.
 *
 * We do not want to start processing messages that will eventually fall out
 * of the event horizon. More importantly, the threshold above may not be safe
 * due to the exit logic of executor_task.
 *
 * When a node removes itself from the group on configuration C starting at
 * synod start(C), the exit logic relies on knowing *when* a majority has
 * executed synod start(C) - 1, i.e. the last message of the last configuration
 * to contain the leaving node.
 *
 * With a constant event horizon, we know that when synod
 * start(C) + event_horizon is learnt, it is because a majority already executed
 * or is ready to execute (and thus learned) synod start(C). This implies that a
 * majority already executed start(C) - 1.
 *
 * With a dynamic event horizon, we cannot be sure that when synod
 * start(C) + event_horizon(C) is learnt, a majority already executed or is
 * ready to execute synod start(C).
 * This is because it is possible for a new, smaller, event horizon to take
 * effect between start(C) and start(C) + event_horizon(C).
 * If that happens, the threshold above allows nodes to participate in synods
 * which are possibly beyond start(C) + event_horizon(C), which can lead to the
 * value of synod start(C) + event_horizon(C) being learnt without a majority
 * already having executed or being ready to execute synod start(C).
 *
 * In order to maintain the assumption made by the executor_task's exit logic,
 * when an event horizon reconfiguration R is pending we set the threshold to
 * the minimum between:
 *
 *   last_executed_synod + event_horizon(active_config)
 *
 * and:
 *
 *   start(R) - 1 + event_horizon(R)
 */
static uint64_t too_far_threshold(xcom_event_horizon active_event_horizon) {
  return executed_msg.msgno + active_event_horizon;
}

static uint64_t too_far_threshold_new_event_horizon_pending(
    site_def const *new_config) {
  uint64_t last_executed = executed_msg.msgno;
  /* compute normal threshold */
  uint64_t possibly_unsafe_threshold;
  site_def const *active_config = find_site_def(executed_msg);
  xcom_event_horizon active_event_horizon = active_config->event_horizon;
  possibly_unsafe_threshold = last_executed + active_event_horizon;
  /* compute threshold taking into account new event horizon */ {
    uint64_t maximum_safe_threshold;
    xcom_event_horizon new_event_horizon;
    uint64_t start_new_event_horizon = new_config->start.msgno;
    new_event_horizon = new_config->event_horizon;
    maximum_safe_threshold = start_new_event_horizon - 1 + new_event_horizon;
    /* use the minimum of both for safety */
    return MIN(possibly_unsafe_threshold, maximum_safe_threshold);
  }
}

static inline int too_far(synode_no s) {
  uint64_t threshold = 0;
  site_def const *active_config = find_site_def(executed_msg);
  if (active_config != nullptr) {
    site_def const *pending_config = first_event_horizon_reconfig();
    bool_t const no_event_horizon_reconfig_pending =
        (pending_config == nullptr);
    if (is_latest_config(active_config) || no_event_horizon_reconfig_pending) {
      threshold = too_far_threshold(active_config->event_horizon);
    } else {
      threshold = too_far_threshold_new_event_horizon_pending(pending_config);
    }
  } else {
    /* we have no configs, resort to default */
    threshold = too_far_threshold(EVENT_HORIZON_MIN);
  }
  return s.msgno >= threshold;
}

#define GOTO(x)                                 \
  {                                             \
    IFDBG(D_NONE, STRLIT("goto "); STRLIT(#x)); \
    goto x;                                     \
  }

static inline int is_view(cargo_type x) { return x == view_msg; }

static inline int is_config(cargo_type x) {
  return x == unified_boot_type || x == add_node_type ||
         x == remove_node_type || x == set_event_horizon_type ||
         x == force_config_type || x == set_max_leaders ||
         x == set_leaders_type;
}

static int wait_for_cache(pax_machine **pm, synode_no synode, double timeout);
static int prop_started = 0;
static int prop_finished = 0;

/* Find a free slot locally.
   Note that we will happily increment past the event horizon.
   The caller is thus responsible for checking the validity of the
   returned value by calling too_far() and ignore_message(). */
static synode_no local_synode_allocator(synode_no synode) {
  assert(!synode_eq(synode, null_synode));

  // Ensure node number of synode is ours, whilst also ensuring that the synode
  // is monotonically increasing
  node_no const my_nodeno = get_nodeno(find_site_def(synode));
  if (my_nodeno >= synode.node) {
    synode.node = my_nodeno;
  } else {
    synode = incr_msgno(synode);
  }

  while (is_busy(synode)) {
    synode = incr_msgno(synode);
  }
  assert(!synode_eq(synode, null_synode));
  return synode;
}

/* Find a likely free slot globally.
   Note that we will happily increment past the event horizon.
   The caller is thus responsible for checking the validity of the
   returned value by calling too_far() and ignore_message().
   The test for ignore_message() here is only valid until the
   event horizon. */
static synode_no global_synode_allocator(site_def *site, synode_no synode) {
  assert(!synode_eq(synode, null_synode));

  while (ignore_message(synode, site, "global_synode_allocator")) {
    synode = incr_synode(synode);
  }
  assert(!synode_eq(synode, null_synode));
  return synode;
}

/* Find a free slot on remote leader */
static node_no remote_synode_allocator(site_def *site, app_data const &a) {
  static node_no distributor = 0;  // Distribute requests equally among leaders
  node_no maxnodes = get_maxnodes(site);
  distributor = distributor % maxnodes;  // Rescale in case site has changed
  node_no i = distributor;
  // Ensure that current_message is associated with site
  if (synode_lt(current_message, site->start)) {
    current_message = site->start;
  }
  for (;;) {
    if (is_active_leader(i, site) &&
        !may_be_dead(site->detected, i,
                     task_now())) {  // Found leader, send request
      pax_msg *p =
          pax_msg_new(current_message, site);  // Message number does not matter
      IFDBG(D_CONS, FN; STRLIT("sending request "); NUMEXP(i);
            SYCEXP(current_message));
      p->op = synode_request;
      send_server_msg(site, i, p);
      distributor = (i + 1) % maxnodes;
      return i;
    }
    i = (i + 1) % maxnodes;
    if (i == distributor) {
      /* There are no leaders, see if we should become leader. Note the special
         case for `force_config_type`. If we are in a network partition
         situation that must be healed using `force_config_type`, the leader
         might not be available and we might not be `iamthegreatest`. If we are
         the one tasked with `force_config_type` the entire system is relying on
         us to get consensus on `force_config_type` to unblock the group.
         Therefore, we self-allocate a synod for `force_config_type` to ensure
         the system makes progress. */
      if (iamthegreatest(site) || a.body.c_t == force_config_type) {
        // Grab message number and answer to self
        synode_no synode = global_synode_allocator(site, current_message);
        if (!too_far(synode)) {
          // We will grab this number, advance current_message
          set_current_message(incr_synode(synode));
          IFDBG(D_CONS, FN; STRLIT("grab message "); SYCEXP(synode);
                SYCEXP(current_message));
          synode_number_pool.put(synode, synode_allocation_type::global);
        }
      }
      return get_nodeno(site);
    }
  }
}

#ifdef DELIVERY_TIMEOUT
static bool check_delivery_timeout(site_def *site, double start_propose,
                                   app_data *a) {
  bool retval =
      (start_propose + a->expiry_time) < task_now() && !enough_live_nodes(site);
  if (retval) {
    DBGOUT_ASSERT(check_lsn(a), STRLIT("NULL lsn"));
    IFDBG(D_NONE, FN; STRLIT("timeout -> delivery_failure"));
    deliver_to_app(NULL, a, delivery_failure);
  }
  return retval;
}
#endif

static int reserve_synode_number(synode_allocation_type *synode_allocation,
                                 site_def **site, synode_no *msgno,
                                 int *remote_retry, app_data *a,
                                 synode_reservation_status *ret) {
  *ret = synode_reservation_status::number_ok;  // Optimistic, will be reset if
                                                // necessary
  DECL_ENV
  int dummy;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN
  do {
    *synode_allocation = synode_allocation_type::todo;
    IFDBG(D_CONS, FN; SYCEXP(current_message));
    *site = find_site_def_rw(current_message);
    if (is_leader(*site)) {  // Use local synode allocator
      *msgno = local_synode_allocator(current_message);
      IFDBG(D_CONS, FN; SYCEXP(outer_ep->msgno));
      *synode_allocation = synode_allocation_type::local;
    } else {  // Cannot use local, try remote
              // Get synode number from another leader
      *remote_retry = 0;
      while (synode_number_pool.empty()) {
        // get_maxnodes(get_site_def()) > 0 is a precondition for
        // `remote_synode_allocator`.
        if (get_maxnodes(get_site_def()) == 0) {
          TASK_DELAY(0.1);
          TASK_RETURN(synode_reservation_status::no_nodes);
        }
#if TASK_DBUG_ON
        node_no allocator_node =
#endif
            remote_synode_allocator(get_site_def_rw(),
                                    *a);  // Send request for synode, use
                                          // latest config
        if (*remote_retry > 10) {
          IFDBG(D_BUG, FN; NUMEXP(outer_ep->self); NUMEXP(allocator_node);
                SYCEXP(executed_msg); SYCEXP(current_message);
                SYCEXP(outer_ep->msgno); SYCEXP(get_site_def_rw()->start));
        }
        if (synode_number_pool.empty()) {  // Only wait if still empty
          TIMED_TASK_WAIT(&synode_number_pool.queue,
                          0.1);  // Wait for incoming synode
        }
        (*remote_retry)++;
      }
      std::tie(*msgno, *synode_allocation) = synode_number_pool.get();
      IFDBG(D_CONS, FN; SYCEXP(outer_ep->msgno));
    }

    // Update site to match synode
    *site = proposer_site = find_site_def_rw(*msgno);

    // Set the global current message for all number allocators
    set_current_message(incr_synode(*msgno));

    while (too_far(*msgno)) { /* Too far ahead of executor */
      TIMED_TASK_WAIT(&exec_wait, 0.2);
      IFDBG(D_NONE, FN; SYCEXP(ep->msgno); TIMECEXP(ep->start_propose);
            TIMECEXP(outer_ep->client_msg->p->a->expiry_time);
            TIMECEXP(task_now()); NDBG(enough_live_nodes(outer_ep->site), d));
#ifdef DELIVERY_TIMEOUT
      if (check_delivery_timeout(outer_ep->site, outer_ep->start_propose,
                                 outer_ep->client_msg->p->a)) {
        TASK_RETURN(delivery_timeout);
      }
#endif
    }
    // Filter out busy or ignored message numbers
  } while (is_busy(*msgno) || ignore_message(*msgno, *site, "proposer_task"));
  FINALLY
  TASK_END;
}

/* Send messages by fetching from the input queue and trying to get it accepted
   by a Paxos instance */
static int proposer_task(task_arg arg) {
  DECL_ENV
  int self;             /* ID of this proposer task */
  pax_machine *p;       /* Pointer to Paxos instance */
  msg_link *client_msg; /* The client message we are trying to push */
  synode_no msgno;
  pax_msg *prepare_msg;
  double start_propose;
  double start_push;
  double delay;
  site_def *site;
  size_t size;
  size_t nr_batched_app_data;
  int remote_retry;
  synode_allocation_type synode_allocation;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  synode_reservation_status reservation_status{
      synode_reservation_status::number_ok};

  TASK_BEGIN

  ep->self = get_int_arg(arg);
  ep->p = nullptr;
  ep->client_msg = nullptr;
  ep->prepare_msg = nullptr;
  ep->start_propose = 0.0;
  ep->start_push = 0.0;
  ep->delay = 0.0;
  ep->msgno = current_message;
  ep->site = nullptr;
  ep->size = 0;
  ep->nr_batched_app_data = 0;
  ep->remote_retry = 0;
  ep->synode_allocation = synode_allocation_type::todo;
  add_proposer_synode(ep->self, &ep->msgno);
  IFDBG(D_NONE, FN; NDBG(ep->self, d); NDBG(task_now(), f));

  while (!xcom_shutdown) { /* Loop until no more work to do */
    /* Wait for client message */
    assert(!ep->client_msg);
    CHANNEL_GET(&prop_input_queue, &ep->client_msg, msg_link);
    prop_started++;
    IFDBG(D_NONE, FN; PTREXP(ep->client_msg->p->a); STRLIT("extracted ");
          SYCEXP(ep->client_msg->p->a->app_key));

    /* Grab rest of messages in queue as well, but never batch config messages,
     * which need a unique number */

    /* The batch is limited either by size or number of batched app_datas.
     * We limit the number of elements because the XDR deserialization
     * implementation is recursive, and batching too many app_datas will cause a
     * call stack overflow. */
    if (!is_config(ep->client_msg->p->a->body.c_t) &&
        !is_view(ep->client_msg->p->a->body.c_t)) {
      ep->size = app_data_size(ep->client_msg->p->a);
      ep->nr_batched_app_data = 1;
      while (AUTOBATCH && ep->size <= MAX_BATCH_SIZE &&
             ep->nr_batched_app_data <= MAX_BATCH_APP_DATA &&
             !link_empty(&prop_input_queue
                              .data)) { /* Batch payloads into single message */
        msg_link *tmp;
        app_data_ptr atmp;

        CHANNEL_GET(&prop_input_queue, &tmp, msg_link);
        atmp = tmp->p->a;
        ep->size += app_data_size(atmp);
        ep->nr_batched_app_data++;
        /* Abort batching if config or too big batch */
        if (is_config(atmp->body.c_t) || is_view(atmp->body.c_t) ||
            ep->nr_batched_app_data > MAX_BATCH_APP_DATA ||
            ep->size > MAX_BATCH_SIZE) {
          channel_put_front(&prop_input_queue, &tmp->l);
          break;
        }
        ADD_T_EV(seconds(), __FILE__, __LINE__, "batching");

        tmp->p->a = nullptr;               /* Steal this payload */
        msg_link_delete(&tmp);             /* Get rid of the empty message */
        atmp->next = ep->client_msg->p->a; /* Add to list of app_data */
                                           /* G_TRACE("Batching %s %s",
                                            * cargo_type_to_str(ep->client_msg->p->a->body.c_t), */
        /* 	cargo_type_to_str(atmp->body.c_t)); */
        ep->client_msg->p->a = atmp;
        IFDBG(D_NONE, FN; PTREXP(ep->client_msg->p->a); STRLIT("extracted ");
              SYCEXP(ep->client_msg->p->a->app_key));
      }
    }

    ep->start_propose = task_now();
    ep->delay = 0.0;

    assert(!ep->client_msg->p->a->chosen);

    /* It is a new message */

    assert(!synode_eq(current_message, null_synode));

    /* Assign a log sequence number only on initial propose */
    {
      uint64_t prop_lsn = assign_lsn();
      app_data_ptr ap = ep->client_msg->p->a;
      /* Assign to all app_data structs */
      while (ap) {
        ap->lsn = prop_lsn;
        ap = ap->next;
      }
    }
    DBGOUT_ASSERT(check_lsn(ep->client_msg->p->a), STRLIT("NULL lsn"));
  retry_new:
    /* Find a free slot */
    TASK_CALL(reserve_synode_number(&ep->synode_allocation, &ep->site,
                                    &ep->msgno, &ep->remote_retry,
                                    ep->client_msg->p->a, &reservation_status));

    // Check result of reservation
    if (reservation_status == synode_reservation_status::no_nodes) {
      GOTO(retry_new);
    } else if (reservation_status ==
               synode_reservation_status::delivery_timeout) {
      GOTO(next);
    }
    // If we get here, we have a valid synode number
    assert(!synode_eq(ep->msgno, null_synode));

    /* See if we can do anything with this message */
    if (!ep->site || get_nodeno(ep->site) == VOID_NODE_NO) {
      /* Give up */
      DBGOUT_ASSERT(check_lsn(ep->client_msg->p->a), STRLIT("NULL lsn"));
      IFDBG(D_NONE, FN; STRLIT("delivery_failure "); SYCEXP(ep->msgno);
            PTREXP(ep->site); NDBG(get_nodeno(ep->site), u));
      deliver_to_app(nullptr, ep->client_msg->p->a, delivery_failure);
      GOTO(next);
    }

    brand_client_msg(ep->client_msg->p, ep->msgno);

    for (;;) { /* Loop until the client message has been learned */
      /* Get a Paxos instance to send the client message */

      TASK_CALL(wait_for_cache(&ep->p, ep->msgno, 60));
      if (!ep->p) {
        G_MESSAGE("Could not get a pax_machine for msgno %lu. Retrying",
                  (unsigned long)ep->msgno.msgno);
        goto retry_new;
      }

      assert(ep->p);
      if (ep->client_msg->p->force_delivery)
        ep->p->force_delivery = ep->client_msg->p->force_delivery;
      {
        [[maybe_unused]] int lock = lock_pax_machine(ep->p);
        assert(!lock);
      }

      /* Set the client message as current proposal */
      assert(ep->client_msg->p);
      replace_pax_msg(&ep->p->proposer.msg, clone_pax_msg(ep->client_msg->p));
      if (ep->p->proposer.msg == nullptr) {
        g_critical(
            "Node %u has run out of memory while sending a message and "
            "will now exit.",
            get_nodeno(proposer_site));
        terminate_and_exit(); /* Tell xcom to stop */
        TERMINATE;
      }
      assert(ep->p->proposer.msg);
      PAX_MSG_SANITY_CHECK(ep->p->proposer.msg);

      /* Create the prepare message */
      unchecked_replace_pax_msg(&ep->prepare_msg,
                                pax_msg_new(ep->msgno, ep->site));
      IFDBG(D_NONE, FN; PTREXP(ep->client_msg->p->a); STRLIT("pushing ");
            SYCEXP(ep->msgno));
      IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_app_data(ep->prepare_msg->a)));

      /* Use 3 phase algorithm if threephase is set or we are forcing or we have
         already accepted something, which may happen if another node has timed
         out waiting for this node and proposed a no_op, which we have accepted.

         We *must* use 3 phase algorithm if the synode was allocated by
         ourselves using `global_synode_allocator`. This is last resort synode
         allocation that does not guarantee we will be the only Proposer using
         it. Therefore, for correctness we must use regular 3 phase Paxos,
         because we may have dueling Proposers.
       */
      if (threephase || ep->p->force_delivery || ep->p->acceptor.promise.cnt ||
          ep->synode_allocation == synode_allocation_type::global) {
        push_msg_3p(ep->site, ep->p, ep->prepare_msg, ep->msgno, normal);
      } else {
        push_msg_2p(ep->site, ep->p);
      }

      ep->start_push = task_now();

      while (!finished(ep->p)) { /* Try to get a value accepted */
        /* We will wake up periodically, and whenever a message arrives */
        TIMED_TASK_WAIT(&ep->p->rv, ep->delay = wakeup_delay(ep->delay));
        if (!synode_eq(ep->msgno, ep->p->synode) ||
            ep->p->proposer.msg == nullptr) {
          IFDBG(D_NONE, FN; STRLIT("detected stolen state machine, retry"););
          /* unlock_pax_machine(ep->p); */
          GOTO(retry_new); /* Need to break out of both loops,
                                                  and we have no "exit named
                              loop" construction */
        }
        assert(synode_eq(ep->msgno, ep->p->synode) && ep->p->proposer.msg);
        if (finished(ep->p)) break;
        {
          double now = task_now();
#ifdef DELIVERY_TIMEOUT
          if ((ep->start_propose + ep->client_msg->p->a->expiry_time) < now) {
            IFDBG(D_NONE, FN; STRLIT("timeout when pushing ");
                  SYCEXP(ep->msgno); SYCEXP(executed_msg));
            /* Proposing a no-op here is a last ditch effort to cancel the
            failed message. If any of the currently reachable nodes have
            participated in the failed consensus round, it is equivalent to
            retrying a final time, otherwise we could get a no-op
            accepted. Proposing a no-op is always harmless.
            Having a timeout on delivery and telling the client is really
            contrary to the spirit of
            Paxos, since we cannot guarantee that the message has not been
            delivered, but at the moment, MCM depends on it.
            Proposing a no-op here increases the probability that the outcome
            matches what we tell MCM about the outcome. */
            propose_noop(ep->msgno, ep->p);
            DBGOUT_ASSERT(check_lsn(ep->client_msg->p->a), STRLIT("NULL lsn"));
            IFDBG(D_NONE, FN; STRLIT("timeout -> delivery_failure"));
            deliver_to_app(ep->p, ep->client_msg->p->a, delivery_failure);
            unlock_pax_machine(ep->p);
            GOTO(next);
          }
#endif
          if ((ep->start_push + ep->delay) <= now) {
            PAX_MSG_SANITY_CHECK(ep->p->proposer.msg);
            IFDBG(D_NONE, FN; STRLIT("retry pushing "); SYCEXP(ep->msgno));
            IFDBG(D_NONE, FN;
                  COPY_AND_FREE_GOUT(dbg_app_data(ep->prepare_msg->a)););
            IFDBG(D_NONE, BALCEXP(ep->p->proposer.bal);
                  BALCEXP(ep->p->acceptor.promise));
            push_msg_3p(ep->site, ep->p, ep->prepare_msg, ep->msgno, normal);
            ep->start_push = now;
          }
        }
      }
      /* When we get here, we know the value for this message number,
         but it may not be the value we tried to push,
         so loop until we have a successful push. */
      unlock_pax_machine(ep->p);
      IFDBG(D_NONE, FN; STRLIT(" found finished message "); SYCEXP(ep->msgno);
            STRLIT("seconds since last push ");
            NPUT(task_now() - ep->start_push, f); STRLIT("ep->client_msg ");
            COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p)););
      IFDBG(D_NONE, FN; STRLIT("ep->p->learner.msg ");
            COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p->learner.msg)););
      if (match_my_msg(ep->p->learner.msg, ep->client_msg->p)) {
        break;
      } else
        GOTO(retry_new);
    }
  next : {
    double now = task_now();
    double used = now - ep->start_propose;
    add_to_filter(used);
    prop_finished++;
    IFDBG(D_NONE, FN; STRLIT("completed ep->msgno "); SYCEXP(ep->msgno);
          NDBG(used, f); NDBG(median_time(), f);
          STRLIT("seconds since last push "); NDBG(now - ep->start_push, f););
    IFDBG(D_NONE, FN; STRLIT("ep->client_msg ");
          COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p)););
    if (ep->p) {
      IFDBG(D_NONE, FN; STRLIT("ep->p->learner.msg ");
            COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p->learner.msg)););
    }
    msg_link_delete(&ep->client_msg);
  }
  }
  FINALLY
  IFDBG(D_BUG, FN; STRLIT("exit "); NDBG(ep->self, d); NDBG(task_now(), f));
  if (ep->p) {
    unlock_pax_machine(ep->p);
  }
  replace_pax_msg(&ep->prepare_msg, nullptr);
  if (ep->client_msg) { /* If we get here with a client message, we have
                                             failed to deliver */
    DBGOUT_ASSERT(check_lsn(ep->client_msg->p->a), STRLIT("NULL lsn"));
    IFDBG(D_NONE, FN;
          STRLIT("undelivered message at task end -> delivery_failure"));
    deliver_to_app(ep->p, ep->client_msg->p->a, delivery_failure);
    msg_link_delete(&ep->client_msg);
  }
  remove_proposer_synode(ep->self);
  TASK_END;
}

static xcom_proto constexpr first_protocol_that_ignores_intermediate_forced_configs_or_views =
    x_1_8;

static bool constexpr should_ignore_forced_config_or_view(
    xcom_proto protocol_version) {
  return protocol_version >=
         first_protocol_that_ignores_intermediate_forced_configs_or_views;
}

static node_no get_leader(site_def const *s) {
  if (s) {
    node_no leader = 0;
    for (leader = 0; leader < get_maxnodes(s); leader++) {
      if (!may_be_dead(s->detected, leader, task_now())) return leader;
    }
  }
  return 0;
}

int iamthegreatest(site_def const *s) {
  if (!s)
    return 0;
  else
    return get_leader(s) == s->nodeno;
}

// Update site based on incoming global node set
static site_def *update_site(site_def *site, node_set const *ns,
                             synode_no boot_key, synode_no start) {
  // If it has not changed, no action is necessary.
  // If it has changed, we need to create and install
  // a new site def, since the changed node set will influence which
  // messages will be ignored.
  // This change needs to be effective after the current pipeline
  // of messages has been emptied, just as if we had
  // changed the config (site_def) itself.

  if (!equal_node_set(ns, &site->global_node_set)) {
    site_def *new_config = clone_site_def(get_site_def());
    assert(new_config);
    new_config->start = start;
    new_config->boot_key = boot_key;
    // Update node set of site
    copy_node_set(ns, &new_config->global_node_set);
    return new_config;
  }
  return nullptr;
}

void execute_msg(site_def *site, pax_machine *pma, pax_msg *p) {
  app_data_ptr a = p->a;
  IFDBG(D_EXEC, FN; COPY_AND_FREE_GOUT(dbg_pax_msg(p)););
  if (a) {
    switch (a->body.c_t) {
      case unified_boot_type:
      case force_config_type:
        deliver_config(a);
      case add_node_type:
      case remove_node_type:
        break;
      case app_type:
        IFDBG(D_NONE, FN; STRLIT(" learner.msg ");
              COPY_AND_FREE_GOUT(dbg_pax_msg(pma->learner.msg)););
        /* DBGOUT_ASSERT(check_lsn(a), STRLIT("NULL lsn")); */
        deliver_to_app(pma, a, delivery_ok);
        break;
      case view_msg: {
        /* Deliver view like we used to when every member was always a leader.
         * This ensures deterministic behaviour in groups with some members
         * running previous XCom instances.
         */
        IFDBG(D_EXEC, FN; STRLIT(" global view ");
              COPY_AND_FREE_GOUT(dbg_pax_msg(pma->learner.msg)););
        if (site && site->global_node_set.node_set_len ==
                        a->body.app_u_u.present.node_set_len) {
          if ((p->force_delivery != 0) &&
              should_ignore_forced_config_or_view(site->x_proto)) {
            G_DEBUG(
                "execute_msg: Ignoring a forced intermediate, pending "
                "view_msg");
          } else {
            assert(site->global_node_set.node_set_len ==
                   a->body.app_u_u.present.node_set_len);
            // Can only mutate site->global_node_set if everyone is a leader and
            // has its own channel.
            if (site->max_active_leaders == active_leaders_all) {
              copy_node_set(&a->body.app_u_u.present, &site->global_node_set);
            }
            deliver_global_view_msg(site, a->body.app_u_u.present, p->synode);
            ADD_DBG(D_BASE,
                    add_event(EVENT_DUMP_PAD,
                              string_arg("deliver_global_view_msg p->synode"));
                    add_synode_event(p->synode););
          }
        }

        /* If this view_msg is:
         *
         * (1) about the latest site, and
         * (2) only some member(s) is (are) leader(s) in the latest site,
         *
         * create a new site to deterministically ignore the channel of leaders
         * that may be dead. */
        site_def *latest_site = get_site_def_rw();
        IFDBG(
            D_EXEC, FN; PTREXP(latest_site); if (latest_site) {
              NUMEXP(latest_site->nodes.node_list_len);
              NUMEXP(latest_site->global_node_set.node_set_len);
              NUMEXP(a->body.app_u_u.present.node_set_len);
              SYCEXP(a->app_key);
              SYCEXP(latest_site->start);
            });
        /*
         * You'll want to install the new site if xcom is operating as
         * single-leader and there were no changes in the configuration. The
         * reason for this is so that you have the latest information about
         * who is the preferred and actual leader.
         */
        bool const is_latest_view = synode_gt(a->app_key, latest_site->start);
        bool const everyone_leader_in_latest_site =
            (latest_site->max_active_leaders == active_leaders_all);
        bool const view_node_set_matches_latest_site =
            (latest_site->global_node_set.node_set_len ==
             a->body.app_u_u.present.node_set_len);
        const bool can_install_site = is_latest_view &&
                                      !everyone_leader_in_latest_site &&
                                      view_node_set_matches_latest_site;

        if (can_install_site) {
          a->app_key = p->synode;  // Patch app_key to avoid fixing getstart()
          site_def *new_config = update_site(
              latest_site, &a->body.app_u_u.present, a->app_key, getstart(a));
          if (new_config) {
            IFDBG(D_EXEC, FN; PTREXP(new_config);
                  NUMEXP(new_config->nodes.node_list_len);
                  NUMEXP(new_config->global_node_set.node_set_len);
                  SYCEXP(a->app_key); SYCEXP(new_config->start););
            site_install_action(new_config, a->body.c_t);
            analyze_leaders(new_config);
          }
        }
      } break;
      default:
        break;
    }
  }
  IFDBG(D_NONE, FN; SYCEXP(p->synode));
}

static void read_missing_values(int n);
static void propose_missing_values(int n);

#ifdef EXECUTOR_TASK_AGGRESSIVE_NO_OP
/* With many nodes sending read_ops on instances that are not decided yet, it
 * may take a very long time until someone finally decides to start a new
 * consensus round. As the cost of a new proposal is not that great, it's
 * acceptable to go directly to proposing a no-op instead of first trying to
 * get the value with a read_op. An added benefit of this is that if more than
 * one node needs the result, they will get it all when the consensus round
 * finishes. */
static void find_value(site_def const *site, unsigned int *wait, int n) {
  IFDBG(D_NONE, FN; NDBG(*wait, d));

  if (get_nodeno(site) == VOID_NODE_NO) {
    read_missing_values(n);
    return;
  }

  if ((*wait) > 1 || /* Only leader will propose initially */
      ((*wait) > 0 && iamthegreatest(site)))
    propose_missing_values(n);

#ifdef TASK_EVENT_TRACE
  if ((*wait) > 1) dump_task_events();
#endif
  (*wait)++;
}
#else
static void find_value(site_def const *site, unsigned int *wait, int n) {
  IFDBG(D_NONE, FN; NDBG(*wait, d));

  if (get_nodeno(site) == VOID_NODE_NO) {
    read_missing_values(n);
    return;
  }

  switch (*wait) {
    case 0:
    case 1:
      read_missing_values(n);
      (*wait)++;
      break;
    case 2:
      if (iamthegreatest(site))
        propose_missing_values(n);
      else
        read_missing_values(n);
      (*wait)++;
      break;
    case 3:
      propose_missing_values(n);
      break;
    default:
      break;
  }
}
#endif /* EXECUTOR_TASK_AGGRESSIVE_NO_OP */

static void dump_debug_exec_state();

#ifdef PROPOSE_IF_LEADER
int get_xcom_message(pax_machine **p, synode_no msgno, int n) {
  DECL_ENV
  unsigned int wait;
  double delay;
  site_def const *site;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN

  ep->wait = 0;
  ep->delay = 0.0;
  *p = force_get_cache(msgno);
  ep->site = NULL;

  dump_debug_exec_state();
  while (!finished(*p)) {
    ep->site = find_site_def(msgno);
    /* The end of the world ?, fake message by skipping */
    if (get_maxnodes(ep->site) == 0) {
      pax_msg *msg = pax_msg_new(msgno, ep->site);
      handle_skip(ep->site, *p, msg);
      break;
    }
    IFDBG(D_NONE, FN; STRLIT(" not finished "); SYCEXP(msgno); PTREXP(*p);
          NDBG(ep->wait, u); SYCEXP(msgno));
    if (get_maxnodes(ep->site) > 1 && iamthegreatest(ep->site) &&
        ep->site->global_node_set.node_set_val &&
        !ep->site->global_node_set.node_set_val[msgno.node] &&
        may_be_dead(ep->site->detected, msgno.node, task_now())) {
      propose_missing_values(n);
    } else {
      find_value(ep->site, &ep->wait, n);
    }
    TIMED_TASK_WAIT(&(*p)->rv, ep->delay = wakeup_delay(ep->delay));
    *p = get_cache(msgno);
    dump_debug_exec_state();
  }

  FINALLY
  IFDBG(D_NONE, FN; SYCEXP(msgno); PTREXP(*p); NDBG(ep->wait, u);
        SYCEXP(msgno));
  TASK_END;
}
#else
int get_xcom_message(pax_machine **p, synode_no msgno, int n) {
  DECL_ENV
  unsigned int wait;
  double delay;
  site_def const *site;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN

  ep->wait = 0;
  ep->delay = 0.0;
  *p = force_get_cache(msgno);
  ep->site = nullptr;

  dump_debug_exec_state();
  while (!finished(*p)) {
    ep->site = find_site_def(msgno);
    /* The end of the world ?, fake message by skipping */
    if (get_maxnodes(ep->site) == 0) {
      pax_msg *msg = pax_msg_new(msgno, ep->site);
      handle_skip(ep->site, *p, msg);
      break;
    }
    IFDBG(D_NONE, FN; STRLIT("before find_value"); SYCEXP(msgno); PTREXP(*p);
          NDBG(ep->wait, u); SYCEXP(msgno));
    find_value(ep->site, &ep->wait, n);
    IFDBG(D_NONE, FN; STRLIT("after find_value"); SYCEXP(msgno); PTREXP(*p);
          NDBG(ep->wait, u); SYCEXP(msgno));
    ep->delay = wakeup_delay(ep->delay);
    IFDBG(D_NONE, FN; NDBG(ep->delay, f));
    TIMED_TASK_WAIT(&(*p)->rv, ep->delay);
    *p = get_cache(msgno);
    dump_debug_exec_state();
  }

  FINALLY
  TASK_END;
}
#endif

synode_no set_executed_msg(synode_no msgno) {
  IFDBG(D_EXEC, FN; STRLIT("changing executed_msg from "); SYCEXP(executed_msg);
        STRLIT(" to "); SYCEXP(msgno));
  if (group_mismatch(msgno, current_message) ||
      synode_gt(msgno, current_message)) {
    IFDBG(D_EXEC, FN; STRLIT("changing current message"));
    set_current_message(first_free_synode_local(msgno));
  }

  if (msgno.msgno > executed_msg.msgno) task_wakeup(&exec_wait);

  executed_msg = msgno;
  executor_site = find_site_def_rw(executed_msg);
  return executed_msg;
}

static synode_no first_free_synode_local(synode_no msgno) {
  site_def const *site = find_site_def(msgno);
  synode_no retval = msgno;
  if (!site) {
    /* purecov: begin deadcode */
    site = get_site_def();
    IFDBG(D_NONE, FN; PTREXP(site); SYCEXP(msgno));
    assert(get_group_id(site) != 0);
    /* purecov: end */
  }
  if (get_group_id(site) == 0) {
    IFDBG(D_NONE, FN; PTREXP(site); SYCEXP(msgno));
    if (site) {
      IFDBG(D_NONE, FN; SYCEXP(site->boot_key); SYCEXP(site->start);
            COPY_AND_FREE_GOUT(dbg_site_def(site)));
    }
  }
  assert(get_group_id(site) != 0);
  assert(!synode_eq(msgno, null_synode));
  if (retval.msgno == 0) retval.msgno = 1;
  retval.node = get_nodeno(site);
  if (synode_lt(retval, msgno))
    return incr_msgno(retval);
  else
    return retval;
}

synode_no set_current_message(synode_no msgno) {
  IFDBG(D_PROPOSE, FN; STRLIT("changing current_message from ");
        SYCEXP(current_message); STRLIT(" to "); SYCEXP(msgno));
  return current_message = msgno;
}

static void update_max_synode(pax_msg *p);

#if TASK_DBUG_ON
static void perf_dbg(int *_n, int *_old_n, double *_old_t) [[maybe_unused]];
static void perf_dbg(int *_n, int *_old_n, double *_old_t) {
  int n = *_n;
  int old_n = *_old_n;
  double old_t = *_old_t;

  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;

  IFDBG(D_NONE, FN; SYCEXP(executed_msg));
  if (!(n % 5000)) {
    GET_GOUT;
    NDBG(get_nodeno(get_site_def()), u);
    NDBG(task_now(), f);
    NDBG(n, d);
    NDBG(median_time(), f);
    SYCEXP(executed_msg);
    PRINT_GOUT;
    FREE_GOUT;
  }
  (*_n)++;
  if (task_now() - old_t > 1.0) {
    GET_GOUT;
    NDBG(get_nodeno(get_site_def()), u);
    NDBG(task_now(), f);
    NDBG(n, d);
    NDBG((n - old_n) / (task_now() - old_t), f);
    PRINT_GOUT;
    FREE_GOUT;
    *_old_t = task_now();
    *_old_n = n;
  }
}
#endif

/* Does address match any current leader? */
static inline int match_leader(char const *addr, leader_array const leaders) {
  u_int i;
  for (i = 0; i < leaders.leader_array_len; i++) {
    IFDBG(D_BASE, FN; NUMEXP(i); NUMEXP(leaders.leader_array_len); STREXP(addr);
          STREXP(leaders.leader_array_val[i].address));
    if (strcmp(addr, leaders.leader_array_val[i].address) == 0) return 1;
  }
  return 0;
}

static inline bool alive_node(site_def const *site, u_int i) {
  return is_set(site->global_node_set, i);
}

// Find up to site->max_active_leaders leaders.
// If leaders are set by the client, and none of those are alive, revert
// to using the set of addresses in the config.

void analyze_leaders(site_def *site) {
  assert(site);
  // No analysis if all nodes are leaders
  if (active_leaders_all == site->max_active_leaders) return;

  // Use leaders from config if forced or not set by client
  bool use_client_leaders = leaders_set_by_client(site);
  site->cached_leaders = true;
  site->found_leaders = 0;  // Number of active leaders found
  // Reset everything
  for (u_int i = 0; i < get_maxnodes(site); i++) {
    site->active_leader[i] = 0;
  }
  // If candidate leaders set by client, check those first
  for (u_int i = 0; use_client_leaders && i < get_maxnodes(site); i++) {
    if (site->found_leaders <
            site->max_active_leaders &&  // Found enough?
                                         // Must be alive according to global
                                         // node set of site
        alive_node(site, i) &&
        // Must match a node in the list of leaders
        match_leader(site->nodes.node_list_val[i].address, site->leaders)) {
      site->active_leader[i] = 1;
      site->found_leaders++;
    }
  }
  // Check rest of nodes
  for (u_int i = 0; i < get_maxnodes(site); i++) {
    if (!site->active_leader[i] &&                         // Avoid duplicates
        site->found_leaders < site->max_active_leaders &&  // Found enough?
        // Must be alive according to global
        // node set of site
        alive_node(site, i)) {
      site->active_leader[i] = 1;
      site->found_leaders++;
    }
  }
  // We need at least one channel otherwise the group grinds to a halt.
  if (site->found_leaders == 0) {
    site->active_leader[0] = 1;
    site->found_leaders = 1;
  }
  free(site->dispatch_table);

  IFDBG(D_BUG, FN; STRLIT("free "); PTREXP(site); PTREXP(site->dispatch_table));
  // Do not work as synode allocator if not active leader. ???
  if (get_nodeno(site) != VOID_NODE_NO &&
      site->active_leader[get_nodeno(site)]) {
    site->dispatch_table = primary_dispatch_table();
  } else {
    site->dispatch_table = secondary_dispatch_table();
  }
  IFDBG(D_BUG, FN; STRLIT("allocate "); PTREXP(site);
        PTREXP(site->dispatch_table));

  for (u_int i = 0; i < get_maxnodes(site); i++) {
    IFDBG(D_BUG, FN; NUMEXP(i); PTREXP(site); NUMEXP(site->found_leaders);
          NUMEXP(site->max_active_leaders); NUMEXP(alive_node(site, i));
          SYCEXP(site->start); STREXP(site->nodes.node_list_val[i].address);
          if (site->active_leader[i]) STRLIT(" says YES");
          else STRLIT(" says NO"));
  }
}

/* Is node number an active leader? */
int is_active_leader(node_no x, site_def *site) {
  /* No site, no active leaders */
  if (!site) return 0;

  /* Node number out of bound, not an active leader */
  if (x >= get_maxnodes(site)) return 0;

  /* All are leaders, no need for further tests */
  if (active_leaders_all == site->max_active_leaders) return 1;
  /* See if cached values are valid */
  if (!site->cached_leaders) {
    analyze_leaders(site);
  }
#if 0
  if (site->active_leader == NULL || x > site->nodes.node_list_len - 1)
    IFDBG(D_BUG, FN; PTREXP(site->active_leader); NUMEXP(x);
          NUMEXP(site->nodeno); NUMEXP(site->nodes.node_list_len););
#endif
  return site->active_leader[x];
}

node_no found_active_leaders(site_def *site) {
  /* No site, no active leaders */
  if (!site) return 0;

  /* All are leaders, no need for further tests */
  if (active_leaders_all == site->max_active_leaders)
    return site->nodes.node_list_len;

  /* See if cached values are valid */
  if (!site->cached_leaders) {
    analyze_leaders(site);
  }
  return site->found_leaders;
}

/* Check if this message belongs to a channel that should be ignored */
static inline int ignore_message(synode_no x, site_def *site,
                                 char const *dbg [[maybe_unused]]) {
  int retval = !is_active_leader(x.node, site);
  IFDBG(D_BASE, STRLIT(dbg); STRLIT(" "); FN; SYCEXP(x); NUMEXP(retval));
  return retval;
}

/* Check if this node is a leader */
static inline bool is_leader(site_def *site) {
  bool retval = site && is_active_leader(site->nodeno, site);
  IFDBG(D_BASE, FN; PTREXP(site); if (site) NUMEXP(site->nodeno);
        NUMEXP(retval));
  return retval;
}

[[maybe_unused]] static void debug_loser(synode_no x);
#if defined(TASK_DBUG_ON) && TASK_DBUG_ON
static void debug_loser(synode_no x) {
  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;
  if (1 || x.msgno < 10) {
    GET_GOUT;
    NDBG(get_nodeno(find_site_def(x)), u);
    STRLIT(" ignoring loser ");
    SYCEXP(x);
    SYCEXP(max_synode);
    PRINT_GOUT;
    FREE_GOUT;
  }
}
#else
/* purecov: begin deadcode */
static void debug_loser(synode_no x [[maybe_unused]]) {}
/* purecov: end */
#endif

static void send_value(site_def const *site, node_no to, synode_no synode) {
  pax_machine *pm = get_cache(synode);
  if (pm && pm->learner.msg) {
    pax_msg *msg = clone_pax_msg(pm->learner.msg);
    if (msg == nullptr) return;
    ref_msg(msg);
    send_server_msg(site, to, msg);
    unref_msg(&msg);
  }
}

/**
 * Returns the message number where it is safe for nodes in previous
 * configuration to exit.
 *
 * @param start start synod of the next configuration
 * @param event_horizon event horizon of the next configuration
 */
static synode_no compute_delay(synode_no start,
                               xcom_event_horizon event_horizon) {
  start.msgno += event_horizon;
  return start;
}

/* Push messages to all nodes which were in the previous site, but not in this
 */
static void inform_removed(int index, int all) {
  site_def **sites = nullptr;
  uint32_t site_count = 0;
  IFDBG(D_NONE, FN; NEXP(index, d));
  get_all_site_defs(&sites, &site_count);
  while (site_count > 1 && index >= 0 && (uint32_t)(index + 1) < site_count) {
    site_def *s = sites[index];
    site_def *ps = sites[index + 1];

    /* Compute diff and push messages */
    IFDBG(D_NONE, FN; NDBG(index, d); PTREXP(s); if (s) SYCEXP(s->boot_key);
          PTREXP(ps); if (ps) SYCEXP(ps->boot_key));

    if (s && ps) {
      node_no i = 0;
      IFDBG(D_NONE, FN; SYCEXP(s->boot_key); SYCEXP(s->start);
            SYCEXP(ps->boot_key); SYCEXP(ps->start));
      for (i = 0; i < ps->nodes.node_list_len; i++) { /* Loop over prev site */
        if (ps->nodeno != i &&
            !node_exists(&ps->nodes.node_list_val[i], &s->nodes)) {
          synode_no synode = s->start;
          synode_no end = max_synode;
          while (!synode_gt(synode, end)) { /* Loop over relevant messages */
            send_value(ps, i, synode);
            synode = incr_synode(synode);
          }
        }
      }
    }
    if (!all) /* Early exit if not all configs should be examined */
      break;
    index--;
  }
}

static bool_t backwards_compatible(xcom_event_horizon event_horizon) {
  return event_horizon == EVENT_HORIZON_MIN;
}

static xcom_proto const first_event_horizon_aware_protocol = x_1_4;

static bool_t reconfigurable_event_horizon(xcom_proto protocol_version) {
  return protocol_version >= first_event_horizon_aware_protocol;
}

static bool_t add_node_unsafe_against_ipv4_old_nodes(app_data_ptr a) {
  assert(a->body.c_t == add_node_type);

  {
    site_def const *latest_config = get_site_def();
    if (latest_config && latest_config->x_proto >= minimum_ipv6_version())
      return FALSE;

    {
      u_int const nr_nodes_to_add = a->body.app_u_u.nodes.node_list_len;
      node_address *nodes_to_add = a->body.app_u_u.nodes.node_list_val;

      u_int i;
      xcom_port node_port = 0;
      char node_addr[IP_MAX_SIZE];

      for (i = 0; i < nr_nodes_to_add; i++) {
        if (get_ip_and_port(nodes_to_add[i].address, node_addr, &node_port)) {
          G_ERROR(
              "Error parsing address from a joining node. Join operation "
              "will be "
              "rejected");
          return TRUE;
        }

        if (!is_node_v4_reachable(node_addr)) return TRUE;
      }
    }

    return FALSE;
  }
}

/**
 * @brief This will test if we are receiving a boot request that contains
 *        ourselves. This could happed in case of a misconfiguration of a
 *        local_address, that causes an add_node request to be erroneous
 *        delivered.
 *
 * @param a app_data with an add node request
 * @return bool_t TRUE in case of error, meaning that my address is in the
 *                     add_node list
 */
static bool_t add_node_adding_own_address(app_data_ptr a) {
  assert(a->body.c_t == add_node_type);

  return node_exists(cfg_app_xcom_get_identity(), &a->body.app_u_u.nodes);
}

/**
 * Check if a node is compatible with the group's event horizon.
 *
 * A node is compatible with the group's configuration if:
 *
 *    a) The node supports event horizon reconfigurations, or
 *    b) The group's event horizon is, or is scheduled to be, the default
 * event horizon.
 */
static bool unsafe_against_event_horizon(node_address const *node) {
  site_def const *latest_config = get_site_def();
  xcom_proto node_max_protocol_version = node->proto.max_proto;
  bool const compatible =
      reconfigurable_event_horizon(node_max_protocol_version) ||
      backwards_compatible(latest_config->event_horizon);

  if (!compatible) {
    /*
     * The node that wants to join does not support event horizon
     * reconfigurations and the group's event horizon is, or is scheduled to
     * be, different from the default.
     * The node can not safely join the group so we deny its attempt to join.
     */
    G_INFO(
        "%s's request to join the group was rejected because the group's "
        "event "
        "horizon is, or will be %" PRIu32 " and %s only supports %" PRIu32,
        node->address, latest_config->event_horizon, node->address,
        EVENT_HORIZON_MIN);
    return true;
  }
  return false;
}

typedef bool (*unsafe_node_check)(node_address const *node);

static bool check_if_add_node_is_unsafe(app_data_ptr a,
                                        unsafe_node_check unsafe) {
  assert(a->body.c_t == add_node_type);
  {
    u_int nodes_len = a->body.app_u_u.nodes.node_list_len;
    node_address *nodes_to_add = a->body.app_u_u.nodes.node_list_val;
    u_int i;
    for (i = 0; i < nodes_len; i++) {
      if (unsafe(&nodes_to_add[i])) return true;
    }
  }
  return false;
}

static bool check_if_add_node_is_unsafe_against_event_horizon(app_data_ptr a) {
  return check_if_add_node_is_unsafe(a, unsafe_against_event_horizon);
}

// Map values of old node set to new node set by matching on
// node addresses
void recompute_node_set(node_set const *old_set, node_list const *old_nodes,
                        node_set *new_set, node_list const *new_nodes) {
  // Return value of node set matching node_address na
  auto value{[&](node_address const *na) {
    assert(old_set->node_set_len == old_nodes->node_list_len);
    for (u_int i = 0; i < old_nodes->node_list_len; i++) {
      if (match_node(&old_nodes->node_list_val[i], na, true)) {
        return old_set->node_set_val[i];
      }
    }
    return 0;
  }};

  for (u_int i = 0; i < new_nodes->node_list_len; i++) {
    new_set->node_set_val[i] = value(&new_nodes->node_list_val[i]);
  }
}

// Remap old global and local node set of site to new
static void recompute_node_sets(site_def const *old_site, site_def *new_site) {
  recompute_node_set(&old_site->global_node_set, &old_site->nodes,
                     &new_site->global_node_set, &new_site->nodes);
  recompute_node_set(&old_site->local_node_set, &old_site->nodes,
                     &new_site->local_node_set, &new_site->nodes);
}

static bool incompatible_proto_and_max_leaders(xcom_proto x_proto,
                                               node_no max_leaders) {
  return x_proto < single_writer_support && max_leaders != active_leaders_all;
}

static bool incompatible_proto_and_leaders(xcom_proto x_proto) {
  return x_proto < single_writer_support;
}

static bool incompatible_proto_and_max_leaders(node_address const *node) {
  site_def const *latest_config = get_site_def();

  if (incompatible_proto_and_max_leaders(node->proto.max_proto,
                                         latest_config->max_active_leaders)) {
    /*
     * The node that wants to join does not allow setting of max number of
     * leaders
     * and the max number of leaders in the group is not all.
     * The node can not safely join the group so we deny its attempt to join.
     */
    G_INFO(
        "%s's request to join the group was rejected because the group's max "
        "number of active leaders is, or will be %" PRIu32
        " and %s only supports "
        "all nodes as leaders",
        node->address, latest_config->max_active_leaders, node->address);
    return true;
  }
  return false;
}

static bool incompatible_proto_and_leaders(node_address const *node) {
  site_def const *latest_config = get_site_def();

  if (incompatible_proto_and_leaders(node->proto.max_proto) &&
      leaders_set_by_client(latest_config)) {
    /*
     * The node that wants to join does not allow changing the set of
     * leaders
     * and the set of leaders in the group is not empty.
     * The node can not safely join the group so we deny its attempt to join.
     */
    G_INFO(
        "%s's request to join the group was rejected because the group "
        "has a non-empty set of leaders specified by the client, "
        "and %s does not support changing the set of leaders",
        node->address, node->address);
    return true;
  }
  return false;
}

bool unsafe_leaders(app_data *a) {
  return check_if_add_node_is_unsafe(a, incompatible_proto_and_max_leaders) ||
         check_if_add_node_is_unsafe(a, incompatible_proto_and_leaders);
}

static void set_start_and_boot(site_def *new_config, app_data_ptr a) {
  new_config->start = getstart(a);
  new_config->boot_key = a->app_key;
}

// Map values of old timestamps to new timestamps by matching on
// node addresses
void recompute_timestamps(detector_state const old_timestamp,
                          node_list const *old_nodes,
                          detector_state new_timestamp,
                          node_list const *new_nodes) {
  // Return value of timestamp matching node_address na
  auto value{[&](node_address const *na) {
    for (u_int i = 0; i < old_nodes->node_list_len; i++) {
      if (match_node(&old_nodes->node_list_val[i], na, true)) {
        return old_timestamp[i];
      }
    }
    return 0.0;
  }};

  for (u_int i = 0; i < new_nodes->node_list_len; i++) {
    new_timestamp[i] = value(&new_nodes->node_list_val[i]);
  }
}

/**
 * Reconfigure the group membership: add new member(s).
 *
 * It is possible that concurrent reconfigurations take effect between the
 * time this reconfiguration was proposed and now.
 *
 * Particularly, it is possible that any of the concurrent reconfigurations
 * modified the event horizon and that the new member(s) do not support event
 * horizon reconfigurations.
 *
 * We account for these situations by validating if adding the new members is
 * still possible under the current state.
 *
 * If it is not, this reconfiguration does not produce any effect, i.e. no new
 * configuration is installed.
 */
site_def *handle_add_node(app_data_ptr a) {
  if (check_if_add_node_is_unsafe_against_event_horizon(a)) {
    /*
     * Note that the result of this function is only applicable to
     * unused and not-fully-implemented code paths where add_node_type is used
     * forcibly.
     * Should this fact change, this obviously does not work.
     */
    return nullptr;
  }

  if (unsafe_leaders(a)) {
    return nullptr;
  }
  {
    for (u_int node = 0; node < a->body.app_u_u.nodes.node_list_len; node++) {
      G_INFO("Adding new node to the configuration: %s",
             a->body.app_u_u.nodes.node_list_val[node].address)
    }

    site_def const *old_site = get_site_def();
    site_def *site = clone_site_def(old_site);
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)););
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)););
    ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("a->app_key"));
            add_synode_event(a->app_key););
    assert(old_site);
    assert(site);
    add_site_def(a->body.app_u_u.nodes.node_list_len,
                 a->body.app_u_u.nodes.node_list_val, site);
    set_start_and_boot(site, a);
    if (site->x_proto >= single_writer_support) {
      recompute_node_sets(old_site, site);
      recompute_timestamps(old_site->detected, &old_site->nodes, site->detected,
                           &site->nodes);
    }
    site_install_action(site, a->body.c_t);
    return site;
  }
}

/**
 * Check if we can reconfigure the event horizon.
 *
 * We can reconfigure the event horizon if all group members support
 * reconfiguring the event horizon, and the new event horizon in the domain
 * [EVENT_HORIZON_MIN, EVENT_HORIZON_MAX].
 *
 * We use the group's latest common XCom protocol as a proxy to decide if all
 * members support reconfiguring the event horizon.
 *
 * If the common protocol is at least version 5 (x_1_4) then all members run
 * compatible server instances.
 *
 * Otherwise there are older instances, and it follows that the event horizon
 * must be the default and cannot be reconfigured.
 */
enum allow_event_horizon_result {
  EVENT_HORIZON_ALLOWED,
  EVENT_HORIZON_INVALID,
  EVENT_HORIZON_UNCHANGEABLE
};
typedef enum allow_event_horizon_result allow_event_horizon_result;

static void log_event_horizon_reconfiguration_failure(
    allow_event_horizon_result error_code,
    xcom_event_horizon attempted_event_horizon) {
  switch (error_code) {
    case EVENT_HORIZON_INVALID:
      G_WARNING("The event horizon was not reconfigured to %" PRIu32
                "because its domain is [%" PRIu32 ", %" PRIu32 "]",
                attempted_event_horizon, xcom_get_minimum_event_horizon(),
                xcom_get_maximum_event_horizon());
      break;
    case EVENT_HORIZON_UNCHANGEABLE:
      G_WARNING("The event horizon was not reconfigured to %" PRIu32
                " because some of the group's members do not support "
                "reconfiguring the event horizon",
                attempted_event_horizon);
      break;
    case EVENT_HORIZON_ALLOWED:
      break;
  }
}

static allow_event_horizon_result allow_event_horizon(
    xcom_event_horizon event_horizon) {
  if (event_horizon < EVENT_HORIZON_MIN || event_horizon > EVENT_HORIZON_MAX)
    return EVENT_HORIZON_INVALID;

  {
    const site_def *latest_config = get_site_def();
    if (!reconfigurable_event_horizon(latest_config->x_proto)) {
      assert(backwards_compatible(latest_config->event_horizon));
      return EVENT_HORIZON_UNCHANGEABLE;
    }
  }
  return EVENT_HORIZON_ALLOWED;
}

static bool_t is_unsafe_event_horizon_reconfiguration(app_data_ptr a) {
  assert(a->body.c_t == set_event_horizon_type);
  {
    xcom_event_horizon new_event_horizon = a->body.app_u_u.event_horizon;
    bool_t result = FALSE;
    allow_event_horizon_result error_code;
    error_code = allow_event_horizon(new_event_horizon);
    switch (error_code) {
      case EVENT_HORIZON_INVALID:
      case EVENT_HORIZON_UNCHANGEABLE:
        log_event_horizon_reconfiguration_failure(error_code,
                                                  new_event_horizon);
        result = TRUE;
        break;
      case EVENT_HORIZON_ALLOWED:
        break;
    }
    return result;
  }
}

// Predicate that checks IF the reconfiguration will be unsafe
static bool_t is_unsafe_max_leaders_reconfiguration(app_data_ptr a) {
  assert(a->body.c_t == set_max_leaders);
  const site_def *latest_config = get_site_def();
  node_no new_max_leaders = a->body.app_u_u.max_leaders;
  if (new_max_leaders > get_maxnodes(latest_config)) {
    G_WARNING("The max number of leaders was not reconfigured to %" PRIu32
              " because its domain is [%" PRIu32 ", %" PRIu32 "]",
              new_max_leaders, 0, get_maxnodes(latest_config));
    return TRUE;
  } else if (incompatible_proto_and_max_leaders(latest_config->x_proto,
                                                new_max_leaders)) {
    G_WARNING(
        "The max number of leaders was not reconfigured "
        " because some of the group's members do not support "
        "reconfiguring the max number of leaders to %" PRIu32,
        new_max_leaders);
    return TRUE;
  } else {
    return FALSE;
  }
}

static bool_t is_unsafe_set_leaders_reconfiguration(app_data_ptr a
                                                    [[maybe_unused]]) {
  assert(a->body.c_t == set_leaders_type);
  const site_def *latest_config = get_site_def();
  if (incompatible_proto_and_leaders(latest_config->x_proto)) {
    G_WARNING(
        "The set of leaders was not reconfigured "
        " because some of the group's members do not support "
        "reconfiguring leaders");
    return TRUE;
  } else {
    return FALSE;
  }
}

static bool_t is_unsafe_leaders_reconfiguration(app_data_ptr a) {
  while (a) {
    switch (a->body.c_t) {
      case set_max_leaders:
        if (is_unsafe_max_leaders_reconfiguration(a)) return TRUE;
        break;
      case set_leaders_type:
        if (is_unsafe_set_leaders_reconfiguration(a)) return TRUE;
        break;
      default:
        break;
    }
    a = a->next;
  }
  return FALSE;
}

static bool_t are_there_dead_nodes_in_new_config(app_data_ptr a) {
  assert(a->body.c_t == force_config_type);

  {
    u_int nr_nodes_to_add = a->body.app_u_u.nodes.node_list_len;
    node_address *nodes_to_change = a->body.app_u_u.nodes.node_list_val;
    uint32_t i;
    G_DEBUG("Checking for dead nodes in Forced Configuration")
    for (i = 0; i < nr_nodes_to_add; i++) {
      node_no node = find_nodeno(get_site_def(), nodes_to_change[i].address);

      if (node == get_nodeno(get_site_def()))
        continue; /* No need to validate myself */

      if (node == VOID_NODE_NO) {
        G_ERROR(
            "%s is not in the current configuration."
            "Only members in the current configuration can be present"
            " in a forced configuration list",
            nodes_to_change[i].address)
        return TRUE;
      }

      if (may_be_dead(get_site_def()->detected, node, task_now())) {
        G_ERROR(
            "%s is suspected to be failed."
            "Only alive members in the current configuration should be "
            "present"
            " in a forced configuration list",
            nodes_to_change[i].address)
        return TRUE;
      }
    }
  }

  return FALSE;
}

/**
 * Reconfigure the event horizon.
 *
 * It is possible that concurrent reconfigurations take effect between the
 * time this reconfiguration was proposed and now.
 *
 * Particularly, it is possible that any of the concurrent reconfigurations
 * added a new member which does not support reconfiguring the event
 * horizon.
 *
 * We account for these situations by validating if the event horizon
 * reconfiguration is still possible under the current state.
 *
 * If it is not, this reconfiguration does not produce any effect, i.e. no
 * new configuration is installed.
 */
bool_t handle_event_horizon(app_data_ptr a) {
  if (is_unsafe_event_horizon_reconfiguration(a)) return FALSE;

  {
    xcom_event_horizon new_event_horizon = a->body.app_u_u.event_horizon;
    const site_def *latest_config = get_site_def();
    site_def *new_config = clone_site_def(latest_config);
    IFDBG(D_NONE, FN; NDBG(new_event_horizon, u));
    IFDBG(D_NONE, FN; NDBG(new_event_horizon, u));
    ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("a->app_key"));
            add_synode_event(a->app_key););
    assert(get_site_def());
    assert(new_config);
    new_config->event_horizon = new_event_horizon;
    set_start_and_boot(new_config, a);
    site_install_action(new_config, a->body.c_t);
    G_INFO("The event horizon was reconfigured to %" PRIu32, new_event_horizon);
  }
  return TRUE;
}

static bool_t handle_max_leaders(site_def *new_config, app_data_ptr a) {
  IFDBG(D_BASE, FN; NUMEXP(a->body.app_u_u.max_leaders));
  assert(new_config);
  new_config->max_active_leaders = a->body.app_u_u.max_leaders;
  set_start_and_boot(new_config, a);
  G_INFO("Maximum number of leaders was reconfigured to %" PRIu32,
         a->body.app_u_u.max_leaders);
  return TRUE;
}

bool_t handle_max_leaders(app_data_ptr a) {
  if (is_unsafe_max_leaders_reconfiguration(a)) return FALSE;

  site_def *new_config = clone_site_def(get_site_def());
  handle_max_leaders(new_config, a);
  site_install_action(new_config, a->body.c_t);
  return TRUE;
}

static void zero_leader_array(leader_array *l) {
  l->leader_array_len = 0;
  l->leader_array_val = nullptr;
}

static void move_leader_array(leader_array *target, leader_array *source) {
  /* Deallocate leader_array from target */
  xdr_free((xdrproc_t)xdr_leader_array, (char *)target);
  *target = *source;
  /* Zero the source */
  zero_leader_array(source);
}

static bool_t handle_set_leaders(site_def *new_config, app_data_ptr a) {
  IFDBG(D_BASE, FN; NUMEXP(a->body.app_u_u.leaders.leader_array_len);
        NUMEXP(new_config->max_active_leaders));
  assert(new_config);
  /* Steal the leaders from a */
  move_leader_array(&new_config->leaders, &a->body.app_u_u.leaders);
  set_start_and_boot(new_config, a);
  return TRUE;
}

bool_t handle_set_leaders(app_data_ptr a) {
  if (is_unsafe_set_leaders_reconfiguration(a)) return FALSE;

  site_def *new_config = clone_site_def(get_site_def());
  handle_set_leaders(new_config, a);
  site_install_action(new_config, a->body.c_t);
  G_INFO("Preferred leaders were reconfigured to leaders[0]=%s",
         new_config->leaders.leader_array_len > 0
             ? new_config->leaders.leader_array_val[0].address
             : "n/a");
  return TRUE;
}

bool_t handle_leaders(app_data_ptr a) {
  if (is_unsafe_leaders_reconfiguration(a)) return FALSE;
  site_def *new_config = clone_site_def(get_site_def());
  cargo_type operation{a->body.c_t};  // Deallocate on scope exit if failure
  bool_t retval = TRUE;
  while (a && retval) {
    switch (a->body.c_t) {
      case set_max_leaders:
        if (!handle_max_leaders(new_config, a)) retval = FALSE;
        break;
      case set_leaders_type:
        if (!handle_set_leaders(new_config, a)) retval = FALSE;
        break;
      default:
        break;
    }
    a = a->next;
  }
  if (retval) {
    site_install_action(new_config, operation);
  } else {
    free_site_def(new_config);
  }
  return retval;
}

void terminate_and_exit() {
  IFDBG(D_NONE, FN;);
  ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
  XCOM_FSM(x_fsm_terminate, int_arg(0)); /* Tell xcom to stop */
  XCOM_FSM(x_fsm_exit, int_arg(0));      /* Tell xcom to exit */
  if (xcom_expel_cb) xcom_expel_cb(0);
}

static inline int is_empty_site(site_def const *s) {
  return s->nodes.node_list_len == 0;
}

site_def *handle_remove_node(app_data_ptr a) {
  site_def const *old_site = get_site_def();
  site_def *site = clone_site_def(old_site);
  IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)));
  ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("a->app_key"));
          add_synode_event(a->app_key);
          add_event(EVENT_DUMP_PAD, string_arg("nodeno"));
          add_event(EVENT_DUMP_PAD, uint_arg(get_nodeno(site))););

  remove_site_def(a->body.app_u_u.nodes.node_list_len,
                  a->body.app_u_u.nodes.node_list_val, site);
  set_start_and_boot(site, a);
  if (site->x_proto >= single_writer_support) {
    recompute_node_sets(old_site, site);
    recompute_timestamps(old_site->detected, &old_site->nodes, site->detected,
                         &site->nodes);
  }
  site_install_action(site, a->body.c_t);
  return site;
}

static void log_ignored_forced_config(app_data_ptr a,
                                      char const *const caller_name) {
  switch (a->body.c_t) {
    case unified_boot_type:
      G_DEBUG("%s: Ignoring a forced intermediate, pending unified_boot",
              caller_name);
      break;
    case add_node_type:
      G_DEBUG("%s: Ignoring a forced intermediate, pending add_node for %s",
              caller_name, a->body.app_u_u.nodes.node_list_val[0].address);
      break;
    case remove_node_type:
      G_DEBUG("%s: Ignoring a forced intermediate, pending remove_node for %s",
              caller_name, a->body.app_u_u.nodes.node_list_val[0].address);
      break;
    case set_event_horizon_type:
      G_DEBUG(
          "%s: Ignoring a forced intermediate, pending set_event_horizon for "
          "%" PRIu32,
          caller_name, a->body.app_u_u.event_horizon);
      break;
    case force_config_type:
      G_DEBUG("%s: Ignoring a forced intermediate, pending force_config",
              caller_name);
      break;
    case set_max_leaders:
      G_DEBUG(
          "%s: Ignoring a forced intermediate, pending set_max_leaders for "
          "%" PRIu32,
          caller_name, a->body.app_u_u.max_leaders);
      break;
    case set_leaders_type:
      G_DEBUG("%s: Ignoring a forced intermediate, pending set_leaders_type",
              caller_name);
      break;
    case abort_trans:
    case app_type:
    case begin_trans:
    case convert_into_local_server_type:
    case disable_arbitrator:
    case enable_arbitrator:
    case exit_type:
    case get_event_horizon_type:
    case get_synode_app_data_type:
    case prepared_trans:
    case remove_reset_type:
    case reset_type:
    case set_cache_limit:
    case view_msg:
    case x_terminate_and_exit:
    case xcom_boot_type:
    case xcom_set_group:
    case get_leaders_type:
      // Meaningless for any other `cargo_type`s. Ignore.
      break;
  }
}

bool_t handle_config(app_data_ptr a, bool const forced) {
  assert(a->body.c_t == unified_boot_type || a->body.c_t == set_max_leaders ||
         a->body.c_t == set_leaders_type ||
         a->next == nullptr); /* Reconfiguration commands are not batched. */
  {
    bool_t success = FALSE;
    if (forced &&
        should_ignore_forced_config_or_view(get_executor_site()->x_proto)) {
      log_ignored_forced_config(a, "handle_config");
      goto end;
    }
    switch (a->body.c_t) {
      case unified_boot_type:
        success = (install_node_group(a) != nullptr);
        assert(success);
        break;
      case add_node_type:
        /*
         * May fail if meanwhile the event horizon was reconfigured and the
         * node is incompatible.
         */
        success = (handle_add_node(a) != nullptr);
        break;
      case remove_node_type:
        ADD_DBG(D_BASE,
                add_event(EVENT_DUMP_PAD, string_arg("got remove_node_type"));)
        success = (handle_remove_node(a) != nullptr);
        assert(success);
        break;
      case set_event_horizon_type:
        /* May fail if meanwhile an incompatible node joined. */
        success = handle_event_horizon(a);
        break;
      case force_config_type:
        success = (install_node_group(a) != nullptr);
        assert(success);
        break;
      case set_max_leaders:
      case set_leaders_type:
        success = handle_leaders(a);
        assert(success);
        break;
      default:
        assert(FALSE); /* Boy oh boy, something is really wrong... */
        break;
    }
  end:
    return success;
  }
}

static inline int is_member(site_def const *site) {
  return site->nodeno != VOID_NODE_NO;
}

/*
Execute xcom message stream.

Beware  of  the exit logic in this task, which is both simple and
not so simple.  Consider three configs C1  and  C2.  C1  has  two
nodes,  A and B. C2 has only node B.  C3 is empty.  A config with
message number N will be activated after a delay  of  (at  least)
alpha  messages,  where alpha is the size of the pipeline (or the
event horizon).

So, C1.start = C1+alpha, and C2.start = C2+alpha. A, which is re‐
moved  from  C1, cannot exit until a majority of nodes in the new
config C2 (in this case B) has learned all the messages from con‐
fig  C1,  which  means all messages less than C2.start. How can A
know that a majority of C2 has learned those messages?

If we denote the first message that is not yet decided (and  exe‐
cuted)  by E, the proposers will not try to propose messages with
number >= E+alpha, and all incoming  tcp  messages  with  message
number  >=  E+alpha will be ignored.  E is incremented by the ex‐
ecutor task, so all messages < E are known.  This means that when
the value of E+alpha is known, all messages up to and including E
are also known, although not all messages  E+1..E+alpha‐1  neces‐
sarily are known.

This  leads  to  the requirement that a node which is removed (A)
needs to wait until it knows the value of  C2.start+alpha,  since
by  then it knows that a majority of the nodes in C2 are ready to
execute C2.start, which in turn implies that a majority of  nodes
in  C2  knows  all  the values from config C1. Note that the last
message that should be delivered to the  application  by  a  node
that  is  leaving  C1 is C2.start‐1, which is the last message of
C1.

How does a node that is removed get to know values from the  next
config?   There  are  two  ways, and we use both. First, the node
that tries to exit can simply ask for the message.  get_xcom_mes‐
sage()  will  do  this for all messages <= max_synode, but it may
take some time.  Second, the nodes of C2 can  send  the  messages
C2.start..C2.start+alpha  to  the  nodes  that are removed (nodes
that are in C1 but not in C2).  inform_removed()  does  this.  We
take  care to handle the case where configs are close enough that
C0 < C1 <= C0+alpha by tracking the oldest config  that  contains
nodes that are leaving.

This  takes care of nodes leaving C1. What about nodes that leave
C2? C3 is empty, so B, which is leaving C2, cannot wait for  mes‐
sages  from  C3. But since C3 is empty, there is no need to wait.
It can exit immediately after  having  executed  C3.start‐1,  the
last message of C2. What if C3.start‐1 < C2.start+alpha? This can
happen if C2 and C3 are close. In that case, B will exit before A
gets the chance to learn C2.start+alpha, which will leave A hang‐
ing forever. Clearly, we need to impose an additional constraint,
that  C3.start must be greater than C2.start+alpha. This is taken
care of by the special test for an empty config.

Complicated and confusing? Not really, but there is a  clean  and
simple  solution which has not been implemented yet, since it re‐
quires more changes to the consensus logic.  If we  require  that
for  the messages C2..C2.start‐1 we have a majority from both the
nodes in C1 and the nodes in C2, the nodes not  in  C2  can  exit
when  they  have  executed message C2.start‐1, since we then know
that a majority of the nodes of C2 has agreed on  those  messages
as  well,  so they do not depend on the nodes not in C2 any more.
This holds even if C2 is empty.  Note that requiring  a  majority
from  both  C1 and C2 is different from requiring a majority from
C1+C2, which means that the proposer logic needs to consider  an‐
swers  from  two  different sets of acceptors for those messages.
Since acceptors are identified by their node number, and the node
numbers  need  not be the same for both configs, we need to main‐
tain a mapping between the nodes numbers of any  two  consecutive
configs.  Alternatively,  we  could remove the node numbers alto‐
gether, and always use a unique, unchanging ID for a  node,  like
IP address + port.

TODO:

Move the delayed delivery logic into MCM-specific code, since it is
only needed by MCM.  Is it still needed?

Rewrite exit logic as FSM with more states. (RUN, EMPTY_EXIT,
NOT_MEMBER_EXIT) to avoid unnecessary tests.

*/

/* FIFO which tracks the message numbers where we should deliver queued messages
or
inform the removed nodes */
#define FIFO_SIZE 1000
static struct {
  int n;
  int front;
  int rear;
  synode_no q[FIFO_SIZE];
} delay_fifo;

static inline int addone(int i) { return ((i + 1) % FIFO_SIZE); }

/* Is queue empty?  */
static inline int fifo_empty() { return delay_fifo.n <= 0; }

/* Is queue full?  */
static inline int fifo_full() { return delay_fifo.n >= FIFO_SIZE; }

/* Insert in queue  */
static inline void fifo_insert(synode_no s) {
  if (!fifo_full()) {
    delay_fifo.n++;
    delay_fifo.q[delay_fifo.rear] = s;
    delay_fifo.rear = addone(delay_fifo.rear);
  }
}

/* Extract first from queue  */
static inline synode_no fifo_extract() {
  if (!fifo_empty()) {
    synode_no ret = delay_fifo.q[delay_fifo.front];
    delay_fifo.front = addone(delay_fifo.front);
    delay_fifo.n--;
    return ret;
  } else {
    return null_synode;
  }
}

/* Return first in queue, but do not dequeue  */
static inline synode_no fifo_front() {
  if (!fifo_empty()) {
    return delay_fifo.q[delay_fifo.front];
  } else {
    return null_synode;
  }
}

struct execute_context;
typedef struct execute_context execute_context;

typedef void (*exec_fp)(execute_context *xc);

struct execute_context {
  pax_machine *p;
  int n;
  int old_n;
  double old_t;
  synode_no exit_synode;
  synode_no delivery_limit;
  exec_fp state;
  int exit_flag; /* To avoid state explosion */
  int inform_index;
};

static void dump_exec_state(execute_context *xc [[maybe_unused]],
                            long dbg [[maybe_unused]]);
static int x_check_exit(execute_context *xc);
static int x_check_execute_inform(execute_context *xc);
static void x_fetch(execute_context *xc);
static void x_execute(execute_context *xc);
static void x_check_increment_fetch(execute_context *xc);
static void x_check_increment_execute(execute_context *xc);
static void x_terminate(execute_context *xc);

struct fp_name {
  exec_fp fp;
  char const *name;
};

#define NAME(f) \
  { f, #f }

/* List of fp, name pairs */
[[maybe_unused]] static struct fp_name oblist[] = {
    NAME(x_fetch), NAME(x_execute), NAME(x_terminate), {nullptr, nullptr}};
#undef NAME

#if TASK_DBUG_ON
/* purecov: begin deadcode */
char const *get_fp_name(exec_fp fp) {
  struct fp_name *list = oblist;
  while (list->fp) {
    if (list->fp == fp) return list->name;
    list++;
  }
  return "no such fp";
}
/* purecov: end */
#endif

static void setup_exit_handling(execute_context *xc, site_def *site) {
  synode_no delay_until;
  if (is_member(site)) {
    delay_until = compute_delay(site->start, site->event_horizon);
  } else { /* Not in this site */
           /* See if site will be empty when we leave. If the new site
            * is empty, we should exit after having delivered the last
            * message from the old site. */

    /* Note limit of delivery. We should never deliver anything after the
     * start of the next site. */
    xc->delivery_limit = site->start;

    /* If we are not a member of the new site, we should exit
      after having seen enough messages beyond the end of the current site.
      This ensures that a majority of the next site will have agreed upon all
      messages that belong to the current site.
     */
    xc->exit_synode = compute_delay(site->start, site->event_horizon);
    if (is_empty_site(site)) {
      /* If site is empty, increase start to allow nodes to terminate before
       * start. This works as if there was a non-empty group after the
       * exit_synode, effectively allowing the majority of the current group
       * to agree on all messages up to exit_synode.
       */
      site->start = compute_delay(
          compute_delay(site->start, site->event_horizon), site->event_horizon);
    }
    if (!synode_lt(xc->exit_synode, max_synode)) {
      /* We need messages from the next site, so set max_synode accordingly.
       */
      set_max_synode(incr_synode(xc->exit_synode));
    }
    /* Note where we switch to execute and inform removed nodes */
    delay_until = xc->exit_synode;

    IFDBG(D_EXEC, FN; SYCEXP(delay_until); SYCEXP(executed_msg);
          SYCEXP(max_synode));
    IFDBG(D_EXEC, FN; SYCEXP(xc->exit_synode); SYCEXP(executed_msg);
          SYCEXP(max_synode));

    /* Note that we will exit */
    xc->exit_flag = 1;
  }

  /* Ensure that max_synode is greater than trigger for delivery
   */
  if (synode_gt(delay_until, max_synode))
    set_max_synode(incr_msgno(delay_until));
  fifo_insert(delay_until);
  (xc->inform_index)++;

  /* If I am the leader, will propose no-ops until current max_synode
   */
}

/* Called immediately after we have got a new message.
   Terminate if we have no site.
   Otherwise, handle config messages immediately.
   Afterwards, switch to check_exit_fetch. */
static void x_fetch(execute_context *xc) {
  /* Execute unified_boot immediately, but do not deliver site message
   * until we are ready to execute messages from the new site
   * definition. At that point we can be certain that a majority have
   * learned everything from the old site. */

  app_data *app = xc->p->learner.msg->a;
  if (app && is_config(app->body.c_t) &&
      synode_gt(executed_msg, get_site_def()->boot_key)) /* Redo test */
  {
    site_def *site = nullptr;
    bool_t reconfiguration_successful =
        handle_config(app, (xc->p->learner.msg->force_delivery != 0));
    if (reconfiguration_successful) {
      /* If the reconfiguration failed then it does not have any
       * effect. What follows only makes sense if the reconfiguration
       * took effect. */
      set_last_received_config(executed_msg);
      synode_no min_synode = min_proposer_synode();
      if (synode_eq(null_synode, min_synode) ||
          synode_lt(delivered_msg, min_synode))
        min_synode = get_last_delivered_msg();
      garbage_collect_site_defs(min_synode);
      site = get_site_def_rw();
      if (site == nullptr) {
        xc->state = x_terminate;
        return;
      }
      IFDBG(D_EXEC, FN; STRLIT("new config "); SYCEXP(site->boot_key););

      if (xc->exit_flag == 0) {
        /* We have not yet set the exit trigger */
        setup_exit_handling(xc, site);
      }
    }
  } else {
    IFDBG(D_EXEC, FN; SYCEXP(executed_msg); SYCEXP(get_site_def()->boot_key));
  }
  /* Check for exit and increment executed_msg */
  x_check_increment_fetch(xc);
}

/* Push messages to nodes that have been removed.
   Signal switch to execute when nothing left to push by returning 1 */
static int x_check_execute_inform(execute_context *xc) {
  IFDBG(D_EXEC, FN; SYCEXP(fifo_front()); SYCEXP(executed_msg);
        SYCEXP(xc->exit_synode); NDBG(xc->exit_flag, d));
  if (fifo_empty()) {
    return 1;
  } else if (!synode_lt(executed_msg, fifo_front())) {
    while (
        !fifo_empty() &&
        !synode_lt(executed_msg, fifo_front())) { /* More than one may match */
      inform_removed(xc->inform_index, 0);
      fifo_extract();
      (xc->inform_index)--;
    }
    garbage_collect_servers();
    return 1;
  }
  dump_exec_state(xc, D_EXEC);
  return 0;
}

/* Check for exit and return 1 if we should exit. */
static int x_check_exit(execute_context *xc) {
  /* See if we should exit when having seen this message */
  return (xc->exit_flag && !synode_lt(executed_msg, xc->exit_synode) &&
          !synode_lt(delivered_msg, xc->delivery_limit));
}

/* Terminate if we should exit, else increment executed_msg and see if we
 * should switch to execute */
static void x_check_increment_fetch(execute_context *xc) {
  if (x_check_exit(xc)) {
    xc->state = x_terminate;
  } else {
    SET_EXECUTED_MSG(incr_synode(executed_msg));
    if (x_check_execute_inform(xc)) {
      xc->state = x_execute;
    }
  }
}

/* Terminate if we should exit, else increment delivered_msg and see if we
 * should switch to fetch */
static void x_check_increment_execute(execute_context *xc) {
  if (x_check_exit(xc)) {
    xc->state = x_terminate;
  } else {
    /* Increment delivered_msg and switch to fetch if delivered_msg equals
     * executed_msg; */
    delivered_msg = incr_synode(delivered_msg);
    if (synode_eq(delivered_msg, executed_msg)) {
      xc->state = x_fetch;
    }
  }
}

/* Deliver one message if it should be delivered. Switch state to see if
   we should exit */
static void x_execute(execute_context *xc) {
  site_def *x_site = find_site_def_rw(delivered_msg);

  IFDBG(D_EXEC, FN; SYCEXP(delivered_msg); SYCEXP(delivered_msg);
        SYCEXP(executed_msg); SYCEXP(xc->exit_synode); NDBG(xc->exit_flag, d));
  if (!is_cached(delivered_msg)) {
/* purecov: begin deadcode */
#ifdef TASK_EVENT_TRACE
    dump_task_events();
#endif
    /* purecov: end */
  }
  if (!ignore_message(delivered_msg, x_site, "x_execute")) {
    assert(is_cached(delivered_msg) && "delivered_msg should have been cached");
    xc->p = get_cache(delivered_msg);
    if ((xc->p)->learner.msg->msg_type != no_op) {
      /* Avoid delivery after start if we should exit */
      if (xc->exit_flag == 0 || synode_lt(delivered_msg, xc->delivery_limit)) {
        IFDBG(D_EXEC, FN; STRLIT("executing "); SYCEXP(delivered_msg);
              SYCEXP(executed_msg); SYCEXP(xc->delivery_limit);
              NDBG(xc->exit_flag, d));
        last_delivered_msg = delivered_msg;
        execute_msg(find_site_def_rw(delivered_msg), xc->p, xc->p->learner.msg);
      }
    }
  }
  /* Garbage collect old servers */
  if (synode_eq(delivered_msg, x_site->start)) {
    garbage_collect_servers();
  }
#if defined(TASK_DBUG_ON) && TASK_DBUG_ON
  IFDBG(D_EXEC, perf_dbg(&xc->n, &xc->old_n, &xc->old_t));
#endif
  /* Check for exit and increment delivered_msg */
  x_check_increment_execute(xc);
}

static execute_context *debug_xc;

static void dump_exec_state(execute_context *xc [[maybe_unused]],
                            long dbg [[maybe_unused]]) {
  IFDBG(dbg, FN; SYCEXP(executed_msg); SYCEXP(delivered_msg);
        SYCEXP(max_synode); SYCEXP(last_delivered_msg); NDBG(delay_fifo.n, d);
        NDBG(delay_fifo.front, d); NDBG(delay_fifo.rear, d);
        SYCEXP(fifo_front()); SYCEXP(xc->exit_synode);
        SYCEXP(xc->delivery_limit); NDBG(xc->exit_flag, d);
        NDBG(xc->inform_index, d); NDBG(prop_started, d);
        NDBG(prop_finished, d););
}

static void dump_debug_exec_state() {
  if (debug_xc) dump_exec_state(debug_xc, D_EXEC);
}

/* Terminate the excutor_task. */
static void x_terminate(execute_context *xc) {
  dump_exec_state(xc, D_BUG);
  xc->state = nullptr;
}

static int executor_task(task_arg arg [[maybe_unused]]) {
  DECL_ENV
  execute_context xc;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  /* xcom_debug_mask = D_BUG; */
  IFDBG(D_EXEC, FN; NDBG(stack->sp->state, d); SYCEXP(executed_msg););
  TASK_BEGIN
  ep->xc.p = nullptr;
  ep->xc.n = 0;
  ep->xc.old_n = 0;
  ep->xc.old_t = task_now();
  ep->xc.exit_synode = null_synode;
  ep->xc.delivery_limit = null_synode;
  ep->xc.exit_flag = 0;
  ep->xc.inform_index = -1;
  delay_fifo.n = 0;
  delay_fifo.front = 0;
  delay_fifo.rear = 0;
  debug_xc = &ep->xc;

  if (executed_msg.msgno == 0) executed_msg.msgno = 1;
  delivered_msg = executed_msg;
  ep->xc.state = x_fetch;
  executor_site = find_site_def_rw(executed_msg);

  /* The following loop implements a state machine based on function pointers,
     effectively acting as non-local gotos.
     The functions all operate on data in the execution context xc, and
     switch state by setting xc->state to the function corresponding to the
     new state.
  */
  while (!xcom_shutdown && ep->xc.state != nullptr) {
    IFDBG(D_EXEC, FN; STRLIT(get_fp_name(ep->xc.state)););
    if (ep->xc.state == x_fetch) { /* Special case because of task macros */
      if (ignore_message(executed_msg, executor_site, "executor_task")) {
        IFDBG(D_EXEC, FN; STRLIT("ignoring message "); SYCEXP(executed_msg));
        x_check_increment_fetch(&ep->xc); /* Just increment past losers */
      } else {
        IFDBG(D_EXEC, FN; STRLIT("fetching message "); SYCEXP(executed_msg));
        TASK_CALL(get_xcom_message(&ep->xc.p, executed_msg, FIND_MAX));
        IFDBG(D_EXEC, FN; STRLIT("got message "); SYCEXP(ep->xc.p->synode);
              COPY_AND_FREE_GOUT(dbg_app_data(ep->xc.p->learner.msg->a)));
        x_fetch(&ep->xc);
      }
    } else {
      ep->xc.state(&ep->xc);
    }
  }

  /* Inform all removed nodes before we exit */
  ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
  inform_removed(ep->xc.inform_index, 1);
  dump_exec_state(&ep->xc, D_EXEC);

#ifndef NO_DELAYED_TERMINATION
  IFDBG(D_EXEC, FN; STRLIT("delayed terminate and exit"));

  /* Wait to allow messages to propagate */
  TASK_DELAY(TERMINATE_DELAY);

  /* Start termination of xcom */
  terminate_and_exit();
#endif

  FINALLY
  dump_exec_state(&ep->xc, D_EXEC);
  IFDBG(D_BUG, FN; STRLIT(" shutdown "); SYCEXP(executed_msg);
        NDBG(task_now(), f));
  TASK_END;
}

static synode_no get_sweep_start() {
  synode_no find = executed_msg;
  find.node = get_nodeno(find_site_def(find));
  if (find.node < executed_msg.node) {
    find = incr_msgno(find);
  }
  return find;
}

/* Allow takeover of channel if not all are leaders. We may need to adjust
 * this if we allow any subset of the nodes as leaders */
static bool allow_channel_takeover(site_def const *site) {
  return site->max_active_leaders != active_leaders_all;
}

static void broadcast_noop(synode_no find, pax_machine *p) {
  site_def const *site = find_site_def(find);

  /*
   If we allow channel hijacking, we cannot send skip_op, but need consensus.
   There are two options here:

   a) We unconditionally propose a `no_op` using the regular 3-phase Paxos
      protocol, or
   b) We propose a `no_op` using the 2-phase Paxos protocol *if* we
      are sure that no other Proposer will try to run the 2-phase Paxos
   protocol on `find`. If we are not sure, we propose using the 3-phase Paxos
      protocol.

   Option (a) is always safe, but we pay the cost of 3-phase Paxos.
   Option (b) can be implemented by having the leaders keep track of the
   synods they allocate to non-leaders. If we are the leader for `find` and we
   allocated it to a non-leader, we must use 3-phase Paxos here to be safe
   against the non-leader using 2-phase Paxos. If we never allocated `find` to
   a non-leader, we can use 2-phase Paxos here if we ensure we don't allocate
   `find` to a non-leader afterwards.

   We go with option (a) because there is no evidence that the additional
   complexity that option (b) requires is worthwhile.
   */
  if (allow_channel_takeover(site)) {
    propose_noop(find, p);  // Single leader
  } else {
    skip_msg(pax_msg_new(find, site));  // Multiple leaders
  }
}

static int sweeper_task(task_arg arg [[maybe_unused]]) {
  DECL_ENV
  synode_no find;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN

  ep->find = get_sweep_start();

  while (!xcom_shutdown) {
    ep->find.group_id =
        executed_msg.group_id; /* In case group id has changed */
#ifndef AGGRESSIVE_SWEEP
    while (!is_only_task()) {
      TASK_YIELD;
    }
#endif
    ADD_DBG(D_NONE, add_event(EVENT_DUMP_PAD, string_arg("sweeper ready"));
            add_synode_event(executed_msg););
    /*		IFDBG(D_NONE, FN; STRLIT("ready to run ");   */
    /*			SYCEXP(executed_msg); SYCEXP(max_synode);
     * SYCEXP(ep->find));
     */
    while (synode_lt(ep->find, max_synode) && !too_far(ep->find)) {
      /* pax_machine * pm = hash_get(ep->find); */
      pax_machine *pm = nullptr;
      ADD_DBG(D_NONE,
              add_event(EVENT_DUMP_PAD, string_arg("sweeper examining"));
              add_synode_event(ep->find););
      if (ep->find.node == VOID_NODE_NO) {
        if (synode_gt(executed_msg, ep->find)) {
          ep->find = get_sweep_start();
        }
        if (ep->find.node == VOID_NODE_NO) goto deactivate;
      }
      pm = get_cache(ep->find);
      ADD_DBG(D_CONS, add_event(EVENT_DUMP_PAD, string_arg("sweeper checking"));
              add_synode_event(ep->find);
              add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(pm->op)));
              add_event(EVENT_DUMP_PAD, string_arg("pm"));
              add_event(EVENT_DUMP_PAD, void_arg(pm)););
      if (pm && !pm->force_delivery) { /* We want full 3 phase Paxos for
                                          forced messages */
        ADD_DBG(
            D_CONS, add_event(EVENT_DUMP_PAD, string_arg("sweeper checking"));
            add_synode_event(ep->find);
            add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(pm->op)));
            add_event(EVENT_DUMP_PAD, string_arg("is_busy_machine"));
            add_event(EVENT_DUMP_PAD, int_arg(is_busy_machine(pm)));
            add_event(EVENT_DUMP_PAD, string_arg("pm->acceptor.promise.cnt"));
            add_event(EVENT_DUMP_PAD, int_arg(pm->acceptor.promise.cnt));
            add_event(EVENT_DUMP_PAD, string_arg("finished(pm)"));
            add_event(EVENT_DUMP_PAD, int_arg(finished(pm)));
            add_event(EVENT_DUMP_PAD, string_arg("pm->acceptor.msg"));
            add_event(EVENT_DUMP_PAD, void_arg(pm->acceptor.msg)););
        /* IFDBG(D_NONE, FN; dbg_pax_machine(pm)); */
        if (!is_busy_machine(pm) && pm->acceptor.promise.cnt == 0 &&
            !pm->acceptor.msg && !finished(pm)) {
          ADD_DBG(
              D_CONS, add_event(EVENT_DUMP_PAD, string_arg("sweeper skipping"));
              add_synode_event(ep->find);
              add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(pm->op))););
          site_def *config = find_site_def_rw(ep->find);
          // Do not send noop if single writer, since there normally will be
          // no holes in the message sequence, and it may interfere with
          // messages delegated to secondary nodes.
          if (config->max_active_leaders != 1 &&
              !ignore_message(ep->find, config, "sweeper_task")) {
            broadcast_noop(ep->find, pm);
          }
          IFDBG(D_NONE, FN; STRLIT("skipping "); SYCEXP(ep->find));
        }
      }
      ep->find = incr_msgno(ep->find);
    }
  deactivate:
    if (!synode_lt(ep->find, max_synode)) {
      TASK_DEACTIVATE;
    } else {
      TASK_DELAY(0.010); /* Let poll_wait check for IO */
    }
  }
  FINALLY
  IFDBG(D_BUG, FN; STRLIT(" shutdown sweeper "); SYCEXP(executed_msg);
        NDBG(task_now(), f));
  TASK_END;
}

static double wakeup_delay(double old) {
  double const minimum_threshold = 0.1;
#ifdef EXECUTOR_TASK_AGGRESSIVE_NO_OP
  double const maximum_threshold = 1.0;
#else
  double const maximum_threshold = 20.0;
#endif /* EXECUTOR_TASK_AGGRESSIVE_NO_OP */
  double retval = 0.0;
  if (0.0 == old) {
    double m = median_time();
    double const fuzz = 5.0;
    IFDBG(D_BUG, FN; NDBG(m, f));
    // Guard against unreasonable estimates of median consensus time
    if (m <= 0.0) m = minimum_threshold;
    if (m > maximum_threshold / fuzz) m = (maximum_threshold / fuzz) / 2.0;
    retval = minimum_threshold + fuzz * m + m * xcom_drand48();
  } else {
    retval = old * 1.4142136; /* Exponential backoff */
  }
  /* If we exceed maximum, choose a random value in the max/2..max interval */
  if (retval > maximum_threshold) {
    double const low = maximum_threshold / 2.0;
    retval = low + xcom_drand48() * (maximum_threshold - low);
  }
  IFDBG(D_BUG, FN; NDBG(retval, f));
  return retval;
}

static site_def const *init_noop(synode_no find, pax_machine *p) {
  /* Prepare to send a noop */
  site_def const *site = find_site_def(find);
  IFDBG(D_NONE, FN; SYCEXP(find); SYCEXP(executed_msg));
  assert(!too_far(find));
  replace_pax_msg(&p->proposer.msg, pax_msg_new(find, site));
  assert(p->proposer.msg);
  create_noop(p->proposer.msg);
  return site;
}

static void propose_noop(synode_no find, pax_machine *p) {
  site_def const *site = init_noop(find, p);
  pax_msg *clone = clone_pax_msg(p->proposer.msg);
  if (clone != nullptr) {
    IFDBG(D_CONS, FN; SYCEXP(find));
    push_msg_3p(site, p, clone, find, no_op);
  } else {
    /* purecov: begin inspected */
    G_DEBUG("Unable to propose NoOp due to an OOM error.");
    /* purecov: end */
  }
}

#if 0
static void propose_noop_2p(synode_no find, pax_machine *p) {
  site_def const *site = init_noop(find, p);
  IFDBG(D_CONS, FN; SYCEXP(find));
  push_msg_2p(site, p);
}
#endif

static void send_read(synode_no find) {
  /* Prepare to send a read_op */
  site_def const *site = find_site_def(find);

  IFDBG(D_NONE, FN; NDBG(get_maxnodes(site), u); NDBG(get_nodeno(site), u););
  ADD_DBG(D_CONS, add_event(EVENT_DUMP_PAD, string_arg("find"));
          add_synode_event(find); add_event(EVENT_DUMP_PAD, string_arg("site"));
          add_event(EVENT_DUMP_PAD, void_arg((void *)find_site_def_rw(find)));
          add_event(EVENT_DUMP_PAD, string_arg("get_nodeno(site)"));
          add_event(EVENT_DUMP_PAD, uint_arg(get_nodeno(site))););

  /* See if node number matches ours */
  if (site) {
    if (find.node != get_nodeno(site)) {
      pax_msg *pm = pax_msg_new(find, site);
      ref_msg(pm);
      create_read(site, pm);
      IFDBG(D_NONE, FN; SYCEXP(find););

      IFDBG(D_NONE, FN; NDBG(get_maxnodes(site), u); NDBG(get_nodeno(site), u);
            PTREXP(pm));
/* send_server_msg(site, find.node, pm); */
#if 0
			send_to_others(site, pm, "send_read");
#else
      /* If we have no node number,  ask all the others */
      if (get_nodeno(site) == VOID_NODE_NO)
        send_to_others(site, pm, "send_read");
      else
        /* Ask a random node */
        send_to_someone(site, pm, "send_read");
#endif
      unref_msg(&pm);
    } else { /* If node number matches our own number, ask all the others */
      pax_msg *pm = pax_msg_new(find, site);
      ref_msg(pm);
      create_read(site, pm);
      send_to_others(site, pm, "send_read");
      unref_msg(&pm);
    }
  }
}

/* Find missing values */

static int ok_to_propose(pax_machine *p) {
  int retval = (is_forcing_node(p) || !recently_active(p)) && !finished(p) &&
               !is_busy_machine(p);
  IFDBG(D_NONE, FN; NDBG(p->synode.node, u); NDBG(recently_active(p), d);
        NDBG(finished(p), d); NDBG(is_busy_machine(p), d); NDBG(retval, d));
  return retval;
}

static void read_missing_values(int n) {
  synode_no find = executed_msg;
  synode_no end = max_synode;
  int i = 0;

  IFDBG(D_NONE, FN; SYCEXP(find); SYCEXP(end));
  if (synode_gt(executed_msg, max_synode) ||
      synode_eq(executed_msg, null_synode))
    return;

  while (!synode_gt(find, end) && i < n && !too_far(find)) {
    pax_machine *p = force_get_cache(find);
    ADD_DBG(D_NONE, add_synode_event(find); add_synode_event(end);
            add_event(EVENT_DUMP_PAD, string_arg("active "));
            add_event(EVENT_DUMP_PAD, int_arg(recently_active(p)));
            add_event(EVENT_DUMP_PAD, string_arg("finished  "));
            add_event(EVENT_DUMP_PAD, int_arg(finished(p)));
            add_event(EVENT_DUMP_PAD, string_arg("busy "));
            add_event(EVENT_DUMP_PAD, int_arg(is_busy_machine(p))););
    IFDBG(D_NONE, FN; SYCEXP(find); SYCEXP(end); NDBG(recently_active(p), d);
          NDBG(finished(p), d); NDBG(is_busy_machine(p), d));
    if (!recently_active(p) && !finished(p) && !is_busy_machine(p)) {
      send_read(find);
    }
    find = incr_synode(find);
    i++;
  }
}

static void propose_missing_values(int n) {
  synode_no find = executed_msg;
  synode_no end = max_synode;
  int i = 0;

  IFDBG(D_NONE, FN; NDBG(get_maxnodes(get_site_def()), u); SYCEXP(find);
        SYCEXP(end));
  if (synode_gt(executed_msg, max_synode) ||
      synode_eq(executed_msg, null_synode))
    return;

  IFDBG(D_NONE, FN; SYCEXP(find); SYCEXP(end));
  i = 0;
  while (!synode_gt(find, end) && i < n && !too_far(find)) {
    pax_machine *p = force_get_cache(find);
    if (wait_forced_config) {
      force_pax_machine(p, 1);
    }
    IFDBG(D_NONE, FN; NDBG(ok_to_propose(p), d); TIMECEXP(task_now());
          TIMECEXP(p->last_modified); SYCEXP(find));
    site_def *site = find_site_def_rw(find);
    if (get_nodeno(site) == VOID_NODE_NO) break;
    if (!ignore_message(find, site, "propose_missing_values") &&
        ok_to_propose(p)) {
      propose_noop(find, p);
    }
    find = incr_synode(find);
    i++;
  }
}

/* Message handlers */

/*
Reply to the sender of a message.
Avoid using the outbound TCP connection to the node that sent the message, since
it is simpler and safer to always use the same TCP connection as the one the
message arrived on. We then know that the answer will always go to the same
client (and the same instance of that client) that sent the request.
*/
#define reply_msg(m)                                              \
  {                                                               \
    if (is_local_node((m)->from, site)) {                         \
      dispatch_op(site, m, NULL);                                 \
    } else {                                                      \
      link_into(&(msg_link_new((m), (m)->from)->l), reply_queue); \
    }                                                             \
  }

#define CREATE_REPLY(x)  \
  pax_msg *reply = NULL; \
  CLONE_PAX_MSG(reply, x)

#define SEND_REPLY  \
  reply_msg(reply); \
  replace_pax_msg(&reply, NULL)

bool_t safe_app_data_copy(pax_msg **target, app_data_ptr source) {
  copy_app_data(&(*target)->a, source);
  if ((*target)->a == nullptr && source != nullptr) {
    oom_abort = 1;
    replace_pax_msg(target, nullptr);
    return FALSE;
  }
  return TRUE;
}

static pax_msg *create_learn_msg_for_ignorant_node(pax_machine *p, pax_msg *pm,
                                                   synode_no synode) {
  CREATE_REPLY(pm);
  IFDBG(D_NONE, FN; SYCEXP(synode));
  reply->synode = synode;
  reply->proposal = p->learner.msg->proposal;
  reply->msg_type = p->learner.msg->msg_type;
  safe_app_data_copy(&reply, p->learner.msg->a);
  if (reply != nullptr) set_learn_type(reply);
  /* set_unique_id(reply, p->learner.msg->unique_id); */
  return reply;
}

static void teach_ignorant_node(site_def const *site, pax_machine *p,
                                pax_msg *pm, synode_no synode,
                                linkage *reply_queue) {
  pax_msg *reply = create_learn_msg_for_ignorant_node(p, pm, synode);
  if (reply != nullptr) SEND_REPLY;
}

/* Handle incoming read */
static void handle_read(site_def const *site, pax_machine *p,
                        linkage *reply_queue, pax_msg *pm) {
  IFDBG(D_NONE, FN; BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise);
        if (p->acceptor.msg) BALCEXP(p->acceptor.msg->proposal);
        STRLIT("type "); STRLIT(pax_msg_type_to_str(pm->msg_type)));

  if (finished(p)) { /* We have learned a value */
    teach_ignorant_node(site, p, pm, pm->synode, reply_queue);
  }
}

static pax_msg *create_ack_prepare_msg(pax_machine *p, pax_msg *pm,
                                       synode_no synode) {
  CREATE_REPLY(pm);
  reply->synode = synode;
  if (accepted(p)) { /* We have accepted a value */
    reply->proposal = p->acceptor.msg->proposal;
    reply->msg_type = p->acceptor.msg->msg_type;
    IFDBG(D_NONE, FN; STRLIT(" already accepted value "); SYCEXP(synode));
    reply->op = ack_prepare_op;
    safe_app_data_copy(&reply, p->acceptor.msg->a);
  } else {
    IFDBG(D_NONE, FN; STRLIT(" no value synode "); SYCEXP(synode));
    reply->op = ack_prepare_empty_op;
  }
  return reply;
}

pax_msg *handle_simple_prepare(pax_machine *p, pax_msg *pm, synode_no synode) {
  pax_msg *reply = nullptr;
  if (finished(p)) { /* We have learned a value */
    IFDBG(D_NONE, FN; SYCEXP(synode); BALCEXP(pm->proposal);
          NDBG(finished(p), d));
    reply = create_learn_msg_for_ignorant_node(p, pm, synode);
  } else {
    int greater =
        gt_ballot(pm->proposal,
                  p->acceptor.promise); /* Paxos acceptor phase 1 decision */
    IFDBG(D_NONE, FN; SYCEXP(synode); BALCEXP(pm->proposal); NDBG(greater, d));
    if (greater || noop_match(p, pm)) {
      p->last_modified = task_now();
      if (greater) {
        p->acceptor.promise = pm->proposal; /* promise to not accept any less */
      }
      reply = create_ack_prepare_msg(p, pm, synode);
    }
  }
  return reply;
}

/* Handle incoming prepare */
static void handle_prepare(site_def const *site, pax_machine *p,
                           linkage *reply_queue, pax_msg *pm) {
  ADD_DBG(D_CONS, add_synode_event(p->synode);
          add_event(EVENT_DUMP_PAD, string_arg("pm->from"));
          add_event(EVENT_DUMP_PAD, uint_arg(pm->from));
          add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(pm->op)));
          add_event(EVENT_DUMP_PAD, string_arg("proposal"));
          add_ballot_event(pm->proposal);
          add_event(EVENT_DUMP_PAD, string_arg("promise"));
          add_ballot_event(p->acceptor.promise););
  IFDBG(D_NONE, FN; BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise);
        if (p->acceptor.msg) BALCEXP(p->acceptor.msg->proposal);
        STRLIT("type "); STRLIT(pax_msg_type_to_str(pm->msg_type)));

  {
    pax_msg *reply = handle_simple_prepare(p, pm, pm->synode);
    if (reply != nullptr) SEND_REPLY;
  }
}

bool_t check_propose(site_def const *site, pax_machine *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode);
        COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(site))););
  PAX_MSG_SANITY_CHECK(p->proposer.msg);
  {
    bool_t can_propose = FALSE;
    if (prep_majority(site, p)) {
      p->proposer.msg->proposal = p->proposer.bal;
      BIT_ZERO(p->proposer.prop_nodeset);
      p->proposer.msg->synode = p->synode;
      init_propose_msg(p->proposer.msg);
      p->proposer.sent_prop = p->proposer.bal;
      can_propose = TRUE;
    }
    return can_propose;
  }
}

static bool learn_ok(site_def const *site, pax_machine const *p) {
  return get_nodeno(site) != VOID_NODE_NO && prop_majority(site, p);
}

static pax_msg *check_learn(site_def const *site, pax_machine *p) {
  IFDBG(D_NONE, FN; SYCEXP(p->synode);
        COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(site))););
  PAX_MSG_SANITY_CHECK(p->proposer.msg);
  {
    pax_msg *learn_msg = nullptr;
    if (learn_ok(site, p)) {
      p->proposer.msg->synode = p->synode;
      if (p->proposer.msg->receivers) free_bit_set(p->proposer.msg->receivers);
      p->proposer.msg->receivers = clone_bit_set(p->proposer.prep_nodeset);
      BIT_SET(get_nodeno(site), p->proposer.msg->receivers);
      if (no_duplicate_payload) {
        learn_msg = create_tiny_learn_msg(p, p->proposer.msg);
      } else {
        /* purecov: begin deadcode */
        init_learn_msg(p->proposer.msg);
        learn_msg = p->proposer.msg;
        /* purecov: end */
      }
      p->proposer.sent_learn = p->proposer.bal;
    }
    return learn_msg;
  }
}

static void do_learn(site_def const *site [[maybe_unused]], pax_machine *p,
                     pax_msg *m) {
  ADD_DBG(D_CONS, add_synode_event(p->synode);
          add_event(EVENT_DUMP_PAD, string_arg("m->from"));
          add_event(EVENT_DUMP_PAD, uint_arg(m->from));
          add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(m->op)));
          add_event(EVENT_DUMP_PAD, string_arg("proposal"));
          add_ballot_event(m->proposal);
          add_event(EVENT_DUMP_PAD, string_arg("promise"));
          add_ballot_event(p->acceptor.promise););
  /* FN; SYCEXP(p->synode); SYCEXP(m->synode); STRLIT(NEWLINE); */
  IFDBG(D_NONE, FN; SYCEXP(p->synode); SYCEXP(m->synode);
        dbg_bitset(m->receivers, get_maxnodes(site)););
  if (m->a) m->a->chosen = TRUE;
  replace_pax_msg(&p->acceptor.msg, m);
  replace_pax_msg(&p->learner.msg, m);
  /*
     Track memory used by client data in the cache.
     If we do not care about instances that are being decided,
     it is only necessary to compute the added memory when we
     record the outcome of a consensus round.
  */
  add_cache_size(p);
  /* Shrink the cache size if necessary */
  shrink_cache();
}

bool_t handle_simple_ack_prepare(site_def const *site, pax_machine *p,
                                 pax_msg *m) {
  if (get_nodeno(site) != VOID_NODE_NO)
    BIT_SET(m->from, p->proposer.prep_nodeset);

  {
    bool_t can_propose = FALSE;
    if (m->op == ack_prepare_op &&
        gt_ballot(m->proposal, p->proposer.msg->proposal)) { /* greater */
      replace_pax_msg(&p->proposer.msg, m);
      assert(p->proposer.msg);
    }
    if (gt_ballot(m->reply_to, p->proposer.sent_prop)) {
      can_propose = check_propose(site, p);
    }
    return can_propose;
  }
}

/* Other node has already accepted a value */
static void handle_ack_prepare(site_def const *site, pax_machine *p,
                               pax_msg *m) {
  ADD_DBG(D_CONS, add_synode_event(p->synode);
          add_event(EVENT_DUMP_PAD, string_arg("m->from"));
          add_event(EVENT_DUMP_PAD, uint_arg(m->from));
          add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(m->op))););
  assert(m);
  IFDBG(D_NONE, FN; if (p->proposer.msg) BALCEXP(p->proposer.msg->proposal);
        BALCEXP(p->proposer.bal); BALCEXP(m->reply_to);
        BALCEXP(p->proposer.sent_prop); SYCEXP(m->synode));
  /*
    If the node is preparing a Noop for another node's slot, it is possible
    that the leader of the slot has since proposed a value. Hence, there is
    no need to move forward if we know that the value has been accepted. This
    also prevents changing the size of a learned pax_machine, which would
    cause inconsistent reporting of memory usage in P_S.
  */
  if (finished(p)) return;

  if (m->from != VOID_NODE_NO &&
      eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my prepare */
    bool_t can_propose = handle_simple_ack_prepare(site, p, m);
    if (can_propose) send_propose_msg(p->proposer.msg);
  }
}

/* #define AUTO_MSG(p,synode) {if(!(p)){replace_pax_msg(&(p),
 * pax_msg_new(synode, site));} */

static pax_msg *create_ack_accept_msg(pax_msg *m, synode_no synode) {
  CREATE_REPLY(m);
  reply->op = ack_accept_op;
  reply->synode = synode;
  return reply;
}

pax_msg *handle_simple_accept(pax_machine *p, pax_msg *m, synode_no synode) {
  pax_msg *reply = nullptr;
  if (finished(p)) { /* We have learned a value */
    reply = create_learn_msg_for_ignorant_node(p, m, synode);
  } else if (!gt_ballot(p->acceptor.promise,
                        m->proposal) || /* Paxos acceptor phase 2 decision */
             noop_match(p, m)) {
    IFDBG(D_NONE, FN; SYCEXP(m->synode); STRLIT("accept ");
          BALCEXP(m->proposal));
    p->last_modified = task_now();
    replace_pax_msg(&p->acceptor.msg, m);
    reply = create_ack_accept_msg(m, synode);
  }
  return reply;
}

/* Accecpt value if promise is not greater */
static void handle_accept(site_def const *site, pax_machine *p,
                          linkage *reply_queue, pax_msg *m) {
  IFDBG(D_NONE, FN; BALCEXP(p->acceptor.promise); BALCEXP(m->proposal);
        STREXP(pax_msg_type_to_str(m->msg_type)));
  PAX_MSG_SANITY_CHECK(m);
  ADD_DBG(D_CONS, add_synode_event(p->synode);
          add_event(EVENT_DUMP_PAD, string_arg("m->from"));
          add_event(EVENT_DUMP_PAD, uint_arg(m->from));
          add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(m->op)));
          add_event(EVENT_DUMP_PAD, string_arg("proposal"));
          add_ballot_event(m->proposal);
          add_event(EVENT_DUMP_PAD, string_arg("promise"));
          add_ballot_event(p->acceptor.promise););

  {
    pax_msg *reply = handle_simple_accept(p, m, m->synode);
    if (reply != nullptr) {
      SEND_REPLY;
      IFDBG(D_CONS, FN; STRLIT("activating sweeper on accept of ");
            SYCEXP(m->synode));
      activate_sweeper();
    }
  }
}

/* Handle answer to accept */
pax_msg *handle_simple_ack_accept(site_def const *site, pax_machine *p,
                                  pax_msg *m) {
  pax_msg *learn_msg = nullptr;
  if (get_nodeno(site) != VOID_NODE_NO && m->from != VOID_NODE_NO &&
      eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my accept */
    BIT_SET(m->from, p->proposer.prop_nodeset);
    if (gt_ballot(m->proposal, p->proposer.sent_learn)) {
      learn_msg = check_learn(site, p);
    }
  }
  return learn_msg;
}
static void handle_ack_accept(site_def const *site, pax_machine *p,
                              pax_msg *m) {
  ADD_DBG(D_CONS, add_synode_event(p->synode);
          add_event(EVENT_DUMP_PAD, string_arg("m->from"));
          add_event(EVENT_DUMP_PAD, uint_arg(m->from));
          add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(m->op))););
  IFDBG(D_NONE, FN; SYCEXP(m->synode); BALCEXP(p->proposer.bal);
        BALCEXP(p->proposer.sent_learn); BALCEXP(m->proposal);
        BALCEXP(m->reply_to););
  IFDBG(D_NONE, FN; SYCEXP(p->synode);
        if (p->acceptor.msg) BALCEXP(p->acceptor.msg->proposal);
        BALCEXP(p->proposer.bal); BALCEXP(m->reply_to););

  {
    pax_msg *learn_msg = handle_simple_ack_accept(site, p, m);
    if (learn_msg != nullptr) {
      if (learn_msg->op == tiny_learn_op) {
        send_tiny_learn_msg(site, learn_msg);
      } else {
        /* purecov: begin deadcode */
        assert(learn_msg->op == learn_op);
        send_learn_msg(site, learn_msg);
        /* purecov: end */
      }
    }
  }
}

/* Handle incoming learn. */
static void activate_sweeper();
void handle_tiny_learn(site_def const *site, pax_machine *pm, pax_msg *p) {
  assert(p->msg_type != no_op);
  if (pm->acceptor.msg) {
    /* 			BALCEXP(pm->acceptor.msg->proposal); */
    if (eq_ballot(pm->acceptor.msg->proposal, p->proposal)) {
      pm->acceptor.msg->op = learn_op;
      pm->last_modified = task_now();
      update_max_synode(p);
      paxos_fsm(pm, site, paxos_learn, p);
      handle_learn(site, pm, pm->acceptor.msg);
    } else {
      send_read(p->synode);
      IFDBG(D_NONE, FN; STRLIT("tiny_learn"); SYCEXP(p->synode);
            BALCEXP(pm->acceptor.msg->proposal); BALCEXP(p->proposal));
    }
  } else {
    send_read(p->synode);
    IFDBG(D_NONE, FN; STRLIT("tiny_learn"); SYCEXP(p->synode);
          BALCEXP(p->proposal));
  }
}

static void force_pax_machine(pax_machine *p, int enforcer) {
  if (!p->enforcer) { /* Not if already marked as forcing node */
    if (enforcer) {   /* Only if forcing node */
      /* Increase ballot count with a large increment without overflowing */
      /* p->proposer.bal.cnt may be -1. */
      int32_t delta = (INT32_MAX - MAX(p->proposer.bal.cnt, 0)) / 3;
      p->proposer.bal.cnt += delta;
    }
  }
  p->force_delivery = 1;
  p->enforcer = enforcer;
}

/* Configure all messages in interval start, end to be forced */
static void force_interval(synode_no start, synode_no end, int enforcer) {
  while (!synode_gt(start, end)) {
    pax_machine *p = get_cache(start);
    if (get_nodeno(find_site_def(start)) == VOID_NODE_NO) break;

    /* The forcing node will call force_interval twice, first when
    the new config is originally installed, and again when it
    receives it as an xcom message. start may be the same, but
    end will be greater the second time, since it is calculated
    based on the message number of the incoming config. Since the forcing
    node is the one responsible for delivering all messages until the
    start of the new site, it is important that all instances belonging to
    the old site are correctly marked. */

    if (p->enforcer) enforcer = 1; /* Extend to new instances */
    force_pax_machine(p, enforcer);

    /* Old nodesets are null and void */
    BIT_ZERO(p->proposer.prep_nodeset);
    BIT_ZERO(p->proposer.prop_nodeset);
    start = incr_synode(start);
  }
}

static void start_force_config(site_def *s, int enforcer) {
  synode_no end = add_event_horizon(s->boot_key);

  IFDBG(D_NONE, FN; SYCEXP(executed_msg); SYCEXP(end));
  if (synode_gt(end, max_synode)) set_max_synode(end);

  free_forced_config_site_def();
  wait_forced_config = 0;
  forced_config = s;
  force_interval(executed_msg, max_synode,
                 enforcer); /* Force everything in the pipeline */
}

/* Learn this value */
void handle_learn(site_def const *site, pax_machine *p, pax_msg *m) {
  IFDBG(D_NONE, FN; STRLIT("proposer nodeset ");
        dbg_bitset(p->proposer.prop_nodeset, get_maxnodes(site)););
  IFDBG(D_NONE, FN; STRLIT("receivers ");
        dbg_bitset(m->receivers, get_maxnodes(site)););
  IFDBG(D_NONE, FN; NDBG(task_now(), f); SYCEXP(p->synode);
        COPY_AND_FREE_GOUT(dbg_app_data(m->a)););

  PAX_MSG_SANITY_CHECK(m);
  p->last_modified = task_now();
  if (!finished(p)) { /* Avoid re-learn */
    activate_sweeper();
    do_learn(site, p, m);
    /* Check for special messages */
    if (m->a && m->a->body.c_t == unified_boot_type) {
      IFDBG(D_NONE, FN; STRLIT("Got unified_boot "); SYCEXP(p->synode);
            SYCEXP(m->synode););
      XCOM_FSM(x_fsm_net_boot, void_arg(m->a));
    }
    /* See if someone is forcing a new config */
    if (m->force_delivery && m->a) {
      IFDBG(D_NONE, FN; STRLIT("Got forced config "); SYCEXP(p->synode);
            SYCEXP(m->synode););
      /* Configure all messages from executed_msg until start of new config
         as forced messages so they will eventually be finished */
      /* Immediately install this new config */
      switch (m->a->body.c_t) {
        case add_node_type:
          /* purecov: begin deadcode */
          if (should_ignore_forced_config_or_view(
                  find_site_def(p->synode)->x_proto)) {
            log_ignored_forced_config(m->a, "handle_learn");
          } else {
            site_def *new_def = handle_add_node(m->a);
            if (new_def) start_force_config(clone_site_def(new_def), 0);
          }
          break;
        /* purecov: end */
        case remove_node_type:
          /* purecov: begin deadcode */
          if (should_ignore_forced_config_or_view(
                  find_site_def(p->synode)->x_proto)) {
            log_ignored_forced_config(m->a, "handle_learn");
          } else {
            start_force_config(clone_site_def(handle_remove_node(m->a)), 0);
          }
          break;
        /* purecov: end */
        case force_config_type:
          start_force_config(clone_site_def(install_node_group(m->a)), 0);
          break;
        default:
          break;
      }
    }
  }

  task_wakeup(&p->rv);
}

/* Skip this value */
static void handle_skip(site_def const *site, pax_machine *p, pax_msg *m) {
  /* IFDBG(D_NONE, FN;); */
  /* IFDBG(D_NONE, FN; NDBG(task_now(),f); SYCEXP(p->msg->synode)); */
  if (!finished(p)) {
    p->last_modified = task_now();
    skip_value(m);
    do_learn(site, p, m);
  }
  /* IFDBG(D_NONE, FN; STRLIT("taskwakeup "); SYCEXP(p->msg->synode)); */
  task_wakeup(&p->rv);
}

static void handle_client_msg(pax_msg *p) {
  if (!p || p->a == nullptr) /* discard invalid message */
    return;
  {
    msg_link *ml = msg_link_new(p, VOID_NODE_NO);

    /* Put it in the proposer queue */
    ADD_T_EV(task_now(), __FILE__, __LINE__, "handle_client_msg");
    channel_put(&prop_input_queue, &ml->l);
  }
}

#ifdef ACCEPT_SITE_TEST
/* See if we should process an incoming ping from a node.
   The purpose is to avoid doing recovery from a node with an obsolete site
   definition */
static int accept_site(site_def const *site) {
  site_def *mysite = (site_def *)get_site_def();

  if (site) {
    if (!mysite) {
      site_def *prev = (site_def *)find_prev_site_def(site->boot_key);
      IFDBG(
          D_NONE, FN; PTREXP(site); PTREXP(mysite); PTREXP(prev);
          SYCEXP(site->boot_key); if (prev) { SYCEXP(prev->boot_key); });
      if (!prev) {
        /** alive when no site, no known previous definition, and present in
         * new is accepted */
        return (site->boot_key.group_id == 0
                    ? 1
                    : (xcom_find_node_index((node_list *)&site->nodes) !=
                       VOID_NODE_NO));
      } else {
        /** alive when no site, a previous definition of groupid is known, but
         * is older than site def, is accepted */
        return synode_gt(site->boot_key, prev->boot_key);
      }
    } else {
      IFDBG(D_NONE, FN; PTREXP(site); PTREXP(mysite); SYCEXP(site->boot_key);
            SYCEXP(mysite->boot_key));
      if (get_group_id(site) != get_group_id(mysite)) {
        /** alive from different site should never be accepted */
        return 0;
      } else {
        /** alive from same site should be accepted if boot_key is larger than
         * mine */
        node_no my_nodeno = xcom_find_node_index((node_list *)&mysite->nodes);
        node_no site_nodeno = xcom_find_node_index((node_list *)&site->nodes);
        return (synode_gt(site->boot_key, mysite->boot_key) &&
                ((my_nodeno != VOID_NODE_NO) || (site_nodeno != VOID_NODE_NO)));
      }
    }
  }
  /** Always accept a NULL site */
  IFDBG(D_NONE, FN; PTREXP(site));
  return 1;
}
#endif

/* Handle incoming "need boot" message. */
/* purecov: begin deadcode */
static inline void handle_boot(site_def const *site, linkage *reply_queue,
                               pax_msg *p) {
  /* This should never be TRUE, but validate it instead of asserting. */
  if (site == nullptr || site->nodes.node_list_len < 1) {
    G_DEBUG(
        "handle_boot: Received an unexpected need_boot_op when site == NULL "
        "or "
        "site->nodes.node_list_len < 1");
    return;
  }

  if (ALWAYS_HANDLE_NEED_BOOT || should_handle_need_boot(site, p)) {
    handle_need_snapshot(reply_queue, p);
  } else {
    G_DEBUG(
        "Ignoring a need_boot_op message from an XCom incarnation that does "
        "not belong to the group.");
  }
}
/* purecov: end */

bool_t should_handle_need_boot(site_def const *site, pax_msg *p) {
  bool_t should_handle = FALSE;
  bool_t const sender_advertises_identity =
      (p->a != nullptr && p->a->body.c_t == xcom_boot_type);

  /*
   If the message advertises the sender's identity, check if it matches the
   membership information.

   The sender's identity may not match if, e.g.:

     a. The member was already removed, or
     b. It is a new incarnation of a crashed member that is yet to be removed.

   ...or some other reason.

   If it is due to reason (b), we do not want to boot the sender because XCom
   only implements a simple fail-stop model. Allowing the sender to rejoin the
   group without going through the full remove+add node path could violate
   safety because the sender does not remember any previous Paxos acceptances
   it acknowledged before crashing. Since the pre-crash incarnation may have
   accepted a value for a given synod but the post-crash incarnation has
   forgotten that fact, the post-crash incarnation will fail to propagate the
   previously accepted value to a higher ballot. Since majorities can overlap
   on a single node, if the overlap node is the post-crash incarnation which
   has forgotten about the previously accepted value, a higher ballot proposer
   may get a different value accepted, leading to conflicting values to be
   accepted for different proposers, which is a violation of the safety
   properties of the Paxos protocol.

   If the sender does not advertise its identity, we boot it unconditionally.
   This is for backwards compatibility.
  */
  if (sender_advertises_identity) {
    bool_t const sender_advertises_one_identity =
        (p->a->body.app_u_u.nodes.node_list_len == 1);

    /* Defensively accept only messages with a single identity. */
    if (sender_advertises_one_identity) {
      node_address *sender_identity = p->a->body.app_u_u.nodes.node_list_val;

      should_handle = node_exists_with_uid(sender_identity, &site->nodes);
    }
  } else {
    should_handle = TRUE;
  }

  return should_handle;
}

void init_need_boot_op(pax_msg *p, node_address *identity) {
  p->op = need_boot_op;
  if (identity != nullptr) {
    p->a = new_app_data();
    p->a->body.c_t = xcom_boot_type;
    init_node_list(1, identity, &p->a->body.app_u_u.nodes);
  }
}

#define PING_GATHERING_TIME_WINDOW 5.0
#define PINGS_GATHERED_BEFORE_CONNECTION_SHUTDOWN 3

int pre_process_incoming_ping(site_def const *site, pax_msg const *pm,
                              int has_client_already_booted,
                              double current_time) {
  // Yes... it is a ping for me, boot is done and it is a are_you_alive_op
  // This means that something wrong is not right...
  int did_shutdown = 0;

  if ((pm->from != get_nodeno(site)) && has_client_already_booted &&
      (pm->op == are_you_alive_op)) {
    G_DEBUG(
        "Received a ping to myself. This means that something must be wrong "
        "in "
        "a bi-directional connection")
    // Going to kill the connection for that node...
    if (site && (pm->from < site->nodes.node_list_len)) {
      // This is not the first ping received in the last 5 seconds...
      if (site->servers[pm->from]->last_ping_received >
          (current_time - PING_GATHERING_TIME_WINDOW)) {
        site->servers[pm->from]->number_of_pings_received++;
      } else {  // First ping since at least more than 5 seconds...
        site->servers[pm->from]->number_of_pings_received = 1;
      }

      site->servers[pm->from]->last_ping_received = current_time;

      // If we keep on receiving periodical pings... lets kill the connection
      if (is_connected(site->servers[pm->from]->con) &&
          site->servers[pm->from]->number_of_pings_received ==
              PINGS_GATHERED_BEFORE_CONNECTION_SHUTDOWN) {
        shutdown_connection(site->servers[pm->from]->con);
        G_WARNING(
            "Shutting down an outgoing connection. This happens because "
            "something might be wrong on a bi-directional connection to node "
            "%s:%d. Please check the connection status to this member",
            site->servers[pm->from]->srv, site->servers[pm->from]->port);
        did_shutdown = 1;
      }
    }
  }

  return did_shutdown;
}

/* Handle incoming alive message */
static double sent_alive = 0.0;
static inline void handle_alive(site_def const *site, linkage *reply_queue,
                                pax_msg *pm) {
  pre_process_incoming_ping(site, pm, client_boot_done, task_now());

  if (client_boot_done || !(task_now() - sent_alive > 1.0)) /* Already done? */
    return;

#ifdef ACCEPT_SITE_TEST
  if (!accept_site(site)) return;
#endif

  /* Avoid responding to own ping */
  if (pm->from == get_nodeno(site) || pm->from == pm->to) return;

  /*
   This code will check if the ping is intended to us.
   If the encoded node does not exist in the current configuration,
   we avoid sending need_boot_op, since it must be from a different
   reincarnation of this node.
   */
  if (site && pm->a && pm->a->body.c_t == xcom_boot_type) {
    IFDBG(D_NONE, FN;
          COPY_AND_FREE_GOUT(dbg_list(&pm->a->body.app_u_u.nodes)););

    if (!node_exists_with_uid(&pm->a->body.app_u_u.nodes.node_list_val[0],
                              &get_site_def()->nodes))
      return;
  }

  if (is_dead_site(pm->group_id)) return; /* Avoid dealing with zombies */

  {
    CREATE_REPLY(pm);
    init_need_boot_op(reply, cfg_app_xcom_get_identity());
    sent_alive = task_now();
    G_INFO(
        "Node has not booted. Requesting an XCom snapshot from node number %d "
        "in the current configuration",
        pm->from);
    SEND_REPLY;
  }
  IFDBG(D_NONE, FN; STRLIT("sent need_boot_op"););
}

static void update_max_synode(pax_msg *p) {
  if (is_dead_site(p->group_id)) return;
  if (get_group_id(get_site_def()) == 0 || max_synode.group_id == 0) {
    set_max_synode(p->synode);
  } else if (max_synode.group_id == p->synode.group_id) {
    if (synode_gt(p->synode, max_synode)) {
      set_max_synode(p->synode);
    }
    if (synode_gt(p->max_synode, max_synode)) {
      set_max_synode(p->max_synode);
    }
  }
}

/* Message dispatch */
#define BAL_FMT "ballot {cnt %d node %d}"
#define BAL_MEM(x) (x).cnt, (x).node

static int clicnt = 0;

xcom_event_horizon xcom_get_minimum_event_horizon() {
  return EVENT_HORIZON_MIN;
}

xcom_event_horizon xcom_get_maximum_event_horizon() {
  return EVENT_HORIZON_MAX;
}

/**
 * Retrieves the latest event horizon.
 *
 * There is no specific reason for this method to return the latest event
 * horizon instead of the current one. Both would be acceptable results of
 * this function, but we had to make a decision of one over the other.
 *
 * @param[out] event_horizon the latest event horizon
 * @retval REQUEST_FAIL XCom is not initialized yet
 * @retval REQUEST_OK function was successful and event_horizon contains the
 *                    latest event horizon
 */
static client_reply_code xcom_get_event_horizon(
    xcom_event_horizon *event_horizon) {
  site_def const *latest_config = get_site_def();
  if (latest_config == nullptr) return REQUEST_FAIL;
  *event_horizon = latest_config->event_horizon;
  return REQUEST_OK;
}

static u_int allow_add_node(app_data_ptr a) {
  /* Get information on the current site definition */
  const site_def *new_site_def = get_site_def();
  const site_def *valid_site_def = find_site_def(executed_msg);

  /* Get information on the nodes to be added */
  u_int nr_nodes_to_add = a->body.app_u_u.nodes.node_list_len;
  node_address *nodes_to_change = a->body.app_u_u.nodes.node_list_val;

  if (check_if_add_node_is_unsafe_against_event_horizon(a)) return 0;

  if (unsafe_leaders(a)) {
    return 0;
  }

  if (add_node_unsafe_against_ipv4_old_nodes(a)) {
    G_MESSAGE(
        "This server is unable to join the group as the NIC used is "
        "configured "
        "with IPv6 only and there are members in the group that are unable "
        "to "
        "communicate using IPv6, only IPv4.Please configure this server to "
        "join the group using an IPv4 address instead.");
    return 0;
  }

  {
    u_int i;
    for (i = 0; i < nr_nodes_to_add; i++) {
      if (node_exists(&nodes_to_change[i], &new_site_def->nodes) ||
          node_exists(&nodes_to_change[i], &valid_site_def->nodes)) {
        /*
        We are simply ignoring the attempt to add a node to the
        group when there is an old incarnation of it, meaning
        that the node has crashed and restarted so fastly that
        nobody has noticed that it has gone.

        In XCOM, the group is not automatically reconfigured
        and it is possible to start reusing a node that has
        crashed and restarted without reconfiguring the group
        by adding the node back to it.

        However, this operation may be unsafe because XCOM
        does not implement a crash-recovery model and nodes
        suffer from amnesia after restarting the service. In
        other words this may lead to inconsistency issues in
        the paxos protocol.

        Unfortunately, preventing that a node is added back
        to the system where there is an old incarnation will
        not fix this problem since other changes are required.
        */
        G_WARNING(
            "Old incarnation found while trying to "
            "add node %s %.*s. Please stop the old node or wait for it to "
            "leave the group.",
            nodes_to_change[i].address, nodes_to_change[i].uuid.data.data_len,
            nodes_to_change[i].uuid.data.data_val);
        return 0;
      }
    }
  }

  return 1;
}

static u_int allow_remove_node(app_data_ptr a) {
  /* Get information on the current site definition */
  const site_def *new_site_def = get_site_def();

  /* Get information on the nodes to be added */
  u_int nodes_len = a->body.app_u_u.nodes.node_list_len;
  node_address *nodes_to_change = a->body.app_u_u.nodes.node_list_val;

  u_int i;
  for (i = 0; i < nodes_len; i++) {
    if (!node_exists_with_uid(&nodes_to_change[i], &new_site_def->nodes)) {
      /*
      If the UID does not exist, then 1) the node has already been
      removed or 2) it has reincarnated.
      */
      /* purecov: begin inspected */
      if (node_exists(&nodes_to_change[i], &new_site_def->nodes)) {
        /*
        We also cannot allow an upper-layer to remove a new incarnation
        of a node when it tries to remove an old one.
        */
        G_MESSAGE(
            "New incarnation found while trying to "
            "remove node %s %.*s.",
            nodes_to_change[i].address, nodes_to_change[i].uuid.data.data_len,
            nodes_to_change[i].uuid.data.data_val);
      } else {
        /* The node has already been removed, so we block the request */
        G_MESSAGE(
            "Node has already been removed: "
            "%s %.*s.",
            nodes_to_change[i].address, nodes_to_change[i].uuid.data.data_len,
            nodes_to_change[i].uuid.data.data_val);
      }
      return 0;
      /* purecov: end */
    }
  }

  return 1;
}

/**
 * Logs the fact that an add/remove node request is aimed at another group.
 *
 * @param a a pointer to the app_data of the configuration command
 * @param message_fmt a formatted message to log, containing a single %s that
 * will be replaced by the node's address
 */
static void log_cfgchange_wrong_group(app_data_ptr a,
                                      const char *const message_fmt) {
  u_int const nr_nodes = a->body.app_u_u.nodes.node_list_len;
  u_int i;
  for (i = 0; i < nr_nodes; i++) {
    char const *const address = a->body.app_u_u.nodes.node_list_val[i].address;
    G_WARNING(message_fmt, address);
  }
}

/**
 * Validates if a configuration command can be executed.
 * Checks whether the configuration command is aimed at the correct group.
 * Checks whether the configuration command pertains to a node reincarnation.
 *
 * @param p a pointer to the pax_msg of the configuration command
 * @retval REQUEST_OK if the reconfiguration command can be executed
 * @retval REQUEST_RETRY if XCom is still booting
 * @retval REQUEST_FAIL if the configuration command cannot be executed
 */
static client_reply_code can_execute_cfgchange(pax_msg *p) {
  app_data_ptr a = p->a;

  if (executed_msg.msgno <= 2) {
    // If we have not booted and we receive an add_node that contains us...
    if (add_node_adding_own_address(a))
      return REQUEST_FAIL;
    else {
      /*G_INFO(
          "Configuration change failed. Request on a node that has not booted "
          "yet.");*/
      G_INFO(
          "This node received a Configuration change request, but it not yet "
          "started. This could happen if one starts several nodes "
          "simultaneously. This request will be retried by whoever sent it.");
      return REQUEST_RETRY;
    }
  }

  if (a && a->group_id != 0 && a->group_id != executed_msg.group_id) {
    switch (a->body.c_t) {
      case add_node_type:
        log_cfgchange_wrong_group(
            a,
            "The request to add %s to the group has been rejected because it "
            "is aimed at another group");
        break;
      case remove_node_type:
        log_cfgchange_wrong_group(a,
                                  "The request to remove %s from the group "
                                  "has been rejected because "
                                  "it is aimed at another group");
        break;
      case force_config_type:
        G_WARNING(
            "The request to force the group membership has been rejected "
            "because it is aimed at another group");
        break;
      case set_max_leaders:
        G_WARNING(
            "The request to change max number of leaders has been rejected "
            "because it is aimed at another group");
        break;
      case set_leaders_type:
        G_WARNING(
            "The request to change leaders has been rejected "
            "because it is aimed at another group");
        break;
      default:
        assert(0 &&
               "A cargo_type different from {add_node_type, remove_node_type, "
               "force_config_type, set_max_leaders, set_leaders_type} should "
               "not "
               "have hit this code path");
    }
    return REQUEST_FAIL;
  }

  if (a && a->body.c_t == add_node_type && !allow_add_node(a))
    return REQUEST_FAIL;

  if (a && a->body.c_t == remove_node_type && !allow_remove_node(a))
    return REQUEST_FAIL;

  if (a && a->body.c_t == set_event_horizon_type &&
      is_unsafe_event_horizon_reconfiguration(a))
    return REQUEST_FAIL;

  if (a && a->body.c_t == force_config_type &&
      are_there_dead_nodes_in_new_config(a))
    return REQUEST_FAIL;

  if (a &&
      (a->body.c_t == set_max_leaders || a->body.c_t == set_leaders_type) &&
      is_unsafe_leaders_reconfiguration(a))
    return REQUEST_FAIL;

  return REQUEST_OK;
}

static void activate_sweeper() {
  if (sweeper) {
    ADD_DBG(D_CONS, add_event(EVENT_DUMP_PAD,
                              string_arg("sweeper activated max_synode"));
            add_synode_event(max_synode););
    task_activate(sweeper);
  }
}

static synode_no start_config = NULL_SYNODE;

void dispatch_get_event_horizon(site_def const *site, pax_msg *p,
                                linkage *reply_queue) {
  CREATE_REPLY(p);
  IFDBG(D_NONE, FN; STRLIT("Got get_event_horizon from client");
        SYCEXP(p->synode););
  reply->op = xcom_client_reply;
  reply->cli_err = xcom_get_event_horizon(&reply->event_horizon);
  SEND_REPLY;
}

static reply_data *new_leader_info(site_def *site) {
  if (site) {
    reply_data *data =
        static_cast<reply_data *>(xcom_calloc((size_t)1, sizeof(reply_data)));
    data->rt = leader_info;
    data->reply_data_u.leaders.max_nr_leaders = site->max_active_leaders;
    if (leaders_set_by_client(site)) {
      data->reply_data_u.leaders.preferred_leaders =
          clone_leader_array(site->leaders);
    }
    active_leaders(site, &data->reply_data_u.leaders.actual_leaders);
    return data;
  } else {
    return nullptr;
  }
}

void dispatch_get_leaders(site_def *site, pax_msg *p, linkage *reply_queue) {
  CREATE_REPLY(p);
  IFDBG(D_NONE, FN; STRLIT("Got get_leaders from client"); SYCEXP(p->synode););
  reply->op = xcom_client_reply;
  reply->rd = new_leader_info(site);
  reply->cli_err = reply->rd ? REQUEST_OK : REQUEST_FAIL;
  SEND_REPLY;
}

/*
 * Log the result of the get_synode_app_data command.
 */
static void log_get_synode_app_data_failure(
    xcom_get_synode_app_data_result error_code) {
  switch (error_code) {
    case XCOM_GET_SYNODE_APP_DATA_OK:
      break;
    case XCOM_GET_SYNODE_APP_DATA_ERROR:
      G_DEBUG("Could not reply successfully to request for synode data.");
      break;
    case XCOM_GET_SYNODE_APP_DATA_NOT_CACHED:
      G_DEBUG(
          "Could not reply successfully to request for synode data because "
          "some of the requested synodes are no longer cached.");
      break;
    case XCOM_GET_SYNODE_APP_DATA_NOT_DECIDED:
      G_DEBUG(
          "Could not reply successfully to request for synode data because "
          "some of the requested synodes are still undecided.");
      break;
    case XCOM_GET_SYNODE_APP_DATA_NO_MEMORY:
      G_DEBUG(
          "Could not reply successfully to request for synode data because "
          "memory could not be allocated.");
      break;
  }
}

void dispatch_get_synode_app_data(site_def const *site, pax_msg *p,
                                  linkage *reply_queue) {
  IFDBG(D_NONE, FN; STRLIT("Got get_synode_app_data from client");
        SYCEXP(p->synode););

  {
    CREATE_REPLY(p);
    reply->op = xcom_client_reply;

    {
      xcom_get_synode_app_data_result error_code;
      error_code = xcom_get_synode_app_data(&p->a->body.app_u_u.synodes,
                                            &reply->requested_synode_app_data);
      switch (error_code) {
        case XCOM_GET_SYNODE_APP_DATA_OK:
          reply->cli_err = REQUEST_OK;
          break;
        case XCOM_GET_SYNODE_APP_DATA_NOT_CACHED:
        case XCOM_GET_SYNODE_APP_DATA_NOT_DECIDED:
        case XCOM_GET_SYNODE_APP_DATA_NO_MEMORY:
        case XCOM_GET_SYNODE_APP_DATA_ERROR:
          reply->cli_err = REQUEST_FAIL;
          log_get_synode_app_data_failure(error_code);
          break;
      }

      SEND_REPLY;
    }
  }
}

static int can_send_snapshot();

static void process_client_msg(site_def const *site, pax_msg *p,
                               linkage *reply_queue) {
  clicnt++;
  if (p->a && (p->a->body.c_t == exit_type)) {
    /* purecov: begin deadcode */
    IFDBG(D_NONE, FN; STRLIT("Got exit from client"); SYCEXP(p->synode););
    bury_site(get_group_id(get_site_def()));
    ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
    terminate_and_exit();
    return;
    /* purecov: end */
  }

  if (p->a && (p->a->body.c_t == reset_type)) {
    /* purecov: begin deadcode */
    IFDBG(D_NONE, FN; STRLIT("Got reset from client"); SYCEXP(p->synode););
    bury_site(get_group_id(get_site_def()));
    ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
    XCOM_FSM(x_fsm_terminate, int_arg(0));
    return;
    /* purecov: end */
  }
  if (p->a && (p->a->body.c_t == remove_reset_type)) {
    /* purecov: begin deadcode */
    IFDBG(D_NONE, FN; STRLIT("Got remove_reset from client");
          SYCEXP(p->synode););
    ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
    XCOM_FSM(x_fsm_terminate, int_arg(0));
    return;
    /* purecov: end */
  }
  if (p->a && (p->a->body.c_t == enable_arbitrator)) {
    CREATE_REPLY(p);
    IFDBG(D_NONE, FN; STRLIT("Got enable_arbitrator from client");
          SYCEXP(p->synode););
    ARBITRATOR_HACK = 1;
    reply->op = xcom_client_reply;
    reply->cli_err = REQUEST_OK;
    SEND_REPLY;
    return;
  }
  if (p->a && (p->a->body.c_t == disable_arbitrator)) {
    CREATE_REPLY(p);
    IFDBG(D_NONE, FN; STRLIT("Got disable_arbitrator from client");
          SYCEXP(p->synode););
    ARBITRATOR_HACK = 0;
    reply->op = xcom_client_reply;
    reply->cli_err = REQUEST_OK;
    SEND_REPLY;
    return;
  }
  if (p->a && (p->a->body.c_t == set_cache_limit)) {
    CREATE_REPLY(p);
    IFDBG(D_NONE, FN; STRLIT("Got set_cache_limit from client");
          SYCEXP(p->synode););
    if (the_app_xcom_cfg) {
      set_max_cache_size(p->a->body.app_u_u.cache_limit);
      reply->cli_err = REQUEST_OK;
    } else {
      reply->cli_err = REQUEST_FAIL;
    }
    reply->op = xcom_client_reply;
    SEND_REPLY;
    return;
  }
  if (p->a && (p->a->body.c_t == x_terminate_and_exit)) {
    /* purecov: begin deadcode */
    CREATE_REPLY(p);
    IFDBG(D_NONE, FN; STRLIT("Got terminate_and_exit from client");
          SYCEXP(p->synode););
    reply->op = xcom_client_reply;
    reply->cli_err = REQUEST_OK;
    SEND_REPLY;
    /*
      The function frees sites which is used by SEND_REPLY,
      so it should be called after SEND_REPLY.
    */
    IFDBG(D_NONE, FN; STRLIT("terminate_and_exit"));
    ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
    terminate_and_exit();
    return;
    /* purecov: end */
  }
  if (p->a && (p->a->body.c_t == get_event_horizon_type)) {
    dispatch_get_event_horizon(get_site_def(), p, reply_queue);
    return;
  }
  if (p->a && (p->a->body.c_t == get_synode_app_data_type)) {
    dispatch_get_synode_app_data(get_site_def(), p, reply_queue);
    return;
  }
  if (p->a && (p->a->body.c_t == get_leaders_type)) {
    dispatch_get_leaders(get_site_def_rw(), p, reply_queue);
    return;
  }
  if (p->a &&
      (p->a->body.c_t == add_node_type || p->a->body.c_t == remove_node_type ||
       p->a->body.c_t == force_config_type ||
       p->a->body.c_t == set_event_horizon_type ||
       p->a->body.c_t == set_max_leaders ||
       p->a->body.c_t == set_leaders_type)) {
    client_reply_code cli_err;
    CREATE_REPLY(p);
    reply->op = xcom_client_reply;
    reply->cli_err = cli_err = can_execute_cfgchange(p);
    SEND_REPLY;
    if (cli_err != REQUEST_OK) {
      return;
    }
  }
  if (p->a && p->a->body.c_t == unified_boot_type) {
    IFDBG(D_NONE, FN; STRLIT("Got unified_boot from client");
          SYCEXP(p->synode););
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    XCOM_FSM(x_fsm_net_boot, void_arg(p->a));
  }
  if (p->a && p->a->body.c_t == add_node_type) {
    IFDBG(D_NONE, FN; STRLIT("Got add_node from client"); SYCEXP(p->synode););
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    assert(get_site_def());
  }
  if (p->a && p->a->body.c_t == remove_node_type) {
    IFDBG(D_NONE, FN; STRLIT("Got remove_node from client");
          SYCEXP(p->synode););
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    assert(get_site_def());
  }
  if (p->a && p->a->body.c_t == set_event_horizon_type) {
    IFDBG(D_NONE, FN; STRLIT("Got set_event_horizon from client");
          SYCEXP(p->synode););
    IFDBG(D_NONE, FN; NDBG(p->a->body.app_u_u.event_horizon, u));
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    assert(get_site_def());
  }
  if (p->a && p->a->body.c_t == force_config_type) {
    IFDBG(D_NONE, FN; STRLIT("Got new force config from client");
          SYCEXP(p->synode););
    IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    assert(get_site_def());
    XCOM_FSM(x_fsm_force_config, void_arg(p->a));
  }
  if (p->a && p->a->body.c_t == set_max_leaders) {
    IFDBG(D_NONE, FN; STRLIT("Got set_max_leaders from client");
          SYCEXP(p->synode););
    IFDBG(D_NONE, FN; NDBG(p->a->body.app_u_u.max_leaders, u));
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    assert(get_site_def());
  }
  if (p->a && p->a->body.c_t == set_leaders_type) {
    IFDBG(D_NONE, FN; STRLIT("Got set_leaders_type from client");
          SYCEXP(p->synode););
    IFDBG(D_NONE, STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
    assert(get_site_def());
  }
  handle_client_msg(p);
}

static void process_prepare_op(site_def const *site, pax_msg *p,
                               linkage *reply_queue) {
  pax_machine *pm = get_cache(p->synode);
  assert(pm);
  if (p->force_delivery) pm->force_delivery = 1;
  IFDBG(D_NONE, FN; dbg_pax_msg(p));

  /*
   We can only be a productive Paxos Acceptor if we have been booted, i.e.
   added to the group and received an up-to-date snapshot from some member.

   We do not allow non-booted members to participate in Paxos because they
   might be a reincarnation of a member that crashed and was then brought up
   without having gone through the remove+add node path.
   Since the pre-crash incarnation may have accepted a value for a given
   synod but the post-crash incarnation has forgotten that fact, the
   post-crash incarnation will fail to propagate the previously accepted
   value to a higher ballot. Since majorities can overlap on a single node,
   if the overlap node is the post-crash incarnation which has forgotten
   about the previously accepted value, the higher ballot proposer may get
   a different value accepted, leading to conflicting values to be accepted
   for different proposers, which is a violation of the safety requirements
   of the Paxos protocol.
  */
  if (ALWAYS_HANDLE_CONSENSUS || client_boot_done) {
    paxos_fsm(pm, site, paxos_prepare, p);
    handle_prepare(site, pm, reply_queue, p);
  }
}

static inline int abort_processing(pax_msg *p) {
  /* Ensure that forced message can be processed */
  return (!p->force_delivery && too_far(p->synode)) || !is_cached(p->synode);
}

static void process_ack_prepare_op(site_def const *site, pax_msg *p,
                                   linkage *reply_queue) {
  (void)reply_queue;
  if (abort_processing(p))
    return;
  else {
    pax_machine *pm = get_cache(p->synode);
    if (p->force_delivery) pm->force_delivery = 1;
    if (!pm->proposer.msg) return;
    assert(pm && pm->proposer.msg);
    handle_ack_prepare(site, pm, p);
    paxos_fsm(pm, site, paxos_ack_prepare, p);
  }
}

static void process_accept_op(site_def const *site, pax_msg *p,
                              linkage *reply_queue) {
  pax_machine *pm = get_cache(p->synode);
  assert(pm);
  if (p->force_delivery) pm->force_delivery = 1;
  IFDBG(D_NONE, FN; dbg_pax_msg(p));

  /*
   We can only be a productive Paxos Acceptor if we have been booted, i.e.
   added to the group and received an up-to-date snapshot from some member.

   We do not allow non-booted members to participate in Paxos because they
   might be a reincarnation of a member that crashed and was then brought up
   without having gone through the remove+add node path.
   Since the pre-crash incarnation may have accepted a value for a given
   synod but the post-crash incarnation has forgotten that fact, the
   post-crash incarnation will fail to propagate the previously accepted
   value to a higher ballot. Since majorities can overlap on a single node,
   if the overlap node is the post-crash incarnation which has forgotten
   about the previously accepted value, the higher ballot proposer may get
   a different value accepted, leading to conflicting values to be accepted
   for different proposers, which is a violation of the safety requirements
   of the Paxos protocol.
  */
  if (ALWAYS_HANDLE_CONSENSUS || client_boot_done) {
    handle_alive(site, reply_queue, p);

    paxos_fsm(pm, site, paxos_accept, p);
    handle_accept(site, pm, reply_queue, p);
  }
}

static void process_ack_accept_op(site_def const *site, pax_msg *p,
                                  linkage *reply_queue) {
  (void)reply_queue;
  if (too_far(p->synode))
    return;
  else {
    pax_machine *pm = get_cache(p->synode);
    if (p->force_delivery) pm->force_delivery = 1;
    if (!pm->proposer.msg) return;
    assert(pm && pm->proposer.msg);
    handle_ack_accept(site, pm, p);
    paxos_fsm(pm, site, paxos_ack_accept, p);
  }
}

static void process_learn_op(site_def const *site, pax_msg *p,
                             linkage *reply_queue) {
  pax_machine *pm = get_cache(p->synode);
  assert(pm);
  (void)reply_queue;
  if (p->force_delivery) pm->force_delivery = 1;
  update_max_synode(p);
  paxos_fsm(pm, site, paxos_learn, p);
  handle_learn(site, pm, p);
}

static void process_recover_learn_op(site_def const *site, pax_msg *p,
                                     linkage *reply_queue) {
  pax_machine *pm = get_cache(p->synode);
  assert(pm);
  (void)reply_queue;
  IFDBG(D_NONE, FN; STRLIT("recover_learn_op receive "); SYCEXP(p->synode));
  if (p->force_delivery) pm->force_delivery = 1;
  update_max_synode(p);
  {
    IFDBG(D_NONE, FN; STRLIT("recover_learn_op learn "); SYCEXP(p->synode));
    p->op = learn_op;
    paxos_fsm(pm, site, paxos_learn, p);
    handle_learn(site, pm, p);
  }
}

static void process_skip_op(site_def const *site, pax_msg *p,
                            linkage *reply_queue) {
  (void)reply_queue;
  pax_machine *pm = get_cache(p->synode);
  assert(pm);
  if (p->force_delivery) pm->force_delivery = 1;
  paxos_fsm(pm, site, paxos_learn, p);
  handle_skip(site, pm, p);
}

static void process_i_am_alive_op(site_def const *site, pax_msg *p,
                                  linkage *reply_queue) {
  /* Update max_synode, but use only p->max_synode, ignore p->synode */
  if (!is_dead_site(p->group_id)) {
    if (max_synode.group_id == p->synode.group_id &&
        synode_gt(p->max_synode, max_synode)) {
      set_max_synode(p->max_synode);
    }
  }
  handle_alive(site, reply_queue, p);
}

static void process_are_you_alive_op(site_def const *site, pax_msg *p,
                                     linkage *reply_queue) {
  handle_alive(site, reply_queue, p);
}

static void process_need_boot_op(site_def const *site, pax_msg *p,
                                 linkage *reply_queue) {
  /* purecov: begin deadcode */
  /* Only in run state. Test state and do it here because we need to use
   * reply queue */
  if (can_send_snapshot() &&
      !synode_eq(get_site_def()->boot_key, null_synode)) {
    handle_boot(site, reply_queue, p);
  }
  /* Wake senders waiting to connect, since new node has appeared */
  wakeup_sender();
  /* purecov: end */
}

static void process_die_op(site_def const *site, pax_msg *p,
                           linkage *reply_queue) {
  (void)reply_queue;
  /* assert("die horribly" == "need coredump"); */
  {
    GET_GOUT;
    FN;
    STRLIT("die_op ");
    SYCEXP(executed_msg);
    SYCEXP(delivered_msg);
    SYCEXP(p->synode);
    SYCEXP(p->delivered_msg);
    SYCEXP(p->max_synode);
    PRINT_GOUT;
    FREE_GOUT;
  }
  /*
  If the message with the number in  the  incoming  die_op  message
  already  has  been  executed  (delivered),  then it means that we
  actually got consensus on it, since otherwise we would  not  have
  delivered it.Such a situation could arise if one of the nodes has
  expelled the message from its cache, but others have not. So when
  sending  out  a  request, we might get two different answers, one
  indicating that we are too far behind  and  should  restart,  and
  another  with  the  actual  consensus value. If the value arrives
  first, we will deliver it, and then the die_op may arrive  later.
  But  it this case it does not matter, since we got what we needed
  anyway. It is only a partial guard against exiting without really
  needing  it  of course, since the die_op may arrive first, and we
  do not wait for a die_op from all the other nodes.  We  could  do
  that  with  some extra housekeeping in the pax_machine (a new bit
  vector), but I am not convinced that it is worth the effort.
  */
  if (!synode_lt(p->synode, executed_msg)) {
    ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("terminating"));)
    g_critical("Node %u is unable to get message {%x %" PRIu64
               " %u}, since the group is too far "
               "ahead. Node will now exit.",
               get_nodeno(site), SY_MEM(p->synode));
    terminate_and_exit();
  }
}

static void process_read_op(site_def const *site, pax_msg *p,
                            linkage *reply_queue) {
  pax_machine *pm = get_cache(p->synode);
  assert(pm);

  handle_read(site, pm, reply_queue, p);
}

static void process_gcs_snapshot_op(site_def const *site, pax_msg *p,
                                    linkage *reply_queue) {
  (void)site;
  (void)reply_queue;
  /* Avoid duplicate snapshots and snapshots from zombies */
  IFDBG(D_BASE, FN; SYCEXP(executed_msg););
  IFDBG(D_BASE, FN; SYCEXP(start_config););
  if (!synode_eq(start_config, get_highest_boot_key(p->gcs_snap)) &&
      !is_dead_site(p->group_id)) {
    update_max_synode(p);
    /* For incoming messages, note delivery of snapshot from sender node */
    note_snapshot(p->from);
    XCOM_FSM(x_fsm_snapshot, void_arg(p->gcs_snap));
  }
}

static void process_tiny_learn_op(site_def const *site, pax_msg *p,
                                  linkage *reply_queue) {
  if (p->msg_type == no_op) {
    process_learn_op(site, p, reply_queue);
  } else {
    pax_machine *pm = get_cache(p->synode);
    assert(pm);
    if (p->force_delivery) pm->force_delivery = 1;
    handle_tiny_learn(site, pm, p);
  }
}

/* If this node is leader, grant a synode number for use by secondary.
 Send reply as synode_allocated. */
static void process_synode_request(site_def const *site, pax_msg *p,
                                   linkage *reply_queue) {
  (void)site;

  /* Find a free slot */
  assert(!synode_eq(current_message, null_synode));
  IFDBG(D_CONS, FN; SYCEXP(executed_msg); SYCEXP(current_message));
  site_def *tmp_site = find_site_def_rw(current_message);
  /* See if we can do anything with this message */
  if (tmp_site && get_nodeno(tmp_site) != VOID_NODE_NO && is_leader(tmp_site)) {
    /* Send reply with msgno */
    synode_no msgno = local_synode_allocator(current_message);
    /* Ensure that reply is sane. Note that we only allocate `msgno` *if* next
       synod is still within the event horizon. This effectively means that
       the leader always reserves at least one synod to himself, the last
       synod of the event horizon. The point is to ensure that the leader does
       not allocate all the possible synods to a non-leader that then doesn't
       act on them, e.g. by crashing. The last synod of the event horizon will
       always be up for grabs either by the leader, or if the leader is
       suspected, by some non-leader that will self-allocate that synod to
       himself. */
    if (!(too_far(incr_msgno(msgno)) ||
          ignore_message(msgno, find_site_def_rw(msgno),
                         "process_synode_request"))) {
      // We will grab this number, advance current_message
      set_current_message(incr_synode(msgno));
      IFDBG(D_CONS, FN; STRLIT("sending reply "); SYCEXP(executed_msg);
            SYCEXP(current_message); SYCEXP(msgno));
      CREATE_REPLY(p);
      reply->synode = msgno;
      reply->op = synode_allocated;
      IFDBG(D_CONS, FN; SYCEXP(msgno));
      SEND_REPLY;
    } else {
      IFDBG(D_CONS, FN; STRLIT("not sending reply "); SYCEXP(executed_msg);
            SYCEXP(msgno));
    }
  } else {
    IFDBG(D_CONS, FN; STRLIT("not leader ");
          if (tmp_site) SYCEXP(tmp_site->start));
  }
}

/* If this node is secondary, add synode to set of available synodes */
static void process_synode_allocated(site_def const *site, pax_msg *p,
                                     linkage *reply_queue) {
  (void)site;
  (void)p;
  (void)reply_queue;

  IFDBG(D_BASE, FN; SYCEXP(p->synode));
  synode_number_pool.put(p->synode, synode_allocation_type::remote);
}

static msg_handler dispatch_table[LAST_OP] = {process_client_msg,
                                              nullptr,
                                              process_prepare_op,
                                              process_ack_prepare_op,
                                              process_ack_prepare_op,
                                              process_accept_op,
                                              process_ack_accept_op,
                                              process_learn_op,
                                              process_recover_learn_op,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              process_skip_op,
                                              process_i_am_alive_op,
                                              process_are_you_alive_op,
                                              process_need_boot_op,
                                              nullptr,
                                              process_die_op,
                                              process_read_op,
                                              process_gcs_snapshot_op,
                                              nullptr,
                                              process_tiny_learn_op,
                                              process_synode_request,
                                              process_synode_allocated};

static msg_handler *clone_dispatch_table(msg_handler const *proto) {
  msg_handler *clone =
      static_cast<msg_handler *>(xcom_calloc(1, sizeof(dispatch_table)));
  if (clone)
    memcpy(clone, proto, sizeof(dispatch_table));
  else
    oom_abort = 1;
  return clone;
}

static msg_handler *primary_dispatch_table() {
  msg_handler *clone = clone_dispatch_table(dispatch_table);
  return clone;
}

static msg_handler *secondary_dispatch_table() {
  msg_handler *clone = clone_dispatch_table(dispatch_table);
  if (clone) {
    clone[synode_request] = nullptr;
  }
  return clone;
}

pax_msg *dispatch_op(site_def const *site, pax_msg *p, linkage *reply_queue) {
  site_def *dsite = find_site_def_rw(p->synode);

  if (dsite && p->op != client_msg && is_server_connected(dsite, p->from)) {
    /* Wake up the detector task if this node was previously marked as
     * potentially failed. */
    if (!note_detected(dsite, p->from)) task_wakeup(&detector_wait);
    update_delivered(dsite, p->from, p->delivered_msg);
  }

  IFDBG(D_BASE, FN; STRLIT("incoming message ");
        COPY_AND_FREE_GOUT(dbg_pax_msg(p)););
  ADD_DBG(D_DISPATCH, add_synode_event(p->synode);
          add_event(EVENT_DUMP_PAD, string_arg("p->from"));
          add_event(EVENT_DUMP_PAD, uint_arg(p->from));
          add_event(EVENT_DUMP_PAD, string_arg("too_far(p->synode)"));
          add_event(EVENT_DUMP_PAD, int_arg(too_far(p->synode)));
          add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(p->op))););

  if (p->op >= 0 && p->op < LAST_OP) {
    if (site && site->dispatch_table) { /* Use site-specific dispatch if any */
      if (site->dispatch_table[p->op])
        site->dispatch_table[p->op](site, p, reply_queue);
    } else {
      if (dispatch_table[p->op]) dispatch_table[p->op](site, p, reply_queue);
    }
  } else {
    G_WARNING("No possible handler for message %d %s", p->op,
              pax_op_to_str(p->op));
  }

  if (oom_abort) {
    g_critical("Node %u has run out of memory and will now exit.",
               get_nodeno(site));
    terminate_and_exit();
  }
  return (p);
}

/* Acceptor-learner task */
#define SERIALIZE_REPLY(msg)                \
  msg->to = ep->p->from;                    \
  msg->from = ep->p->to;                    \
  msg->delivered_msg = get_delivered_msg(); \
  msg->max_synode = get_max_synode();       \
  serialize_msg(msg, ep->rfd->x_proto, &ep->buflen, &ep->buf);

#define WRITE_REPLY                                                    \
  if (ep->buflen) {                                                    \
    int64_t sent;                                                      \
    IFDBG(D_TRANSPORT, FN; STRLIT("task_write "); NDBG(ep->rfd.fd, d); \
          NDBG(ep->buflen, u));                                        \
    TASK_CALL(task_write(ep->rfd, ep->buf, ep->buflen, &sent));        \
    send_count[ep->p->op]++;                                           \
    send_bytes[ep->p->op] += ep->buflen;                               \
    X_FREE(ep->buf);                                                   \
  }                                                                    \
  ep->buf = NULL;

static inline void update_srv(server **target, server *srv) {
  if (srv) srv_ref(srv);
  if (*target) srv_unref(*target);
  *target = srv;
}

/* A message is harmless if it cannot change the outcome of a consensus round.
 * learn_op does change the value, but we trust that the sender has correctly
 * derived the value from a majority of the acceptors, so in that sense it is
 * harmless. */
static int harmless(pax_msg const *p) {
  if (p->synode.msgno == 0) return 1;
  switch (p->op) {
    case i_am_alive_op:
    case are_you_alive_op:
    case need_boot_op:
    case gcs_snapshot_op:
    case learn_op:
    case recover_learn_op:
    case tiny_learn_op:
    case die_op:
      return 1;
    default:
      return 0;
  }
}

static int wait_for_cache(pax_machine **pm, synode_no synode, double timeout) {
  DECL_ENV
  double now;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN
  ep->now = task_now();
  while ((*pm = get_cache(synode)) == nullptr) {
    /* Wait for executor to make progress */
    TIMED_TASK_WAIT(&exec_wait, 0.5);
    if (task_now() - ep->now > timeout) break; /* Timeout, return NULL. */
  }
  FINALLY
  TASK_END;
}

/*
  Verify if we need to poll the cache before calling dispatch_op.
  Avoid waiting for a machine if it is not going to be used.
 */
static bool_t should_poll_cache(pax_op op) {
  if (op == die_op || op == gcs_snapshot_op || op == initial_op ||
      op == client_msg)
    return FALSE;
  return TRUE;
}

int acceptor_learner_task(task_arg arg) {
  DECL_ENV
  connection_descriptor *rfd;
  srv_buf *in_buf;
  pax_msg *p;
  u_int buflen;
  char *buf;
  linkage reply_queue;
  int errors;
  server *srv;
  site_def const *site;
  int behind;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t n{0};
  pax_machine *pm{nullptr};

  TASK_BEGIN

  ep->rfd = (connection_descriptor *)get_void_arg(arg);
  ep->in_buf = (srv_buf *)xcom_calloc(1, sizeof(srv_buf));
  ep->p = nullptr;
  ep->buflen = 0;
  ep->buf = nullptr;
  ep->errors = 0;
  ep->srv = nullptr;
  ep->behind = FALSE;

  /* We have a connection, make socket non-blocking and wait for request */
  unblock_fd(ep->rfd->fd);
  set_nodelay(ep->rfd->fd);
  wait_io(stack, ep->rfd->fd, 'r');
  TASK_YIELD;

  set_connected(ep->rfd, CON_FD);
  link_init(&ep->reply_queue, TYPE_HASH("msg_link"));

again:
  while (!xcom_shutdown) {
    ep->site = nullptr;
    unchecked_replace_pax_msg(&ep->p, pax_msg_new_0(null_synode));

    if (use_buffered_read) {
      TASK_CALL(buffered_read_msg(ep->rfd, ep->in_buf, ep->p, ep->srv, &n));
    } else {
      /* purecov: begin deadcode */
      TASK_CALL(read_msg(ep->rfd, ep->p, ep->srv, &n));
      /* purecov: end */
    }
    ADD_DBG(D_NONE, add_synode_event(ep->p->synode);
            add_event(EVENT_DUMP_PAD, string_arg("ep->p->from"));
            add_event(EVENT_DUMP_PAD, uint_arg(ep->p->from));
            add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(ep->p->op))););

    if (ep->srv && !ep->srv->invalid && ((int)ep->p->op != (int)client_msg) &&
        is_connected(ep->srv->con))
      server_detected(ep->srv);

    if (((int)ep->p->op < (int)client_msg || ep->p->op > LAST_OP)) {
      /* invalid operation, ignore message */
      delete_pax_msg(ep->p);
      ep->p = nullptr;
      TASK_YIELD;
      continue;
    }
    if (n <= 0) {
      break;
    }
    if (ep->p->op != client_msg) {  // Clients have no site
      ep->site = find_site_def(ep->p->synode);
    }

    /* Handle this connection on a local_server task instead of this
       acceptor_learner_task task. */
    /* purecov: begin deadcode */
    if (ep->p->op == client_msg && ep->p->a &&
        ep->p->a->body.c_t == convert_into_local_server_type) {
      if (local_server_is_setup()) {
        /* Launch local_server task to handle this connection. */
        {
          connection_descriptor *con = (connection_descriptor *)xcom_malloc(
              sizeof(connection_descriptor));
          *con = *ep->rfd;
          task_new(local_server, void_arg(con), "local_server",
                   XCOM_THREAD_DEBUG);
        }
      }
      /* Reply to client:
         - OK if local_server task is setup, or
         - FAIL otherwise. */
      {
        CREATE_REPLY(ep->p);
        reply->op = xcom_client_reply;
        reply->cli_err = local_server_is_setup() ? REQUEST_OK : REQUEST_FAIL;
        SERIALIZE_REPLY(reply);
        replace_pax_msg(&reply, nullptr);
      }
      WRITE_REPLY;
      delete_pax_msg(ep->p);
      ep->p = nullptr;
      if (local_server_is_setup()) {
        /* Relinquish ownership of the connection. It is now onwed by the
           launched local_server task. */
        reset_connection(ep->rfd);
      }
      /* Terminate this task. */
      TERMINATE;
    }
    /* purecov: end */

    /*
      Getting a pointer to the server needs to be done after we have
      received a message, since without having received a message, we
      cannot know who it is from. We could peek at the message and de‐
      serialize the message number and from field, but since the server
      does not change, it should be sufficient to cache the server in
      the acceptor_learner task. A cleaner solution would have been to
      move the timestamps out of the server object, and have a map in‐
      dexed by IP/port or UUID to track the timestamps, since this is
      common to both the sender_task, reply_handler_task,  and the ac‐
      ceptor_learner_task.
    */
    update_srv(&ep->srv, get_server(ep->site, ep->p->from));
    ep->p->refcnt = 1; /* Refcnt from other end is void here */
    IFDBG(D_NONE, FN; NDBG(ep->rfd.fd, d); NDBG(task_now(), f);
          COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p)););
    receive_count[ep->p->op]++;
    receive_bytes[ep->p->op] += (uint64_t)n + MSG_HDR_SIZE;
    {
      if (get_maxnodes(ep->site) > 0) {
        ep->behind = ep->p->synode.msgno < delivered_msg.msgno;
      }
      ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("before dispatch "));
              add_synode_event(ep->p->synode);
              add_event(EVENT_DUMP_PAD, string_arg("ep->p->from"));
              add_event(EVENT_DUMP_PAD, uint_arg(ep->p->from));
              add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(ep->p->op)));
              add_event(EVENT_DUMP_PAD,
                        string_arg(pax_msg_type_to_str(ep->p->msg_type)));
              add_event(EVENT_DUMP_PAD, string_arg("is_cached(ep->p->synode)"));
              add_event(EVENT_DUMP_PAD, int_arg(is_cached(ep->p->synode)));
              add_event(EVENT_DUMP_PAD, string_arg("behind"));
              add_event(EVENT_DUMP_PAD, int_arg(ep->behind)););
      /* Special treatment to see if synode number is valid. Return no-op if
       * not. */
      if (ep->p->op == read_op || ep->p->op == prepare_op ||
          ep->p->op == accept_op) {
        if (ep->site) {
          ADD_DBG(
              D_BASE, add_event(EVENT_DUMP_PAD, string_arg("ep->p->synode"));
              add_synode_event(ep->p->synode);
              add_event(EVENT_DUMP_PAD, string_arg("ep->site->start"));
              add_synode_event(ep->site->start); add_event(
                  EVENT_DUMP_PAD, string_arg("ep->site->nodes.node_list_len"));
              add_event(EVENT_DUMP_PAD,
                        uint_arg(ep->site->nodes.node_list_len)););
          if (ep->p->synode.node >= ep->site->nodes.node_list_len) {
            {
              CREATE_REPLY(ep->p);
              create_noop(reply);
              set_learn_type(reply);
              SERIALIZE_REPLY(reply);
              delete_pax_msg(reply); /* Deallocate BEFORE potentially blocking
                                        call which will lose value of reply */
            }
            WRITE_REPLY;
            goto again;
          }
        }
      }
      /* Reject any message that might compromise the integrity of a consensus
       * instance. We do this by not processing any message which may change
       * the
       * outcome if the consensus instance has been evicted from the cache */
      if (harmless(ep->p) ||          /* Harmless message */
          is_cached(ep->p->synode) || /* Already in cache */
          (!ep->behind)) { /* Guard against cache pollution from other nodes
                            */

        if (should_poll_cache(ep->p->op)) {
          TASK_CALL(wait_for_cache(&pm, ep->p->synode, 10));
          if (!pm) continue; /* Could not get a machine, discarding message. */
        }

        dispatch_op(ep->site, ep->p, &ep->reply_queue);

        /* Send replies on same fd */
        while (!link_empty(&ep->reply_queue)) {
          {
            msg_link *reply =
                (msg_link *)(link_extract_first(&ep->reply_queue));
            IFDBG(D_DISPATCH, FN; PTREXP(reply);
                  COPY_AND_FREE_GOUT(dbg_linkage(&ep->reply_queue));
                  COPY_AND_FREE_GOUT(dbg_msg_link(reply));
                  COPY_AND_FREE_GOUT(dbg_pax_msg(reply->p)););
            assert(reply->p);
            assert(reply->p->refcnt > 0);
            IFDBG(D_DISPATCH, FN; STRLIT("serialize "); PTREXP(reply));
            SERIALIZE_REPLY(reply->p);
            msg_link_delete(&reply); /* Deallocate BEFORE potentially blocking
                                        call which will lose value of reply */
          }
          WRITE_REPLY;
        }
      } else {
        IFDBG(D_EXEC, FN; STRLIT("rejecting ");
              STRLIT(pax_op_to_str(ep->p->op)); NDBG(ep->p->from, d);
              NDBG(ep->p->to, d); SYCEXP(ep->p->synode);
              BALCEXP(ep->p->proposal));
        if (/* xcom_booted() && */ ep->behind) {
          if (/*ep->p->op == prepare_op && */ was_removed_from_cache(
              ep->p->synode)) {
            IFDBG(D_NONE, FN; STRLIT("send_die ");
                  STRLIT(pax_op_to_str(ep->p->op)); NDBG(ep->p->from, d);
                  NDBG(ep->p->to, d); SYCEXP(ep->p->synode);
                  BALCEXP(ep->p->proposal));
            if (get_maxnodes(ep->site) > 0) {
              {
                pax_msg *np = nullptr;
                np = pax_msg_new(ep->p->synode, ep->site);
                np->op = die_op;
                SERIALIZE_REPLY(np);
                IFDBG(D_NONE, FN; STRLIT("sending die_op to node ");
                      NDBG(np->to, d); SYCEXP(executed_msg); SYCEXP(max_synode);
                      SYCEXP(np->synode));
                delete_pax_msg(np); /* Deallocate BEFORE potentially blocking
                                   call which will lose value of np */
              }
              WRITE_REPLY;
            }
          }
        }
      }
    }
    /* TASK_YIELD; */
  }

  FINALLY
  IFDBG(D_BUG, FN; STRLIT(" shutdown "); NDBG(ep->rfd.fd, d);
        NDBG(task_now(), f));
  if (ep->reply_queue.suc && !link_empty(&ep->reply_queue))
    empty_msg_list(&ep->reply_queue);
  unchecked_replace_pax_msg(&ep->p, nullptr);
  shutdown_connection(ep->rfd);
  free(ep->rfd);
  IFDBG(D_NONE, FN; NDBG(xcom_shutdown, d));
  if (ep->buf) X_FREE(ep->buf);
  free(ep->in_buf);

  /* Unref srv to avoid leak */
  update_srv(&ep->srv, nullptr);

  IFDBG(D_BUG, FN; STRLIT(" shutdown completed"); NDBG(ep->rfd.fd, d);
        NDBG(task_now(), f));
  TASK_END;
}

/* Reply handler task */

static void server_handle_need_snapshot(server *srv, site_def const *s,
                                        node_no node);

int reply_handler_task(task_arg arg) {
  DECL_ENV
  server *s;
  pax_msg *reply;
  double dtime;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  int64_t n{0};
  TASK_BEGIN

  ep->dtime = INITIAL_CONNECT_WAIT; /* Initial wait is short, to avoid
                                       unnecessary waiting */
  ep->s = (server *)get_void_arg(arg);
  srv_ref(ep->s);
  ep->reply = nullptr;

  while (!xcom_shutdown) {
    while (!is_connected(ep->s->con)) {
      IFDBG(D_NONE, FN; STRLIT("waiting for connection"));
      TASK_DELAY(ep->dtime);
      if (xcom_shutdown) {
        TERMINATE;
      }
      ep->dtime += CONNECT_WAIT_INCREASE; /* Increase wait time for next try */
      if (ep->dtime > MAX_CONNECT_WAIT) {
        ep->dtime = MAX_CONNECT_WAIT;
      }
    }
    ep->dtime = INITIAL_CONNECT_WAIT;
    {
      unchecked_replace_pax_msg(&ep->reply, pax_msg_new_0(null_synode));

      ADD_DBG(D_NONE, add_event(EVENT_DUMP_PAD, string_arg("ep->s->con.fd"));
              add_event(EVENT_DUMP_PAD, int_arg(ep->s->con.fd)););
      TASK_CALL(read_msg(ep->s->con, ep->reply, ep->s, &n));
      ADD_DBG(D_NONE, add_event(EVENT_DUMP_PAD, string_arg("ep->s->con.fd"));
              add_event(EVENT_DUMP_PAD, int_arg(ep->s->con.fd)););
      ep->reply->refcnt = 1; /* Refcnt from other end is void here */
      if (n <= 0) {
        shutdown_connection(ep->s->con);
        continue;
      }
      receive_bytes[ep->reply->op] += (uint64_t)n + MSG_HDR_SIZE;
    }
    IFDBG(D_NONE, FN; NDBG(ep->s->con.fd, d); NDBG(task_now(), f);
          COPY_AND_FREE_GOUT(dbg_pax_msg(ep->reply)););
    receive_count[ep->reply->op]++;

    ADD_DBG(D_NONE, add_synode_event(ep->reply->synode);
            add_event(EVENT_DUMP_PAD, string_arg("ep->reply->from"));
            add_event(EVENT_DUMP_PAD, uint_arg(ep->reply->from));
            add_event(EVENT_DUMP_PAD, string_arg(pax_op_to_str(ep->reply->op)));
            add_event(EVENT_DUMP_PAD, string_arg("get_site_def()->boot_key"));
            add_synode_event(get_site_def()->boot_key););
    /* Special test for need_snapshot, since node and site may not be
     * consistent
     */
    if (ep->reply->op == need_boot_op &&
        !synode_eq(get_site_def()->boot_key, null_synode)) {
      pax_msg *p = ep->reply;

      ADD_DBG(D_BASE,
              add_event(EVENT_DUMP_PAD,
                        string_arg("calling server_handle_need_snapshot")););
      if (should_handle_need_boot(find_site_def(p->synode), p)) {
        server_handle_need_snapshot(ep->s, find_site_def(p->synode), p->from);
        /* Wake senders waiting to connect, since new node has appeared */
        wakeup_sender();
      } else {
        ep->s->invalid = 1;
      }
    } else {
      /* We only handle messages from this connection if the server is valid.
       */
      if (ep->s->invalid == 0)
        dispatch_op(find_site_def(ep->reply->synode), ep->reply, nullptr);
    }
    TASK_YIELD;
  }

  FINALLY
  replace_pax_msg(&ep->reply, nullptr);

  shutdown_connection(ep->s->con);
  ep->s->reply_handler = nullptr;
  IFDBG(D_BUG, FN; STRLIT(" shutdown "); NDBG(ep->s->con.fd, d);
        NDBG(task_now(), f));
  srv_unref(ep->s);

  TASK_END;
}

/* purecov: begin deadcode */
void xcom_sleep(unsigned int seconds) {
#if defined(_WIN32)
  Sleep((DWORD)seconds * 1000); /* windows sleep takes milliseconds */
#else
  sleep(seconds);
#endif
}
/* purecov: end */

/*
 * Get a unique long as the basis for XCom group id creation.
 *
 * NOTE:
 * As there is no gethostid() on win, we use seconds since epoch instead,
 * so it might fail if you try simultaneous create sites at the same second.
 */

#ifndef _WIN32
#include <sys/utsname.h>
#endif

long xcom_unique_long(void) {
#if defined(_WIN32)
  __time64_t ltime;

  _time64(&ltime);
  return (long)(ltime ^ GetCurrentProcessId());
#else
  struct utsname buf;
  uname(&buf);
  long id = (long)fnv_hash((unsigned char *)&buf, sizeof(buf), 0);
  return id ^ getpid();
#endif
}

app_data_ptr init_config_with_group(app_data *a, node_list *nl, cargo_type type,
                                    uint32_t group_id) {
  init_app_data(a);
  a->app_key.group_id = a->group_id = group_id;
  a->body.c_t = type;
  init_node_list(nl->node_list_len, nl->node_list_val, &a->body.app_u_u.nodes);
  return a;
}

app_data_ptr init_set_event_horizon_msg(app_data *a, uint32_t group_id,
                                        xcom_event_horizon event_horizon) {
  init_app_data(a);
  a->app_key.group_id = a->group_id = group_id;
  a->body.c_t = set_event_horizon_type;
  a->body.app_u_u.event_horizon = event_horizon;
  return a;
}

app_data_ptr init_get_msg(app_data *a, uint32_t group_id, cargo_type const t) {
  init_app_data(a);
  a->app_key.group_id = a->group_id = group_id;
  a->body.c_t = t;
  return a;
}

app_data_ptr init_get_leaders_msg(app_data *a, uint32_t group_id) {
  return init_get_msg(a, group_id, get_leaders_type);
}

app_data_ptr init_get_event_horizon_msg(app_data *a, uint32_t group_id) {
  return init_get_msg(a, group_id, get_event_horizon_type);
}

app_data_ptr init_app_msg(app_data *a, char *payload, u_int payload_size) {
  init_app_data(a);
  a->body.c_t = app_type;
  a->body.app_u_u.data.data_val = payload; /* Takes ownership of payload. */
  a->body.app_u_u.data.data_len = payload_size;
  return a;
}

static app_data_ptr init_get_synode_app_data_msg(
    app_data *a, uint32_t group_id, synode_no_array *const synodes) {
  init_get_msg(a, group_id, get_synode_app_data_type);
  /* Move synodes (as in C++ move semantics) into a->body.app_u_u.synodes. */
  synode_array_move(&a->body.app_u_u.synodes, synodes);
  return a;
}

app_data_ptr init_set_cache_size_msg(app_data *a, uint64_t cache_limit) {
  init_app_data(a);
  a->body.c_t = set_cache_limit;
  a->body.app_u_u.cache_limit = cache_limit;
  return a;
}

app_data_ptr init_convert_into_local_server_msg(app_data *a) {
  init_app_data(a);
  a->body.c_t = convert_into_local_server_type;
  return a;
}

static void server_send_snapshot(server *srv, site_def const *s,
                                 gcs_snapshot *gcs_snap, node_no node) {
  pax_msg *p = pax_msg_new(gcs_snap->log_start, get_site_def());
  ref_msg(p);
  p->op = gcs_snapshot_op;
  p->gcs_snap = gcs_snap;
  send_msg(srv, s->nodeno, node, get_group_id(s), p);
  unref_msg(&p);
}

static void server_push_log(server *srv, synode_no push, node_no node) {
  site_def const *s = get_site_def();
  if (srv && s) {
    while (!synode_gt(push, get_max_synode())) {
      if (is_cached(push)) {
        /* Need to clone message here since pax_machine may be re-used while
         * message is sent */
        pax_machine *p = get_cache_no_touch(push, FALSE);
        if (pm_finished(p)) {
          pax_msg *pm = clone_pax_msg(p->learner.msg);
          if (pm != nullptr) {
            ref_msg(pm);
            pm->op = recover_learn_op;
            IFDBG(D_NONE, FN; PTREXP(srv); PTREXP(s););
            send_msg(srv, s->nodeno, node, get_group_id(s), pm);
            unref_msg(&pm);
          }
        }
      }
      push = incr_synode(push);
    }
  }
}

/* purecov: begin deadcode */
static void reply_push_log(synode_no push, linkage *reply_queue) {
  while (!synode_gt(push, get_max_synode())) {
    if (is_cached(push)) {
      /* Need to clone message here since pax_machine may be re-used while
       * message is sent */
      pax_machine *p = get_cache_no_touch(push, FALSE);
      if (pm_finished(p)) {
        pax_msg *reply = clone_pax_msg(p->learner.msg);
        ref_msg(reply);
        reply->op = recover_learn_op;
        {
          msg_link *msg_x = msg_link_new(reply, reply->from);
          IFDBG(D_NONE, FN; PTREXP(msg_x));
          link_into(&(msg_x->l), reply_queue);
        }
        replace_pax_msg(&reply, nullptr);
        unref_msg(&reply);
      }
    }
    push = incr_synode(push);
  }
}
/* purecov: end */

static app_snap_getter get_app_snap_cb;
static app_snap_handler handle_app_snap_cb;

static gcs_snapshot *create_snapshot() {
  gcs_snapshot *gs = nullptr;
  if (get_app_snap_cb) {
    /* purecov: begin deadcode */
    blob app_snap = {{0, nullptr}}; /* Initialize in case get_app_snap_cb does
                                       not assign a value */
    synode_no app_lsn = get_app_snap_cb(&app_snap);

    /* We have a valid callback, abort if it did not return anything */
    if (app_snap.data.data_len == 0) {
      ADD_DBG(D_BASE,
              add_event(EVENT_DUMP_PAD, string_arg("no data, return")););
      return nullptr;
    }
    gs = export_config();
    if (!gs) return nullptr;
    ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("export config ok")););
    gs->app_snap = app_snap;
    IFDBG(D_BUG, FN; SYCEXP(app_lsn); SYCEXP(gs->log_start);
          SYCEXP(gs->log_end));

    /* Set starting point of log to match the snapshot */
    /* If we have a valid synode from application snapshot, see if it should
     * be used */
    if (!synode_eq(null_synode, app_lsn)) {
      /* If log_start is null_synode, always use valid synode from application
       * snapshot */
      if (synode_eq(null_synode, gs->log_start) ||
          !synode_gt(app_lsn, gs->log_start)) {
        gs->log_start = app_lsn;
        IFDBG(D_BUG, FN; STRLIT("using "); SYCEXP(app_lsn));
      }
    }
    ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("gs->log_start"));
            add_synode_event(gs->log_start);
            add_event(EVENT_DUMP_PAD, string_arg("gs->log_end"));
            add_synode_event(gs->log_end););
    /* purecov: end */
  } else {
    gs = export_config();
    if (!gs) return nullptr;
    ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("export config ok")););
    if (!synode_eq(null_synode, last_config_modification_id)) {
      /* No valid valid synode from application snapshot, use
       * last_config_modification_id if not null_synode */
      gs->log_start = last_config_modification_id;
      IFDBG(D_BUG, FN; STRLIT("using "); SYCEXP(last_config_modification_id));
    }
    IFDBG(D_BUG, FN; SYCEXP(gs->log_start); SYCEXP(gs->log_end));
    ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("gs->log_start"));
            add_synode_event(gs->log_start);
            add_event(EVENT_DUMP_PAD, string_arg("gs->log_end"));
            add_synode_event(gs->log_end););
  }
  IFDBG(D_BUG, FN; SYCEXP(gs->log_start); SYCEXP(gs->log_end));
  return gs;
}

/* purecov: begin deadcode */
static void handle_need_snapshot(linkage *reply_queue, pax_msg *pm) {
  gcs_snapshot *gs = create_snapshot();
  if (gs) {
    pax_msg *reply = clone_pax_msg(pm);
    ref_msg(reply);
    reply->op = gcs_snapshot_op;
    reply->gcs_snap = gs;
    {
      msg_link *msg_x = msg_link_new(reply, reply->from);
      IFDBG(D_NONE, FN; PTREXP(msg_x));
      link_into(&(msg_x->l), reply_queue);
    }
    unref_msg(&reply);
    IFDBG(D_NONE, FN; STRLIT("sent snapshot"););
    reply_push_log(gs->log_start, reply_queue);
    send_global_view();
  }
}
/* purecov: end */

static task_env *x_timer = nullptr;

/* Timer for use with the xcom FSM. Will deliver x_fsm_timeout */
static int xcom_timer(task_arg arg) {
  DECL_ENV
  double t;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN

  ep->t = get_double_arg(arg);
  TASK_DELAY(ep->t);
  XCOM_FSM(x_fsm_timeout, double_arg(ep->t));
  FINALLY
  if (stack == x_timer) set_task(&x_timer, nullptr);
  IFDBG(D_CONS, FN; STRLIT(" timeout "));
  TASK_END;
}

/* Stop the xcom FSM timer */
static void stop_x_timer() {
  if (x_timer) {
    task_terminate(x_timer);
    set_task(&x_timer, nullptr);
  }
}

/* Start the xcom FSM timer */
static void start_x_timer(double t) {
  stop_x_timer();
  set_task(&x_timer, task_new(xcom_timer, double_arg(t), "xcom_timer",
                              XCOM_THREAD_DEBUG));
}

/* Deliver x_fsm_complete to xcom FSM */
/* purecov: begin deadcode */
static int x_fsm_completion_task(task_arg arg) {
  DECL_ENV
  int dummy;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  TASK_BEGIN

      (void)
  arg;
  XCOM_FSM(x_fsm_complete, null_arg);
  FINALLY
  IFDBG(D_FSM, FN; STRLIT(" delivered "));
  TASK_END;
}
/* purecov: end */

/* Send x_fsm_complete to xcom FSM in the context of the xcom thread. The
 * calling thread and the xcom thread must be in a rendezvous. Using a task to
 * deliver a message is an abstraction inversion, but it's the simplest
 * solution until we get a proper queue-based communication system going. */
/* purecov: begin deadcode */
void send_x_fsm_complete() {
  task_new(x_fsm_completion_task, null_arg, "x_fsm_completion_task",
           XCOM_THREAD_DEBUG);
}
/* purecov: end */

static void server_handle_need_snapshot(server *srv, site_def const *s,
                                        node_no node) {
  G_INFO("Received an XCom snapshot request from %s:%d", srv->srv, srv->port);
  gcs_snapshot *gs = create_snapshot();

  if (gs) {
    server_send_snapshot(srv, s, gs, node);
    IFDBG(D_NONE, FN; STRLIT("sent snapshot"););
    G_INFO("XCom snapshot sent to %s:%d", srv->srv, srv->port);
    server_push_log(srv, gs->log_start, node);
    send_global_view();
  }
}

#define X(b) #b
const char *xcom_actions_name[] = {x_actions};
#undef X

static int snapshots[NSERVERS];

/* Note that we have received snapshot from node */
static void note_snapshot(node_no node) {
  if (node != VOID_NODE_NO) {
    snapshots[node] = 1;
  }
}

/* Reset set of received snapshots */
static void reset_snapshot_mask() {
  int i;
  for (i = 0; i < NSERVERS; i++) {
    snapshots[i] = 0;
  }
}

/* See if we have got a snapshot from every node */
static int got_all_snapshots() {
  node_no i;
  node_no max = get_maxnodes(get_site_def());
  if (0 == max) {
    return 0;
  }
  for (i = 0; i < max; i++) {
    if (!snapshots[i]) {
      return 0;
    }
  }
  return 1;
}

static synode_no log_start_max; /* Initialized by xcom_fsm */
static synode_no log_end_max;   /* Initialized by xcom_fsm */

/* See if this snapshot is better than what we already have */
/* purecov: begin deadcode */
static int better_snapshot(gcs_snapshot *gcs) {
  synode_no boot_key = config_max_boot_key(gcs);
  return synode_gt(boot_key, get_site_def()->boot_key) ||
         (synode_eq(boot_key, get_site_def()->boot_key) &&
          (synode_gt(gcs->log_start, log_start_max) ||
           (synode_eq(gcs->log_start, log_start_max) &&
            synode_gt(gcs->log_end, log_end_max))));
}
/* purecov: end */

/* Install snapshot */
static void handle_x_snapshot(gcs_snapshot *gcs) {
  G_INFO(
      "Installing requested snapshot. Importing all incoming configurations.");
  import_config(gcs);
  if (get_nodeno(get_site_def()) == VOID_NODE_NO) {
    IFDBG(D_BASE, FN; STRLIT("Not member of site, not executing log"));
    gcs->log_end =
        gcs->log_start; /* Avoid executing log if not member of site */
  }
  if (handle_app_snap_cb)
    handle_app_snap_cb(&gcs->app_snap, gcs->log_start, gcs->log_end);
  set_max_synode(gcs->log_end);
  set_executed_msg(incr_synode(gcs->log_start));
  log_start_max = gcs->log_start;
  log_end_max = gcs->log_end;

  set_last_received_config(get_highest_boot_key(gcs));

  G_INFO("Finished snapshot installation. My node number is %d",
         get_nodeno(get_site_def()));

  IFDBG(D_BUG, FN; SYCEXP(gcs->log_start); SYCEXP(gcs->log_end);
        SYCEXP(last_config_modification_id); SYCEXP(executed_msg););
}

/* Note that we have received snapshot, and install if better than old */
/* purecov: begin deadcode */
static void update_best_snapshot(gcs_snapshot *gcs) {
  if (get_site_def() == nullptr || better_snapshot(gcs)) {
    handle_x_snapshot(gcs);
  }
}
/* purecov: end */

/* Send need_boot_op to all nodes in current config */
/* purecov: begin deadcode */
static void send_need_boot() {
  pax_msg *p = pax_msg_new_0(null_synode);
  ref_msg(p);
  p->synode = get_site_def()->start;
  p->op = need_boot_op;
  send_to_all_except_self(get_site_def(), p, "need_boot_op");
  unref_msg(&p);
}
/* purecov: end */

/* Set log_end of snapshot based on log_end in snapshot and max synode */
void set_log_end(gcs_snapshot *gcs) {
  if (synode_gt(get_max_synode(), gcs->log_end)) {
    gcs->log_end = get_max_synode();
  }
}

struct xcom_fsm_state;
typedef struct xcom_fsm_state xcom_fsm_state;

/* Function pointer corresponding to a state. Return 1 if execution should
 * continue, 0 otherwise */
typedef int (*xcom_fsm_fp)(xcom_actions action, task_arg fsmargs,
                           xcom_fsm_state *ctxt);

/* Function pointer and name */
struct xcom_fsm_state {
  xcom_fsm_fp state_fp;
  char const *state_name;
};

#define X_FSM_STATE(s) \
  { s, #s }
#define SET_X_FSM_STATE(s) \
  do {                     \
    ctxt->state_fp = s;    \
    ctxt->state_name = #s; \
  } while (0)

/* The state functions/thunks */
static int xcom_fsm_init(xcom_actions action, task_arg fsmargs,
                         xcom_fsm_state *ctxt);
static int xcom_fsm_start_enter(xcom_actions action, task_arg fsmargs,
                                xcom_fsm_state *ctxt);
static int xcom_fsm_start(xcom_actions action, task_arg fsmargs,
                          xcom_fsm_state *ctxt);
static int xcom_fsm_snapshot_wait_enter(xcom_actions action, task_arg fsmargs,
                                        xcom_fsm_state *ctxt);
static int xcom_fsm_snapshot_wait(xcom_actions action, task_arg fsmargs,
                                  xcom_fsm_state *ctxt);
static int xcom_fsm_recover_wait_enter(xcom_actions action, task_arg fsmargs,
                                       xcom_fsm_state *ctxt);
static int xcom_fsm_recover_wait(xcom_actions action, task_arg fsmargs,
                                 xcom_fsm_state *ctxt);
static int xcom_fsm_run_enter(xcom_actions action, task_arg fsmargs,
                              xcom_fsm_state *ctxt);
static int xcom_fsm_run(xcom_actions action, task_arg fsmargs,
                        xcom_fsm_state *ctxt);

/* You are in a twisting maze of little functions ... */

/* init state */
static int xcom_fsm_init(xcom_actions action, task_arg fsmargs,
                         xcom_fsm_state *ctxt) {
  (void)action;
  (void)fsmargs;
  IFDBG(D_NONE, FN;);
  /* Initialize basic xcom data */
  xcom_thread_init();
  SET_X_FSM_STATE(xcom_fsm_start_enter);
  return 1;
}

/* start_enter state */
static int xcom_fsm_start_enter(xcom_actions action, task_arg fsmargs,
                                xcom_fsm_state *ctxt) {
  (void)action;
  (void)fsmargs;
  /* push_dbg(D_DETECT | D_FSM | D_FILEOP | D_CONS | D_BASE | D_TRANSPORT);
   */
  push_dbg(D_FSM);
  IFDBG(D_NONE, FN; STRLIT("state x_start"););
  empty_prop_input_queue();
  empty_synode_number_pool();
  reset_snapshot_mask();
  set_last_received_config(null_synode);

  SET_X_FSM_STATE(xcom_fsm_start);
  return 1;
}

static int handle_fsm_net_boot(task_arg fsmargs, xcom_fsm_state *ctxt,
                               int cont) {
  app_data *a = (app_data *)get_void_arg(fsmargs);
  install_node_group(a);
  if (is_member(get_site_def())) {
    empty_prop_input_queue();
    empty_synode_number_pool();
    {
      synode_no start = get_site_def()->start;
      if (start.msgno == 0) { /* May happen during initial boot */
        start.msgno = 1;      /* Start with first xcom message */
        /* If msgno is 0, it means that this node installed a unified_boot
        which came from the client, thus this node is the one that will send
        the unified_boot on xcom, so set the node number of start accordingly
      */
        start.node = get_nodeno(get_site_def());
      }
      set_executed_msg(start);
    }
    pop_dbg();
    SET_X_FSM_STATE(xcom_fsm_run_enter);
    cont = 1;
  }
  return cont;
}

static int handle_fsm_snapshot(task_arg fsmargs, xcom_fsm_state *ctxt) {
  gcs_snapshot *gcs = (gcs_snapshot *)get_void_arg(fsmargs);
  empty_prop_input_queue();
  empty_synode_number_pool();
  set_log_end(gcs);
  handle_x_snapshot(gcs);

  /* Get recovery manager going again */
  if (recovery_restart_cb) recovery_restart_cb();

  /* If we run under control of the recovery manager, we need to call
   * recovery_begin_cb to rendezvous with the recovery manager */
  if (recovery_begin_cb) recovery_begin_cb();

  /* If we run under control of the recovery manager, we need to call
   * recovery_end_cb to rendezvous with the recovery manager */
  if (recovery_end_cb) recovery_end_cb();

  /* If we are here, it means that we are recovering from another node
   */
  /* Do not bother to wait for more snapshots, just handle it and
  enter run state */
  pop_dbg();
  SET_X_FSM_STATE(xcom_fsm_run_enter);
  return 1;
}

/* purecov: begin deadcode */
static int handle_fsm_snapshot_wait(xcom_fsm_state *ctxt) {
  empty_prop_input_queue();
  empty_synode_number_pool();
  start_x_timer(SNAPSHOT_WAIT_TIME);
  pop_dbg();
  SET_X_FSM_STATE(xcom_fsm_snapshot_wait_enter);
  return 1;
}
/* purecov: end */

static void handle_fsm_exit() {
  /* Xcom is finished when we get here */
  push_dbg(D_BUG);
  bury_site(get_group_id(get_site_def()));
  task_terminate_all(); /* Kill, kill, kill, kill, kill, kill. This is
                           the end. */

  /* init_xcom_base(); */ /* Reset shared variables */
  init_tasks();           /* Reset task variables */
  free_site_defs();
  free_forced_config_site_def();
  wait_forced_config = 0;
  garbage_collect_servers();
  IFDBG(D_NONE, FN; STRLIT("shutting down"));
  xcom_shutdown = 1;
  start_config = null_synode;
  G_DEBUG("Exiting xcom thread");
}

/* start state */
static int xcom_fsm_start(xcom_actions action, task_arg fsmargs,
                          xcom_fsm_state *ctxt) {
  static int need_init_cache = 0;
  int cont = 0; /* Set to 1 if we should continue execution */

  switch (action) {
    case x_fsm_init:
      xcom_shutdown = 0;
      sent_alive = 0.0;
      oom_abort = 0;
      if (need_init_cache) init_cache();
      break;

    case x_fsm_net_boot:
      cont = handle_fsm_net_boot(fsmargs, ctxt, cont);
      break;

    case x_fsm_snapshot:
      cont = handle_fsm_snapshot(fsmargs, ctxt);
      break;

    /* This is the entry point for the initial recovery after the process
     * has started when running under an external recovery manager. */
    /* If we get x_fsm_snapshot_wait, we are called from the recovery
     * manager thread */
    /* purecov: begin deadcode */
    case x_fsm_snapshot_wait:
      cont = handle_fsm_snapshot_wait(ctxt);
      break;
      /* purecov: end */

    case x_fsm_exit:
      handle_fsm_exit();
      break;

    default:
      break;
  }
  need_init_cache = 1;
  return cont;
}

/* snapshot_wait_enter state */
/* purecov: begin deadcode */
static int xcom_fsm_snapshot_wait_enter(xcom_actions action, task_arg fsmargs,
                                        xcom_fsm_state *ctxt) {
  (void)action;
  (void)fsmargs;
  push_dbg(D_DETECT | D_FSM | D_FILEOP | D_CONS | D_BASE | D_TRANSPORT);
  IFDBG(D_NONE, FN; STRLIT("state x_snapshot_wait"););
  log_start_max = null_synode;
  log_end_max = null_synode;
  SET_X_FSM_STATE(xcom_fsm_snapshot_wait);
  return 0;
}
/* purecov: end */

/* purecov: begin deadcode */
static int handle_local_snapshot(task_arg fsmargs, xcom_fsm_state *ctxt) {
  update_best_snapshot((gcs_snapshot *)get_void_arg(fsmargs));
  /* When recovering locally, fetch node number from site_def after
   * processing the snapshot */
  note_snapshot(get_site_def()->nodeno);
  send_need_boot();
  pop_dbg();
  SET_X_FSM_STATE(xcom_fsm_recover_wait_enter);
  return 1;
}
/* purecov: end */

/* purecov: begin deadcode */
static int handle_snapshot(task_arg fsmargs, xcom_fsm_state *ctxt) {
  /* Snapshot from another node */
  gcs_snapshot *gcs = (gcs_snapshot *)get_void_arg(fsmargs);
  set_log_end(gcs);
  update_best_snapshot(gcs);
  /* We now have a site, so note that we have processed the local
   * snapshot even if we have not seen one, since if we are here, no
   * local snapshot will ever arrive. This simplifies the test in
   * got_all_snapshots() */
  note_snapshot(get_site_def()->nodeno);
  send_need_boot();
  pop_dbg();
  SET_X_FSM_STATE(xcom_fsm_recover_wait_enter);
  return 1;
}
/* purecov: end */

/* snapshot_wait state */
/* purecov: begin deadcode */
static int xcom_fsm_snapshot_wait(xcom_actions action, task_arg fsmargs,
                                  xcom_fsm_state *ctxt) {
  switch (action) {
    /* If we get x_fsm_local_snapshot, we are called from the recovery
     * manager thread */
    case x_fsm_local_snapshot:
      return handle_local_snapshot(fsmargs, ctxt);

    case x_fsm_snapshot:
      return handle_snapshot(fsmargs, ctxt);

    case x_fsm_timeout:
      /* Will time out if no snapshot available */
      /* If we run under control of the recovery manager, we need to call
       * recovery_end_cb to rendezvous with the recovery manager */
      if (recovery_end_cb) recovery_end_cb();
      pop_dbg();
      SET_X_FSM_STATE(xcom_fsm_start_enter);
      return 1;

    default:
      break;
  }
  return 0;
}
/* purecov: end */

/* recover_wait_enter state */
/* purecov: begin deadcode */
static int xcom_fsm_recover_wait_enter(xcom_actions action, task_arg fsmargs,
                                       xcom_fsm_state *ctxt) {
  (void)action;
  (void)fsmargs;
  push_dbg(D_DETECT | D_FSM | D_FILEOP | D_CONS | D_BASE | D_TRANSPORT);
  IFDBG(D_NONE, FN; STRLIT("state x_recover_wait"););
  if (got_all_snapshots()) {
    /* Need to send message to trigger transition in context of xcom
     * thread */
    send_x_fsm_complete();
  }
  SET_X_FSM_STATE(xcom_fsm_recover_wait);
  return 0;
}
/* purecov: end */

/* recover_wait state */
/* purecov: begin deadcode */
static int xcom_fsm_recover_wait(xcom_actions action, task_arg fsmargs,
                                 xcom_fsm_state *ctxt) {
  if (action == x_fsm_snapshot) {
    gcs_snapshot *gcs = (gcs_snapshot *)get_void_arg(fsmargs);
    set_log_end(gcs);
    update_best_snapshot(gcs);
  } else if (action == x_fsm_timeout || action == x_fsm_complete) {
    /* Wait terminated by timeout or because all nodes have sent a
     * snapshot */
    /* If we run under control of the recovery manager, we need to call
     * recovery_end_cb to rendezvous with the recovery manager */
    if (recovery_end_cb) recovery_end_cb();
    pop_dbg();
    SET_X_FSM_STATE(xcom_fsm_run_enter);
    return 1;
  }
  if (got_all_snapshots()) {
    /* Need to send message to trigger transition in context of xcom
     * thread */
    send_x_fsm_complete();
  }
  return 0;
}
/* purecov: end */

/* run_enter state */
static int xcom_fsm_run_enter(xcom_actions action, task_arg fsmargs,
                              xcom_fsm_state *ctxt) {
  (void)action;
  (void)fsmargs;
  start_config = get_site_def()->boot_key;

  /* Final sanity check of executed_msg */
  if (find_site_def(executed_msg) == nullptr) {
    /* No site_def matches executed_msg, set it to site->start */
    set_executed_msg(get_site_def()->start);
  }

  IFDBG(D_NONE, FN; STRLIT("state x_run"););
  IFDBG(D_BUG, FN; SYCEXP(executed_msg););
  IFDBG(D_BUG, FN; SYCEXP(start_config););
  stop_x_timer();
  if (xcom_run_cb) xcom_run_cb(0);
  client_boot_done = 1;
  netboot_ok = 1;
  set_proposer_startpoint();
  create_proposers();
  set_task(&executor, task_new(executor_task, null_arg, "executor_task",
                               XCOM_THREAD_DEBUG));
  set_task(&sweeper,
           task_new(sweeper_task, null_arg, "sweeper_task", XCOM_THREAD_DEBUG));
  set_task(&detector, task_new(detector_task, null_arg, "detector_task",
                               XCOM_THREAD_DEBUG));
  set_task(&alive_t,
           task_new(alive_task, null_arg, "alive_task", XCOM_THREAD_DEBUG));
  set_task(&cache_task, task_new(cache_manager_task, null_arg,
                                 "cache_manager_task", XCOM_THREAD_DEBUG));

  push_dbg(D_FSM /* | D_EXEC | D_BASE | D_TRANSPORT */);
  SET_X_FSM_STATE(xcom_fsm_run);
  return 1;
}

static int handle_fsm_terminate(task_arg fsmargs, xcom_fsm_state *ctxt) {
  dump_debug_exec_state();
  client_boot_done = 0;
  netboot_ok = 0;
  oom_abort = 0;
  terminate_proposers();
  init_proposers();
  task_terminate(executor);
  set_task(&executor, nullptr);
  task_terminate(sweeper);
  set_task(&sweeper, nullptr);
  task_terminate(detector);
  set_task(&detector, nullptr);
  task_terminate(alive_t);
  set_task(&alive_t, nullptr);
  task_terminate(cache_task);
  set_task(&cache_task, nullptr);

  init_xcom_base(); /* Reset shared variables */
  free_site_defs();
  free_forced_config_site_def();
  wait_forced_config = 0;
  garbage_collect_servers();
  if (xcom_terminate_cb) xcom_terminate_cb(get_int_arg(fsmargs));
  pop_dbg();
  SET_X_FSM_STATE(xcom_fsm_start_enter);
  return 1;
}

static void handle_fsm_force_config(task_arg fsmargs) {
  app_data *a = (app_data *)get_void_arg(fsmargs);
  site_def *s = create_site_def_with_start(a, executed_msg);

  s->boot_key = executed_msg;
  invalidate_servers(get_site_def(), s);
  start_force_config(s, 1);
  wait_forced_config = 1; /* Note that forced config has not yet arrived */
}

/* run state */
static int xcom_fsm_run(xcom_actions action, task_arg fsmargs,
                        xcom_fsm_state *ctxt) {
  switch (action) {
    case x_fsm_terminate:
      return handle_fsm_terminate(fsmargs, ctxt);

    /* purecov: begin deadcode */
    case x_fsm_need_snapshot:
      IFDBG(D_NONE, STRLIT("got snapshot request in x_run state"));
      break;
      /* purecov: end */

    case x_fsm_force_config:
      handle_fsm_force_config(fsmargs);
      break;

    default:
      break;
  }
  return 0;
}

/* Trampoline which loops calling thunks pointed to by ctxt.state_fp until 0
 * is returned. Return pointer to ctxt. */
xcom_fsm_state *xcom_fsm_impl(xcom_actions action, task_arg fsmargs) {
  static xcom_fsm_state ctxt = X_FSM_STATE(xcom_fsm_init);

  G_DEBUG("%f pid %d xcom_id %x state %s action %s", seconds(), xpid(),
          get_my_xcom_id(), ctxt.state_name, xcom_actions_name[action]);
  ADD_DBG(D_FSM, add_event(EVENT_DUMP_PAD, string_arg("state"));
          add_event(EVENT_DUMP_PAD, string_arg(ctxt.state_name));
          add_event(EVENT_DUMP_PAD, string_arg("action"));
          add_event(EVENT_DUMP_PAD, string_arg(xcom_actions_name[action]));
          add_event(EVENT_DUMP_PAD, string_arg("executed_msg"));
          add_synode_event(executed_msg););
#ifdef TASK_EVENT_TRACE
  dump_task_events();
#endif
  /* Crank the state machine until it stops */
  IFDBG(D_BUG, FN; STREXP(ctxt.state_name); STREXP(xcom_actions_name[action]));
  while (ctxt.state_fp(action, fsmargs, &ctxt)) {
    IFDBG(D_BUG, FN; STREXP(ctxt.state_name);
          STREXP(xcom_actions_name[action]));
  }
  return &ctxt;
}

/* Call FSM trampoline and return state name of resulting state */
char const *xcom_fsm(xcom_actions action, task_arg fsmargs) {
  xcom_fsm_state *s = xcom_fsm_impl(action, fsmargs);
  return s->state_name;
}

/* See if we can send a snapshot to another node */
/* purecov: begin deadcode */
static int can_send_snapshot() {
  xcom_fsm_state *state = xcom_fsm_impl(x_fsm_need_snapshot, null_arg);
  return state->state_fp == xcom_fsm_run;
}
/* purecov: end */

void set_app_snap_handler(app_snap_handler x) { handle_app_snap_cb = x; }

/* purecov: begin deadcode */
void set_app_snap_getter(app_snap_getter x) { get_app_snap_cb = x; }
/* purecov: end */

/* Read max n bytes from socket fd into buffer buf */
static result socket_read(connection_descriptor *rfd, void *buf, int n) {
  result ret = {0, 0};

  assert(n >= 0);

  do {
    ret = con_read(rfd, buf, n);
    task_dump_err(ret.funerr);
  } while (ret.val < 0 && can_retry_read(ret.funerr));
  return ret;
}

/* Read exactly n bytes from socket fd into buffer buf */
static int64_t socket_read_bytes(connection_descriptor *rfd, char *p,
                                 uint32_t n) {
  uint32_t left = n;
  char *bytes = p;

  result nread = {0, 0};

  while (left > 0) {
    /*
      socket_read just reads no more than INT_MAX bytes. We should not pass
      a length more than INT_MAX to it.
    */
    int r = (int)MIN(left, INT_MAX);

    nread = socket_read(rfd, bytes, r);
    if (nread.val == 0) {
      return 0;
    } else if (nread.val < 0) {
      return -1;
    } else {
      bytes += nread.val;
      left -= (uint32_t)nread.val;
    }
  }
  assert(left == 0);
  return n;
}

/* Write n bytes from buffer buf to socket fd */
static int64_t socket_write(connection_descriptor *wfd, void *_buf, uint32_t n,
                            connnection_write_method write_function) {
  char *buf = (char *)_buf;
  result ret = {0, 0};

  uint32_t total; /* Keeps track of number of bytes written so far */

  total = 0;
  while (total < n) {
    int w = (int)MIN(n - total, INT_MAX);

    while ((ret = write_function(wfd, buf + total, w)).val < 0 &&
           can_retry_write(ret.funerr)) {
      task_dump_err(ret.funerr);
      IFDBG(D_NONE, FN; STRLIT("retry "); NEXP(total, d); NEXP(n, d));
    }
    if (ret.val <= 0) { /* Something went wrong */
      task_dump_err(ret.funerr);
      return -1;
    } else {
      total += (uint32_t)ret.val; /* Add number of bytes written to total */
    }
  }
  IFDBG(D_TRANSPORT, FN; NEXP(total, u); NEXP(n, u));
  assert(total == n);
  return (total);
}

#define CONNECT_FAIL \
  ret_fd = -1;       \
  goto end

connection_descriptor *xcom_open_client_connection(char const *server,
                                                   xcom_port port) {
  return open_new_connection(server, port);
}

/* Send a protocol negotiation message on connection con */
static int xcom_send_proto(connection_descriptor *con, xcom_proto x_proto,
                           x_msg_type x_type, unsigned int tag) {
  char buf[MSG_HDR_SIZE];
  memset(buf, 0, MSG_HDR_SIZE);

  if (con->fd >= 0) {
    con->snd_tag = tag;
    write_protoversion(VERS_PTR((unsigned char *)buf), x_proto);
    put_header_1_0((unsigned char *)buf, 0, x_type, tag);
    {
      int sent;
      sent = (int)socket_write(con, buf, MSG_HDR_SIZE);
      if (con->fd < 0) {
        return -1;
      }
      return sent;
    }
  } else {
    return -1;
  }
}

static int xcom_recv_proto(connection_descriptor *rfd, xcom_proto *x_proto,
                           x_msg_type *x_type, unsigned int *tag) {
  int n;
  unsigned char header_buf[MSG_HDR_SIZE];
  uint32_t msgsize;

  /* Read length field, protocol version, and checksum */
  n = (int)socket_read_bytes(rfd, (char *)header_buf, MSG_HDR_SIZE);

  if (n != MSG_HDR_SIZE) {
    IFDBG(D_NONE, FN; NDBG(n, d));
    return -1;
  }

  *x_proto = read_protoversion(VERS_PTR(header_buf));
  get_header_1_0(header_buf, &msgsize, x_type, tag);

  return n;
}

enum { TAG_START = 313 };

/**
 * @brief Checks if a given app_data is from a given cargo_type.
 *
 * @param a the app_data
 * @param t the cargo type
 * @return int TRUE (1) if app_data a is from cargo_type t
 */

static inline int is_cargo_type(app_data_ptr a, cargo_type t) {
  return a ? (a->body.c_t == t) : 0;
}

/**
 * @brief Retrieves the address that was used in the add_node request
 *
 * @param a app data containing the node to add
 * @param member address we used to present ourselves to other nodes
 * @return char* a pointer to the address being added.
 */
static char *get_add_node_address(app_data_ptr a, unsigned int *member) {
  char *retval = nullptr;
  if (!is_cargo_type(a, add_node_type)) return nullptr;

  if ((*member) < a->body.app_u_u.nodes.node_list_len) {
    retval = a->body.app_u_u.nodes.node_list_val[(*member)].address;
    (*member)++;
  }

  return retval;
}

int is_node_v4_reachable_with_info(struct addrinfo *retrieved_addr_info) {
  int v4_reachable = 0;

  /* Verify if we are reachable either by V4 and by V6 with the provided
     address. */
  struct addrinfo *my_own_information_loop = nullptr;

  my_own_information_loop = retrieved_addr_info;
  while (!v4_reachable && my_own_information_loop) {
    if (my_own_information_loop->ai_family == AF_INET) {
      v4_reachable = 1;
    }
    my_own_information_loop = my_own_information_loop->ai_next;
  }

  return v4_reachable;
}

int is_node_v4_reachable(char *node_address) {
  int v4_reachable = 0;

  /* Verify if we are reachable either by V4 and by V6 with the provided
     address. */
  struct addrinfo *my_own_information = nullptr;

  checked_getaddrinfo(node_address, nullptr, nullptr, &my_own_information);
  if (my_own_information == nullptr) {
    return v4_reachable;
  }

  v4_reachable = is_node_v4_reachable_with_info(my_own_information);

  if (my_own_information) freeaddrinfo(my_own_information);

  return v4_reachable;
}

int are_we_allowed_to_upgrade_to_v6(app_data_ptr a) {
  /* This should the address we used to present ourselves to other nodes. */
  unsigned int list_member = 0;
  char *added_node = nullptr;

  int is_v4_reachable = 0;
  while ((added_node = get_add_node_address(a, &list_member)) != nullptr) {
    xcom_port my_own_port;
    char my_own_address[IP_MAX_SIZE];
    int ip_and_port_error =
        get_ip_and_port(added_node, my_own_address, &my_own_port);

    if (ip_and_port_error) {
      G_DEBUG("Error retrieving IP and Port information");
      return 0;
    }

    /* Verify if we are reachable either by V4 and by V6 with the provided
       address.
       This means that the other side won't be able to contact us since we
       do not provide a public V4 address */
    if (!(is_v4_reachable = is_node_v4_reachable(my_own_address))) {
      G_ERROR(
          "Unable to add node to a group of older nodes. Please "
          "reconfigure "
          "you local address to an IPv4 address or configure your DNS to "
          "provide "
          "an IPv4 address");
      return 0;
    }
  }

  return is_v4_reachable;
}

int64_t xcom_send_client_app_data(connection_descriptor *fd, app_data_ptr a,
                                  int force) {
  pax_msg *msg = pax_msg_new(null_synode, nullptr);
  uint32_t buflen = 0;
  char *buf = nullptr;
  int64_t retval = 0;
  int serialized = 0;

  if (!proto_done(fd)) {
    xcom_proto x_proto;
    x_msg_type x_type;
    unsigned int tag;
    retval = xcom_send_proto(fd, my_xcom_version, x_version_req, TAG_START);
    G_DEBUG("client sent negotiation request for protocol %d", my_xcom_version);
    if (retval < 0) goto end;
    retval = xcom_recv_proto(fd, &x_proto, &x_type, &tag);
    if (retval < 0) goto end;
    if (tag != TAG_START) {
      retval = -1;
      goto end;
    }
    if (x_type != x_version_reply) {
      retval = -1;
      goto end;
    }

    if (x_proto == x_unknown_proto) {
      G_DEBUG("no common protocol, returning error");
      retval = -1;
      goto end;
    }

    /* This code will check if, in case of an upgrade if:
       - We are a node able to speak IPv6.
       - If we are connecting to a group that does not speak IPv6.
       - If our address is IPv4-compatible in order for the old group to be
       able to contact us back. */
    if (is_cargo_type(a, add_node_type) && x_proto < minimum_ipv6_version() &&
        !are_we_allowed_to_upgrade_to_v6(a)) {
      retval = -1;
      goto end;
    }

    G_DEBUG("client connection will use protocol version %d", x_proto);
    IFDBG(D_NONE, STRLIT("client connection will use protocol version ");
          NDBG(x_proto, u); STRLIT(xcom_proto_to_str(x_proto)));
    fd->x_proto = x_proto;
    set_connected(fd, CON_PROTO);
  }
  msg->a = a;
  msg->to = VOID_NODE_NO;
  msg->op = client_msg;
  msg->force_delivery = force;

  serialized = serialize_msg(msg, fd->x_proto, &buflen, &buf);
  if (serialized) {
    retval = socket_write(fd, buf, buflen);
    if (buflen != retval) {
      IFDBG(D_NONE, FN; STRLIT("write failed "); NDBG(fd->fd, d);
            NDBG(buflen, d); NDBG64(retval));
    }
  } else {
    /* Failed to serialize, set retval accordingly. */
    retval = -1;
  }
  X_FREE(buf);
end:
  msg->a = nullptr; /* Do not deallocate a */
  XCOM_XDR_FREE(xdr_pax_msg, msg);
  return retval;
}

/* purecov: begin tested */
/*
 * Tested by TEST_F(XComMultinodeSmokeTest,
 * 3_nodes_member_crashes_with_dieop_and_joins_again_immediately) GCS smoke
 * test
 */
int64_t xcom_client_send_die(connection_descriptor *fd) {
  if (!fd) return 0;
  uint32_t buflen = 0;
  char *buf = nullptr;
  int64_t retval = 0;
  app_data a;
  pax_msg *msg = pax_msg_new(null_synode, nullptr);

  if (!proto_done(fd)) {
    xcom_proto x_proto;
    x_msg_type x_type;
    unsigned int tag;
    retval = xcom_send_proto(fd, my_xcom_version, x_version_req, TAG_START);
    G_DEBUG("client sent negotiation request for protocol %d", my_xcom_version);
    if (retval < 0) goto end;
    retval = xcom_recv_proto(fd, &x_proto, &x_type, &tag);
    if (retval < 0) goto end;
    if (tag != TAG_START) {
      retval = -1;
      goto end;
    }
    if (x_type != x_version_reply) {
      retval = -1;
      goto end;
    }

    if (x_proto == x_unknown_proto) {
      G_DEBUG("no common protocol, returning error");
      retval = -1;
      goto end;
    }
    G_DEBUG("client connection will use protocol version %d", x_proto);
    IFDBG(D_NONE, STRLIT("client connection will use protocol version ");
          NDBG(x_proto, u); STRLIT(xcom_proto_to_str(x_proto)));
    fd->x_proto = x_proto;
    set_connected(fd, CON_PROTO);
  }
  init_app_data(&a);
  a.body.c_t = app_type;
  msg->a = &a;
  msg->op = die_op;
  /*
    Set the msgno to a value that ensures the die_op will be processed by
    XCom when it is received (it needs to be higher than the msgno of the
    executed_msg, otherwise XCom will simply ignore it).
   */
  msg->synode.msgno = UINT64_MAX;

  serialize_msg(msg, fd->x_proto, &buflen, &buf);
  if (buflen) {
    retval = socket_write(fd, buf, buflen);
    if (buflen != retval) {
      IFDBG(D_NONE, FN; STRLIT("write failed "); NDBG(fd->fd, d);
            NDBG(buflen, d); NDBG64(retval));
    }
    X_FREE(buf);
  }
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
end:
  msg->a = nullptr;
  XCOM_XDR_FREE(xdr_pax_msg, msg);
  return retval > 0 && retval == buflen ? 1 : 0;
}
/* purecov: end */

#ifdef XCOM_STANDALONE
/* purecov: begin deadcode */
int64_t xcom_client_send_data(uint32_t size, char *data,
                              connection_descriptor *fd) {
  if (!fd) return 0;
  app_data a;
  int64_t retval = 0;
  init_app_data(&a);
  a.body.c_t = app_type;
  a.body.app_u_u.data.data_len = size;
  a.body.app_u_u.data.data_val = data;
  retval = xcom_send_client_app_data(fd, &a, 0);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int64_t xcom_client_send_data_no_free(uint32_t size, char *data,
                                      connection_descriptor *fd) {
  if (!fd) return 0;
  app_data a;
  int64_t retval = 0;
  init_app_data(&a);
  a.body.c_t = app_type;
  a.body.app_u_u.data.data_len = size;
  a.body.app_u_u.data.data_val = data;
  retval = xcom_send_client_app_data(fd, &a, 0);
  return retval;
}
/* purecov: end */
#endif

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/* Output warning in log periodically if we receive messages
with a protocol version that does not match our own */
/* purecov: begin inspected */
void warn_protoversion_mismatch(connection_descriptor *rfd) {
  struct sockaddr_storage sock_addr;
  socklen_t sock_size = sizeof(sock_addr);

  if (task_now() - protoversion_warning_time > PROTOVERSION_WARNING_TIMEOUT) {
    if (0 ==
        xcom_getpeername(rfd->fd, (struct sockaddr *)&sock_addr, &sock_size)) {
      char buf[INET6_ADDRSTRLEN + 1];
      struct sockaddr_in *s4 = (struct sockaddr_in *)&sock_addr;
      struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sock_addr;
      char const *ok;

      memset((void *)buf, 0, sizeof(buf));
      if (sock_addr.ss_family == AF_INET) {
        ok = inet_ntop(sock_addr.ss_family, (void *)&s4->sin_addr, buf,
                       sizeof(buf));
      } else {
        ok = inet_ntop(sock_addr.ss_family, (void *)&s6->sin6_addr, buf,
                       sizeof(buf));
      }
      if (ok) {
        G_WARNING(
            "Detected incorrect xcom protocol version in connection from %s "
            "indicates  missing cleanup of, or incorrect, xcom group "
            "definition on remote host. Please upgrade the process running on "
            "%s to a compatible version or stop it.",
            buf, buf);
        protoversion_warning_time = task_now();
      }
    }
  }
}
/* purecov: end */

static pax_msg *socket_read_msg(connection_descriptor *rfd, pax_msg *p)
/* Should buffer reads as well */
{
  int64_t n;
  char *bytes;
  unsigned char header_buf[MSG_HDR_SIZE];
  xcom_proto x_version;
  uint32_t msgsize;
  x_msg_type x_type;
  unsigned int tag;
  int deserialize_ok = 0;

  bytes = nullptr;

  /* Read version, length, type, and tag */
  n = socket_read_bytes(rfd, (char *)header_buf, MSG_HDR_SIZE);

  if (n <= 0) {
    IFDBG(D_NONE, FN; NDBG64(n));
    return nullptr;
  }
  assert(n == MSG_HDR_SIZE);
  x_version = (xcom_proto)get_32(VERS_PTR(header_buf));
/* Check the protocol version before doing anything else */
#ifdef XCOM_PARANOID
  assert(check_protoversion(x_version, rfd->x_proto));
#endif
  if (!check_protoversion(x_version, rfd->x_proto)) {
    /* purecov: begin inspected */
    warn_protoversion_mismatch(rfd);
    return nullptr;
    /* purecov: end */
  }

  /* OK, we can grok this version */

  get_header_1_0(header_buf, &msgsize, &x_type, &tag);

  /* Allocate buffer space for message */
  bytes = (char *)xcom_calloc(1, msgsize);

  /* Read message */
  n = socket_read_bytes(rfd, bytes, msgsize);

  if (n > 0) {
    /* Deserialize message */
    deserialize_ok = deserialize_msg(p, rfd->x_proto, bytes, msgsize);
    IFDBG(D_NONE, FN; STRLIT(" deserialized message"));
  }
  /* Deallocate buffer */
  X_FREE(bytes);
  if (n <= 0 || deserialize_ok == 0) {
    IFDBG(D_NONE, FN; NDBG64(n));
    return nullptr;
  }
  return (p);
}

#ifdef XCOM_STANDALONE
/* purecov: begin deadcode */
int xcom_client_boot(connection_descriptor *fd, node_list *nl,
                     uint32_t group_id) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  retval = (int)xcom_send_client_app_data(
      fd, init_config_with_group(&a, nl, unified_boot_type, group_id), 0);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}
/* purecov: end */
#endif

enum xcom_send_app_wait_result {
  SEND_REQUEST_FAILED = 0,
  RECEIVE_REQUEST_FAILED,
  REQUEST_BOTCHED,
  RETRIES_EXCEEDED,
  REQUEST_OK_RECEIVED,
  REQUEST_FAIL_RECEIVED,
  REQUEST_OK_REDIRECT
};
typedef enum xcom_send_app_wait_result xcom_send_app_wait_result;

/**
 * Send a message and wait for response.
 *
 * The caller is responsible for freeing p after calling this function,
 * i.e. xdr_free((xdrproc_t)xdr_pax_msg, (char *)p)
 */
static xcom_send_app_wait_result xcom_send_app_wait_and_get(
    connection_descriptor *fd, app_data *a, int force, pax_msg *p,
    leader_info_data *leaders) {
  int retval = 0;
  int retry_count = 10; /* Same as 'connection_attempts' */
  pax_msg *rp = nullptr;

  do {
    std::packaged_task<void()> send_client_app_data_task([&]() {
      retval = (int)xcom_send_client_app_data(fd, a, force);
      if (retval >= 0) rp = socket_read_msg(fd, p);
    });

    auto send_client_app_data_result = send_client_app_data_task.get_future();
    std::thread(std::move(send_client_app_data_task)).detach();

    std::future_status request_status = send_client_app_data_result.wait_for(
        std::chrono::seconds(XCOM_SEND_APP_WAIT_TIMEOUT));
    if ((retval < 0) || request_status == std::future_status::timeout) {
      memset(p, 0, sizeof(*p)); /* before return so caller can free p */
      G_INFO(
          "Client sent negotiation request for protocol failed. Please check "
          "the remote node log for more details.")
      return SEND_REQUEST_FAILED;
    }

    if (rp) {
      client_reply_code cli_err = rp->cli_err;
      switch (cli_err) {
        case REQUEST_OK:
          return REQUEST_OK_RECEIVED;
        case REQUEST_FAIL:
          G_INFO(
              "Sending a request to a remote XCom failed. Please check the "
              "remote node log for more details.")
          return REQUEST_FAIL_RECEIVED;
        case REQUEST_RETRY:
          if (retry_count > 1) xdr_free((xdrproc_t)xdr_pax_msg, (char *)p);
          G_INFO(
              "Retrying a request to a remote XCom. Please check the remote "
              "node log for more details.")
          xcom_sleep(1);
          break;
        case REQUEST_REDIRECT:
          G_DEBUG("cli_err %d", cli_err);
          if (leaders && rp->rd && rp->rd->rt == leader_info) {
            *leaders = steal_leader_info_data(rp->rd->reply_data_u.leaders);
          }
          xdr_free((xdrproc_t)xdr_pax_msg, (char *)p);
          return REQUEST_OK_REDIRECT;
        default:
          G_WARNING("XCom client connection has received an unknown response.");
          return REQUEST_BOTCHED;
      }
    } else {
      G_WARNING("Reading a request from a remote XCom failed.");
      return RECEIVE_REQUEST_FAILED;
    }
  } while (--retry_count);
  /* Timeout after REQUEST_RETRY has been received 'retry_count' times */
  G_MESSAGE(
      "Request failed: maximum number of retries (10) has been exhausted.");
  return RETRIES_EXCEEDED;
}

static int xcom_send_app_wait(connection_descriptor *fd, app_data *a, int force,
                              leader_info_data *leaders) {
  pax_msg p;
  int result = 0;
  memset(&p, 0, sizeof(p));
  xcom_send_app_wait_result res =
      xcom_send_app_wait_and_get(fd, a, force, &p, leaders);
  switch (res) {
    case SEND_REQUEST_FAILED:
    case RECEIVE_REQUEST_FAILED:
    case REQUEST_BOTCHED:
    case RETRIES_EXCEEDED:
    case REQUEST_FAIL_RECEIVED:
    case REQUEST_OK_REDIRECT:
      result = 0;
      break;
    case REQUEST_OK_RECEIVED:
      result = 1;
      break;
  }
  xdr_free((xdrproc_t)xdr_pax_msg, (char *)&p);
  return result;
}

int xcom_send_cfg_wait(connection_descriptor *fd, node_list *nl,
                       uint32_t group_id, cargo_type ct, int force) {
  app_data a;
  int retval = 0;
  IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_list(nl)););
  retval = xcom_send_app_wait(fd, init_config_with_group(&a, nl, ct, group_id),
                              force, nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                         uint32_t group_id) {
  if (!fd) return 0;
  u_int i;
  for (i = 0; i < nl->node_list_len; i++) {
    assert(nl->node_list_val[i].proto.max_proto > x_unknown_proto);
  }
  return xcom_send_cfg_wait(fd, nl, group_id, add_node_type, 0);
}

int xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                            uint32_t group_id) {
  if (!fd) return 0;
  return xcom_send_cfg_wait(fd, nl, group_id, remove_node_type, 0);
}

static int xcom_check_reply(int const res) {
  return res == REQUEST_OK_RECEIVED;
}

#if 0
/* purecov: begin deadcode */
int xcom_client_get_event_horizon(connection_descriptor *fd, uint32_t group_id,
                                  xcom_event_horizon *event_horizon) {
  if (!fd) return 0;
  pax_msg p;
  app_data a;
  int result = 0;

  memset(&p, 0, sizeof(p));

  xcom_send_app_wait_result res = xcom_send_app_wait_and_get(
      fd, init_get_event_horizon_msg(&a, group_id), 0, &p, 0);
  result = xcom_check_reply(res);
  if (result) {
    *event_horizon = p.event_horizon;
  }

  xdr_free((xdrproc_t)xdr_pax_msg, (char *)&p);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);

  return result;
}
/* purecov: end */

/* purecov: begin deadcode */
int xcom_client_set_event_horizon(connection_descriptor *fd, uint32_t group_id,
                                  xcom_event_horizon event_horizon) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  retval = xcom_send_app_wait(
      fd, init_set_event_horizon_msg(&a, group_id, event_horizon), 0, 0);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}
/* purecov: end */
#endif

int xcom_client_get_synode_app_data(connection_descriptor *const fd,
                                    uint32_t group_id,
                                    synode_no_array *const synodes,
                                    synode_app_data_array *const reply) {
  if (!fd) return 0;
  bool_t const success = TRUE;
  bool_t const failure = FALSE;
  bool_t result = failure;
  pax_msg p;
  app_data a;
  u_int const nr_synodes_requested = synodes->synode_no_array_len;

  /* This call moves, as in C++ move semantics, synodes into app_data a. */
  init_get_synode_app_data_msg(&a, group_id, synodes);

  {
    xcom_send_app_wait_result res =
        xcom_send_app_wait_and_get(fd, &a, 0, &p, nullptr);
    switch (res) {
      case RECEIVE_REQUEST_FAILED:
      case REQUEST_BOTCHED:
      case RETRIES_EXCEEDED:
      case SEND_REQUEST_FAILED:
      case REQUEST_FAIL_RECEIVED:
      case REQUEST_OK_REDIRECT: {
        G_TRACE(
            "xcom_client_get_synode_app_data: XCom did not have the required "
            "%u "
            "synodes.",
            nr_synodes_requested);
        break;
      }
      case REQUEST_OK_RECEIVED: {
        u_int const nr_synodes_received =
            p.requested_synode_app_data.synode_app_data_array_len;
        G_TRACE(
            "xcom_client_get_synode_app_data: Got %u synode payloads, we "
            "asked "
            "for %u.",
            nr_synodes_received, nr_synodes_requested);

        /* This should always be TRUE.
         * But rather than asserting it, let's treat an unexpected number of
         * synode payloads in the reply as a failure. */
        if (nr_synodes_received == nr_synodes_requested) {
          /* Move (as in C++ move semantics) into reply */
          synode_app_data_array_move(reply, &p.requested_synode_app_data);
          result = success;
        }
        break;
      }
    }
  }

  xdr_free((xdrproc_t)xdr_pax_msg, (char *)&p);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);

  return result;
}

#ifdef NOTDEF
/* Not completely implemented, need to be handled properly
   when received as a client message in dispatch_op.
   Should have separate opcode from normal add/remove,
   like force config_type */
int xcom_client_force_add_node(connection_descriptor *fd, node_list *nl,
                               uint32_t group_id) {
  if (!fd) return 0;
  return xcom_send_cfg_wait(fd, nl, group_id, add_node_type, 1);
}

int xcom_client_force_remove_node(connection_descriptor *fd, node_list *nl,
                                  uint32_t group_id) {
  if (!fd) return 0;
  return xcom_send_cfg_wait(fd, nl, group_id, remove_node_type, 1);
}
#endif

/* purecov: begin deadcode */
int xcom_client_enable_arbitrator(connection_descriptor *fd) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = enable_arbitrator;
  retval = xcom_send_app_wait(fd, &a, 0, nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}
/* purecov: end */

/* purecov: begin deadcode */
int xcom_client_disable_arbitrator(connection_descriptor *fd) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = disable_arbitrator;
  retval = xcom_send_app_wait(fd, &a, 0, nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}
/* purecov: end */

/* purecov: begin deadcode */
int xcom_client_set_cache_limit(connection_descriptor *fd,
                                uint64_t cache_limit) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = set_cache_limit;
  a.body.app_u_u.cache_limit = cache_limit;
  retval = xcom_send_app_wait(fd, &a, 0, nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}
/* purecov: end */

int xcom_client_convert_into_local_server(connection_descriptor *const fd) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  retval = xcom_send_app_wait(fd, init_convert_into_local_server_msg(&a), 0,
                              nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

/* Set max number of leaders */
void init_set_max_leaders(uint32_t group_id, app_data *a, node_no max_leaders) {
  init_app_data(a);
  a->app_key.group_id = a->group_id = group_id;
  a->body.c_t = set_max_leaders;
  a->body.app_u_u.max_leaders = max_leaders;
}

/* Set max number of leaders */
int xcom_client_set_max_leaders(connection_descriptor *fd, node_no max_leaders,
                                uint32_t group_id) {
  if (!fd) return 0;
  app_data a;
  int retval = 0;
  init_set_max_leaders(group_id, &a, max_leaders);
  retval = xcom_send_app_wait(fd, &a, 0, nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

leader_array new_leader_array(u_int n, char const *names[]) {
  leader_array leaders = alloc_leader_array(n);
  for (u_int i = 0; i < n; i++) {
    leaders.leader_array_val[i].address = strdup(names[i]);
  }
  return leaders;
}

/* Set new set of active leaders. Does not deallocate leaders. */
void init_set_leaders(uint32_t group_id, app_data *a,
                      leader_array const leaders) {
  init_app_data(a);
  a->app_key.group_id = a->group_id = group_id;
  a->body.c_t = set_leaders_type;
  // We could have avoided this copy, but having leaders as const
  // makes it easier to reason about sharing
  a->body.app_u_u.leaders = clone_leader_array(leaders);
}

/* Set new set of active leaders. */
void init_set_leaders(uint32_t group_id, app_data *a, u_int n,
                      char const *names[]) {
  leader_array leaders = new_leader_array(n, names);
  init_set_leaders(group_id, a, leaders);
  // leaders have been copied, so deallocate
  xdr_free((xdrproc_t)xdr_leader_array, (char *)(&leaders));
}

void init_set_leaders(uint32_t group_id, app_data *leader_app,
                      leader_array const leaders, app_data *max_app,
                      node_no max_leaders) {
  init_set_leaders(group_id, leader_app, leaders);
  init_set_max_leaders(group_id, max_app, max_leaders);
  leader_app->next = max_app;
}

void init_set_leaders(uint32_t group_id, app_data *leader_app, u_int n,
                      char const *names[], app_data *max_app,
                      node_no max_leaders) {
  leader_array leaders = new_leader_array(n, names);
  init_set_leaders(group_id, leader_app, leaders, max_app, max_leaders);
  // leaders have been copied, so deallocate
  xdr_free((xdrproc_t)xdr_leader_array, (char *)(&leaders));
}

/* Set new set of active leaders. */
int xcom_client_set_leaders(connection_descriptor *fd, u_int n,
                            char const *names[], uint32_t group_id) {
  if (!fd) return 0;
  app_data a;
  init_set_leaders(group_id, &a, n, names);
  int retval = xcom_send_app_wait(fd, &a, 0, nullptr);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

std::unique_ptr<Network_provider_management_interface>
get_network_management_interface() {
  return std::make_unique<Network_Management_Interface>();
}

std::unique_ptr<Network_provider_operations_interface>
get_network_operations_interface() {
  return std::make_unique<Network_Management_Interface>();
}

/* Set new set of active leaders and number of leaders. */
int xcom_client_set_leaders(connection_descriptor *fd, u_int n,
                            char const *names[], node_no max_leaders,
                            uint32_t group_id) {
  if (!fd) return 0;
  app_data leader_app;
  app_data max_app;
  int retval = 0;
  init_set_leaders(group_id, &leader_app, n, names, &max_app, max_leaders);
  retval = xcom_send_app_wait(fd, &leader_app, 0, nullptr);
  // leader_app and max_app have been linked, so unlink
  // to avoid deallocating the stack objects.
  leader_app.next = nullptr;
  max_app.next = nullptr;
  xdr_free((xdrproc_t)xdr_app_data, (char *)&leader_app);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&max_app);
  return retval;
}

int xcom_client_get_leaders(connection_descriptor *fd, uint32_t group_id,
                            leader_info_data *leaders) {
  if (!fd) return 0;
  pax_msg p;
  app_data a;
  int result = 0;

  memset(&p, 0, sizeof(p));

  xcom_send_app_wait_result res = xcom_send_app_wait_and_get(
      fd, init_get_msg(&a, group_id, get_leaders_type), 0, &p, nullptr);
  result = xcom_check_reply(res);
  if (result) {
    // Steal the returned data
    *leaders = steal_leader_info_data(p.rd->reply_data_u.leaders);
  }

  xdr_free((xdrproc_t)xdr_pax_msg, (char *)&p);
  xdr_free((xdrproc_t)xdr_app_data, (char *)&a);

  return result;
}

#if 0
/* Called when leader changes */
[[maybe_unused]] static xcom_election_cb election_cb = NULL;

void set_xcom_election_cb(xcom_election_cb x) { election_cb = x; }
#endif

// The timer code and the associated Paxos FSM stuff is only used for
// tracking/debugging Paxos state transitions at the moment, but I believe the
// FSM is correct, and if used for actually handling the incoming messages,
// would make the code simpler, and easier to understand and reason about by
// making lots of tests scattered around in the code unnecessary. I have had
// this on the backburner for a long time, and used the single writer worklog
// to test it, but did not dare to actually replace the existing logic in the
// scope of this worklog.Take it as a hint for future improvement of the code
// base...

// The time queue as configured now will allow up to 10 seconds delay with
// TICK_PERIOD (0.01) seconds granularity. All machines which map to the same
// time slot will wake up simultaneously. The complexity when inserting or
// removing a pax_machine is O(1), but this is somewhat offset by the need to
// advance the current tick for every TICK_PERIOD. Not a problem in practice,
// and the code is dead simple.

/* Max number of ticks before wrapping. With 10 ms per step, this will give a
 * max delay of 10 seconds, which is plenty for the Paxos timers */

enum { paxos_timer_range = 1000 };
/* Ten milliseconds granularity is sufficient */
#define TICK_PERIOD 0.01

/* The index into the time queue */
static unsigned int current_tick = 0;

/* The time queue is an array of timers. Each timer is the head of
a possibly empty list of timers */

static linkage time_queue[paxos_timer_range];

static void init_time_queue() {
  unsigned int i;
  for (i = 0; i < paxos_timer_range; i++) {
    link_init(&time_queue[i], TYPE_HASH("time_queue"));
  }
}

/* Put pax_machine into the time queue at the correct place */
static void paxos_twait(pax_machine *p, unsigned int t) {
  /* Guard against 0 delay, which would become max delay */
  if (0 == t) t = 1;
  unsigned int pos = (current_tick + t) % paxos_timer_range;
  link_into(&p->watchdog, &time_queue[pos]);
  assert(!link_empty(&time_queue[pos]));
}

/* Remove pax_machine from timer queue */
static void paxos_twait_cancel(pax_machine *p) { link_out(&p->watchdog); }

/* Wake all pax_machines waiting at time slot t */
static void paxos_wakeup(unsigned int t) {
  linkage *head = &time_queue[t];
  linkage *p;
  if (!link_empty(head)) {
    IFDBG(D_CONS, FN; NUMEXP(t); NUMEXP(link_empty(head)));
  }
  while (!link_empty(head)) {
    p = link_first(head);
    paxos_timeout(container_of(p, pax_machine, watchdog));
    link_out(p);
  }
}

/* Advance current_tick to next slot and wake all pax_machines there */
static void paxos_timer_advance() {
  current_tick = (current_tick + 1) % paxos_timer_range;
  paxos_wakeup(current_tick);
}

/* Fire any expired timer for a Paxos machine */
static int paxos_timer_task(task_arg arg [[maybe_unused]]) {
  DECL_ENV
  double start;
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  TASK_BEGIN
  ep->start = task_now();
  while (!xcom_shutdown) {
    ep->start += TICK_PERIOD;
    TASK_DELAY_UNTIL(ep->start);
    paxos_timer_advance();
  }
  FINALLY
  IFDBG(D_CONS, FN; STRLIT(" shutdown "));
  TASK_END;
}

/* The state functions/thunks */
static int paxos_fsm_p1_master_enter(pax_machine *paxos, site_def const *site,
                                     paxos_event event, pax_msg *mess);

static int paxos_fsm_p1_master_wait(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess);

static int paxos_fsm_p2_master_enter(pax_machine *paxos, site_def const *site,
                                     paxos_event event, pax_msg *mess);

static int paxos_fsm_p2_master_wait(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess);

static int paxos_fsm_p2_slave_enter(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess);

static int paxos_fsm_p2_slave_wait(pax_machine *paxos, site_def const *site,
                                   paxos_event event, pax_msg *mess);

static int paxos_fsm_p3_master_wait(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess);

static int paxos_fsm_p3_slave_enter(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess);

static int paxos_fsm_p3_slave_wait(pax_machine *paxos, site_def const *site,
                                   paxos_event event, pax_msg *mess);

static int paxos_fsm_finished(pax_machine *paxos, site_def const *site,
                              paxos_event event, pax_msg *mess);

typedef void (*paxos_state_action)(pax_machine *paxos, site_def const *site,
                                   pax_msg *mess);

static int accept_new_prepare(pax_machine *paxos, pax_msg *mess) {
  return noop_match(paxos, mess) ||
         gt_ballot(mess->proposal, paxos->acceptor.promise);
}

static int accept_new_accept(pax_machine *paxos, pax_msg *mess) {
  return noop_match(paxos, mess) ||
         !gt_ballot(paxos->acceptor.promise, mess->proposal);
}

static int own_message(pax_msg *mess, site_def const *site) {
  return is_local_node(mess->from, site);
}

// Default paxos timeout in ticks
// Change this if the FSM is used for anything else than debugging
unsigned int constexpr const paxos_default_timeout = 100;

/* You are in a maze of little twisting functions ... */

static void action_paxos_prepare(pax_machine *paxos, site_def const *site,
                                 pax_msg *mess) {
  if (own_message(mess, site)) {
    /* Wait for ack_prepare */
    SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p1_master_wait);
  } else {
    /* Wait for accept */
    SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_slave_enter);
  }
  paxos_twait(paxos, paxos_default_timeout);
}

static void action_paxos_accept(pax_machine *paxos, site_def const *site,
                                pax_msg *mess) {
  if (own_message(mess, site)) {
    /* Wait for ack_accept */
    SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_master_wait);
  } else {
    /* Wait for learn */
    SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p3_slave_enter);
  }
  paxos_twait(paxos, paxos_default_timeout);
}

static void action_paxos_learn(pax_machine *paxos, site_def const *site,
                               pax_msg *mess) {
  (void)site;
  (void)mess;
  /* We are finished */
  SET_PAXOS_FSM_STATE(paxos, paxos_fsm_finished);
  paxos_twait_cancel(paxos);
}

static void action_paxos_start(pax_machine *paxos, site_def const *site,
                               pax_msg *mess) {
  (void)site;
  (void)mess;
  /* Find value of this instance */
  SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p1_master_enter);
  paxos_twait(paxos, paxos_default_timeout);
}

static void action_new_prepare(pax_machine *paxos, site_def const *site,
                               pax_msg *mess) {
  (void)site;
  if (accept_new_prepare(paxos, mess)) {
    /* Wait for accept */
    if (own_message(mess, site)) {
      /* Wait for ack_prepare */
      SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p1_master_enter);
    } else {
      /* Wait for accept */
      SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_slave_enter);
    }
    paxos_twait(paxos, paxos_default_timeout);
  }
}

static void action_ack_prepare(pax_machine *paxos, site_def const *site,
                               pax_msg *mess) {
  (void)mess;
  if (check_propose(site, paxos)) {
    /* Wait for accept */
    SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_master_enter);
  }
}

static void action_new_accept(pax_machine *paxos, site_def const *site,
                              pax_msg *mess) {
  if (accept_new_accept(paxos, mess)) {
    /* Wait for accept */
    if (own_message(mess, site)) {
      /* Wait for ack_accept */
      SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_master_enter);
    } else {
      /* Wait for learn */
      SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p3_slave_enter);
    }
    paxos_twait(paxos, paxos_default_timeout);
  }
}

static void action_ack_accept(pax_machine *paxos, site_def const *site,
                              pax_msg *mess) {
  (void)mess;
  if (learn_ok(site, paxos)) {
    /* Wait for learn message */
    SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p3_master_wait);
  }
}

static void action_ignorant(pax_machine *paxos, site_def const *site,
                            pax_msg *mess) {
  (void)paxos;
  (void)site;
  (void)mess;
}

/* Dispatch tables for each state */
paxos_state_action p1_idle_vtbl[last_p_event] = {action_paxos_prepare,
                                                 nullptr,
                                                 action_paxos_accept,
                                                 nullptr,
                                                 action_paxos_learn,
                                                 action_paxos_start,
                                                 nullptr};
paxos_state_action p1_master_enter_vtbl[last_p_event] = {action_new_prepare,
                                                         action_ack_prepare,
                                                         action_new_accept,
                                                         nullptr,
                                                         action_paxos_learn,
                                                         nullptr,
                                                         nullptr};
paxos_state_action p1_master_wait_vtbl[last_p_event] = {action_new_prepare,
                                                        action_ack_prepare,
                                                        action_new_accept,
                                                        nullptr,
                                                        action_paxos_learn,
                                                        nullptr,
                                                        nullptr};
paxos_state_action p2_master_enter_vtbl[last_p_event] = {action_new_accept,
                                                         nullptr,
                                                         action_new_accept,
                                                         action_ack_accept,
                                                         action_paxos_learn,
                                                         nullptr,
                                                         nullptr};
paxos_state_action p2_master_wait_vtbl[last_p_event] = {action_new_prepare,
                                                        nullptr,
                                                        action_new_accept,
                                                        action_ack_accept,
                                                        action_paxos_learn,
                                                        nullptr,
                                                        nullptr};
paxos_state_action p2_slave_wait_vtbl[last_p_event] = {action_new_prepare,
                                                       nullptr,
                                                       action_new_accept,
                                                       nullptr,
                                                       action_paxos_learn,
                                                       nullptr,
                                                       nullptr};
paxos_state_action p3_master_wait_vtbl[last_p_event] = {action_new_prepare,
                                                        nullptr,
                                                        action_new_accept,
                                                        nullptr,
                                                        action_paxos_learn,
                                                        nullptr,
                                                        nullptr};
paxos_state_action p3_slave_wait_vtbl[last_p_event] = {action_new_prepare,
                                                       nullptr,
                                                       action_new_accept,
                                                       nullptr,
                                                       action_paxos_learn,
                                                       nullptr,
                                                       nullptr};
paxos_state_action p_finished_vtbl[last_p_event] = {
    action_ignorant, nullptr, action_ignorant, nullptr,
    nullptr,         nullptr, nullptr};

static inline void dispatch_p_event(paxos_state_action *vtbl,
                                    pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess) {
  if (vtbl[event]) {
    vtbl[event](paxos, site, mess);
  }
}

/* init state */
int paxos_fsm_idle(pax_machine *paxos, site_def const *site, paxos_event event,
                   pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p1_idle_vtbl, paxos, site, event, mess);
  return 0;
}

/* Phase 1 master enter */
static int paxos_fsm_p1_master_enter(pax_machine *paxos, site_def const *site,
                                     paxos_event event, pax_msg *mess) {
  (void)site;
  (void)event;
  (void)mess;
  IFDBG(D_CONS, FN;);
  /* Send prepare and start timer */
  SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p1_master_wait);
  return 0;
}

/* Phase 1 master wait */
static int paxos_fsm_p1_master_wait(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p1_master_wait_vtbl, paxos, site, event, mess);
  return 0;
}

/* Phase 2 master enter */
static int paxos_fsm_p2_master_enter(pax_machine *paxos, site_def const *site,
                                     paxos_event event, pax_msg *mess) {
  (void)site;
  (void)event;
  (void)mess;
  IFDBG(D_CONS, FN;);
  /* Send prepare and start timer */
  SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_master_wait);
  return 0;
}

/* Phase 2 master wait */
static int paxos_fsm_p2_master_wait(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p2_master_wait_vtbl, paxos, site, event, mess);
  return 0;
}

/* Phase 2 slave enter */
static int paxos_fsm_p2_slave_enter(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess) {
  (void)site;
  (void)event;
  (void)mess;
  IFDBG(D_CONS, FN;);
  /* Start timer */
  SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p2_slave_wait);
  return 1;
}

/* Phase 2 slave wait */
static int paxos_fsm_p2_slave_wait(pax_machine *paxos, site_def const *site,
                                   paxos_event event, pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p2_slave_wait_vtbl, paxos, site, event, mess);
  return 0;
}

/* Phase 3 master wait */
static int paxos_fsm_p3_master_wait(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p3_master_wait_vtbl, paxos, site, event, mess);
  return 0;
}

/* Phase 3 slave enter */
static int paxos_fsm_p3_slave_enter(pax_machine *paxos, site_def const *site,
                                    paxos_event event, pax_msg *mess) {
  (void)site;
  (void)event;
  (void)mess;
  IFDBG(D_CONS, FN;);
  /* Start timer */
  SET_PAXOS_FSM_STATE(paxos, paxos_fsm_p3_slave_wait);
  return 1;
}

/* Phase 3 slave wait */
static int paxos_fsm_p3_slave_wait(pax_machine *paxos, site_def const *site,
                                   paxos_event event, pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p3_slave_wait_vtbl, paxos, site, event, mess);
  return 0;
}

/* Finished */
static int paxos_fsm_finished(pax_machine *paxos, site_def const *site,
                              paxos_event event, pax_msg *mess) {
  IFDBG(D_CONS, FN;);
  dispatch_p_event(p_finished_vtbl, paxos, site, event, mess);
  return 0;
}

#define X(b) #b
const char *paxos_event_name[] = {p_events};
#undef X

/* Trampoline which loops calling thunks pointed to by paxos->state.state_fp
 * until 0 is returned. */
static void paxos_fsm(pax_machine *paxos, site_def const *site,
                      paxos_event event, pax_msg *mess) {
  /* Crank the state machine until it stops */
  IFDBG(D_CONS, FN; PTREXP(paxos); SYCEXP(paxos->synode);
        BALCEXP(mess->proposal); STRLIT(paxos->state.state_name); STRLIT(" : ");
        STRLIT(paxos_event_name[event]));
  while (paxos->state.state_fp(paxos, site, event, mess)) {
    IFDBG(D_CONS, FN; PTREXP(paxos); SYCEXP(paxos->synode);
          BALCEXP(mess->proposal); STRLIT(paxos->state.state_name);
          STRLIT(" : "); STRLIT(paxos_event_name[event]));
  }
}
