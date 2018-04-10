/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef _WIN32
#include <poll.h>
#endif

/**
  @file
  plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.c
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
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"

#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"

#ifndef _WIN32
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef __linux__
#include <sys/sockio.h>
#endif
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_os.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xdr_utils.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#ifdef XCOM_HAVE_OPENSSL
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_ssl_transport.h"
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/bitset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/pax_msg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_net.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cache.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_msg_queue.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_recover.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_statistics.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_vp_str.h"

#ifdef XCOM_HAVE_OPENSSL
#ifdef WIN32
// In OpenSSL before 1.1.0, we need this first.
#include <winsock2.h>
#endif  // WIN32
#include <wolfssl_fix_namespace_pollution_pre.h>

#include <openssl/ssl.h>

#include <wolfssl_fix_namespace_pollution.h>
#endif

/* {{{ Defines and constants */

#define SYS_STRERROR_SIZE 512
unsigned int event_horizon = EVENT_HORIZON_MIN;

static void set_event_horizon(unsigned int eh) MY_ATTRIBUTE((unused));
/* purecov: begin deadcode */
static void set_event_horizon(unsigned int eh) {
  DBGOUT(FN; NDBG(eh, u));
  event_horizon = eh;
}
/* purecov: end */

/* Skip prepare for first ballot */
#ifdef ALWAYS_THREEPHASE
int const threephase = 1;
#else
int const threephase = 0;
#endif

/* }}} */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/retry.h"

#ifdef NODE_0_IS_ARBITRATOR
int ARBITRATOR_HACK = 1;
#else
int ARBITRATOR_HACK = 0;
#endif

extern void handle_need_boot(server *srv, site_def const *s, node_no node);

static int const no_duplicate_payload = 1;

/* Use buffered read when reading messages from the network */
static int use_buffered_read = 1;

/* {{{ Forward declarations */
long xcom_unique_long(void);
void get_host_name(char *a, char *name);

static double wakeup_delay(double old);

/* Task types */
static int proposer_task(task_arg arg);
static int executor_task(task_arg arg);
static int sweeper_task(task_arg arg);
extern int alive_task(task_arg arg);
extern int detector_task(task_arg arg);

static int finished(pax_machine *p);
static int accepted(pax_machine *p);
static int started(pax_machine *p);
static synode_no first_free_synode(synode_no msgno);
static void free_forced_config_site_def();
static void force_pax_machine(pax_machine *p, int enforcer);

extern void bit_set_or(bit_set *x, bit_set const *y);

/* }}} */

/* {{{ Global variables */

int xcom_shutdown = 0;  /* Xcom_Shutdown flag */
synode_no executed_msg; /* The message we are waiting to execute */
synode_no max_synode;   /* Max message number seen so far */
task_env *boot = NULL;
task_env *detector = NULL;
task_env *killer = NULL;
task_env *net_boot = NULL;
task_env *net_recover = NULL;
void *xcom_thread_input = 0;

static void init_proposers();
void initialize_lsn(uint64_t n);

void init_base_vars() {
  xcom_shutdown = 0;          /* Xcom_Shutdown flag */
  executed_msg = null_synode; /* The message we are waiting to execute */
  max_synode = null_synode;   /* Max message number seen so far */
  boot = NULL;
  detector = NULL;
  killer = NULL;
  net_boot = NULL;
  net_recover = NULL;
  xcom_thread_input = 0;
}

static task_env *executor = NULL;
static task_env *sweeper = NULL;
static task_env *retry = NULL;
static task_env *proposer[PROPOSERS];
static task_env *alive_t = NULL;

static uint32_t my_id = 0;        /* Unique id of this instance */
static synode_no current_message; /* Current message number */
static synode_no
    last_config_modification_id; /*Last configuration change proposal*/
static uint64_t lsn = 0;         /* Current log sequence number */

synode_no get_current_message() { return current_message; }

static channel prop_input_queue; /* Proposer task input queue */

/* purecov: begin deadcode */
channel *get_prop_input_queue() { return &prop_input_queue; }
/* purecov: end */

extern int client_boot_done;
extern int netboot_ok;
extern int booting;

extern start_t start_type;
static linkage exec_wait = {
    0, &exec_wait, &exec_wait}; /* Executor will wake up tasks sleeping here */

static struct {
  int n;
  unsigned long id[MAX_DEAD];
} dead_sites;

synode_no get_max_synode() { return max_synode; }

static synode_no add_event_horizon(synode_no s) {
  s.msgno += event_horizon + 1;
  s.node = 0;
  return s;
}

void set_max_synode_from_unified_boot(synode_no unified_boot_synode) {
  DBGOUT(FN; SYCEXP(unified_boot_synode); SYCEXP(max_synode);
         SYCEXP(add_event_horizon(unified_boot_synode)));
  if (synode_gt(add_event_horizon(unified_boot_synode), max_synode))
    set_max_synode(add_event_horizon(unified_boot_synode));
  DBGOUT(FN; SYCEXP(unified_boot_synode); SYCEXP(max_synode);
         SYCEXP(add_event_horizon(unified_boot_synode)));
}

/**
   Set node group
*/
void set_group(uint32_t id) {
  MAY_DBG(FN; STRLIT("changing group id of global variables ");
          NDBG((unsigned long)id, lu););
  /*	set_group_id(id); */
  current_message.group_id = id;
  executed_msg.group_id = id;
  max_synode.group_id = id;
  set_log_group_id(id);
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

define_xdr_funcs(node_no)

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

#if 0
/* Given message number, compute which node it belongs to */
static unsigned int	msgno_to_node(synode_no msgno)
{
	return msgno.node;
}
#endif

synode_no incr_synode(synode_no synode) {
  synode_no ret = synode;
  ret.node++;
  if (ret.node >= get_maxnodes(find_site_def(synode))) {
    ret.node = 0;
    ret.msgno++;
  }
  /* 	DBGOUT(FN; SYCEXP(synode); SYCEXP(ret)); */
  return ret; /* Change this if we change message number type */
}

synode_no decr_synode(synode_no synode) {
  synode_no ret = synode;
  if (ret.node == 0) {
    ret.msgno--;
    ret.node = get_maxnodes(find_site_def(ret));
  }
  ret.node--;
  return ret; /* Change this if we change message number type */
}

static void skip_value(pax_msg *p) {
  MAY_DBG(FN; SYCEXP(p->synode));
  p->op = learn_op;
  p->msg_type = no_op;
}

/* }}} */

/* {{{ Utilities and debug */

/* purecov: begin deadcode */
/* Print message and exit */
static void pexitall(int i) {
  int *r = (int *)calloc(1, sizeof(int));
  *r = i;
  DBGOUT(FN; NDBG(i, d); STRLIT("time "); NDBG(task_now(), f););
  XCOM_FSM(xa_terminate, int_arg(i)); /* Tell xcom to stop */
}
  /* purecov: end */

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
static int ignoresig(int signum) { return 0; }
#endif

  /* }}} */

#if 0
static void	dbg_machine_and_msg(pax_machine *p, pax_msg *pm)
{
  GET_GOUT;

  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE))
    return;

  STRLIT("machine ");
  ADD_GOUT(dbg_pax_machine(p));
  STRLIT(" ");
  STRLIT("msg ");
  COPY_AND_FREE_GOUT(dbg_pax_msg(pm));
  PRINT_GOUT;
  FREE_GOUT;
}

#endif

static int recently_active(pax_machine *p) {
  MAY_DBG(FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p);
          STRLIT(p->learner.msg ? pax_op_to_str(p->learner.msg->op) : "NULL");
          NDBG(p->last_modified, f); NDBG(task_now(), f));
  return p->last_modified != 0.0 &&
         (p->last_modified + 0.5 + median_time()) > task_now();
}

static inline int finished(pax_machine *p) {
  MAY_DBG(FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p);
          STRLIT(p->learner.msg ? pax_op_to_str(p->learner.msg->op) : "NULL"););
  return p->learner.msg && (p->learner.msg->op == learn_op ||
                            p->learner.msg->op == tiny_learn_op);
}

int pm_finished(pax_machine *p) { return finished(p); }

static inline int accepted(pax_machine *p) {
  MAY_DBG(FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p); STRLIT(
              p->acceptor.msg ? pax_op_to_str(p->acceptor.msg->op) : "NULL"););
  return p->acceptor.msg && p->acceptor.msg->op != initial_op;
}

static inline int accepted_noop(pax_machine *p) {
  MAY_DBG(FN; SYCEXP(p->synode); STRLIT(" op "); PTREXP(p); STRLIT(
              p->acceptor.msg ? pax_op_to_str(p->acceptor.msg->op) : "NULL"););
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

/* }}} */

void set_last_received_config(synode_no received_config_change) {
  last_config_modification_id = received_config_change;
}

/* {{{ Definition of majority */
static inline node_no max_check(site_def const *site) {
#ifdef MAXACCEPT
  return MIN(get_maxnodes(site), MAXACCEPT);
#else
  return get_maxnodes(site);
#endif
}

static site_def *forced_config = 0;
static int is_forcing_node(pax_machine const *p) { return p->enforcer; }
static int wait_forced_config = 0;

/* Definition of majority */
static inline int majority(bit_set const *nodeset, site_def const *s, int all,
                           int delay MY_ATTRIBUTE((unused)), int force) {
  node_no ok = 0;
  node_no i = 0;
  int retval = 0;
#ifdef WAIT_FOR_ALL_FIRST
  double sec = task_now();
#endif
  node_no max = max_check(s);

  /* DBGOUT(FN; NDBG(max,lu); NDBG(all,d); NDBG(delay,d); NDBG(force,d)); */

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
    DBGOUT(FN; STRLIT("force majority"); NDBG(ok, u); NDBG(max, u);
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
    /* 	DBGOUT(FN; NDBG(max,lu); NDBG(all,d); NDBG(delay,d); NDBG(retval,d)); */
    return retval;
  }
}

#define IS_CONS_ALL(p) \
  ((p)->proposer.msg->a ? (p)->proposer.msg->a->consensus == cons_all : 0)

/* See if a majority of acceptors have answered our prepare */
static int prep_majority(site_def const *site, pax_machine *p) {
  int ok = 0;

  assert(p);
  assert(p->proposer.prep_nodeset);
  assert(p->proposer.msg);
  /* DBGOUT(FN; BALCEXP(p->proposer.bal)); */
  ok = majority(p->proposer.prep_nodeset, site, IS_CONS_ALL(p),
                p->proposer.bal.cnt == 1,
                p->proposer.msg->force_delivery || p->force_delivery);
  return ok;
}

/* See if a majority of acceptors have answered our propose */
static int prop_majority(site_def const *site, pax_machine *p) {
  int ok = 0;

  assert(p);
  assert(p->proposer.prop_nodeset);
  assert(p->proposer.msg);
  /* DBGOUT(FN; BALCEXP(p->proposer.bal)); */
  ok = majority(p->proposer.prop_nodeset, site, IS_CONS_ALL(p),
                p->proposer.bal.cnt == 1,
                p->proposer.msg->force_delivery || p->force_delivery);
  return ok;
}

/* }}} */

/* {{{ Xcom thread */

/* purecov: begin deadcode */
/* Xcom thread start function */
void *xcom_thread_main(void *cp) {
  G_MESSAGE("Starting xcom on port %d at %f", atoi((char *)cp), seconds());
  xcom_thread_init();
  /* Initialize task system and enter main loop */
  taskmain((xcom_port)atoi((char *)cp));
  /* Xcom is finished when we get here */
  DBGOUT(FN; STRLIT("Deconstructing xcom thread"));
  xcom_thread_deinit();
  G_MESSAGE("Exiting xcom thread at %f", seconds());
  return NULL;
}
/* purecov: end */
static site_def const *executor_site = 0;

site_def const *get_executor_site() { return executor_site; }

static site_def *proposer_site = 0;

site_def const *get_proposer_site() { return proposer_site; }

void init_xcom_base() {
  xcom_shutdown = 0;
  current_message = null_synode;
  executed_msg = null_synode;
  max_synode = null_synode;
  client_boot_done = 0;
  netboot_ok = 0;
  booting = 0;
  start_type = IDLE;

  xcom_recover_init();
  my_id = new_id();
  push_site_def(NULL);
  /*	update_servers(NULL); */
  xcom_cache_var_init();
  median_filter_init();
  link_init(&exec_wait, type_hash("task_env"));
  executor_site = 0;
  proposer_site = 0;

  /** Reset lsn */
  initialize_lsn(0);
}

static void init_tasks() {
  set_task(&boot, NULL);
  set_task(&net_boot, NULL);
  set_task(&net_recover, NULL);
  set_task(&killer, NULL);
  set_task(&executor, NULL);
  set_task(&retry, NULL);
  set_task(&detector, NULL);
  init_proposers();
  set_task(&alive_t, NULL);
  set_task(&sweeper, NULL);
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
  init_cache();

  /* Initialize input queue */
  channel_init(&prop_input_queue, type_hash("msg_link"));
  init_link_list();
  task_sys_init();
}

/* Empty the proposer input queue */
static void empty_prop_input_queue() {
  empty_msg_channel(&prop_input_queue);
  MAY_DBG(FN; STRLIT("prop_input_queue empty"));
}

/* De-initialize the xcom thread */
void xcom_thread_deinit() {
#if !defined(_WIN32) && defined(TASK_EVENT_TRACE)
  dump_task_events();
#endif
  DBGOUT(FN; STRLIT("Empty proposer input queue"));
  empty_prop_input_queue();
  DBGOUT(FN; STRLIT("Empty link free list"));
  empty_link_free_list();
  DBGOUT(FN; STRLIT("De-initialize cache"));
  deinit_cache();
  garbage_collect_servers();
}

#define PROP_ITER \
  int i;          \
  for (i = 0; i < PROPOSERS; i++)

static bool_t force_recover = FALSE;
/* purecov: begin deadcode */
bool_t must_force_recover() { return force_recover; }

void set_force_recover(bool_t const x) { force_recover = x; }
/* purecov: end */

static void init_proposers() {
  PROP_ITER { set_task(&proposer[i], NULL); }
}

static void create_proposers() {
  PROP_ITER {
    set_task(&proposer[i], task_new(proposer_task, int_arg(i), "proposer_task",
                                    XCOM_THREAD_DEBUG));
  }
}

static void terminate_proposers() {
  PROP_ITER { task_terminate(proposer[i]); }
}

static void free_forced_config_site_def() {
  free_site_def(forced_config);
  forced_config = NULL;
}

#if TASK_DBUG_ON
static void dbg_proposers() MY_ATTRIBUTE((unused));
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
  DBGOHK(FN; STRLIT("changing current message"));
  if (max_synode.msgno <= 1)
    set_current_message(first_free_synode(max_synode));
  else
    set_current_message(incr_msgno(first_free_synode(max_synode)));
}

/* }}} */

/* {{{ Task functions */
/* purecov: begin deadcode */
/* Match any port */
static int yes(xcom_port port MY_ATTRIBUTE((unused))) { return 1; }

/* Create tasks and enter the task main loop */
int taskmain(xcom_port listen_port) {
  init_xcom_transport(listen_port);
  set_port_matcher(yes); /* For clients that use only addr, not addr:port  */

  MAY_DBG(FN; STRLIT("enter taskmain"));
  ignoresig(SIGPIPE);

  {
    result fd = {0, 0};

    if ((fd = announce_tcp(listen_port)).val < 0) {
      MAY_DBG(FN; STRLIT("cannot annonunce tcp "); NDBG(listen_port, d));
      task_dump_err(fd.funerr);
      g_critical("Unable to announce tcp port %d. Port already in use?",
                 listen_port);
      pexitall(1);
    }

    MAY_DBG(FN; STRLIT("Creating tasks"));
    task_new(tcp_server, int_arg(fd.val), "tcp_server", XCOM_THREAD_DEBUG);
    /* task_new(tcp_reaper_task, null_arg, "tcp_reaper_task",
     * XCOM_THREAD_DEBUG); */
    /* task_new(xcom_statistics, null_arg, "xcom_statistics",
     * XCOM_THREAD_DEBUG); */
    /* task_new(detector_task, null_arg, "detector_task", XCOM_THREAD_DEBUG); */
    MAY_DBG(FN; STRLIT("XCOM is listening on "); NPUT(listen_port, d));
  }

  task_loop();

  MAY_DBG(FN; STRLIT(" exit"));
  return 1;
}

void start_run_tasks() {
  force_recover = 0;
  client_boot_done = 1;
  netboot_ok = 1;
  booting = 0;
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
}

/* Create tasks and enter the task main loop */
int xcom_taskmain(xcom_port listen_port) {
  init_xcom_transport(listen_port);

  MAY_DBG(FN; STRLIT("enter taskmain"));
  ignoresig(SIGPIPE);

  {
    result fd = {0, 0};
    if ((fd = announce_tcp(listen_port)).val < 0) {
      MAY_DBG(FN; STRLIT("cannot annonunce tcp "); NDBG(listen_port, d));
      task_dump_err(fd.funerr);
      g_critical("Unable to announce tcp port %d. Port already in use?",
                 listen_port);
      pexitall(1);
    }

    MAY_DBG(FN; STRLIT("Creating tasks"));
    /* task_new(generator_task, null_arg, "generator_task", XCOM_THREAD_DEBUG);
     */
    task_new(tcp_server, int_arg(fd.val), "tcp_server", XCOM_THREAD_DEBUG);
    task_new(tcp_reaper_task, null_arg, "tcp_reaper_task", XCOM_THREAD_DEBUG);
    /* task_new(xcom_statistics, null_arg, "xcom_statistics",
     * XCOM_THREAD_DEBUG); */
    /* task_new(detector_task, null_arg, "detector_task", XCOM_THREAD_DEBUG); */
    MAY_DBG(FN; STRLIT("XCOM is listening on "); NPUT(listen_port, d));
  }

  start_run_tasks();
  task_loop();

  MAY_DBG(FN; STRLIT(" exit"));
  return 1;
}
/* purecov: end */
static xcom_state_change_cb xcom_run_cb = 0;
static xcom_state_change_cb xcom_terminate_cb = 0;
static xcom_state_change_cb xcom_comms_cb = 0;
static xcom_state_change_cb xcom_exit_cb = 0;

void set_xcom_run_cb(xcom_state_change_cb x) { xcom_run_cb = x; }

void set_xcom_comms_cb(xcom_state_change_cb x) { xcom_comms_cb = x; }
/* purecov: begin deadcode */
void set_xcom_terminate_cb(xcom_state_change_cb x) { xcom_terminate_cb = x; }
/* purecov: end */
void set_xcom_exit_cb(xcom_state_change_cb x) { xcom_exit_cb = x; }

int xcom_taskmain2(xcom_port listen_port) {
  init_xcom_transport(listen_port);

  MAY_DBG(FN; STRLIT("enter taskmain"));
  ignoresig(SIGPIPE);

  {
    result fd = {0, 0};
    if ((fd = announce_tcp(listen_port)).val < 0) {
      MAY_DBG(FN; STRLIT("cannot annonunce tcp "); NDBG(listen_port, d));
      task_dump_err(fd.funerr);
      g_critical("Unable to announce tcp port %d. Port already in use?",
                 listen_port);
      if (xcom_comms_cb) {
        xcom_comms_cb(XCOM_COMMS_ERROR);
      }
      if (xcom_terminate_cb) {
        xcom_terminate_cb(0);
      }
      return 1;
    }

    if (xcom_comms_cb) {
      xcom_comms_cb(XCOM_COMMS_OK);
    }

    MAY_DBG(FN; STRLIT("Creating tasks"));
    /* task_new(generator_task, null_arg, "generator_task", XCOM_THREAD_DEBUG);
     */
    task_new(tcp_server, int_arg(fd.val), "tcp_server", XCOM_THREAD_DEBUG);
    task_new(tcp_reaper_task, null_arg, "tcp_reaper_task", XCOM_THREAD_DEBUG);
    /* task_new(xcom_statistics, null_arg, "xcom_statistics",
     * XCOM_THREAD_DEBUG); */
    /* task_new(detector_task, null_arg, "detector_task", XCOM_THREAD_DEBUG); */
    MAY_DBG(FN; STRLIT("XCOM is listening on "); NPUT(listen_port, d));
  }

  task_loop();

#if defined(XCOM_HAVE_OPENSSL)
  xcom_cleanup_ssl();
#endif

  MAY_DBG(FN; STRLIT(" exit"));
  xcom_thread_deinit();
  return 1;
}

/* {{{ Paxos message construction and sending */

/* Initialize a message for sending */
static void prepare(pax_msg *p, pax_op op) {
  p->op = op;
  p->reply_to = p->proposal;
}

/* Initialize a prepare_msg */
static int prepare_msg(pax_msg *p) {
  prepare(p, prepare_op);
  /* p->msg_type = normal; */
  return send_to_acceptors(p, "prepare_msg");
}

/* Initialize a noop_msg */
static pax_msg *create_noop(pax_msg *p) {
  prepare(p, prepare_op);
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
  MAY_DBG(FN; STRLIT("skipping message "); SYCEXP(p->synode));
  p->msg_type = no_op;
  return send_to_all(p, "skip_msg");
}

static void brand_app_data(pax_msg *p) {
  if (p->a) {
    p->a->app_key.msgno = p->synode.msgno;
    p->a->app_key.node = p->synode.node;
    p->a->app_key.group_id = p->a->group_id = p->synode.group_id;
  }
}

static synode_no my_unique_id(synode_no synode) {
  assert(my_id != 0);
  /* Random number derived from node number and timestamp which uniquely defines
   * this instance */
  synode.group_id = my_id;
  return synode;
}

static void set_unique_id(pax_msg *msg, synode_no synode) {
  app_data_ptr a = msg->a;
  while (a) {
    a->unique_id = synode;
    a = a->next;
  }
}

static int propose_msg(pax_msg *p) {
  p->op = accept_op;
  p->reply_to = p->proposal;
  brand_app_data(p);
  /* set_unique_id(p, my_unique_id(synode)); */
  return send_to_acceptors(p, "propose_msg");
}

static void set_learn_type(pax_msg *p) {
  p->op = learn_op;
  p->msg_type = p->a ? normal : no_op;
}

/* purecov: begin deadcode */
static int learn_msg(site_def const *site, pax_msg *p) {
  set_learn_type(p);
  p->reply_to = p->proposal;
  brand_app_data(p);
  MAY_DBG(FN; dbg_bitset(p->receivers, get_maxnodes(site)););
  return send_to_all_site(site, p, "learn_msg");
}
/* purecov: end */
static int tiny_learn_msg(site_def const *site, pax_msg *p) {
  int retval;
  pax_msg *tmp = clone_pax_msg_no_app(p);
  pax_machine *pm = get_cache(p->synode);

  ref_msg(tmp);
  tmp->msg_type = p->a ? normal : no_op;
  tmp->op = tiny_learn_op;
  tmp->reply_to = pm->proposer.bal;
  brand_app_data(tmp);
  MAY_DBG(FN; dbg_bitset(tmp->receivers, get_maxnodes(site)););
  retval = send_to_all_site(site, tmp, "tiny_learn_msg");
  unref_msg(&tmp);
  return retval;
}

/* }}} */

/* {{{ Proposer task */

static void prepare_push_3p(site_def const *site, pax_machine *p, pax_msg *msg,
                            synode_no msgno) {
  MAY_DBG(FN; SYCEXP(msgno); NDBG(p->proposer.bal.cnt, d);
          NDBG(p->acceptor.promise.cnt, d));
  p->proposer.bal.node = get_nodeno(site);
  {
    int maxcnt = MAX(p->proposer.bal.cnt, p->acceptor.promise.cnt);
    p->proposer.bal.cnt = ++maxcnt;
  }
  msg->synode = msgno;
  msg->proposal = p->proposer.bal;
}

static void push_msg_2p(site_def const *site, pax_machine *p) {
  assert(p->proposer.msg);

  BIT_ZERO(p->proposer.prop_nodeset);
  MAY_DBG(FN; SYCEXP(p->synode));
  p->proposer.bal.cnt = 0;
  p->proposer.bal.node = get_nodeno(site);
  p->proposer.msg->proposal = p->proposer.bal;
  p->proposer.msg->synode = p->synode;
  p->proposer.msg->force_delivery = p->force_delivery;
  propose_msg(p->proposer.msg);
}

static void push_msg_3p(site_def const *site, pax_machine *p, pax_msg *msg,
                        synode_no msgno, pax_msg_type msg_type) {
  if (wait_forced_config) {
    force_pax_machine(p, 1);
  }

  assert(msgno.msgno != 0);
  prepare_push_3p(site, p, msg, msgno);
  msg->msg_type = msg_type;
  BIT_ZERO(p->proposer.prep_nodeset);
  assert(p->proposer.msg);
  msg->force_delivery = p->force_delivery;
  prepare_msg(msg);
  MAY_DBG(FN; BALCEXP(msg->proposal); SYCEXP(msgno); STRLIT(" op ");
          STRLIT(pax_op_to_str(msg->op)));
}

/* Brand client message with unique ID */
static void brand_client_msg(pax_msg *msg, synode_no msgno) {
  assert(!synode_eq(msgno, null_synode));
  set_unique_id(msg, my_unique_id(msgno));
}

/* purecov: begin deadcode */
#ifdef NOTDEF
static void dbg_live_nodes(site_def const *site) {
  node_no i = 0;
  node_no maxnodes = get_maxnodes(site);
  node_no self = get_nodeno(site);

  if (site) {
    DBGOUT(FN; NDBG(self, lu); NDBG(maxnodes, lu);
           NDBG(site->detector_updated, d); NDBG(task_now(), f););
    for (i = 0; i < maxnodes; i++) {
      DBGOUT(FN; NDBG(self, lu); NDBG(i, lu); NDBG(site->detected[i], f););
    }
  } else {
    DBGOUT(FN; NDBG(self, lu); NDBG(maxnodes, lu); NDBG(task_now(), f););
  }
}
#endif

void xcom_send(app_data_ptr a, pax_msg *msg) {
  MAY_DBG(FN; PTREXP(a); SYCEXP(a->app_key); SYCEXP(msg->synode));
  msg->a = a;
  msg->op = client_msg;
  {
    msg_link *link = msg_link_new(msg, VOID_NODE_NO);
    MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_pax_msg(msg)));
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
  G_DEBUG("getstart group_id %x", a->group_id);
  if (!a || a->group_id == null_id) {
    retval.group_id = new_id();
  } else {
    a->app_key.group_id = a->group_id;
    retval = a->app_key;
    if (get_site_def() && retval.msgno != 1) {
      /* Not valid until after event horizon has been passed */
      retval = add_event_horizon(retval);
    }
  }
  return retval;
}

void site_install_action(site_def *site, cargo_type operation) {
  DBGOUT(FN; NDBG(get_nodeno(get_site_def()), u));
  if (synode_gt(site->start, max_synode)) set_max_synode(site->start);
  site->nodeno = xcom_find_node_index(&site->nodes);
  push_site_def(site);

  DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_site_def(site)));
  set_group(get_group_id(site));
  if (get_maxnodes(get_site_def())) {
    update_servers(site, operation);
  }
  site->install_time = task_now();
  G_INFO("Installed site start=" SY_FMT " boot_key=" SY_FMT " node %u",
         SY_MEM(site->start), SY_MEM(site->boot_key), get_nodeno(site));
  DBGOUT(FN; SYCEXP(site->start); SYCEXP(site->boot_key));
  DBGOUT(FN; NDBG(get_nodeno(site), u));
  DBGOUT(FN; SYCEXP(site->start); SYCEXP(site->boot_key);
         NDBG(site->install_time, f));
  DBGOUT(FN; NDBG(get_nodeno(site), u));
}

static site_def *create_site_def_with_start(app_data_ptr a, synode_no start) {
  site_def *site = new_site_def();
  MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)););
  init_site_def(a->body.app_u_u.nodes.node_list_len,
                a->body.app_u_u.nodes.node_list_val, site);
  site->start = start;
  site->boot_key = a->app_key;
  return site;
}

static site_def *install_ng_with_start(app_data_ptr a, synode_no start) {
  if (a) {
    site_def *site = create_site_def_with_start(a, start);
    site_install_action(site, a->body.c_t);
    return site;
  }
  return 0;
}

site_def *install_node_group(app_data_ptr a) {
  ADD_EVENTS(add_event(string_arg("a->app_key"));
             add_synode_event(a->app_key););
  if (a)
    return install_ng_with_start(a, getstart(a));
  else
    return 0;
}

/* purecov: begin deadcode */
int is_real_recover(app_data_ptr a) {
  return a && a->body.c_t == xcom_recover &&
         a->body.app_u_u.rep.msg_list.synode_no_array_len > 0;
}
/* purecov: end */

void set_max_synode(synode_no synode) {
  max_synode = synode; /* Track max synode number */
  MAY_DBG(FN; STRLIT("new "); SYCEXP(max_synode));
}

static int is_busy(synode_no s) {
  pax_machine *p = hash_get(s);
  if (!p) {
    return 0;
  } else {
    return started(p);
  }
}

#if 0
static synode_no find_slot(synode_no msgno, site_def **site)
{
	assert(!synode_eq(msgno, null_synode));
	while (is_busy(msgno)) {
		msgno = incr_msgno(msgno);
	}
	assert(!synode_eq(msgno, null_synode));
	*site = find_site_def_rw(msgno);
	return msgno;
}
#endif

bool_t match_my_msg(pax_msg *learned, pax_msg *mine) {
  MAY_DBG(FN; PTREXP(learned->a); if (learned->a) SYCEXP(learned->a->unique_id);
          PTREXP(mine->a); if (mine->a) SYCEXP(mine->a->unique_id););
  if (learned->a && mine->a) { /* Both have app data, see if data is mine */
    return synode_eq(learned->a->unique_id, mine->a->unique_id);
  } else if (!(learned->a || mine->a)) { /* None have app data, anything goes */
    return TRUE;
  } else { /* Definitely mismatch */
    return FALSE;
  }
}

#if TASK_DBUG_ON
static void dbg_reply_set(site_def const *site, const char *s, bit_set *bs) {
  unsigned int i = 0;
  unsigned int n = get_maxnodes(site);
  GET_GOUT;

  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;

  STRLIT(s);
  for (i = 0; i < n && i < bs->bits.bits_len * sizeof(*bs->bits.bits_val) *
                               BITS_PER_BYTE;
       i++) {
    NPUT(BIT_ISSET(i, bs), d);
  }
  PRINT_GOUT;
  FREE_GOUT;
}
#endif

/*
 * Initialize the log sequence number (lsn).
 */
void initialize_lsn(uint64_t n) { lsn = n; }

/**
 * Assign the next log sequence number (lsn) for a message.
 *
 * Initial propose sets lsn to msgno of current_message as safe starting point,
 * otherwise lsn shall be ever increasing. lsn ensures sender order known on
 * receiver side, as messages may arrive "out of order" due to retransmission.
 */
static uint64_t assign_lsn() {
  if (lsn == 0) {
    initialize_lsn(current_message.msgno);
  } else {
    lsn++;
  }
  return lsn;
}

static void propose_noop(synode_no find, pax_machine *p);

static inline int too_far(synode_no s) {
  return s.msgno >= executed_msg.msgno + event_horizon;
}

#define GOTO(x)                          \
  {                                      \
    DBGOUT(STRLIT("goto "); STRLIT(#x)); \
    goto x;                              \
  }

static inline int is_view(cargo_type x) { return x == view_msg; }

static inline int is_config(cargo_type x) {
  return x == unified_boot_type || x == add_node_type ||
         x == remove_node_type || x == force_config_type;
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
  site_def const *site;
  size_t size;
  END_ENV;

  TASK_BEGIN

  ep->self = get_int_arg(arg);
  ep->p = NULL;
  ep->client_msg = NULL;
  ep->prepare_msg = NULL;
  ep->start_propose = 0.0;
  ep->start_push = 0.0;
  ep->delay = 0.0;
  ep->msgno = current_message;
  ep->site = 0;
  ep->size = 0;

  MAY_DBG(FN; NDBG(ep->self, d); NDBG(task_now(), f));

  while (!xcom_shutdown) { /* Loop until no more work to do */
    int MY_ATTRIBUTE((unused)) lock = 0;
    /* Wait for client message */
    assert(!ep->client_msg);
    CHANNEL_GET(&prop_input_queue, &ep->client_msg, msg_link);
    MAY_DBG(FN; PTREXP(ep->client_msg->p->a); STRLIT("extracted ");
            SYCEXP(ep->client_msg->p->a->app_key));

    /* Grab rest of messages in queue as well, but never batch config messages,
     * which need a unique number */

    if (!is_config(ep->client_msg->p->a->body.c_t) &&
        !is_view(ep->client_msg->p->a->body.c_t)) {
      ep->size = app_data_size(ep->client_msg->p->a);
      while (AUTOBATCH && ep->size <= MAX_BATCH_SIZE &&
             !link_empty(&prop_input_queue
                              .data)) { /* Batch payloads into single message */
        msg_link *tmp;
        app_data_ptr atmp;

        CHANNEL_GET(&prop_input_queue, &tmp, msg_link);
        atmp = tmp->p->a;
        ep->size += app_data_size(atmp);
        /* Abort batching if config or too big batch */
        if (is_config(atmp->body.c_t) || is_view(atmp->body.c_t) ||
            ep->size > MAX_BATCH_SIZE) {
          channel_put_front(&prop_input_queue, &tmp->l);
          break;
        }
        ADD_T_EV(seconds(), __FILE__, __LINE__, "batching");

        tmp->p->a = 0;                     /* Steal this payload */
        msg_link_delete(&tmp);             /* Get rid of the empty message */
        atmp->next = ep->client_msg->p->a; /* Add to list of app_data */
                                           /* G_TRACE("Batching %s %s",
                                            * cargo_type_to_str(ep->client_msg->p->a->body.c_t), */
        /* 	cargo_type_to_str(atmp->body.c_t)); */
        ep->client_msg->p->a = atmp;
        MAY_DBG(FN; PTREXP(ep->client_msg->p->a); STRLIT("extracted ");
                SYCEXP(ep->client_msg->p->a->app_key));
      }
    }

    ep->start_propose = task_now();
    ep->delay = 0.0;

    assert(!ep->client_msg->p->a->chosen);

    /* It is a new message */

    assert(!synode_eq(current_message, null_synode));

    /* Assign a log sequence number only on initial propose */
    ep->client_msg->p->a->lsn = assign_lsn();

  retry_new:
    /* Find a free slot */

    assert(!synode_eq(current_message, null_synode));
    ep->msgno = current_message;
    proposer_site = find_site_def_rw(ep->msgno);
    ep->site = proposer_site;

    while (is_busy(ep->msgno)) {
      while (/* ! ep->client_msg->p->force_delivery &&  */ too_far(
          incr_msgno(ep->msgno))) { /* Too far ahead of executor */
        TIMED_TASK_WAIT(&exec_wait, 1.0);
        DBGOUT(FN; TIMECEXP(ep->start_propose);
               TIMECEXP(ep->client_msg->p->a->expiry_time);
               TIMECEXP(task_now());

               NDBG(enough_live_nodes(ep->site), d));
#ifdef DELIVERY_TIMEOUT
        if ((ep->start_propose + ep->client_msg->p->a->expiry_time) <
                task_now() &&
            !enough_live_nodes(ep->site)) {
          /* Give up */
          deliver_to_app(NULL, ep->client_msg->p->a, delivery_failure);
          GOTO(next);
        }
#endif
      }
      ep->msgno = incr_msgno(ep->msgno);
      /* Refresh site to next msgno */
      proposer_site = find_site_def_rw(ep->msgno);
      ep->site = proposer_site;
    }
    assert(!synode_eq(ep->msgno, null_synode));

    /* See if we can do anything with this message */
    if (!ep->site || get_nodeno(ep->site) == VOID_NODE_NO) {
      /* Give up */
      deliver_to_app(NULL, ep->client_msg->p->a, delivery_failure);
      GOTO(next);
    }
    DBGOHK(FN; STRLIT("changing current message"));
    set_current_message(ep->msgno);

    brand_client_msg(ep->client_msg->p, ep->msgno);

    for (;;) { /* Loop until the client message has been learned */
      /* Get a Paxos instance to send the client message */
      ep->p = get_cache(ep->msgno);
      assert(ep->p);
      if (ep->client_msg->p->force_delivery)
        ep->p->force_delivery = ep->client_msg->p->force_delivery;
      lock = lock_pax_machine(ep->p);
      assert(!lock);

      /* Set the client message as current proposal */
      assert(ep->client_msg->p);
      replace_pax_msg(&ep->p->proposer.msg, clone_pax_msg(ep->client_msg->p));
      assert(ep->p->proposer.msg);
      PAX_MSG_SANITY_CHECK(ep->p->proposer.msg);

      /* Create the prepare message */
      unchecked_replace_pax_msg(&ep->prepare_msg,
                                pax_msg_new(ep->msgno, ep->site));
      DBGOUT(FN; PTREXP(ep->client_msg->p->a); STRLIT("pushing ");
             SYCEXP(ep->msgno));
      MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_app_data(ep->prepare_msg->a)));

      if (threephase || ep->p->force_delivery) {
        push_msg_3p(ep->site, ep->p, ep->prepare_msg, ep->msgno, normal);
      } else {
        push_msg_2p(ep->site, ep->p);
      }

      ep->start_push = task_now();

      while (!finished(ep->p)) { /* Try to get a value accepted */
        /* We will wake up periodically, and whenever a message arrives */
        TIMED_TASK_WAIT(&ep->p->rv, ep->delay = wakeup_delay(ep->delay));
        if (!synode_eq(ep->msgno, ep->p->synode) ||
            ep->p->proposer.msg == NULL) {
          DBGOHK(FN; STRLIT("detected stolen state machine, retry"););
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
            /* propose_noop(ep->msgno, ep->p); */
            deliver_to_app(ep->p, ep->client_msg->p->a, delivery_failure);
            unlock_pax_machine(ep->p);
            GOTO(next);
          }
#endif
          if ((ep->start_push + ep->delay) <= now) {
            PAX_MSG_SANITY_CHECK(ep->p->proposer.msg);
            DBGOUT(FN; STRLIT("retry pushing "); SYCEXP(ep->msgno));
            MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_app_data(ep->prepare_msg->a)););
            DBGOUT(BALCEXP(ep->p->proposer.bal);
                   BALCEXP(ep->p->acceptor.promise));
            MAY_DBG(FN; dbg_reply_set(ep->site, "prep_node_set",
                                      ep->p->proposer.prep_nodeset);
                    dbg_reply_set(ep->site, "prop_node_set",
                                  ep->p->proposer.prop_nodeset););
            push_msg_3p(ep->site, ep->p, ep->prepare_msg, ep->msgno, normal);
            ep->start_push = now;
          }
        }
      }
      /* When we get here, we know the value for this message number,
         but it may not be the value we tried to push,
         so loop until we have a successfull push. */
      unlock_pax_machine(ep->p);
      MAY_DBG(FN; STRLIT(" found finished message "); SYCEXP(ep->msgno);
              STRLIT("seconds since last push ");
              NPUT(task_now() - ep->start_push, f); STRLIT("ep->client_msg ");
              COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p)););
      MAY_DBG(FN; STRLIT("ep->p->learner.msg ");
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
    DBGOUT(FN; STRLIT("completed ep->msgno "); SYCEXP(ep->msgno); NDBG(used, f);
           NDBG(median_time(), f); STRLIT("seconds since last push ");
           NDBG(now - ep->start_push, f););
    MAY_DBG(FN; STRLIT("ep->client_msg ");
            COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p)););
    if (ep->p) {
      MAY_DBG(FN; STRLIT("ep->p->learner.msg ");
              COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p->learner.msg)););
    }
    msg_link_delete(&ep->client_msg);
  }
  }
  FINALLY
  MAY_DBG(FN; STRLIT("exit "); NDBG(ep->self, d); NDBG(task_now(), f));
  if (ep->p) {
    unlock_pax_machine(ep->p);
  }
  replace_pax_msg(&ep->prepare_msg, NULL);
  if (ep->client_msg) { /* If we get here with a client message, we have
                           failed to deliver */
    deliver_to_app(ep->p, ep->client_msg->p->a, delivery_failure);
    msg_link_delete(&ep->client_msg);
  }
  TASK_END;
}

/* }}} */

/* {{{ Executor task */

static node_no leader(site_def const *s) {
  node_no leader = 0;
  for (leader = 0; leader < get_maxnodes(s); leader++) {
    if (!may_be_dead(s->detected, leader, task_now())) return leader;
  }
  return 0;
}

int iamthegreatest(site_def const *s) { return leader(s) == s->nodeno; }

void execute_msg(site_def const *site, pax_machine *pma, pax_msg *p) {
  app_data_ptr a = p->a;
  DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_pax_msg(p)););
  if (a) {
    switch (p->a->body.c_t) {
      case unified_boot_type:
      case add_node_type:
      case remove_node_type:
      case force_config_type:
        break;
      case xcom_recover:
        break;
      case app_type:
        MAY_DBG(FN; STRLIT(" learner.msg ");
                COPY_AND_FREE_GOUT(dbg_pax_msg(pma->learner.msg)););
        deliver_to_app(pma, a, delivery_ok);
        break;
      case view_msg:
        MAY_DBG(FN; STRLIT(" learner.msg ");
                COPY_AND_FREE_GOUT(dbg_pax_msg(pma->learner.msg)););
        if (site && site->global_node_set.node_set_len ==
                        a->body.app_u_u.present.node_set_len) {
          assert(site->global_node_set.node_set_len ==
                 a->body.app_u_u.present.node_set_len);
          copy_node_set(&a->body.app_u_u.present,
                        &(((site_def *)site)->global_node_set));
          deliver_global_view_msg(site, p->synode);
        }
        break;
#ifdef USE_EXIT_TYPE
      case exit_type:
        g_critical(
            "Unable to get message, process will now exit. Please ensure that "
            "the process is restarted");
        exit(1);
        break;
#endif
      default:
        break;
    }
  }
  MAY_DBG(FN; SYCEXP(p->synode));
}

static void read_missing_values(int n);
static void propose_missing_values(int n);

static void find_value(site_def const *site, unsigned int *wait, int n) {
  DBGOHK(FN; NDBG(*wait, d));

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

#ifdef PROPOSE_IF_LEADER
int get_xcom_message(pax_machine **p, synode_no msgno, int n) {
  DECL_ENV
  unsigned int wait;
  double delay;
  END_ENV;

  TASK_BEGIN

  ep->wait = 0;
  ep->delay = 0.0;
  *p = get_cache(msgno);

  while (!finished(*p)) {
    MAY_DBG(FN; STRLIT("before find_value"); SYCEXP(msgno); PTREXP(*p);
            NDBG(ep->wait, u); SYCEXP(msgno));
    find_value(find_site_def(msgno), &ep->wait, n);
    MAY_DBG(FN; STRLIT("after find_value"); SYCEXP(msgno); PTREXP(*p);
            NDBG(ep->wait, u); SYCEXP(msgno));
    ep->delay = wakeup_delay(ep->delay);
    MAY_DBG(FN; NDBG(ep->delay, f));
    TIMED_TASK_WAIT(&(*p)->rv, ep->delay);
    *p = get_cache(msgno);
  }

  FINALLY
  TASK_END;
}
#else
int get_xcom_message(pax_machine **p, synode_no msgno, int n) {
  DECL_ENV
  unsigned int wait;
  double delay;
  END_ENV;

  TASK_BEGIN

  ep->wait = 0;
  ep->delay = 0.0;
  *p = get_cache(msgno);

  while (!finished(*p)) {
    site_def const *site = find_site_def(msgno);
    DBGOHK(FN; STRLIT(" not finished "); SYCEXP(msgno); PTREXP(*p);
           NDBG(ep->wait, u); SYCEXP(msgno));
    if (get_maxnodes(site) > 1 && iamthegreatest(site) &&
        site->global_node_set.node_set_val &&
        !site->global_node_set.node_set_val[msgno.node] &&
        may_be_dead(site->detected, msgno.node, task_now())) {
      propose_missing_values(n);
    } else {
      find_value(site, &ep->wait, n);
    }
    TIMED_TASK_WAIT(&(*p)->rv, ep->delay = wakeup_delay(ep->delay));
    *p = get_cache(msgno);
  }

  FINALLY
  DBGOHK(FN; SYCEXP(msgno); PTREXP(*p); NDBG(ep->wait, u); SYCEXP(msgno));
  TASK_END;
}
#endif

synode_no set_executed_msg(synode_no msgno) {
  MAY_DBG(FN; STRLIT("changing executed_msg from "); SYCEXP(executed_msg);
          STRLIT(" to "); SYCEXP(msgno));
  if (synode_gt(msgno, current_message)) {
    DBGOHK(FN; STRLIT("changing current message"));
    set_current_message(first_free_synode(msgno));
  }

  if (msgno.msgno > executed_msg.msgno) task_wakeup(&exec_wait);

  executed_msg = msgno;
  executor_site = find_site_def(executed_msg);
  return executed_msg;
}

static synode_no first_free_synode(synode_no msgno) {
  site_def const *site = find_site_def(msgno);
  synode_no retval = msgno;
  if (get_group_id(site) == 0) {
    DBGOUT(FN; PTREXP(site); SYCEXP(msgno));
    if (site) {
      DBGOUT(FN; SYCEXP(site->boot_key); SYCEXP(site->start);
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
  MAY_DBG(FN; STRLIT("changing current_message from "); SYCEXP(current_message);
          STRLIT(" to "); SYCEXP(msgno));
  return current_message = msgno;
}

static void handle_learn(site_def const *site, pax_machine *p, pax_msg *m);
static void update_max_synode(pax_msg *p);

#if TASK_DBUG_ON
static void perf_dbg(int *_n, int *_old_n, double *_old_t)
    MY_ATTRIBUTE((unused));
static void perf_dbg(int *_n, int *_old_n, double *_old_t) {
  int n = *_n;
  int old_n = *_old_n;
  double old_t = *_old_t;

  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;

  DBGOHK(FN; SYCEXP(executed_msg));
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

#ifdef IGNORE_LOSERS

static inline int LOSER(synode_no x, site_def const *site) {
  /*  node_no i = 0;
    node_no n = 0;
    node_no maxnodes = get_maxnodes(site);

    if (maxnodes == 0)
      return 0;

    for (i = 0; i < maxnodes; i++) {
      if (site->global_node_set.node_set_val[i]) {
        n++;
      }
    }
    DBGOUT(NEXP(maxnodes,u); NEXP(n,u)); */
  DBGOUT(NEXP(x.node, u);
         NEXP(site->global_node_set.node_set_val[(x).node], d));
  return
      /* ( n > maxnodes / 2 || (ARBITRATOR_HACK && (2 == maxnodes && 0 ==
         get_nodeno(site)))) && */
      (!(site)->global_node_set.node_set_val[(x).node]);
}

#else
#define LOSER(x, site) 0
#endif

static void debug_loser(synode_no x) MY_ATTRIBUTE((unused));
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
static void debug_loser(synode_no x MY_ATTRIBUTE((unused))) {}
/* purecov: end */
#endif

/* #define DBGFIX2(x){ if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;
 * GET_GOUT; ADD_F_GOUT("%f ",task_now()); x; PRINT_GOUT; FREE_GOUT; } */
#define DBGFIX2(x)
static void send_value(site_def const *site, node_no to, synode_no synode) {
  pax_machine *pm = get_cache(synode);
  if (pm && pm->learner.msg) {
    pax_msg *msg = clone_pax_msg(pm->learner.msg);
    ref_msg(msg);
    send_server_msg(site, to, msg);
    unref_msg(&msg);
  }
}

/* Peturn message number where it is safe for nodes in prev config to exit */
static synode_no compute_delay(synode_no start) {
  start.msgno += event_horizon;
  return start;
}

/* Push messages to all nodes which were in the previous site, but not in this
 */
static void inform_removed(int index, int all) {
  site_def **sites = 0;
  uint32_t site_count = 0;
  DBGFIX2(FN; NEXP(index, d));
  get_all_site_defs(&sites, &site_count);
  while (site_count > 1 && index >= 0 && (uint32_t)(index + 1) < site_count) {
    site_def *s = sites[index];
    site_def *ps = sites[index + 1];

    /* Compute diff and push messages */
    DBGFIX2(FN; NDBG(index, d); PTREXP(s); if (s) SYCEXP(s->boot_key);
            PTREXP(ps); if (ps) SYCEXP(ps->boot_key));

    if (s && ps) {
      node_no i = 0;
      DBGFIX2(FN; SYCEXP(s->boot_key); SYCEXP(s->start); SYCEXP(ps->boot_key);
              SYCEXP(ps->start));
      for (i = 0; i < ps->nodes.node_list_len; i++) { /* Loop over prev site */
        if (ps->nodeno != i &&
            !node_exists(&ps->nodes.node_list_val[i], &s->nodes)) {
          synode_no synode = s->start;
          synode_no end = compute_delay(s->start);
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

site_def *handle_add_node(app_data_ptr a) {
  site_def *site = clone_site_def(get_site_def());
  DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)););
  MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)););
  ADD_EVENTS(add_event(string_arg("a->app_key"));
             add_synode_event(a->app_key););
  assert(get_site_def());
  assert(site);
  add_site_def(a->body.app_u_u.nodes.node_list_len,
               a->body.app_u_u.nodes.node_list_val, site);
  site->start = getstart(a);
  site->boot_key = a->app_key;
  site_install_action(site, a->body.c_t);
  return site;
}

static void terminate_and_exit() {
  XCOM_FSM(xa_terminate, int_arg(0)); /* Tell xcom to stop */
  XCOM_FSM(xa_exit, int_arg(0));      /* Tell xcom to exit */
}

int terminator_task(task_arg arg) {
  DECL_ENV
  double t;
  END_ENV;

  TASK_BEGIN

  ep->t = get_double_arg(arg);
  TASK_DELAY(ep->t);
  terminate_and_exit();
  FINALLY
  TASK_END;
}

#ifndef NO_DELAYED_TERMINATION
static void delayed_terminate_and_exit(double t) {
  task_new(terminator_task, double_arg(t), "terminator_task",
           XCOM_THREAD_DEBUG);
}
#endif

static inline int is_empty_site(site_def const *s) {
  return s->nodes.node_list_len == 0;
}

site_def *handle_remove_node(app_data_ptr a) {
  site_def *site = clone_site_def(get_site_def());
  DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)));
  ADD_EVENTS(add_event(string_arg("a->app_key")); add_synode_event(a->app_key);
             add_event(string_arg("nodeno"));
             add_event(uint_arg(get_nodeno(site))););

  remove_site_def(a->body.app_u_u.nodes.node_list_len,
                  a->body.app_u_u.nodes.node_list_val, site);
  site->start = getstart(a);
  site->boot_key = a->app_key;
  site_install_action(site, a->body.c_t);
  return site;
}

void handle_config(app_data_ptr a) {
  while (a) {
    switch (a->body.c_t) {
      case unified_boot_type:
        install_node_group(a);
        break;
      case add_node_type:
        handle_add_node(a);
        break;
      case remove_node_type:
        handle_remove_node(a);
        if (xcom_shutdown) return;
        break;
      case force_config_type:
        install_node_group(a);
        break;
      default:
        break;
    }
    a = a->next;
  }
}

enum exec_state { FETCH = 0, EXECUTE = 1 };
typedef enum exec_state exec_state;

#define NEXTSTATE(x) ep->state = (x)

static synode_no delivered_msg;

synode_no get_delivered_msg() { return delivered_msg; }

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

static int executor_task(task_arg arg MY_ATTRIBUTE((unused))) {
  DECL_ENV
  pax_machine *p;
  int n;
  int old_n;
  double old_t;
  synode_no exit_synode;
  exec_state state;
  enum { no_exit, not_member_exit, empty_exit } exit_type;
  int inform_index;
  END_ENV;

  TASK_BEGIN
  ep->p = NULL;
  ep->n = 0;
  ep->old_n = 0;
  ep->old_t = task_now();
  ep->exit_synode = null_synode;
  ep->exit_type = no_exit;
  ep->inform_index = -1;
  delay_fifo.n = 0;
  delay_fifo.front = 0;
  delay_fifo.rear = 0;

  set_last_received_config(null_synode);

  if (executed_msg.msgno == 0) executed_msg.msgno = 1;
  delivered_msg = executed_msg;
  NEXTSTATE(FETCH);
  executor_site = find_site_def(executed_msg);

  while (!xcom_shutdown) {
    for (;;) {
      if (ep->state == FETCH) {
        if (!LOSER(executed_msg, executor_site)) {
          TASK_CALL(get_xcom_message(&ep->p, executed_msg, FIND_MAX));
          DBGOUT(FN; STRLIT("got message "); SYCEXP(ep->p->synode);
                 COPY_AND_FREE_GOUT(dbg_app_data(XAPP)));
          /* Execute unified_boot immediately, but do not deliver site message
           * until we */
          /* are ready to execute messages from the new site definition. */
          /* At that point we can be certain that a majority have learned */
          /* everything from the old site. */

          if ((XAPP) && is_config((XAPP)->body.c_t) &&
              synode_gt(executed_msg, get_site_def()->boot_key)) /* Redo test */
          {
            site_def *site = 0;
            set_last_received_config(executed_msg);
            handle_config(XAPP);
            garbage_collect_site_defs(delivered_msg);
            site = get_site_def_rw();
            if (site == 0) {
              TERMINATE;
            }
            DBGFIX2(FN; STRLIT("new config "); SYCEXP(site->boot_key););

            /* If site is empty, increase start to allow nodes to terminate
             * before start */
            if (is_empty_site(site)) {
              site->start = compute_delay(compute_delay(site->start));
            }
            if (ep->exit_type ==
                no_exit) { /* We have not yet set the exit trigger */
              synode_no delay_until;
              if (is_member(site)) {
                delay_until = compute_delay(site->start);
              } else { /* Not in this site */
                       /*
                               See if site will be empty when we leave.
                               If the new site is empty, we should exit after having
                               delivered the last message from the old site.
                       */
                if (is_empty_site(site)) {
                  ep->exit_synode = decr_synode(site->start);
                  ep->exit_type = empty_exit;
                  delay_until = ep->exit_synode;
                  DBGFIX2(FN; SYCEXP(ep->exit_synode); SYCEXP(executed_msg);
                          SYCEXP(max_synode));
                } else {
                  /*
                          If we are not a member of the new site, we should exit
                     after having
                          seen enough messages from the new site.
                   */
                  ep->exit_synode = compute_delay(site->start);
                  ep->exit_type = not_member_exit;
                  if (!synode_lt(ep->exit_synode, max_synode)) {
                    /* We need messages from the next site, so set max_synode
                     * accordingly. */
                    set_max_synode(incr_synode(ep->exit_synode));
                  }
                  delay_until = ep->exit_synode;
                  DBGFIX2(FN; SYCEXP(delay_until); SYCEXP(executed_msg);
                          SYCEXP(max_synode));
                  DBGFIX2(FN; SYCEXP(ep->exit_synode); SYCEXP(executed_msg);
                          SYCEXP(max_synode));
                }
              }

              if (synode_gt(delay_until, max_synode))
                set_max_synode(delay_until);
              fifo_insert(delay_until);
              ep->inform_index++;
            }
          } else {
            DBGOUT(FN; SYCEXP(executed_msg); SYCEXP(get_site_def()->boot_key));
          }
        } else {
          DBGOUT(
              FN; debug_loser(executed_msg); PTREXP(executor_site);
              COPY_AND_FREE_GOUT(dbg_node_set(executor_site->global_node_set)));
        }
        DBGOUT(FN; NDBG(ep->state, d); SYCEXP(delivered_msg);
               SYCEXP(executed_msg); SYCEXP(ep->exit_synode);
               NDBG(ep->exit_type, d));

        /* See if we should exit when having seen this message */
        if (ep->exit_type == not_member_exit &&
            synode_eq(executed_msg, ep->exit_synode)) {
          inform_removed(ep->inform_index,
                         1); /* Inform all removed nodes before we exit */
#ifndef NO_DELAYED_TERMINATION
          delayed_terminate_and_exit(TERMINATE_DELAY); /* Tell xcom to stop */
          TERMINATE;
#endif
        }

        if (fifo_empty()) {
          NEXTSTATE(EXECUTE);
        } else if (synode_eq(executed_msg, fifo_front())) {
          DBGFIX2(FN; SYCEXP(fifo_front()); SYCEXP(executed_msg);
                  SYCEXP(ep->exit_synode); NDBG(ep->exit_type, d));
          while (synode_eq(executed_msg,
                           fifo_front())) { /* More than one may match */
            inform_removed(ep->inform_index, 0);
            fifo_extract();
            ep->inform_index--;
          }
          garbage_collect_servers();
          NEXTSTATE(EXECUTE);
        }

        SET_EXECUTED_MSG(incr_synode(executed_msg));
        MAY_DBG(FN; NDBG(ep->state, d); SYCEXP(fifo_front());
                SYCEXP(executed_msg));
        MAY_DBG(FN; NDBG(ep->state, d); SYCEXP(ep->exit_synode);
                SYCEXP(executed_msg));
      } else if (ep->state == EXECUTE) {
        site_def const *x_site = find_site_def(delivered_msg);

        DBGOUT(FN; NDBG(ep->state, d); SYCEXP(delivered_msg);
               SYCEXP(delivered_msg); SYCEXP(executed_msg);
               SYCEXP(ep->exit_synode); NDBG(ep->exit_type, d));
        ep->p = get_cache(delivered_msg);
        if (LOSER(delivered_msg, x_site)) {
#ifdef IGNORE_LOSERS
          DBGOUT(FN; debug_loser(delivered_msg); PTREXP(x_site);
                 dbg_node_set(x_site->global_node_set));
#endif
        } else if ((ep->p)->learner.msg->msg_type != no_op) {
          execute_msg(find_site_def(delivered_msg), ep->p, ep->p->learner.msg);
#if defined(TASK_DBUG_ON) && TASK_DBUG_ON
          DBGOUT(perf_dbg(&ep->n, &ep->old_n, &ep->old_t));
#endif
        }
        /* Garbage collect old servers */
        if (synode_eq(delivered_msg, x_site->start)) {
          garbage_collect_servers();
        }
        /* See if we should exit when having delivered this message */
        if (ep->exit_type == empty_exit &&
            synode_eq(delivered_msg, ep->exit_synode)) {
          inform_removed(ep->inform_index,
                         1); /* Inform all removed nodes before we exit */
#ifndef NO_DELAYED_TERMINATION
          delayed_terminate_and_exit(TERMINATE_DELAY); /* Tell xcom to stop */
          TERMINATE;
#endif
        }
        delivered_msg = incr_synode(delivered_msg);
        if (synode_eq(delivered_msg, executed_msg)) {
          NEXTSTATE(FETCH);
        }
      } else {
        abort();
      }
    }
  }
  FINALLY
  DBGOUT(FN; STRLIT(" shutdown "); SYCEXP(executed_msg); NDBG(task_now(), f));
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

static int sweeper_task(task_arg arg MY_ATTRIBUTE((unused))) {
  DECL_ENV
  synode_no find;
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
    ADD_EVENTS(add_event(string_arg("sweeper ready"));
               add_synode_event(executed_msg););
    /*		DBGOUT(FN; STRLIT("ready to run ");   */
    /*			SYCEXP(executed_msg); SYCEXP(max_synode);
     * SYCEXP(ep->find));
     */
    {
      while (synode_lt(ep->find, max_synode) && !too_far(ep->find)) {
        /* pax_machine * pm = hash_get(ep->find); */
        pax_machine *pm = 0;
        ADD_EVENTS(add_event(string_arg("sweeper examining"));
                   add_synode_event(ep->find););
        DBGOUT(FN; STRLIT("examining "); SYCEXP(ep->find));
        if (ep->find.node == VOID_NODE_NO) {
          if (synode_gt(executed_msg, ep->find)) {
            ep->find = get_sweep_start();
          }
          if (ep->find.node == VOID_NODE_NO) goto deactivate;
        }
        pm = get_cache(ep->find);
        if (pm && !pm->force_delivery) { /* We want full 3 phase Paxos for
                                            forced messages */
          /* DBGOUT(FN; dbg_pax_machine(pm)); */
          if (!is_busy_machine(pm) && pm->acceptor.promise.cnt == 0 &&
              !pm->acceptor.msg && !finished(pm)) {
            pm->op = skip_op;
            ADD_EVENTS(add_event(string_arg("sweeper skipping"));
                       add_synode_event(ep->find);
                       add_event(string_arg(pax_op_to_str(pm->op))););
            skip_msg(pax_msg_new(ep->find, find_site_def(ep->find)));
            MAY_DBG(FN; STRLIT("skipping "); SYCEXP(ep->find));
            /* 						MAY_DBG(FN;
             * dbg_pax_machine(pm));
             */
          }
        }
        ep->find = incr_msgno(ep->find);
      }
    }
  deactivate:
    TASK_DEACTIVATE;
  }
  FINALLY
  MAY_DBG(FN; STRLIT(" shutdown sweeper "); SYCEXP(executed_msg);
          NDBG(task_now(), f));
  TASK_END;
}

  /* }}} */

#if 0
static double	wakeup_delay(double old)
{
	/*	return 1.0 + xcom_drand48(); */
	double	retval = 0.0;
	if (0.0 == old) {
		double	m = median_time();
		if (m == 0.0 || m > 1.0)
			m = 0.1;
		retval = 0.1 + 10.0 * m + m * xcom_drand48();
	} else {
		retval = old * 1.4142136; /* Exponential backoff */
	}
	while (retval > 10.0)
		retval /= 1.31415926;
	/* DBGOUT(FN; NDBG(retval,d)); */
	return retval;
}

#else
static double wakeup_delay(double old) {
  double retval = 0.0;
  if (0.0 == old) {
    double m = median_time();
    if (m == 0.0 || m > 0.3) m = 0.1;
    retval = 0.1 + 5.0 * m + m * xcom_drand48();
  } else {
    retval = old * 1.4142136; /* Exponential backoff */
  }
  while (retval > 3.0) retval /= 1.31415926;
  /* DBGOUT(FN; NDBG(retval,d)); */
  return retval;
}

#endif

static void propose_noop(synode_no find, pax_machine *p) {
  /* Prepare to send a noop */
  site_def const *site = find_site_def(find);
  assert(!too_far(find));
  replace_pax_msg(&p->proposer.msg, pax_msg_new(find, site));
  assert(p->proposer.msg);
  create_noop(p->proposer.msg);
  /*  	DBGOUT(FN; SYCEXP(find););  */
  push_msg_3p(site, p, clone_pax_msg(p->proposer.msg), find, no_op);
}

static void send_read(synode_no find) {
  /* Prepare to send a read_op */
  site_def const *site = find_site_def(find);

  MAY_DBG(FN; NDBG(get_maxnodes(site), u); NDBG(get_nodeno(site), u););
  ADD_EVENTS(add_event(string_arg("find")); add_synode_event(find);
             add_event(string_arg("site")); add_event(void_arg((void *)site));
             add_event(string_arg("get_nodeno(site)"));
             add_event(uint_arg(get_nodeno(site))););

  /* See if node number matches ours */
  if (site) {
    if (find.node != get_nodeno(site)) {
      pax_msg *pm = pax_msg_new(find, site);
      ref_msg(pm);
      create_read(site, pm);
      MAY_DBG(FN; SYCEXP(find););

      MAY_DBG(FN; NDBG(get_maxnodes(site), u); NDBG(get_nodeno(site), u);
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

/* }}} */

/* {{{ Find missing values */

static int ok_to_propose(pax_machine *p) {
#if 0
	site_def const *s = find_site_def(p->synode.group_id);
	int	retval = (p->synode.node == get_nodeno(s) || task_now() -p->last_modified > DETECTOR_LIVE_TIMEOUT || may_be_dead(s->detected, p->synode.node, task_now()))
	 && !recently_active(p) && !finished(p) && !is_busy_machine(p);
#else
  int retval = (is_forcing_node(p) || !recently_active(p)) && !finished(p) &&
               !is_busy_machine(p);
#endif
  MAY_DBG(FN; NDBG(p->synode.node, u); NDBG(recently_active(p), d);
          NDBG(finished(p), d); NDBG(is_busy_machine(p), d); NDBG(retval, d));
  return retval;
}

static void read_missing_values(int n) {
  synode_no find = executed_msg;
  synode_no end = max_synode;
  int i = 0;

  MAY_DBG(FN; SYCEXP(find); SYCEXP(end));
  if (synode_gt(executed_msg, max_synode) ||
      synode_eq(executed_msg, null_synode))
    return;

  while (!synode_gt(find, end) && i < n && !too_far(find)) {
    pax_machine *p = get_cache(find);
    ADD_EVENTS(add_synode_event(find); add_synode_event(end);
               add_event(string_arg("active "));
               add_event(int_arg(recently_active(p)));
               add_event(string_arg("finished  "));
               add_event(int_arg(finished(p))); add_event(string_arg("busy "));
               add_event(int_arg(is_busy_machine(p))););
    MAY_DBG(FN; SYCEXP(find); SYCEXP(end); NDBG(recently_active(p), d);
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

  DBGOHK(FN; NDBG(get_maxnodes(get_site_def()), u); SYCEXP(find); SYCEXP(end));
  if (synode_gt(executed_msg, max_synode) ||
      synode_eq(executed_msg, null_synode))
    return;

  MAY_DBG(FN; SYCEXP(find); SYCEXP(end));
  i = 0;
  while (!synode_gt(find, end) && i < n && !too_far(find)) {
    pax_machine *p = get_cache(find);
    if (wait_forced_config) {
      force_pax_machine(p, 1);
    }
    DBGOHK(FN; NDBG(ok_to_propose(p), d); TIMECEXP(task_now());
           TIMECEXP(p->last_modified); SYCEXP(find));
    if (get_nodeno(find_site_def(find)) == VOID_NODE_NO) break;
    if (ok_to_propose(p)) {
      if (is_forcing_node(p) ||
          (task_now() - BUILD_TIMEOUT > p->last_modified)) {
        propose_noop(find, p);
      }
    }
    find = incr_synode(find);
    i++;
  }
}

/* Propose a noop for the range find..end */
void request_values(synode_no find, synode_no end) {
  DBGOUT(FN; SYCEXP(find); SYCEXP(end););
  while (!synode_gt(find, end) && !too_far(find)) {
    pax_machine *p = get_cache(find);
    site_def const *site = find_site_def(find);
    if (get_nodeno(site) == VOID_NODE_NO) break;
    if (!finished(p) && !is_busy_machine(p)) {
      /* Prepare to send a noop */
      replace_pax_msg(&p->proposer.msg, pax_msg_new(find, site));
      assert(p->proposer.msg);
      create_noop(p->proposer.msg);

      DBGOUT(FN; STRLIT("propose "); SYCEXP(find););
      push_msg_3p(site, p, pax_msg_new(find, site), find, no_op);
    }
    find = incr_synode(find);
  }
}

/* }}} */

/* {{{ Message handlers */
#define reply_msg(m)                                                \
  {                                                                 \
    if (is_local_node((m)->from, site)) {                           \
      dispatch_op(site, m, NULL);                                   \
    } else {                                                        \
      if (node_no_exists((m)->from, site) &&                        \
          (m)->group_id == get_group_id(site) &&                    \
          get_server(site, (m)->from)) {                            \
        send_server_msg(site, (m)->from, m);                        \
      } else {                                                      \
        link_into(&(msg_link_new((m), (m)->from)->l), reply_queue); \
      }                                                             \
    }                                                               \
  }

#define CREATE_REPLY(x)  \
  pax_msg *reply = NULL; \
  CLONE_PAX_MSG(reply, x)
#define SEND_REPLY  \
  reply_msg(reply); \
  replace_pax_msg(&reply, NULL)

static void teach_ignorant_node(site_def const *site, pax_machine *p,
                                pax_msg *pm, synode_no synode,
                                linkage *reply_queue) {
  CREATE_REPLY(pm);
  DBGOUT(FN; SYCEXP(synode));
  reply->synode = synode;
  reply->proposal = p->learner.msg->proposal;
  reply->msg_type = p->learner.msg->msg_type;
  copy_app_data(&reply->a, p->learner.msg->a);
  set_learn_type(reply);
  /* set_unique_id(reply, p->learner.msg->unique_id); */
  SEND_REPLY;
}

/* Handle incoming read */
static void handle_read(site_def const *site, pax_machine *p,
                        linkage *reply_queue, pax_msg *pm) {
  DBGOUT(FN; BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise);
         if (p->acceptor.msg) BALCEXP(p->acceptor.msg->proposal);
         STRLIT("type "); STRLIT(pax_msg_type_to_str(pm->msg_type)));

  if (finished(p)) { /* We have learned a value */
    teach_ignorant_node(site, p, pm, pm->synode, reply_queue);
  }
}

#ifdef USE_EXIT_TYPE
static void miss_prepare(site_def const *site, pax_msg *pm,
                         linkage *reply_queue) {
  CREATE_REPLY(pm);
  DBGOUT(FN; SYCEXP(pm->synode));
  reply->msg_type = normal;
  reply->a = new_app_data();
  reply->a->body.c_t = exit_type;
  reply->op = ack_prepare_op;
  SEND_REPLY;
}

static void miss_accept(site_def const *site, pax_msg *pm,
                        linkage *reply_queue) {
  CREATE_REPLY(pm);
  DBGOUT(FN; SYCEXP(pm->synode));
  ref_msg(reply);
  reply->msg_type = normal;
  reply->op = ack_accept_op;
  if (servers[pm->from]) {
    send_server_msg(site, pm->from, pm);
  }
  SEND_REPLY;
}

#endif

static void handle_simple_prepare(site_def const *site, pax_machine *p,
                                  pax_msg *pm, synode_no synode,
                                  linkage *reply_queue) {
  if (finished(p)) { /* We have learned a value */
    MAY_DBG(FN; SYCEXP(synode); BALCEXP(pm->proposal); NDBG(finished(p), d));
    teach_ignorant_node(site, p, pm, synode, reply_queue);
  } else {
    int greater =
        gt_ballot(pm->proposal,
                  p->acceptor.promise); /* Paxos acceptor phase 1 decision */
    MAY_DBG(FN; SYCEXP(synode); BALCEXP(pm->proposal); NDBG(greater, d));
    if (greater || noop_match(p, pm)) {
      CREATE_REPLY(pm);
      reply->synode = synode;
      if (greater)
        p->acceptor.promise = pm->proposal; /* promise to not accept any less */
      if (accepted(p)) {                    /* We have accepted a value */
        reply->proposal = p->acceptor.msg->proposal;
        reply->msg_type = p->acceptor.msg->msg_type;
        copy_app_data(&reply->a, p->acceptor.msg->a);
        MAY_DBG(FN; STRLIT(" already accepted value "); SYCEXP(synode));
        reply->op = ack_prepare_op;
      } else {
        MAY_DBG(FN; STRLIT(" no value synode "); SYCEXP(synode));
        reply->op = ack_prepare_empty_op;
      }
      SEND_REPLY;
    }
  }
}

/* Handle incoming prepare */
static void handle_prepare(site_def const *site, pax_machine *p,
                           linkage *reply_queue, pax_msg *pm) {
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("pm->from"));
             add_event(int_arg(pm->from));
             add_event(string_arg(pax_op_to_str(pm->op))););
#if 0
	DBGOUT(FN;
	    NDBG(pm->from, d); NDBG(pm->to, d);
	    SYCEXP(pm->synode);
	    BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise));
#endif
  MAY_DBG(FN; BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise);
          if (p->acceptor.msg) BALCEXP(p->acceptor.msg->proposal);
          STRLIT("type "); STRLIT(pax_msg_type_to_str(pm->msg_type)));

  handle_simple_prepare(site, p, pm, pm->synode, reply_queue);
}

static void check_propose(site_def const *site, pax_machine *p) {
  MAY_DBG(FN; SYCEXP(p->synode);
          COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(site))););
  PAX_MSG_SANITY_CHECK(p->proposer.msg);
  if (prep_majority(site, p)) {
    p->proposer.msg->proposal = p->proposer.bal;
    BIT_ZERO(p->proposer.prop_nodeset);
    p->proposer.msg->synode = p->synode;
    propose_msg(p->proposer.msg);
    p->proposer.sent_prop = p->proposer.bal;
  }
}

static void check_learn(site_def const *site, pax_machine *p) {
  MAY_DBG(FN; SYCEXP(p->synode);
          COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(site))););
  PAX_MSG_SANITY_CHECK(p->proposer.msg);
  if (get_nodeno(site) != VOID_NODE_NO && prop_majority(site, p)) {
    p->proposer.msg->synode = p->synode;
    if (p->proposer.msg->receivers) free_bit_set(p->proposer.msg->receivers);
    p->proposer.msg->receivers = clone_bit_set(p->proposer.prep_nodeset);
    BIT_SET(get_nodeno(site), p->proposer.msg->receivers);
    if (no_duplicate_payload)
      tiny_learn_msg(site, p->proposer.msg);
    else
      learn_msg(site, p->proposer.msg);
    p->proposer.sent_learn = p->proposer.bal;
  }
}

static void do_learn(site_def const *site MY_ATTRIBUTE((unused)),
                     pax_machine *p, pax_msg *m) {
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("m->from"));
             add_event(int_arg(m->from));
             add_event(string_arg(pax_op_to_str(m->op))););
  /* FN; SYCEXP(p->synode); SYCEXP(m->synode); STRLIT(NEWLINE); */
  MAY_DBG(FN; SYCEXP(p->synode); SYCEXP(m->synode);
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

static void handle_simple_ack_prepare(
    site_def const *site MY_ATTRIBUTE((unused)), pax_machine *p, pax_msg *m) {
  if (get_nodeno(site) != VOID_NODE_NO)
    BIT_SET(m->from, p->proposer.prep_nodeset);
}

/* Other node has already accepted a value */
static void handle_ack_prepare(site_def const *site, pax_machine *p,
                               pax_msg *m) {
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("m->from"));
             add_event(int_arg(m->from));
             add_event(string_arg(pax_op_to_str(m->op))););
#if 0
	DBGOUT(FN;
	    NDBG(pm->from, d); NDBG(pm->to, d);
	    SYCEXP(pm->synode);
	    BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise));
#endif
  assert(m);
  MAY_DBG(FN; if (p->proposer.msg) BALCEXP(p->proposer.msg->proposal);
          BALCEXP(p->proposer.bal); BALCEXP(m->reply_to);
          BALCEXP(p->proposer.sent_prop); SYCEXP(m->synode));
  if (m->from != VOID_NODE_NO &&
      eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my prepare */
    handle_simple_ack_prepare(site, p, m);
    if (gt_ballot(m->proposal, p->proposer.msg->proposal)) { /* greater */
      replace_pax_msg(&p->proposer.msg, m);
      assert(p->proposer.msg);
    }
    if (gt_ballot(m->reply_to, p->proposer.sent_prop)) check_propose(site, p);
  }
}

/* Other node has not already accepted a value */
static void handle_ack_prepare_empty(site_def const *site, pax_machine *p,
                                     pax_msg *m) {
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("m->from"));
             add_event(int_arg(m->from));
             add_event(string_arg(pax_op_to_str(m->op))););
#if 0
	DBGOUT(FN;
	    NDBG(m->from, d); NDBG(m->to, d);
	    SYCEXP(m->synode);
	    BALCEXP(m->proposal); BALCEXP(p->acceptor.promise));
#endif
  MAY_DBG(FN; if (p->proposer.msg) BALCEXP(p->proposer.msg->proposal);
          BALCEXP(p->proposer.bal); BALCEXP(m->reply_to);
          BALCEXP(p->proposer.sent_prop); SYCEXP(m->synode));
  if (m->from != VOID_NODE_NO &&
      eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my prepare */
    handle_simple_ack_prepare(site, p, m);
    if (gt_ballot(m->reply_to, p->proposer.sent_prop)) check_propose(site, p);
  }
}

/* #define AUTO_MSG(p,synode) {if(!(p)){replace_pax_msg(&(p),
 * pax_msg_new(synode, site));} */

static void handle_simple_accept(site_def const *site, pax_machine *p,
                                 pax_msg *m, synode_no synode,
                                 linkage *reply_queue) {
  if (finished(p)) { /* We have learned a value */
    teach_ignorant_node(site, p, m, synode, reply_queue);
  } else if (!gt_ballot(p->acceptor.promise,
                        m->proposal) || /* Paxos acceptor phase 2 decision */
             noop_match(p, m)) {
    MAY_DBG(FN; SYCEXP(m->synode); STRLIT("accept "); BALCEXP(m->proposal));
    replace_pax_msg(&p->acceptor.msg, m);
    {
      CREATE_REPLY(m);
      reply->op = ack_accept_op;
      reply->synode = synode;
      SEND_REPLY;
    }
  }
}

/* Accecpt value if promise is not greater */
static void handle_accept(site_def const *site, pax_machine *p,
                          linkage *reply_queue, pax_msg *m) {
  MAY_DBG(FN; BALCEXP(p->acceptor.promise); BALCEXP(m->proposal);
          STREXP(pax_msg_type_to_str(m->msg_type)));
  PAX_MSG_SANITY_CHECK(m);
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("m->from"));
             add_event(int_arg(m->from));
             add_event(string_arg(pax_op_to_str(m->op))););

  handle_simple_accept(site, p, m, m->synode, reply_queue);
}

/* Handle answer to accept */
static void handle_ack_accept(site_def const *site, pax_machine *p,
                              pax_msg *m) {
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("m->from"));
             add_event(int_arg(m->from));
             add_event(string_arg(pax_op_to_str(m->op))););
  MAY_DBG(FN; SYCEXP(m->synode); BALCEXP(p->proposer.bal);
          BALCEXP(p->proposer.sent_learn); BALCEXP(m->proposal);
          BALCEXP(m->reply_to););
  MAY_DBG(FN; SYCEXP(p->synode);
          if (p->acceptor.msg) BALCEXP(p->acceptor.msg->proposal);
          BALCEXP(p->proposer.bal); BALCEXP(m->reply_to););
  if (get_nodeno(site) != VOID_NODE_NO && m->from != VOID_NODE_NO &&
      eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my accept */
    BIT_SET(m->from, p->proposer.prop_nodeset);
    if (gt_ballot(m->proposal, p->proposer.sent_learn)) check_learn(site, p);
  }
}

static void force_pax_machine(pax_machine *p, int enforcer) {
  if (!p->enforcer) { /* Not if already marked as forcing node */
    if (enforcer) {   /* Only if forcing node */
      /* Increase ballot count with a large increment without overflowing */
      int32_t delta = (INT32_MAX - p->proposer.bal.cnt) / 3;
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

  DBGOUT(FN; SYCEXP(executed_msg); SYCEXP(end));
  if (synode_gt(end, max_synode)) set_max_synode(end);

  free_forced_config_site_def();
  wait_forced_config = 0;
  forced_config = s;
  force_interval(executed_msg, max_synode,
                 enforcer); /* Force everything in the pipeline */
}

/* Learn this value */
static void handle_learn(site_def const *site, pax_machine *p, pax_msg *m) {
  MAY_DBG(FN; STRLIT("proposer nodeset ");
          dbg_bitset(p->proposer.prop_nodeset, get_maxnodes(site)););
  MAY_DBG(FN; STRLIT("receivers ");
          dbg_bitset(m->receivers, get_maxnodes(site)););
  MAY_DBG(FN; NDBG(task_now(), f); SYCEXP(p->synode);
          COPY_AND_FREE_GOUT(dbg_app_data(m->a)););

  PAX_MSG_SANITY_CHECK(m);
  if (is_real_recover(m->a)) {
    DBGOUT(FN; NDBG(force_recover, d); STREXP(start_t_to_str(start_type));
           STREXP(start_t_to_str(m->start_type)); SYCEXP(m->synode);
           SYCEXP(m->a->app_key); SYCEXP(p->synode); NDBG(finished(p), d););
  }
  if (!finished(p)) { /* Avoid re-learn */
    do_learn(site, p, m);
    /* Check for special messages */
    if (m->a && m->a->body.c_t == unified_boot_type) {
      DBGOUT(FN; STRLIT("Got unified_boot "); SYCEXP(p->synode);
             SYCEXP(m->synode););
      XCOM_FSM(xa_net_boot, void_arg(m->a));
    }
    /* See if someone is forcing a new config */
    if (m->force_delivery && m->a) {
      DBGOUT(FN; STRLIT("Got forced config "); SYCEXP(p->synode);
             SYCEXP(m->synode););
      /* Configure all messages from executed_msg until start of new config
         as forced messages so they will eventually be finished */
      /* Immediately install this new config */
      switch (m->a->body.c_t) {
        case add_node_type:
          /* purecov: begin deadcode */
          start_force_config(clone_site_def(handle_add_node(m->a)), 0);
          break;
        /* purecov: end */
        case remove_node_type:
          /* purecov: begin deadcode */
          start_force_config(clone_site_def(handle_remove_node(m->a)), 0);
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
  /*   MAY_DBG(FN;); */
  /*   MAY_DBG(FN; NDBG(task_now(),f); SYCEXP(p->msg->synode)); */
  if (!finished(p)) {
    skip_value(m);
    do_learn(site, p, m);
  }
  /*   MAY_DBG(FN; STRLIT("taskwakeup "); SYCEXP(p->msg->synode)); */
  task_wakeup(&p->rv);
}

static void handle_client_msg(pax_msg *p) {
  if (!p || p->a == NULL) /* discard invalid message */
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
      MAY_DBG(FN; PTREXP(site); PTREXP(mysite); PTREXP(prev);
              SYCEXP(site->boot_key); if (prev) { SYCEXP(prev->boot_key); });
      if (!prev) {
        /** alive when no site, no known previous definition, and present in new
         * is accepted */
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
      MAY_DBG(FN; PTREXP(site); PTREXP(mysite); SYCEXP(site->boot_key);
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
  MAY_DBG(FN; PTREXP(site));
  return 1;
}
#endif

/* Handle incoming alive message */
static double sent_alive = 0.0;
static inline void handle_alive(site_def const *site, linkage *reply_queue,
                                pax_msg *pm) {
#ifdef ACCEPT_SITE_TEST
  int accept = accept_site(site);
#else
  int accept = 1; /* accept_site(site); */
#endif
  int not_to_oneself = (pm->from != get_nodeno(site) && pm->from != pm->to);

  MAY_DBG(FN; SYCEXP(pm->synode); NDBG(pm->from, u); NDBG(pm->to, u);
          NUMEXP(accept); NUMEXP(get_group_id(site)));

  /*
   This code will check if the ping is intended to us.
   If the encoded node does not exist in the current configuration,
   we avoid sending need_boot_op, since it must be from a different
   reincarnation of this node.
   */
  if (site && pm->a && pm->a->body.c_t == xcom_boot_type) {
    DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&pm->a->body.app_u_u.nodes)););

    not_to_oneself &= node_exists_with_uid(
        &pm->a->body.app_u_u.nodes.node_list_val[0], &get_site_def()->nodes);
  }

  if (!client_boot_done &&           /* Already done? */
      not_to_oneself &&              /* Not to oneself */
      accept &&                      /* Accept new site definition */
      !is_dead_site(pm->group_id)) { /* Avoid dealing with zombies */
    double t = task_now();
    if (t - sent_alive > 1.0) {
      CREATE_REPLY(pm);
      reply->op = need_boot_op;
      SEND_REPLY;
      sent_alive = t;
      DBGOUT(FN; STRLIT("sent need_boot_op"););
    }
  }
}

static void update_max_synode(pax_msg *p) {
#if 0
	/* if (group_id == 0 || synode_eq(max_synode, null_synode) ||  */
	/*     synode_gt(p->max_synode, max_synode)) { */
	/*   set_max_synode(p->max_synode); */
	/* } */
	if (group_id == 0 || synode_eq(max_synode, null_synode) ||
	    (p->msg_type == normal &&
	    (max_synode.group_id == 0 || p->synode.group_id == 0 ||
	    max_synode.group_id == p->synode.group_id) &&
	    synode_gt(p->synode, max_synode))) {
		set_max_synode(p->synode);
	}
#else
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
#endif
}

/* Add app_data to message cache */
/* purecov: begin deadcode */
void add_to_cache(app_data_ptr a, synode_no synode) {
  pax_machine *pm = get_cache(synode);
  pax_msg *msg = pax_msg_new_0(synode);
  ref_msg(msg);
  assert(pm);
  copy_app_data(&msg->a, a);
  set_learn_type(msg);
  /* msg->unique_id = a->unique_id; */
  do_learn(0, pm, msg);
  unref_msg(&msg);
}
/* purecov: end */

/* }}} */

/* {{{ Message dispatch */
#define BAL_FMT "ballot {cnt %d node %d}"
#define BAL_MEM(x) (x).cnt, (x).node

static int clicnt = 0;

static u_int allow_add_node(app_data_ptr a) {
  /* Get information on the current site definition */
  const site_def *new_site_def = get_site_def();
  const site_def *valid_site_def = find_site_def(executed_msg);

  /* Get information on the nodes to be added */
  u_int nodes_len = a->body.app_u_u.nodes.node_list_len;
  node_address *nodes_to_change = a->body.app_u_u.nodes.node_list_val;

  u_int i = 0;
  for (; i < nodes_len; i++) {
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
      G_MESSAGE(
          "Old incarnation found while trying to "
          "add node %s %.*s.",
          nodes_to_change[i].address, nodes_to_change[i].uuid.data.data_len,
          nodes_to_change[i].uuid.data.data_val);
      return 0;
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

  u_int i = 0;
  for (; i < nodes_len; i++) {
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

  if (executed_msg.msgno <= 2) return REQUEST_RETRY;

  if (a && a->group_id != 0 && a->group_id != executed_msg.group_id) {
    switch (a->body.c_t) {
      case add_node_type:
        log_cfgchange_wrong_group(
            a,
            "The request to add %s to the group has been rejected because it "
            "is aimed at another group");
        break;
      case remove_node_type:
        log_cfgchange_wrong_group(
            a,
            "The request to remove %s from the group has been rejected because "
            "it is aimed at another group");
        break;
      case force_config_type:
        G_WARNING(
            "The request to force the group membership has been rejected "
            "because it is aimed at another group");
        break;
      default:
        assert(0 &&
               "A cargo_type different from {add_node_type, remove_node_type, "
               "force_config_type} should not have hit this code path");
    }
    return REQUEST_FAIL;
  }

  if (a && a->body.c_t == add_node_type && !allow_add_node(a))
    return REQUEST_FAIL;

  if (a && a->body.c_t == remove_node_type && !allow_remove_node(a))
    return REQUEST_FAIL;

  return REQUEST_OK;
}

static void activate_sweeper() {
  if (sweeper) {
    ADD_EVENTS(add_event(string_arg("sweeper activated max_synode"));
               add_synode_event(max_synode););
    task_activate(sweeper);
  }
}

pax_msg *dispatch_op(site_def const *site, pax_msg *p, linkage *reply_queue) {
  pax_machine *pm = NULL;
  site_def *dsite = find_site_def_rw(p->synode);
  int in_front = too_far(p->synode);

  if (p->force_delivery) {
    /* Ensure that forced message can be processed */
    in_front = 0;
  }

  if (dsite && p->op != client_msg) {
    note_detected(dsite, p->from);
    update_delivered(dsite, p->from, p->delivered_msg);
  }

  MAY_DBG(FN; STRLIT("incoming message "); COPY_AND_FREE_GOUT(dbg_pax_msg(p)););
  ADD_EVENTS(add_synode_event(p->synode); add_event(string_arg("p->from"));
             add_event(int_arg(p->from));
             add_event(string_arg(pax_op_to_str(p->op))););

  switch (p->op) {
    case client_msg:
      clicnt++;
      if (p->a && (p->a->body.c_t == enable_arbitrator)) {
        CREATE_REPLY(p);
        DBGOUT(FN; STRLIT("Got enable_arbitrator from client");
               SYCEXP(p->synode););
        ARBITRATOR_HACK = 1;
        reply->op = xcom_client_reply;
        reply->cli_err = REQUEST_OK;
        SEND_REPLY;
        break;
      }
      if (p->a && (p->a->body.c_t == disable_arbitrator)) {
        CREATE_REPLY(p);
        DBGOUT(FN; STRLIT("Got disable_arbitrator from client");
               SYCEXP(p->synode););
        ARBITRATOR_HACK = 0;
        reply->op = xcom_client_reply;
        reply->cli_err = REQUEST_OK;
        SEND_REPLY;
        break;
      }
      if (p->a && (p->a->body.c_t == set_cache_limit)) {
        CREATE_REPLY(p);
        DBGOUT(FN; STRLIT("Got set_cache_limit from client");
               SYCEXP(p->synode););
        if (the_app_xcom_cfg) {
          set_max_cache_size((size_t)p->a->body.app_u_u.cache_limit);
          reply->cli_err = REQUEST_OK;
        } else {
          reply->cli_err = REQUEST_FAIL;
        }
        reply->op = xcom_client_reply;
        SEND_REPLY;
        break;
      }
      if (p->a && (p->a->body.c_t == x_terminate_and_exit)) {
        CREATE_REPLY(p);
        DBGOUT(FN; STRLIT("Got terminate_and_exit from client");
               SYCEXP(p->synode););
        reply->op = xcom_client_reply;
        reply->cli_err = REQUEST_OK;
        SEND_REPLY;
        /*
          The function frees sites which is used by SEND_REPLY,
          so it should be called after SEND_REPLY.
        */
        terminate_and_exit();
        break;
      }
      if (p->a && (p->a->body.c_t == add_node_type ||
                   p->a->body.c_t == remove_node_type ||
                   p->a->body.c_t == force_config_type)) {
        client_reply_code cli_err;
        CREATE_REPLY(p);
        reply->op = xcom_client_reply;
        reply->cli_err = cli_err = can_execute_cfgchange(p);
        SEND_REPLY;
        if (cli_err != REQUEST_OK) {
          break;
        }
      }
      if (p->a && p->a->body.c_t == unified_boot_type) {
        DBGOUT(FN; STRLIT("Got unified_boot from client"); SYCEXP(p->synode););
        DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
        DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
        XCOM_FSM(xa_net_boot, void_arg(p->a));
      }
      if (p->a && p->a->body.c_t == add_node_type) {
        DBGOUT(FN; STRLIT("Got add_node from client"); SYCEXP(p->synode););
        DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
        DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
        assert(get_site_def());
      }
      if (p->a && p->a->body.c_t == remove_node_type) {
        DBGOUT(FN; STRLIT("Got remove_node from client"); SYCEXP(p->synode););
        DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
        DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
        assert(get_site_def());
      }
      if (p->a && p->a->body.c_t == force_config_type) {
        DBGOUT(FN; STRLIT("Got new force config from client");
               SYCEXP(p->synode););
        DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)););
        DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
        assert(get_site_def());
        XCOM_FSM(xa_force_config, void_arg(p->a));
      }
      handle_client_msg(p);
      break;
    case initial_op:
      break;
    case read_op:
      pm = get_cache(p->synode);
      assert(pm);

      if (client_boot_done) handle_alive(site, reply_queue, p);

      handle_read(site, pm, reply_queue, p);
      break;
    case prepare_op:
      pm = get_cache(p->synode);
      assert(pm);
      if (p->force_delivery) pm->force_delivery = 1;
      pm->last_modified = task_now();
      MAY_DBG(FN; dbg_pax_msg(p));

      if (client_boot_done) handle_alive(site, reply_queue, p);

      handle_prepare(site, pm, reply_queue, p);
      break;
    case ack_prepare_op:
      if (in_front || !is_cached(p->synode)) break;
      pm = get_cache(p->synode);
      if (p->force_delivery) pm->force_delivery = 1;
      if (!pm->proposer.msg) break;
      assert(pm && pm->proposer.msg);
      handle_ack_prepare(site, pm, p);
      break;
    case ack_prepare_empty_op:
      if (in_front || !is_cached(p->synode)) break;
      pm = get_cache(p->synode);
      if (p->force_delivery) pm->force_delivery = 1;
      if (!pm->proposer.msg) break;
      assert(pm && pm->proposer.msg);
      handle_ack_prepare_empty(site, pm, p);
      break;
    case accept_op:
      pm = get_cache(p->synode);
      assert(pm);
      if (p->force_delivery) pm->force_delivery = 1;
      pm->last_modified = task_now();
      MAY_DBG(FN; dbg_pax_msg(p));

      handle_alive(site, reply_queue, p);

      handle_accept(site, pm, reply_queue, p);
      break;
    case ack_accept_op:
      if (in_front || !is_cached(p->synode)) break;
      pm = get_cache(p->synode);
      if (p->force_delivery) pm->force_delivery = 1;
      if (!pm->proposer.msg) break;
      assert(pm && pm->proposer.msg);
      handle_ack_accept(site, pm, p);
      break;
    case recover_learn_op:
      DBGOUT(FN; STRLIT("recover_learn_op receive "); SYCEXP(p->synode));
      pm = get_cache(p->synode);
      assert(pm);
      if (p->force_delivery) pm->force_delivery = 1;
      pm->last_modified = task_now();
      update_max_synode(p);
      {
        DBGOUT(FN; STRLIT("recover_learn_op learn "); SYCEXP(p->synode));
        p->op = learn_op;
        handle_learn(site, pm, p);
      }
      break;
    case learn_op:
    learnop:
      pm = get_cache(p->synode);
      assert(pm);
      if (p->force_delivery) pm->force_delivery = 1;
      pm->last_modified = task_now();
      update_max_synode(p);
      activate_sweeper();
      handle_learn(site, pm, p);
      break;
    case tiny_learn_op:
      if (p->msg_type == no_op) goto learnop;
      pm = get_cache(p->synode);
      assert(pm);
      if (p->force_delivery) pm->force_delivery = 1;
      if (pm->acceptor.msg) {
        /* 			BALCEXP(pm->acceptor.msg->proposal); */
        if (eq_ballot(pm->acceptor.msg->proposal, p->proposal)) {
          pm->acceptor.msg->op = learn_op;
          pm->last_modified = task_now();
          update_max_synode(p);
          activate_sweeper();
          handle_learn(site, pm, pm->acceptor.msg);
        } else {
          send_read(p->synode);
          DBGOUT(FN; STRLIT("tiny_learn"); SYCEXP(p->synode);
                 BALCEXP(pm->acceptor.msg->proposal); BALCEXP(p->proposal));
        }
      } else {
        send_read(p->synode);
        DBGOUT(FN; STRLIT("tiny_learn"); SYCEXP(p->synode);
               BALCEXP(p->proposal));
      }
      break;
    case skip_op:
      pm = get_cache(p->synode);
      assert(pm);
      if (p->force_delivery) pm->force_delivery = 1;
      pm->last_modified = task_now();
      handle_skip(site, pm, p);
      break;
    case i_am_alive_op:
      handle_alive(site, reply_queue, p);
      break;
    case are_you_alive_op:
      handle_alive(site, reply_queue, p);
      break;
    case need_boot_op:
      /* purecov: begin deadcode */
      XCOM_FSM(xa_need_snapshot, void_arg(p));
      break;
    /* purecov: end */
    case snapshot_op:
      if (!is_dead_site(p->group_id)) {
        update_max_synode(p);
      }
      break;
    case gcs_snapshot_op:
      if (!is_dead_site(p->group_id)) {
        update_max_synode(p);
        XCOM_FSM(xa_snapshot, void_arg(p));
        XCOM_FSM(xa_complete, int_arg(0));
      }
      break;
    case die_op:
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
        g_critical(
            "Node %u unable to get message, process will now exit. Please "
            "ensure that the process is restarted",
            get_nodeno(site));
        exit(1);
      }
    default:
      break;
  }
  return (p);
}

/* }}} */

/* {{{ Acceptor-learner task */
#define SERIALIZE_REPLY(msg)                \
  msg->to = ep->p->from;                    \
  msg->from = ep->p->to;                    \
  msg->delivered_msg = get_delivered_msg(); \
  msg->max_synode = get_max_synode();       \
  serialize_msg(msg, ep->rfd.x_proto, &ep->buflen, &ep->buf);

#define WRITE_REPLY                                              \
  if (ep->buflen) {                                              \
    int64_t sent;                                                \
    TASK_CALL(task_write(&ep->rfd, ep->buf, ep->buflen, &sent)); \
    send_count[ep->p->op]++;                                     \
    send_bytes[ep->p->op] += ep->buflen;                         \
    X_FREE(ep->buf);                                             \
  }                                                              \
  ep->buf = NULL;

int acceptor_learner_task(task_arg arg) {
  DECL_ENV
  connection_descriptor rfd;
  srv_buf *in_buf;

  pax_msg *p;
  u_int buflen;
  char *buf;
  linkage reply_queue;
  int errors;
  server *srv;
  END_ENV;

  TASK_BEGIN

  ep->in_buf = calloc(1, sizeof(srv_buf));

  ep->rfd.fd = get_int_arg(arg);
#ifdef XCOM_HAVE_OPENSSL
  ep->rfd.ssl_fd = 0;
#endif
  ep->p = NULL;
  ep->buflen = 0;
  ep->buf = NULL;
  ep->errors = 0;
  ep->srv = 0;

  /* We have a connection, make socket non-blocking and wait for request */
  unblock_fd(ep->rfd.fd);
  set_nodelay(ep->rfd.fd);
  wait_io(stack, ep->rfd.fd, 'r');
  TASK_YIELD;

#ifdef XCOM_HAVE_OPENSSL
  if (xcom_use_ssl()) {
    ep->rfd.ssl_fd = SSL_new(server_ctx);
    SSL_set_fd(ep->rfd.ssl_fd, ep->rfd.fd);

    {
      int ret_ssl;
      int err;
      ERR_clear_error();
      ret_ssl = SSL_accept(ep->rfd.ssl_fd);
      err = SSL_get_error(ep->rfd.ssl_fd, ret_ssl);

      while (ret_ssl != SSL_SUCCESS) {
        if (err == SSL_ERROR_WANT_READ) {
          wait_io(stack, ep->rfd.fd, 'r');
        } else if (err == SSL_ERROR_WANT_WRITE) {
          wait_io(stack, ep->rfd.fd, 'w');
        } else { /* Some other error, give up */
          break;
        }
        TASK_YIELD;
        SET_OS_ERR(0);
        G_DEBUG("acceptor learner accept retry fd %d", ep->rfd.fd);
        ERR_clear_error();
        ret_ssl = SSL_accept(ep->rfd.ssl_fd);
        err = SSL_get_error(ep->rfd.ssl_fd, ret_ssl);
      }

      if (ret_ssl != SSL_SUCCESS) {
        ssl_free_con(&ep->rfd);
        close_connection(&ep->rfd);
        TERMINATE;
      }
    }

  } else {
    ep->rfd.ssl_fd = 0;
  }
#endif
  set_connected(&ep->rfd, CON_FD);
  link_init(&ep->reply_queue, type_hash("msg_link"));

again:
  while (!xcom_shutdown) {
    int64_t n = 0;
    site_def const *site = 0;
    unchecked_replace_pax_msg(&ep->p, pax_msg_new_0(null_synode));

    if (use_buffered_read) {
      TASK_CALL(buffered_read_msg(&ep->rfd, ep->in_buf, ep->p, ep->srv, &n));
    } else {
      TASK_CALL(read_msg(&ep->rfd, ep->p, ep->srv, &n));
    }
    if (((int)ep->p->op < (int)client_msg || ep->p->op > LAST_OP)) {
      /* invalid operation, ignore message */
      delete_pax_msg(ep->p);
      ep->p = NULL;
      TASK_YIELD;
      continue;
    }
    if (n <= 0) {
      break;
    }
    site = find_site_def(ep->p->synode);
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
    ep->srv = get_server(site, ep->p->from);
    ep->p->refcnt = 1; /* Refcnt from other end is void here */
    MAY_DBG(FN; NDBG(ep->rfd.fd, d); NDBG(task_now(), f);
            COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p)););
    receive_count[ep->p->op]++;
    receive_bytes[ep->p->op] += (uint64_t)n + MSG_HDR_SIZE;
    {
      int behind = FALSE;
      if (get_maxnodes(site) > 0) {
        behind = ep->p->synode.msgno < delivered_msg.msgno;
      }
      ADD_EVENTS(add_event(string_arg("before dispatch "));
                 add_synode_event(ep->p->synode);
                 add_event(string_arg("ep->p->from"));
                 add_event(int_arg(ep->p->from));
                 add_event(string_arg(pax_op_to_str(ep->p->op)));
                 add_event(string_arg(pax_msg_type_to_str(ep->p->msg_type)));
                 add_event(string_arg("is_cached(ep->p->synode)"));
                 add_event(int_arg(is_cached(ep->p->synode)));
                 add_event(string_arg("behind")); add_event(int_arg(behind)););
      /* Special treatment to see if synode number is valid. Return no-op if
       * not. */
      if (ep->p->op == read_op || ep->p->op == prepare_op ||
          ep->p->op == accept_op) {
        if (site) {
          ADD_EVENTS(add_event(string_arg("ep->p->synode"));
                     add_synode_event(ep->p->synode);
                     add_event(string_arg("site->start"));
                     add_synode_event(site->start);
                     add_event(string_arg("site->nodes.node_list_len"));
                     add_event(int_arg(site->nodes.node_list_len)););
          if (ep->p->synode.node >= site->nodes.node_list_len) {
            CREATE_REPLY(ep->p);
            ref_msg(reply);
            create_noop(reply);
            set_learn_type(reply);
            SERIALIZE_REPLY(reply);
            delete_pax_msg(reply); /* Deallocate BEFORE potentially blocking
                                      call which will lose value of reply */
            WRITE_REPLY;
            goto again;
          }
        }
      }
      if (ep->p->msg_type == normal ||
          ep->p->synode.msgno == 0 || /* Used by i-am-alive and so on */
          is_cached(ep->p->synode) || /* Already in cache */
          (!behind)) { /* Guard against cache pollution from other nodes */
        dispatch_op(site, ep->p, &ep->reply_queue);

        /* Send replies on same fd */
        while (!link_empty(&ep->reply_queue)) {
          msg_link *reply = (msg_link *)(link_extract_first(&ep->reply_queue));
          MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_linkage(&ep->reply_queue));
                  COPY_AND_FREE_GOUT(dbg_msg_link(reply));
                  COPY_AND_FREE_GOUT(dbg_pax_msg(reply->p)););
          assert(reply->p);
          assert(reply->p->refcnt > 0);
          SERIALIZE_REPLY(reply->p);
          msg_link_delete(&reply); /* Deallocate BEFORE potentially blocking
                                      call which will lose value of reply */
          WRITE_REPLY;
        }
      } else {
        DBGOUT(FN; STRLIT("rejecting "); STRLIT(pax_op_to_str(ep->p->op));
               NDBG(ep->p->from, d); NDBG(ep->p->to, d); SYCEXP(ep->p->synode);
               BALCEXP(ep->p->proposal));
        if (xcom_booted() && behind) {
#ifdef USE_EXIT_TYPE
          if (ep->p->op == prepare_op) {
            miss_prepare(ep->p, &ep->reply_queue);
          } else if (ep->p->op == accept_op) {
            miss_accept(ep->p, &ep->reply_queue);
          }
#else
          if (/*ep->p->op == prepare_op && */ was_removed_from_cache(
              ep->p->synode)) {
            DBGOUT(FN; STRLIT("send_die "); STRLIT(pax_op_to_str(ep->p->op));
                   NDBG(ep->p->from, d); NDBG(ep->p->to, d);
                   SYCEXP(ep->p->synode); BALCEXP(ep->p->proposal));
            if (get_maxnodes(site) > 0) {
              pax_msg *np = NULL;
              np = pax_msg_new(ep->p->synode, site);
              ref_msg(np);
              np->op = die_op;
              SERIALIZE_REPLY(np);
              DBGOUT(FN; STRLIT("sending die_op to node "); NDBG(np->to, d);
                     SYCEXP(executed_msg); SYCEXP(max_synode);
                     SYCEXP(np->synode));
              unref_msg(&np); /* Deallocate BEFORE potentially blocking call
                                 which will lose value of np */
              WRITE_REPLY;
            }
          }
#endif
        }
      }
    }
    /* TASK_YIELD; */
  }

  FINALLY
  MAY_DBG(FN; STRLIT(" shutdown "); NDBG(ep->rfd.fd, d); NDBG(task_now(), f));
  if (ep->reply_queue.suc && !link_empty(&ep->reply_queue))
    empty_msg_list(&ep->reply_queue);
  unchecked_replace_pax_msg(&ep->p, NULL);
  shutdown_connection(&ep->rfd);
  DBGOUT(FN; NDBG(xcom_shutdown, d));
  if (ep->buf) X_FREE(ep->buf);
  free(ep->in_buf);

  TASK_END;
}

/* }}} */

/* {{{ Reply handler task */

static void server_handle_need_snapshot(server *srv, site_def const *s,
                                        node_no node);

int reply_handler_task(task_arg arg) {
  DECL_ENV
  server *s;
  pax_msg *reply;
  double dtime;
  END_ENV;

  TASK_BEGIN

  ep->dtime = INITIAL_CONNECT_WAIT; /* Initial wait is short, to avoid
                                       unnecessary waiting */
  ep->s = (server *)get_void_arg(arg);
  srv_ref(ep->s);
  ep->reply = NULL;

  for (;;) {
    while (!is_connected(&ep->s->con)) {
      MAY_DBG(FN; STRLIT("waiting for connection"));
      TASK_DELAY(ep->dtime);
      ep->dtime *= CONNECT_WAIT_INCREASE; /* Increase wait time for next try */
      if (ep->dtime > MAX_CONNECT_WAIT) {
        ep->dtime = MAX_CONNECT_WAIT;
      }
    }
    ep->dtime = INITIAL_CONNECT_WAIT;
    {
      int64_t n;
      unchecked_replace_pax_msg(&ep->reply, pax_msg_new_0(null_synode));

      ADD_EVENTS(add_event(string_arg("ep->s->con.fd"));
                 add_event(int_arg(ep->s->con.fd)););
      TASK_CALL(read_msg(&ep->s->con, ep->reply, ep->s, &n));
      ADD_EVENTS(add_event(string_arg("ep->s->con.fd"));
                 add_event(int_arg(ep->s->con.fd)););
      ep->reply->refcnt = 1; /* Refcnt from other end is void here */
      if (n <= 0) {
        shutdown_connection(&ep->s->con);
        continue;
      }
      receive_bytes[ep->reply->op] += (uint64_t)n + MSG_HDR_SIZE;
    }
    MAY_DBG(FN; NDBG(ep->s->con.fd, d); NDBG(task_now(), f);
            COPY_AND_FREE_GOUT(dbg_pax_msg(ep->reply)););
    receive_count[ep->reply->op]++;

    /* Special test for need_snapshot, since node and site may not be consistent
     */
    if (ep->reply->op == need_boot_op) {
      pax_msg *p = ep->reply;
      server_handle_need_snapshot(ep->s, find_site_def(p->synode), p->from);
    } else {
      // We only handle messages from this connection is the server is valid.
      if (ep->s->invalid == 0)
        dispatch_op(find_site_def(ep->reply->synode), ep->reply, NULL);
    }
    TASK_YIELD;
  }

  FINALLY
  replace_pax_msg(&ep->reply, NULL);

  shutdown_connection(&ep->s->con);
  ep->s->reply_handler = NULL;
  DBGOUT(FN; STRLIT(" shutdown "); NDBG(ep->s->con.fd, d); NDBG(task_now(), f));
  srv_unref(ep->s);

  TASK_END;
}

/* }}} */

/* purecov: begin deadcode */
static inline void xcom_sleep(unsigned int seconds) {
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
long xcom_unique_long(void) {
#if defined(_WIN32)
  __time64_t ltime;

  _time64(&ltime);
  return (long)(ltime ^ GetCurrentProcessId());
#else
  return gethostid() ^ getpid();
#endif
}

  /* {{{ Coroutine macros */
  /*
     Coroutine device (or more precisely, a finite state machine, as the
     stack is not preserved), described by its inventor Tom Duff as
     being "too horrid to go into". The basic idea is that the switch
     can be used to jump anywhere in the code, so we note where we are
     when we return, and jump there when we enter the routine again by
     switching on the state, which is really a line number supplied by
     the CO_RETURN macro.
  */

#define CO_BEGIN          \
  switch (state) {        \
    default:              \
      assert(state == 0); \
    case 0:
#define CO_END }

#define CO_RETURN(x)  \
  {                   \
    state = __LINE__; \
    return x;         \
    case __LINE__:;   \
  }

#define HALT(x)   \
  while (1) {     \
    CO_RETURN(x); \
  }

/* purecov: begin deadcode */
void send_app_data(app_data_ptr a) {
  pax_msg *msg = pax_msg_new(null_synode, get_proposer_site());
  xcom_send(a, msg);
}

void xcom_send_data(uint32_t size, char *data) {
  app_data_ptr a = new_app_data();
  a->body.c_t = app_type;
  a->body.app_u_u.data.data_len = size;
  a->body.app_u_u.data.data_val = data;
  send_app_data(a);
}

app_data_ptr create_config(node_list *nl, cargo_type type) {
  app_data_ptr a = new_app_data();
  a->body.c_t = type;
  init_node_list(nl->node_list_len, nl->node_list_val, &a->body.app_u_u.nodes);

  return a;
}
/* purecov: end */

app_data_ptr init_config_with_group(app_data *a, node_list *nl, cargo_type type,
                                    uint32_t group_id) {
  init_app_data(a);
  a->app_key.group_id = a->group_id = group_id;
  a->body.c_t = type;
  init_node_list(nl->node_list_len, nl->node_list_val, &a->body.app_u_u.nodes);
  return a;
}

/* purecov: begin deadcode */
app_data_ptr create_config_with_group(node_list *nl, cargo_type type,
                                      uint32_t group_id) {
  app_data_ptr a = new_app_data();
  return init_config_with_group(a, nl, type, group_id);
}

void send_boot(node_list *nl) {
  app_data_ptr a = create_config(nl, unified_boot_type);
  install_node_group(a); /* Cannot get consensus unless group is known */
  send_app_data(a);
}

void send_add_node(node_list *nl) {
  send_app_data(create_config(nl, add_node_type));
}

void send_remove_node(node_list *nl) {
  send_app_data(create_config(nl, remove_node_type));
}

void send_config(node_list *nl) {
  send_app_data(create_config(nl, force_config_type));
}

void send_client_app_data(char *srv, xcom_port port, app_data_ptr a) {
  pax_msg *msg = pax_msg_new(null_synode, 0);
  envelope *e = calloc(1, sizeof(envelope));

  msg->a = a;
  msg->to = VOID_NODE_NO;
  msg->op = client_msg;
  e->srv = strdup(srv);
  e->port = port;
  e->p = msg;
  e->crash_on_error = 0;
  task_new(client_task, void_arg(e), "client_task", XCOM_THREAD_DEBUG);
}

void send_client_boot(char *srv, xcom_port port, node_list *nl) {
  send_client_app_data(srv, port, create_config(nl, unified_boot_type));
}

void send_client_add_node(char *srv, xcom_port port, node_list *nl) {
  send_client_app_data(srv, port, create_config(nl, add_node_type));
}

void send_client_remove_node(char *srv, xcom_port port, node_list *nl) {
  send_client_app_data(srv, port, create_config(nl, remove_node_type));
}

void send_client_config(char *srv, xcom_port port, node_list *nl) {
  send_client_app_data(srv, port, create_config(nl, force_config_type));
}
/* purecov: end */

static void server_send_snapshot(server *srv, site_def const *s,
                                 gcs_snapshot *gcs_snap, node_no node) {
  pax_msg *p = pax_msg_new(gcs_snap->log_start, get_site_def());
  ref_msg(p);
  p->op = gcs_snapshot_op;
  p->gcs_snap = gcs_snap;
  send_msg(srv, s->nodeno, node, get_group_id(s), p);
  unref_msg(&p);
}

/* purecov: begin deadcode */
static void send_snapshot(site_def const *s, gcs_snapshot *gcs_snap,
                          node_no node) {
  assert(s->servers[node]);
  server_send_snapshot(s->servers[node], s, gcs_snap, node);
}
/* purecov: end */

static void server_push_log(server *srv, synode_no push, node_no node) {
  site_def const *s = get_site_def();
  if (srv && s) {
    while (!synode_gt(push, get_max_synode())) {
      if (is_cached(push)) {
        /* Need to clone message here since pax_machine may be re-used while
         * message is sent */
        pax_machine *p = get_cache_no_touch(push);
        if (pm_finished(p)) {
          pax_msg *pm = clone_pax_msg(p->learner.msg);
          ref_msg(pm);
          pm->op = recover_learn_op;
          DBGOUT(FN; PTREXP(srv); PTREXP(s););
          send_msg(srv, s->nodeno, node, get_group_id(s), pm);
          unref_msg(&pm);
        }
      }
      push = incr_synode(push);
    }
  }
}

/* purecov: begin deadcode */
static void push_log(synode_no push, node_no node) {
  site_def const *s = get_site_def();
  if (s) {
    assert(s->servers[node]);
    server_push_log(s->servers[node], push, node);
  }
}
/* purecov: end */

static app_snap_getter get_app_snap;
static app_snap_handler handle_app_snap;

/* purecov: begin deadcode */
static void handle_need_snapshot(site_def const *s, node_no node) {
  gcs_snapshot *gs = export_config();
  synode_no app_lsn = get_app_snap(&gs->app_snap);
  if (!synode_eq(null_synode, app_lsn) && synode_lt(app_lsn, gs->log_start))
    gs->log_start = app_lsn;
  send_snapshot(s, gs, node);
  push_log(gs->log_start, node);
}
/* purecov: end */

static task_env *x_timer = NULL;

/* Timer for use with the xcom FSM. Will deliver xa_timeout */
static int xcom_timer(task_arg arg) {
  DECL_ENV
  double t;
  END_ENV;

  TASK_BEGIN

  ep->t = get_double_arg(arg);
  TASK_DELAY(ep->t);
  XCOM_FSM(xa_timeout, double_arg(ep->t));
  FINALLY
  if (stack == x_timer) set_task(&x_timer, NULL);
  TASK_END;
}

/* Stop the xcom FSM timer */
static void stop_x_timer() {
  if (x_timer) {
    task_terminate(x_timer);
    set_task(&x_timer, NULL);
  }
}

/* Start the xcom FSM timer */
static void start_x_timer(double t) {
  stop_x_timer();
  set_task(&x_timer, task_new(xcom_timer, double_arg(t), "xcom_timer",
                              XCOM_THREAD_DEBUG));
}

static void server_handle_need_snapshot(server *srv, site_def const *s,
                                        node_no node) {
  gcs_snapshot *gs = export_config();
  synode_no app_lsn = get_app_snap(&gs->app_snap);
  if (!synode_eq(null_synode, app_lsn) && synode_lt(app_lsn, gs->log_start)) {
    gs->log_start = app_lsn;
  } else if (!synode_eq(null_synode, last_config_modification_id)) {
    gs->log_start = last_config_modification_id;
  }

  server_send_snapshot(srv, s, gs, node);
  server_push_log(srv, gs->log_start, node);
}

#define X(b) #b,
const char *xcom_state_name[] = {x_state_list};

const char *xcom_actions_name[] = {x_actions};
#undef X

static int snapshots[NSERVERS];

/* Note that we have received snapshot from node */
static void note_snapshot(task_arg fsmargs) {
  pax_msg *p = get_void_arg(fsmargs);
  if (p->from != VOID_NODE_NO) {
    snapshots[p->from] = 1;
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

/* See if this snapshot is better than what we already have */
static int better_snapshot(task_arg fsmargs) {
  pax_msg *p = get_void_arg(fsmargs);
  synode_no boot_key = config_max_boot_key(p->gcs_snap);
  return synode_gt(boot_key, get_site_def()->boot_key);
}

/* Install snapshot */
static void handle_x_snapshot(task_arg fsmargs) {
  pax_msg *p = get_void_arg(fsmargs);
  import_config(p->gcs_snap);
  handle_app_snap(&p->gcs_snap->app_snap);
  set_executed_msg(p->gcs_snap->log_start);

  set_last_received_config(p->gcs_snap->log_start);

  DBGOUT(FN; SYCEXP(executed_msg););
}

/* Note that we have received snapshot, and install if better than old */
static void update_best_snapshot(task_arg fsmargs) {
  if (get_site_def() == 0 || better_snapshot(fsmargs)) {
    handle_x_snapshot(fsmargs);
  }
  note_snapshot(fsmargs);
}

/* Send need_boot_op to all nodes in current config */
static void send_need_boot() {
  pax_msg *p = pax_msg_new_0(null_synode);
  ref_msg(p);
  p->synode = get_site_def()->start;
  p->op = need_boot_op;
  send_to_all(p, "need_boot_op");
  unref_msg(&p);
}

xcom_state xcom_fsm(xcom_actions action, task_arg fsmargs) {
  static int state = 0;
  G_DEBUG("state %d action %s", state, xcom_actions_name[action]);
  switch (state) {
    default:
      assert(state == 0);
    case 0:
      /* Initialize basic xcom data */
      xcom_thread_init();
    start:
      reset_snapshot_mask();
      for (;;) {
        if (action == xa_init) {
          xcom_shutdown = 0;
          sent_alive = 0.0;
        }
        if (action == xa_u_boot) {
          /* purecov: begin deadcode */
          node_list *nl = get_void_arg(fsmargs);
          app_data_ptr a = create_config(nl, unified_boot_type);
          install_node_group(
              a); /* Cannot get consensus unless group is known */
          send_app_data(a);
          set_executed_msg(incr_msgno(get_site_def()->start));
          goto run;
          /* purecov: end */
        }
        if (action == xa_add) {
          /* purecov: begin deadcode */
          add_args *a = get_void_arg(fsmargs);
          send_client_add_node(a->addr, a->port, a->nl);
          /* purecov: end */
        }
        if (action == xa_net_boot) {
          app_data *a = get_void_arg(fsmargs);
          install_node_group(a);
          set_executed_msg(incr_msgno(get_site_def()->start));
          goto run;
        }
        if (action == xa_snapshot) {
          handle_x_snapshot(fsmargs);
          goto recover;
        }
        if (action == xa_snapshot_wait) {
          start_x_timer(SNAPSHOT_WAIT_TIME);
          goto snapshot_wait;
        }
        if (action == xa_exit) {
          /* Xcom is finished when we get here */
          bury_site(get_group_id(get_site_def()));
          task_terminate_all(); /* Kill, kill, kill, kill, kill, kill. This is
                                   the end. */

          init_xcom_base(); /* Reset shared variables */
          init_tasks();     /* Reset task variables */
          free_site_defs();
          free_forced_config_site_def();
          wait_forced_config = 0;
          garbage_collect_servers();
          DBGOUT(FN; STRLIT("shutting down"));
          xcom_shutdown = 1;
          if (xcom_exit_cb) xcom_exit_cb(get_int_arg(fsmargs));
          G_DEBUG("Exiting xcom thread");
        }
        CO_RETURN(x_start);
      }
    snapshot_wait:
      for (;;) {
        CO_RETURN(x_snapshot_wait);
        if (action == xa_snapshot) {
          update_best_snapshot(fsmargs);
          send_need_boot();
          goto recover_wait;
        } else if (action == xa_timeout) { /* Should not time out here */
          goto start;
        }
      }
    recover_wait:
      for (;;) {
        if (got_all_snapshots()) {
          goto recover;
        }
        CO_RETURN(x_recover_wait);
        if (action == xa_snapshot) {
          update_best_snapshot(fsmargs);
        } else if (action == xa_timeout) {
          goto recover;
        }
      }
    recover:
      stop_x_timer();
      for (;;) {
        if (action == xa_terminate) {
          goto start;
        }
        if (action == xa_complete) {
          goto run;
        }

        CO_RETURN(x_recover);
      }
    run:
      DBGOUT(FN; SYCEXP(executed_msg););
      if (xcom_run_cb) xcom_run_cb(0);
      force_recover = 0;
      client_boot_done = 1;
      netboot_ok = 1;
      booting = 0;
      set_proposer_startpoint();
      create_proposers();
      set_task(&executor, task_new(executor_task, null_arg, "executor_task",
                                   XCOM_THREAD_DEBUG));
      set_task(&sweeper, task_new(sweeper_task, null_arg, "sweeper_task",
                                  XCOM_THREAD_DEBUG));
      set_task(&detector, task_new(detector_task, null_arg, "detector_task",
                                   XCOM_THREAD_DEBUG));
      set_task(&alive_t,
               task_new(alive_task, null_arg, "alive_task", XCOM_THREAD_DEBUG));

      for (;;) {
        if (action == xa_terminate) {
          force_recover = 0;
          client_boot_done = 0;
          netboot_ok = 0;
          booting = 0;
          terminate_proposers();
          init_proposers();
          task_terminate(executor);
          set_task(&executor, NULL);
          task_terminate(sweeper);
          set_task(&sweeper, NULL);
          task_terminate(detector);
          set_task(&detector, NULL);
          task_terminate(alive_t);
          set_task(&alive_t, NULL);

          init_xcom_base(); /* Reset shared variables */
          free_site_defs();
          free_forced_config_site_def();
          wait_forced_config = 0;
          garbage_collect_servers();
          if (xcom_terminate_cb) xcom_terminate_cb(get_int_arg(fsmargs));
          goto start;
        }
        if (action == xa_need_snapshot) {
          pax_msg *p = get_void_arg(fsmargs);
          handle_need_snapshot(find_site_def(p->synode), p->from);
        }
        if (action == xa_force_config) {
          app_data *a = get_void_arg(fsmargs);
          site_def *s = create_site_def_with_start(a, executed_msg);

          s->boot_key = executed_msg;
          invalidate_servers(get_site_def(), s);
          start_force_config(s, 1);
          wait_forced_config =
              1; /* Note that forced config has not yet arrived */
        }
        CO_RETURN(x_run);
      }
  }
}

/* purecov: begin deadcode */
void xcom_add_node(char *addr, xcom_port port, node_list *nl) {
  if (xcom_mynode_match(addr, port)) {
    XCOM_FSM(xa_u_boot, void_arg(nl)); /* Boot */
  } else {
    add_args a;
    a.addr = addr;
    a.port = port;
    a.nl = nl;
    XCOM_FSM(xa_add, void_arg(&a)); /* Only initialize xcom */
  }
}

void xcom_fsm_add_node(char *addr, node_list *nl) {
  xcom_port node_port = xcom_get_port(addr);
  char *node_addr = xcom_get_name(addr);

  if (xcom_mynode_match(node_addr, node_port)) {
    node_list x_nl;
    x_nl.node_list_len = 1;
    x_nl.node_list_val = new_node_address(x_nl.node_list_len, &addr);
    XCOM_FSM(xa_u_boot, void_arg(&x_nl));
    delete_node_address(x_nl.node_list_len, x_nl.node_list_val);
  } else {
    add_args a;
    a.addr = node_addr;
    a.port = node_port;
    a.nl = nl;
    XCOM_FSM(xa_add, void_arg(&a));
  }
  free(node_addr);
}
/* purecov: end */

void set_app_snap_handler(app_snap_handler x) { handle_app_snap = x; }

void set_app_snap_getter(app_snap_getter x) { get_app_snap = x; }

/* Initialize sockaddr based on server and port */
static int init_sockaddr(char *server, struct sockaddr_in *sock_addr,
                         socklen_t *sock_size, xcom_port port) {
  /* Get address of server */
  struct addrinfo *addr = 0;

  checked_getaddrinfo(server, 0, 0, &addr);

  if (addr == 0) {
    return 0;
  }

  /* Copy first address */
  memcpy(sock_addr, addr->ai_addr, addr->ai_addrlen);
  *sock_size = (socklen_t)addr->ai_addrlen;
  sock_addr->sin_port = htons(port);
  freeaddrinfo(addr);

  return 1;
}

static result checked_create_socket(int domain, int type, int protocol) {
  result retval = {0, 0};
  int retry = 1000;

  do {
    SET_OS_ERR(0);
    retval.val = (int)socket(domain, type, protocol);
    retval.funerr = to_errno(GET_OS_ERR);
  } while (--retry && retval.val == -1 &&
           (from_errno(retval.funerr) == SOCK_EAGAIN));

  if (retval.val == -1) {
    task_dump_err(retval.funerr);
#if defined(_WIN32)
    G_MESSAGE("Socket creation failed with error %d.", retval.funerr);
#else
    G_MESSAGE("Socket creation failed with error %d - %s.", retval.funerr,
              strerror(retval.funerr));
#endif
    abort();
  }
  return retval;
}

/* Read max n bytes from socket fd into buffer buf */
static result socket_read(connection_descriptor *rfd, void *buf, int n) {
  result ret = {0, 0};

  assert(n >= 0);

  do {
    ret = con_read(rfd, buf, n);
    task_dump_err(ret.funerr);
  } while (ret.val < 0 && can_retry_read(ret.funerr));
  assert(!can_retry_read(ret.funerr));
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
static int64_t socket_write(connection_descriptor *wfd, void *_buf,
                            uint32_t n) {
  char *buf = (char *)_buf;
  result ret = {0, 0};

  uint32_t total; /* Keeps track of number of bytes written so far */

  total = 0;
  while (total < n) {
    int w = (int)MIN(n - total, INT_MAX);

    while ((ret = con_write(wfd, buf + total, w)).val < 0 &&
           can_retry_write(ret.funerr)) {
      task_dump_err(ret.funerr);
      DBGOUT(FN; STRLIT("retry "); NEXP(total, d); NEXP(n, d));
    }
    if (ret.val <= 0) { /* Something went wrong */
      task_dump_err(ret.funerr);
      return -1;
    } else {
      total += (uint32_t)ret.val; /* Add number of bytes written to total */
    }
  }
  DBGOUT(FN; NEXP(total, u); NEXP(n, u));
  assert(total == n);
  return (total);
}

static inline result xcom_close_socket(int *sock) {
  result res = {0, 0};
  if (*sock != -1) {
    do {
      SET_OS_ERR(0);
      res.val = CLOSESOCKET(*sock);
      res.funerr = to_errno(GET_OS_ERR);
    } while (res.val == -1 && from_errno(res.funerr) == SOCK_EINTR);
    *sock = -1;
  }
  return res;
}

static inline result xcom_shut_close_socket(int *sock) {
  result res = {0, 0};
  if (*sock >= 0) {
    shutdown_socket(sock);
    res = xcom_close_socket(sock);
  }
  return res;
}

#define CONNECT_FAIL \
  ret_fd = -1;       \
  goto end

static int timed_connect(int fd, sockaddr *sock_addr, socklen_t sock_size) {
  int timeout = 10000;
  int ret_fd = fd;
  int syserr;
  int sysret;
  struct pollfd fds;

  fds.fd = fd;
  fds.events = POLLOUT;
  fds.revents = 0;

  /* Set non-blocking */
  if (unblock_fd(fd) < 0) return -1;

  /* Trying to connect with timeout */
  SET_OS_ERR(0);
  sysret = connect(fd, sock_addr, sock_size);

  if (is_socket_error(sysret)) {
    syserr = GET_OS_ERR;
    /* If the error is SOCK_EWOULDBLOCK or SOCK_EINPROGRESS or SOCK_EALREADY,
     * wait. */
    switch (syserr) {
      case SOCK_EWOULDBLOCK:
      case SOCK_EINPROGRESS:
      case SOCK_EALREADY:
        break;
      default:
        G_DEBUG(
            "connect - Error connecting "
            "(socket=%d, error=%d).",
            fd, GET_OS_ERR);
        CONNECT_FAIL;
    }

    SET_OS_ERR(0);
    while ((sysret = poll(&fds, 1, timeout)) < 0) {
      syserr = GET_OS_ERR;
      if (syserr != SOCK_EINTR && syserr != SOCK_EINPROGRESS) break;
      SET_OS_ERR(0);
    }
    MAY_DBG(FN; STRLIT("poll - Finished. "); NEXP(sysret, d));

    if (sysret == 0) {
      G_DEBUG(
          "Timed out while waiting for connection to be established! "
          "Cancelling connection attempt. (socket= %d, error=%d)",
          fd, sysret);
      /* G_WARNING("poll - Timeout! Cancelling connection..."); */
      CONNECT_FAIL;
    }

    if (is_socket_error(sysret)) {
      G_DEBUG(
          "poll - Error while connecting! "
          "(socket= %d, error=%d)",
          fd, GET_OS_ERR);
      CONNECT_FAIL;
    }

    {
      int socket_errno = 0;
      socklen_t socket_errno_len = sizeof(socket_errno);

      if ((fds.revents & POLLOUT) == 0) {
        MAY_DBG(FN; STRLIT("POLLOUT not set - Socket failure!"););
        ret_fd = -1;
      }

      if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        MAY_DBG(FN;
                STRLIT("POLLERR | POLLHUP | POLLNVAL set - Socket failure!"););
        ret_fd = -1;
      }
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_errno,
                     &socket_errno_len) != 0) {
        G_DEBUG("getsockopt socket %d failed.", fd);
        ret_fd = -1;
      } else {
        if (socket_errno != 0) {
          G_DEBUG("Connection to socket %d failed with error %d.", fd,
                  socket_errno);
          ret_fd = -1;
        }
      }
    }
  }

end:
  /* Set blocking */
  SET_OS_ERR(0);
  if (block_fd(fd) < 0) {
    G_DEBUG(
        "Unable to set socket back to blocking state. "
        "(socket=%d, error=%d).",
        fd, GET_OS_ERR);
    return -1;
  }
  return ret_fd;
}

/* Connect to server on given port */
static connection_descriptor *connect_xcom(char *server, xcom_port port) {
  result fd = {0, 0};
  result ret = {0, 0};
  struct sockaddr_in sock_addr;
  socklen_t sock_size;
  char buf[SYS_STRERROR_SIZE];

  DBGOUT(FN; STREXP(server); NEXP(port, d));
  G_DEBUG("connecting to %s %d", server, port);
  /* Create socket */
  if ((fd = checked_create_socket(AF_INET, SOCK_STREAM, 0)).val < 0) {
    G_DEBUG("Error creating sockets.");
    return NULL;
  }

  /* Get address of server */
  if (!init_sockaddr(server, &sock_addr, &sock_size, port)) {
    xcom_close_socket(&fd.val);
    G_DEBUG("Error initializing socket addresses.");
    return NULL;
  }

  /* Connect socket to address */

  SET_OS_ERR(0);
  if (timed_connect(fd.val, (struct sockaddr *)&sock_addr, sock_size) == -1) {
    fd.funerr = to_errno(GET_OS_ERR);
    G_DEBUG(
        "Connecting socket to address %s in port %d failed with error %d - %s.",
        server, port, fd.funerr, strerr_msg(buf, sizeof(buf), fd.funerr));
    xcom_close_socket(&fd.val);
    return NULL;
  }
  {
    int peer = 0;
    /* Sanity check before return */
    SET_OS_ERR(0);
    ret.val = peer =
        getpeername(fd.val, (struct sockaddr *)&sock_addr, &sock_size);
    ret.funerr = to_errno(GET_OS_ERR);
    if (peer >= 0) {
      ret = set_nodelay(fd.val);
      if (ret.val < 0) {
        task_dump_err(ret.funerr);
        xcom_shut_close_socket(&fd.val);
#if defined(_WIN32)
        G_DEBUG(
            "Setting node delay failed  while connecting to %s with error %d.",
            server, ret.funerr);
#else
        G_DEBUG(
            "Setting node delay failed  while connecting to %s with error %d - "
            "%s.",
            server, ret.funerr, strerror(ret.funerr));
#endif
        return NULL;
      }
      G_DEBUG("client connected to %s %d fd %d", server, port, fd.val);
    } else {
      /* Something is wrong */
      socklen_t errlen = sizeof(ret.funerr);
      DBGOUT(FN; STRLIT("getpeername failed"););
      if (ret.funerr) {
        DBGOUT(FN; NEXP(from_errno(ret.funerr), d);
               STRLIT(strerror(from_errno(ret.funerr))));
      }
      getsockopt(fd.val, SOL_SOCKET, SO_ERROR, (void *)&ret.funerr, &errlen);
      if (ret.funerr == 0) {
        ret.funerr = to_errno(SOCK_ECONNREFUSED);
      }
      xcom_shut_close_socket(&fd.val);
#if defined(_WIN32)
      G_DEBUG(
          "Getting the peer name failed while connecting to server %s with "
          "error %d.",
          server, ret.funerr);
#else
      G_DEBUG(
          "Getting the peer name failed while connecting to server %s with "
          "error %d -%s.",
          server, ret.funerr, strerror(ret.funerr));
#endif
      return NULL;
    }

#ifdef XCOM_HAVE_OPENSSL
    if (xcom_use_ssl()) {
      connection_descriptor *cd = 0;
      SSL *ssl = SSL_new(client_ctx);
      G_DEBUG("Trying to connect using SSL.")
      SSL_set_fd(ssl, fd.val);

      ERR_clear_error();
      ret.val = SSL_connect(ssl);
      ret.funerr = to_ssl_err(SSL_get_error(ssl, ret.val));

      if (ret.val != SSL_SUCCESS) {
        G_MESSAGE("Error connecting using SSL %d %d.", ret.funerr,
                  SSL_get_error(ssl, ret.val));
        task_dump_err(ret.funerr);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        xcom_shut_close_socket(&fd.val);
        return NULL;
      }
      DBGOUT(FN; STRLIT("ssl connected to "); STRLIT(server); NDBG(port, d);
             NDBG(fd.val, d); PTREXP(ssl));

      if (ssl_verify_server_cert(ssl, server)) {
        G_MESSAGE("Error validating certificate and peer.");
        task_dump_err(ret.funerr);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        xcom_shut_close_socket(&fd.val);
        return NULL;
      }

      cd = new_connection(fd.val, ssl);
      set_connected(cd, CON_FD);
      G_DEBUG("Success connecting using SSL.")
      return cd;
    } else {
      connection_descriptor *cd = new_connection(fd.val, 0);
      set_connected(cd, CON_FD);
      return cd;
    }
#else
    {
      connection_descriptor *cd = new_connection(fd.val);
      set_connected(cd, CON_FD);
      return cd;
    }
#endif
  }
}

connection_descriptor *xcom_open_client_connection(char *server,
                                                   xcom_port port) {
  return connect_xcom(server, port);
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
    DBGOUT(FN; NDBG(n, d));
    return -1;
  }

  *x_proto = read_protoversion(VERS_PTR(header_buf));
  get_header_1_0(header_buf, &msgsize, x_type, tag);

  return n;
}

enum { TAG_START = 313 };

static int64_t xcom_send_client_app_data(connection_descriptor *fd,
                                         app_data_ptr a, int force) {
  pax_msg *msg = pax_msg_new(null_synode, 0);
  uint32_t buflen = 0;
  char *buf = 0;
  int64_t retval = 0;

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
    DBGOUT(STRLIT("client connection will use protocol version ");
           NDBG(x_proto, u); STRLIT(xcom_proto_to_str(x_proto)));
    fd->x_proto = x_proto;
    set_connected(fd, CON_PROTO);
  }
  msg->a = a;
  msg->to = VOID_NODE_NO;
  msg->op = client_msg;
  msg->force_delivery = force;

  serialize_msg(msg, fd->x_proto, &buflen, &buf);
  if (buflen) {
    retval = socket_write(fd, buf, buflen);
    if (buflen != retval) {
      DBGOUT(FN; STRLIT("write failed "); NDBG(fd->fd, d); NDBG(buflen, d);
             NDBG64(retval));
    }
    X_FREE(buf);
  }
end:
  msg->a = 0; /* Do not deallocate a */
  XCOM_XDR_FREE(xdr_pax_msg, msg);
  return retval;
}

int64_t xcom_client_send_data(uint32_t size, char *data,
                              connection_descriptor *fd) {
  app_data a;
  int64_t retval = 0;
  init_app_data(&a);
  a.body.c_t = app_type;
  a.body.app_u_u.data.data_len = size;
  a.body.app_u_u.data.data_val = data;
  retval = xcom_send_client_app_data(fd, &a, 0);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

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

  bytes = NULL;

  /* Read version, length, type, and tag */
  n = socket_read_bytes(rfd, (char *)header_buf, MSG_HDR_SIZE);

  if (n <= 0) {
    DBGOUT(FN; NDBG64(n));
    return 0;
  }
  assert(n == MSG_HDR_SIZE);
  x_version = get_32(VERS_PTR(header_buf));
/* Check the protocol version before doing anything else */
#ifdef XCOM_PARANOID
  assert(check_protoversion(x_version, rfd->x_proto));
#endif
  if (!check_protoversion(x_version, rfd->x_proto)) {
    return 0;
  }

  /* OK, we can grok this version */

  get_header_1_0(header_buf, &msgsize, &x_type, &tag);

  /* Allocate buffer space for message */
  bytes = calloc(1, msgsize);

  /* Read message */
  n = socket_read_bytes(rfd, bytes, msgsize);

  if (n > 0) {
    /* Deserialize message */
    deserialize_ok = deserialize_msg(p, rfd->x_proto, bytes, msgsize);
    MAY_DBG(FN; STRLIT(" deserialized message"));
  }
  /* Deallocate buffer */
  X_FREE(bytes);
  if (n <= 0 || deserialize_ok == 0) {
    DBGOUT(FN; NDBG64(n));
    return 0;
  }
  return (p);
}

int xcom_close_client_connection(connection_descriptor *connection) {
  int retval = 0;

#ifdef XCOM_HAVE_OPENSSL
  if (connection->ssl_fd) {
    SSL_shutdown(connection->ssl_fd);
    ssl_free_con(connection);
  }
#endif
  retval = xcom_shut_close_socket(&connection->fd).val;
  free(connection);
  return retval;
}

int xcom_client_boot(connection_descriptor *fd, node_list *nl,
                     uint32_t group_id) {
  app_data a;
  int retval = 0;
  retval = (int)xcom_send_client_app_data(
      fd, init_config_with_group(&a, nl, unified_boot_type, group_id), 0);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int xcom_send_app_wait(connection_descriptor *fd, app_data *a, int force) {
  int retval = 0;
  int retry_count = 10;  // Same as 'connection_attempts'
  pax_msg p;
  pax_msg *rp = 0;

  do {
    retval = (int)xcom_send_client_app_data(fd, a, force);
    if (retval < 0) return 0;
    memset(&p, 0, sizeof(p));
    rp = socket_read_msg(fd, &p);
    if (rp) {
      client_reply_code cli_err = rp->cli_err;
      my_xdr_free((xdrproc_t)xdr_pax_msg, (char *)&p);
      switch (cli_err) {
        case REQUEST_OK:
          return 1;
        case REQUEST_FAIL:
          G_DEBUG("cli_err %d", cli_err);
          return 0;
        case REQUEST_RETRY:
          G_DEBUG("cli_err %d", cli_err);
          xcom_sleep(1);
          break;
        default:
          G_WARNING("client protocol botched");
          return 0;
      }
    } else {
      G_WARNING("read failed");
      return 0;
    }
  } while (--retry_count);
  // Timeout after REQUEST_RETRY has been received 'retry_count' times
  G_MESSAGE(
      "Request failed: maximum number of retries (10) has been exhausted.");
  return 0;
}

int xcom_send_cfg_wait(connection_descriptor *fd, node_list *nl,
                       uint32_t group_id, cargo_type ct, int force) {
  app_data a;
  int retval = 0;
  DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(nl)););
  retval = xcom_send_app_wait(fd, init_config_with_group(&a, nl, ct, group_id),
                              force);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                         uint32_t group_id) {
  return xcom_send_cfg_wait(fd, nl, group_id, add_node_type, 0);
}

int xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                            uint32_t group_id) {
  return xcom_send_cfg_wait(fd, nl, group_id, remove_node_type, 0);
}

#ifdef NOTDEF
/* Not completely implemented, need to be handled properly
   when received as a client message in dispatch_op.
   Should have separate opcode from normal add/remove,
   like force config_type */
int xcom_client_force_add_node(connection_descriptor *, node_list *nl,
                               uint32_t group_id) {
  return xcom_send_cfg_wait(fd, nl, group_id, add_node_type, 1);
}

int xcom_client_force_remove_node(connection_descriptor *, node_list *nl,
                                  uint32_t group_id) {
  return xcom_send_cfg_wait(fd, nl, group_id, remove_node_type, 1);
}
#endif

int xcom_client_force_config(connection_descriptor *fd, node_list *nl,
                             uint32_t group_id) {
  return xcom_send_cfg_wait(fd, nl, group_id, force_config_type, 1);
}

int xcom_client_enable_arbitrator(connection_descriptor *fd) {
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = enable_arbitrator;
  retval = xcom_send_app_wait(fd, &a, 0);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int xcom_client_disable_arbitrator(connection_descriptor *fd) {
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = disable_arbitrator;
  retval = xcom_send_app_wait(fd, &a, 0);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int xcom_client_terminate_and_exit(connection_descriptor *fd) {
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = x_terminate_and_exit;
  retval = xcom_send_app_wait(fd, &a, 0);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}

int xcom_client_set_cache_limit(connection_descriptor *fd,
                                uint64_t cache_limit) {
  app_data a;
  int retval = 0;
  init_app_data(&a);
  a.body.c_t = set_cache_limit;
  a.body.app_u_u.cache_limit = cache_limit;
  retval = xcom_send_app_wait(fd, &a, 0);
  my_xdr_free((xdrproc_t)xdr_app_data, (char *)&a);
  return retval;
}
