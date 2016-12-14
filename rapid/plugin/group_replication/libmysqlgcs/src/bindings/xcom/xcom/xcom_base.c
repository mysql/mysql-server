/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA */

/** \file The new version of xcom is a major rewrite to allow
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

    static int boot_task(task_arg);

    The boot task is started whenever xcom has no site definition. It
    listens on the input queue until it detects either a boot or
    recovery. In case of a boot, it will wait for a unified boot
    message.  In case of local recovery, it will wait until has seen
    all recover messages.  In both cases, the proposer task will try
    to get those messages accepted/

    static int boot_killer_task(task_arg);

    Abort the boot process if there is no progress.

    static int net_boot_task(task_arg);
    static int net_recover_task(task_arg);

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

#include "x_platform.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include <limits.h>

#ifndef WIN
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifndef __linux__
#include <sys/sockio.h>
#endif
#endif

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

#include "xdr_utils.h"
#include "xcom_common.h"

#include "task_os.h"

#include "xcom_vp.h"

#include "simset.h"
#include "app_data.h"

#include "task.h"
#include "node_no.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "xcom_transport.h"
#include "xcom_base.h"

#ifdef XCOM_HAVE_OPENSSL
#include "xcom_ssl_transport.h"
#endif

#include "task.h"
#include "task_net.h"
#include "task_debug.h"
#include "xcom_statistics.h"
#include "node_set.h"
#include "node_list.h"
#include "bitset.h"

#include "xcom_cache.h"

#include "xcom_vp_str.h"
#include "pax_msg.h"
#include "xcom_msg_queue.h"
#include "xcom_recover.h"
#include "synode_no.h"
#include "sock_probe.h"
#include "xcom_interface.h"
#include "xcom_memory.h"
#include "site_def.h"

#ifdef XCOM_HAVE_OPENSSL
#include "openssl/ssl.h"
#endif

/* {{{ Defines and constants */

#define TERMINATE_DELAY 3.0
#define EVENT_HORIZON_MIN 10
unsigned int event_horizon = EVENT_HORIZON_MIN;

static void set_event_horizon(unsigned int eh) MY_ATTRIBUTE((unused));
/* purecov: begin deadcode */
static void set_event_horizon(unsigned int eh)
{
	DBGOUT(FN; NDBG(eh,u));
	event_horizon = eh;
}
/* purecov: end */

/* The number of proposers on one node */
#define PROPOSERS 10

/* Limit the number of acceptors */
/* #define MAXACCEPT 5  */

/* Skip prepare for first ballot */
int const threephase = 0;

/* Error injection for testing */
#define INJECT_ERROR 0

/* Crash a node early */
/* #define CRASH 1 */

/* }}} */


#include "retry.h"

/* #define USE_EXIT_TYPE */
/* #define NO_SWEEPER_TASK */

/* Limit batch size to sensible ? amount */
enum{
	MAX_BATCH_SIZE = 0x3fffffff
};

int ARBITRATOR_HACK = 0;
static int AUTOBATCH = 1;
#define AGGRESSIVE_SWEEP

static int const no_duplicate_payload = 1;

/* Use buffered read when reading messages from the network */
static int use_buffered_read = 1;

/* {{{ Forward declarations */

long	get_unique_long(void);
unsigned long	msg_count(app_data_ptr a);
void	get_host_name(char *a, char name[MAXHOSTNAMELEN+1]);

static double	wakeup_delay(double old);

/* Task types */
static int	proposer_task(task_arg arg);
static int	executor_task(task_arg arg);
static int	sweeper_task(task_arg arg);
extern int	alive_task(task_arg arg);
static int	generator_task(task_arg arg);
extern int	detector_task(task_arg arg);

static int	finished(pax_machine *p);
static int	accepted(pax_machine *p);
static int	started(pax_machine *p);
static synode_no first_free_synode(synode_no msgno);
static void free_forced_config_site_def();

extern void	bit_set_or(bit_set *x, bit_set const *y);

/* }}} */

/* {{{ Global variables */

int	xcom_shutdown = 0; /* Xcom_Shutdown flag */
synode_no executed_msg;    /* The message we are waiting to execute */
synode_no max_synode;      /* Max message number seen so far */
task_env *boot = NULL;
task_env *detector = NULL;
task_env *killer = NULL;
task_env *net_boot = NULL;
task_env *net_recover = NULL;
void	*xcom_thread_input = 0;

static void	init_proposers();

void	init_base_vars()
{
	xcom_shutdown = 0; /* Xcom_Shutdown flag */
	executed_msg = null_synode; /* The message we are waiting to execute */
	max_synode = null_synode; /* Max message number seen so far */
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

static uint32_t	my_id = 0; /* Unique id of this instance */
static synode_no current_message; /* Current message number */
static synode_no last_config_modification_id; /*Last configuration change proposal*/

synode_no get_current_message()
{
	return current_message;
}

static channel prop_input_queue;         /* Proposer task input queue */

/* purecov: begin deadcode */
channel *get_prop_input_queue()
{
	return & prop_input_queue;
}
/* purecov: end */

extern int	client_boot_done;
extern int	netboot_ok;
extern int	booting;

extern start_t start_type;
static linkage exec_wait = {0,&exec_wait, &exec_wait};           /* Executor will wake up tasks sleeping here */

/*
#define IGNORE_LOSERS
*/

#define BUILD_TIMEOUT 3.0

#define MAX_DEAD 10
static struct {
	int	n;
	unsigned long	id[MAX_DEAD];
} dead_sites;

synode_no get_max_synode()
{
	return max_synode;
}

static
void synode_set_to_event_horizon(synode_no *s)
{
  s->msgno += event_horizon + 1;
  s->node= 0;
}


/**
   Set node group
*/
void	set_group(uint32_t id)
{
	MAY_DBG(FN; STRLIT("changing group id of global variables "); NDBG(id,lx););
/*	set_group_id(id); */
	current_message.group_id = id;
	executed_msg.group_id = id;
	max_synode.group_id = id;
	set_log_group_id(id);
}


static void	bury_site(uint32_t id)
{
	if (id != 0) {
		dead_sites.id[dead_sites.n % MAX_DEAD] = id;
		dead_sites.n = (dead_sites.n + 1) % MAX_DEAD;
	}
}


static bool_t is_dead_site(uint32_t id)
{
	int	i = 0;
	for (i = 0; i < MAX_DEAD; i++) {
		if (dead_sites.id[i] == id)
			return TRUE;
		else if (dead_sites.id[i] == 0)
			return FALSE;
	}
	return FALSE;
}

d_xdr_funcs(node_no)
define_xdr_funcs(node_no)

extern void	init_recovery_sema();
extern void	end_xcom_recovery();
extern void	end_recovery();
extern void	send_instance_info();
extern void	send_end_recover();

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
static synode_no incr_msgno(synode_no msgno)
{
	synode_no ret = msgno;
	ret.msgno++;
	ret.node = get_nodeno(find_site_def(ret)); /* In case site and node number has changed */
	return ret;
}

#if 0
/* Given message number, compute which node it belongs to */
static unsigned int	msgno_to_node(synode_no msgno)
{
	return msgno.node;
}
#endif

synode_no incr_synode(synode_no synode)
{
	synode_no ret = synode;
	ret.node++;
	if (ret.node >= get_maxnodes(find_site_def(synode))) {
		ret.node = 0;
		ret.msgno++;
	}
/* 	DBGOUT(FN; SYCEXP(synode); SYCEXP(ret)); */
	return ret; /* Change this if we change message number type */
}


synode_no decr_synode(synode_no synode)
{
	synode_no ret = synode;
	if (ret.node == 0) {
		ret.msgno--;
		ret.node = get_maxnodes(find_site_def(ret));
	}
	ret.node--;
	return ret; /* Change this if we change message number type */
}


static void	skip_value(pax_msg *p)
{
	MAY_DBG(FN; SYCEXP(p->synode));
	p->op = learn_op;
	p->msg_type = no_op;
}


/* }}} */

/* {{{ Utilities and debug */

/* purecov: begin deadcode */
/* Print message and exit */
static void	pexitall(int i)
{
	int	*r = (int*)calloc(1, sizeof(int));
	*r = i;
	DBGOUT(FN; NDBG(i, d); STRLIT("time "); NDBG(task_now(), f); );
	XCOM_FSM(xa_terminate, int_arg(i));	/* Tell xcom to stop */
}
/* purecov: end */

#ifndef WIN
/* Ignore this signal */
static int	ignoresig(int signum)
{
	struct sigaction act;
	struct sigaction oldact;

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	memset(&oldact, 0, sizeof(oldact));

	return sigaction(signum,
	    &act,
	    &oldact);
}
#else
#define SIGPIPE 0
static int	ignoresig(int signum)
{
	return 0;
}
#endif

/* }}} */

#if 0
static void	dbg_machine_and_msg(pax_machine *p, pax_msg *pm)
{
  GET_GOUT;
  STRLIT("machine ");
  ADD_GOUT(dbg_pax_machine(p));
  STRLIT(" ");
  STRLIT("msg ");
  COPY_AND_FREE_GOUT(dbg_pax_msg(pm));
  PRINT_GOUT;
  FREE_GOUT;
}


#endif

static int	recently_active(pax_machine *p)
{
	MAY_DBG(FN;
			SYCEXP(p->synode); STRLIT(" op ");
            PTREXP(p); STRLIT(p->learner.msg ? pax_op_to_str(p->learner.msg->op) : "NULL");
	    NDBG(p->last_modified, f); NDBG(task_now(), f));
	return p->last_modified != 0.0 && (p->last_modified + 0.5 + median_time()) > task_now();
}


static inline int	finished(pax_machine *p)
{
	MAY_DBG(FN;
	    SYCEXP(p->synode); STRLIT(" op ");
	    PTREXP(p); STRLIT(p->learner.msg ? pax_op_to_str(p->learner.msg->op) : "NULL");
	    );
	return p->learner.msg && (p->learner.msg->op == learn_op || p->learner.msg->op == tiny_learn_op);
}


int	pm_finished(pax_machine *p)
{
	return finished(p);
}


static inline int	accepted(pax_machine *p)
{
	MAY_DBG(FN;
	    SYCEXP(p->synode); STRLIT(" op ");
	    PTREXP(p); STRLIT(p->acceptor.msg ? pax_op_to_str(p->acceptor.msg->op) : "NULL");
	    );
	return p->acceptor.msg && p->acceptor.msg->op != initial_op;
}


static inline int	accepted_noop(pax_machine *p)
{
	MAY_DBG(FN;
	    SYCEXP(p->synode); STRLIT(" op ");
	    PTREXP(p); STRLIT(p->acceptor.msg ? pax_op_to_str(p->acceptor.msg->op) : "NULL");
	    );
	return accepted(p) && p->acceptor.msg->msg_type == no_op;
}


static inline int	noop_match(pax_machine *p, pax_msg *pm)
{
	return pm->msg_type == no_op && accepted_noop(p);
}


static inline int	started(pax_machine *p)
{
	return
	    p->op != initial_op ||
	    (p->acceptor.promise.cnt > 0) ||
	    (p->proposer.msg && (p->proposer.msg->op != initial_op)) ||
	    accepted(p) ||
	    finished(p);
}


/* }}} */

void set_last_received_config(synode_no received_config_change)
{
  last_config_modification_id= received_config_change;
}

/* {{{ Definition of majority */
static inline node_no max_check(site_def const *site)
{
#ifdef MAXACCEPT
	return MIN(get_maxnodes(site), MAXACCEPT);
#else
	return get_maxnodes(site);
#endif
}

static site_def * forced_config = 0;

/* Definition of majority */
static inline int	majority(bit_set const *nodeset, site_def const *s, int all, int delay MY_ATTRIBUTE((unused)), int force)
{
	node_no ok = 0;
	node_no i = 0;
	int	retval = 0;
	node_no max = max_check(s);

 	/* DBGOUT(FN; NDBG(max,lu); NDBG(all,d); NDBG(delay,d); NDBG(force,d)); */

	/* Count nodes that has answered */
	for (i = 0; i < max; i++) {
		if (BIT_ISSET(i, nodeset)) {
			ok++;
		}
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

	if(force){
		DBGOUT(FN; STRLIT("force majority"); NDBG(ok ,u);  NDBG(max ,u); NDBG(get_maxnodes(forced_config),u));
		return ok == get_maxnodes(forced_config);
	}else{
		/* Have now seen answer from all live nodes */
		retval = all ? ok == max : ok > max / 2
			|| (ARBITRATOR_HACK && (2 == max));
		/* 	DBGOUT(FN; NDBG(max,lu); NDBG(all,d); NDBG(delay,d); NDBG(retval,d)); */
		return retval;
	}
}


#define IS_CONS_ALL(p) ((p)->proposer.msg->a ? (p)->proposer.msg->a->consensus == cons_all : 0)

/* See if a majority of acceptors have answered our prepare */
static int	prep_majority(site_def const * site, pax_machine *p)
{

	int	ok = 0;

	assert(p);
	assert(p->proposer.prep_nodeset);
	assert(p->proposer.msg);
	/* DBGOUT(FN; BALCEXP(p->proposer.bal)); */
	ok = majority(p->proposer.prep_nodeset, site, IS_CONS_ALL(p), p->proposer.bal.cnt == 1, p->proposer.msg->force_delivery || p->force_delivery);
	return ok;
}

/* See if a majority of acceptors have answered our propose */
static int	prop_majority(site_def const * site, pax_machine *p)
{
	int	ok = 0;

	assert(p);
	assert(p->proposer.prop_nodeset);
	assert(p->proposer.msg);
	/* DBGOUT(FN; BALCEXP(p->proposer.bal)); */
	ok = majority(p->proposer.prop_nodeset, site, IS_CONS_ALL(p), p->proposer.bal.cnt == 1, p->proposer.msg->force_delivery || p->force_delivery);
	return ok;
}

/* }}} */

/* {{{ Xcom thread */

/* purecov: begin deadcode */
/* Xcom thread start function */
gpointer xcom_thread_main(gpointer cp)
{
	G_MESSAGE("Starting xcom on port %d", atoi((char *)cp));
	xcom_thread_init();
	/* Initialize task system and enter main loop */
	taskmain((xcom_port)atoi((char *)cp));
	/* Xcom is finished when we get here */
	DBGOUT(FN; STRLIT("Deconstructing xcom thread"));
	xcom_thread_deinit();
	G_MESSAGE("Exiting xcom thread");
	return NULL;
}
/* purecov: end */
static site_def const * executor_site = 0;

site_def const * get_executor_site()
{
	return executor_site;
}

static site_def *proposer_site = 0;

site_def const *get_proposer_site()
{
	return proposer_site;
}


void	init_xcom_base()
{
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
}

static void	init_tasks()
{
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
void	xcom_thread_init()
{
#ifndef NO_SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
	init_base_vars();
	init_site_vars();
	init_crc32c();
	my_srand48((long int)task_now());

	init_xcom_base();
	init_tasks();
	init_cache();

	/* Initialize input queue */
	channel_init(&prop_input_queue, type_hash("msg_link"));
	init_link_list();
	task_sys_init();
}


/* Empty the proposer input queue */
static void	empty_prop_input_queue()
{
	empty_msg_channel(&prop_input_queue);
	MAY_DBG(FN; STRLIT("prop_input_queue empty"));
}


/* De-initialize the xcom thread */
void	xcom_thread_deinit()
{
	DBGOUT(FN; STRLIT("Empty proposer input queue"));
	empty_prop_input_queue();
	DBGOUT(FN; STRLIT("Empty link free list"));
	empty_link_free_list();
	DBGOUT(FN; STRLIT("De-initialize cache"));
	deinit_cache();
	garbage_collect_servers();
}

#define PROP_ITER int i;  for(i = 0; i < PROPOSERS; i++)

static bool_t force_recover = FALSE;
/* purecov: begin deadcode */
bool_t must_force_recover()
{
	return force_recover;
}


void	set_force_recover(bool_t const x)
{
	force_recover = x;
}
/* purecov: end */

static void	init_proposers()
{
	PROP_ITER {
		set_task(&proposer[i], NULL);
	}
}


static void	create_proposers()
{
	PROP_ITER {
		set_task(&proposer[i], task_new(proposer_task, int_arg(i), "proposer_task", XCOM_THREAD_DEBUG));
	}
}


static void	terminate_proposers()
{
	PROP_ITER {
		task_terminate(proposer[i]);
	}
}

static void free_forced_config_site_def()
{
  free_site_def(forced_config);
  forced_config= NULL;
}

#if TASK_DBUG_ON
static void	dbg_proposers() MY_ATTRIBUTE((unused));
static void	dbg_proposers()
{
	GET_GOUT;
	NDBG(PROPOSERS, d);
	{
		PROP_ITER {
			PPUT(proposer[i]);
		}
	}
	PRINT_GOUT;
	FREE_GOUT;
}
#endif

static void	set_proposer_startpoint()
{
	DBGOHK(FN; STRLIT("changing current message"));
	if (max_synode.msgno <= 1)
		set_current_message(first_free_synode(max_synode));
	else
		set_current_message(incr_msgno(first_free_synode(max_synode)));
}



void	check_tasks()
{
}


/* }}} */

/* {{{ Task functions */
/* purecov: begin deadcode */
/* Match any port */
static int yes(xcom_port port MY_ATTRIBUTE((unused)))
{
  return 1;
}

/* Create tasks and enter the task main loop */
int	taskmain(xcom_port listen_port)
{
	init_xcom_transport(listen_port);
	set_port_matcher(yes); /* For clients that use only addr, not addr:port  */

	MAY_DBG(FN; STRLIT("enter taskmain"));
	ignoresig(SIGPIPE);

	{
		result	fd = {0,0};

		if ((fd = announce_tcp(listen_port)).val < 0) {
			MAY_DBG(FN; STRLIT("cannot annonunce tcp "); NDBG(listen_port, d));
			task_dump_err(fd.funerr);
			g_critical("Unable to announce tcp port %d. Port already in use?", listen_port);
		}

		MAY_DBG(FN; STRLIT("Creating tasks"));
		task_new(generator_task, null_arg, "generator_task", XCOM_THREAD_DEBUG);
		task_new(tcp_server, int_arg(fd.val), "tcp_server", XCOM_THREAD_DEBUG);
		/* task_new(tcp_reaper_task, null_arg, "tcp_reaper_task", XCOM_THREAD_DEBUG); */
		/* task_new(xcom_statistics, null_arg, "xcom_statistics", XCOM_THREAD_DEBUG); */
		/* task_new(detector_task, null_arg, "detector_task", XCOM_THREAD_DEBUG); */
		MAY_DBG(FN; STRLIT("XCOM is listening on "); NPUT(listen_port, d));
	}

	task_loop();

	MAY_DBG(FN; STRLIT(" exit"));
	return 1;
}


void	start_run_tasks()
{
	force_recover = 0;
	client_boot_done = 1;
	netboot_ok = 1;
	booting = 0;
	set_proposer_startpoint();
	create_proposers();
	set_task(&executor, task_new(executor_task, null_arg, "executor_task", XCOM_THREAD_DEBUG));
	set_task(&sweeper, task_new(sweeper_task, null_arg, "sweeper_task", XCOM_THREAD_DEBUG));
	set_task(&detector, task_new(detector_task, null_arg, "detector_task", XCOM_THREAD_DEBUG));
	set_task(&alive_t, task_new(alive_task, null_arg, "alive_task", XCOM_THREAD_DEBUG));
}

/* Create tasks and enter the task main loop */
int	xcom_taskmain(xcom_port listen_port)
{
	init_xcom_transport(listen_port);

	MAY_DBG(FN; STRLIT("enter taskmain"));
	ignoresig(SIGPIPE);

	{
		result fd = {0,0};
		if ((fd = announce_tcp(listen_port)).val < 0) {
			MAY_DBG(FN; STRLIT("cannot annonunce tcp "); NDBG(listen_port, d));
			task_dump_err(fd.funerr);
			g_critical("Unable to announce tcp port %d. Port already in use?", listen_port);
			pexitall(1);
		}

		MAY_DBG(FN; STRLIT("Creating tasks"));
		/* task_new(generator_task, null_arg, "generator_task", XCOM_THREAD_DEBUG); */
		task_new(tcp_server, int_arg(fd.val), "tcp_server", XCOM_THREAD_DEBUG);
		task_new(tcp_reaper_task, null_arg, "tcp_reaper_task", XCOM_THREAD_DEBUG);
		/* task_new(xcom_statistics, null_arg, "xcom_statistics", XCOM_THREAD_DEBUG); */
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

void set_xcom_run_cb(xcom_state_change_cb x)
{
	xcom_run_cb = x;
}

void set_xcom_comms_cb(xcom_state_change_cb x)
{
  xcom_comms_cb = x;
}
/* purecov: begin deadcode */
void set_xcom_terminate_cb(xcom_state_change_cb x)
{
	xcom_terminate_cb = x;
}
/* purecov: end */
void set_xcom_exit_cb(xcom_state_change_cb x)
{
	xcom_exit_cb = x;
}

int	xcom_taskmain2(xcom_port listen_port)
{
  init_xcom_transport(listen_port);

	MAY_DBG(FN; STRLIT("enter taskmain"));
	ignoresig(SIGPIPE);

	 {
		result fd = {0,0};
		if ((fd = announce_tcp(listen_port)).val < 0) {
			MAY_DBG(FN; STRLIT("cannot annonunce tcp "); NDBG(listen_port, d));
			task_dump_err(fd.funerr);
			g_critical("Unable to announce tcp port %d. Port already in use?", listen_port);
			if(xcom_comms_cb){
				xcom_comms_cb(XCOM_COMMS_ERROR);
			}
			if(xcom_terminate_cb){
				xcom_terminate_cb(0);
			}
			return 1;
		}

		if(xcom_comms_cb){
			xcom_comms_cb(XCOM_COMMS_OK);
		}

		MAY_DBG(FN; STRLIT("Creating tasks"));
		/* task_new(generator_task, null_arg, "generator_task", XCOM_THREAD_DEBUG); */
		task_new(tcp_server, int_arg(fd.val), "tcp_server", XCOM_THREAD_DEBUG);
		task_new(tcp_reaper_task, null_arg, "tcp_reaper_task", XCOM_THREAD_DEBUG);
		/* task_new(xcom_statistics, null_arg, "xcom_statistics", XCOM_THREAD_DEBUG); */
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
static void	prepare(pax_msg *p, pax_op op)
{
	p->op = op;
	p->reply_to = p->proposal;
}


/* Initialize a prepare_msg */
static int	prepare_msg(pax_msg *p)
{
	prepare(p, prepare_op);
	/* p->msg_type = normal; */
	return send_to_acceptors(p, "prepare_msg");
}


/* Initialize a noop_msg */
static pax_msg *create_noop(pax_msg *p)
{
	prepare(p, prepare_op);
	p->msg_type = no_op;
	return p;
}


/* Initialize a read_msg */
static pax_msg *create_read(site_def const * site, pax_msg *p)
{
	p->msg_type = normal;
	p->proposal.node = get_nodeno(site);
	prepare(p, read_op);
	return p;
}


static int	skip_msg(pax_msg *p)
{
	prepare(p, skip_op);
	MAY_DBG(FN; STRLIT("skipping message "); SYCEXP(p->synode));
	p->msg_type = no_op;
	return send_to_all(p, "skip_msg");
}


static void	brand_app_data(pax_msg *p)
{
	if (p->a) {
		p->a->app_key.msgno = p->synode.msgno;
		p->a->app_key.node = p->synode.node;
		p->a->app_key.group_id = p->a->group_id = p->synode.group_id;
	}
}


static synode_no my_unique_id(synode_no synode)
{
	assert(my_id != 0);
/* Random number derived from node number and timestamp which uniquely defines this instance */
	synode.group_id = my_id;
	return synode;
}


static void	set_unique_id(pax_msg *msg, synode_no synode )
{
	app_data_ptr a = msg->a;
	while (a) {
		a->unique_id = synode;
		a = a->next;
	}
}


static int	propose_msg(pax_msg *p)
{
	p->op = accept_op;
	p->reply_to = p->proposal;
	brand_app_data(p);
	/* set_unique_id(p, my_unique_id(synode)); */
	return send_to_acceptors(p, "propose_msg");
}


static void	set_learn_type(pax_msg *p)
{
	p->op = learn_op;
	p->msg_type = p->a ? normal : no_op;
}

/* purecov: begin deadcode */
static int	learn_msg(site_def const * site, pax_msg *p)
{
	set_learn_type(p);
	p->reply_to = p->proposal;
	brand_app_data(p);
	MAY_DBG(FN;
	    dbg_bitset(p->receivers, get_maxnodes(site));
	    );
	return send_to_all_site(site, p, "learn_msg");
}
/* purecov: end */
static int	tiny_learn_msg(site_def const *site, pax_msg *p)
{
	int	retval;
	pax_msg * tmp = clone_pax_msg_no_app(p);
	pax_machine * pm = get_cache(p->synode);

	ref_msg(tmp);
	tmp->msg_type = p->a ? normal : no_op;
	tmp->op = tiny_learn_op;
	tmp->reply_to = pm->proposer.bal;
	brand_app_data(tmp);
	MAY_DBG(FN;
	    dbg_bitset(tmp->receivers, get_maxnodes(site));
	    );
	retval = send_to_all_site(site, tmp, "tiny_learn_msg");
	unref_msg(&tmp);
	return retval;
}


/* }}} */

/* {{{ Proposer task */

static void	prepare_push_3p(site_def const * site, pax_machine *p, pax_msg *msg, synode_no msgno)
{
	MAY_DBG(FN;
	    SYCEXP(msgno);
	    NDBG(p->proposer.bal.cnt, d); NDBG(p->acceptor.promise.cnt, d));
	p->proposer.bal.node = get_nodeno(site);
	 {
		int	maxcnt = MAX(p->proposer.bal.cnt, p->acceptor.promise.cnt);
		p->proposer.bal.cnt = ++maxcnt;
	}
	msg->synode = msgno;
	msg->proposal = p->proposer.bal;
}


static void	push_msg_2p(site_def const * site, pax_machine *p)
{
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


static void	push_msg_3p(site_def const * site, pax_machine *p, pax_msg *msg, synode_no msgno, pax_msg_type msg_type)
{
	assert(msgno.msgno != 0);
	prepare_push_3p(site, p, msg, msgno);
	msg->msg_type = msg_type;
	BIT_ZERO(p->proposer.prep_nodeset);
	assert(p->proposer.msg);
	msg->force_delivery = p->force_delivery;
	prepare_msg(msg);
	MAY_DBG(FN; BALCEXP(msg->proposal);
	    SYCEXP(msgno); STRLIT(" op "); STRLIT(pax_op_to_str(msg->op)));
}


/* Brand client message with unique ID */
static void	brand_client_msg(pax_msg *msg, synode_no msgno)
{
	assert(!synode_eq(msgno, null_synode));
	set_unique_id(msg, my_unique_id(msgno));
}

/* purecov: begin deadcode */
static int	reject_send(site_def const * site, recover_action r)
{
	return r != rec_send && xcom_booted() && (!site || !enough_live_nodes(site));
}
/* purecov: end */

void	xcom_send(app_data_ptr a, pax_msg *msg)
{
	MAY_DBG(FN; PTREXP(a); SYCEXP(a->app_key); SYCEXP(msg->synode));
	msg->a = a;
	msg->op = client_msg;
	 {
		msg_link * link = msg_link_new(msg, VOID_NODE_NO);
		MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_pax_msg(msg)));
		channel_put(&prop_input_queue, &link->l);
	}
}

/* purecov: begin deadcode */
static int	generator_task(task_arg arg MY_ATTRIBUTE((unused)))
{
	DECL_ENV
	    int	dummy;
	END_ENV;

	TASK_BEGIN
	    MAY_DBG(FN; );
	check_tasks(); /* Start tasks which should be running */
	for(;;) {
		app_data_ptr a = 0;
		while (a) {
			assert(!(a->chosen && synode_eq(a->app_key, null_synode)));
			MAY_DBG(FN; PTREXP(a); SYCEXP(a->app_key));
			MAY_DBG(FN;
			    COPY_AND_FREE_GOUT(dbg_app_data(a));
			    );
			if (a->body.c_t == exit_type) {
				bury_site(get_group_id(get_site_def()));
				copy_app_data(&a, NULL);
				task_terminate_all();    /* Kill, kill, kill, kill, kill, kill. This is the end. */

				init_xcom_base();             /* Reset shared variables */
				init_tasks();            /* Reset task variables */
				free_site_defs();
				free_forced_config_site_def();
				garbage_collect_servers();
				DBGOUT(FN; STRLIT("shutting down"));
				xcom_shutdown = 1;
				TERMINATE;
			} else if (a->body.c_t == reset_type || a->body.c_t == remove_reset_type) {
				if(a->body.c_t == reset_type) /* Not for remove node */
					bury_site(get_group_id(get_site_def()));
				copy_app_data(&a, NULL);
				init_xcom_base();             /* Reset shared variables */
				check_tasks(); /* Stop tasks which should not be running */
				free_site_defs();
				free_forced_config_site_def();
				garbage_collect_servers();
			} else {
				if (reject_send(get_site_def(), a->recover)) {
					copy_app_data(&a, NULL);
				} else {
					pax_msg * msg = pax_msg_new(null_synode, get_site_def());
					if (is_real_recover(a)) {
						msg->start_type = RECOVER;
						if (force_recover) {
							/* We are desperate to recover,
							   fake an accepted message with null key */
							DBGOUT(FN; STRLIT("forcing recovery "));
							a->chosen = TRUE;
						}
					}
					xcom_send(a, msg);
				}
			}
		}

		TASK_DELAY(0.1);
	}
	FINALLY
	    TASK_END;
}
/* purecov: end */

#define FNVSTART 0x811c9dc5

/* Fowler-Noll-Vo type multiplicative hash */
static uint32_t fnv_hash(unsigned char *buf, size_t length, uint32_t sum)
{
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
uint32_t new_id()
{
	long	id = get_unique_long();
	double	timestamp = task_now();
	uint32_t retval = 0;
	while (retval == 0 ||
	    is_dead_site(retval)) { /* Avoid returning 0 or already used site id */
		retval = fnv_hash((unsigned char *) & id, sizeof(id), 0);
		retval = fnv_hash((unsigned char *) & timestamp, sizeof(timestamp), retval);
	}
	return retval;
}

static synode_no getstart(app_data_ptr a)
{
	synode_no retval = null_synode;
	G_MESSAGE("getstart group_id %x", a->group_id);
	if (!a || a->group_id == null_id) {
		retval.group_id = new_id();
	} else {
		a->app_key.group_id = a->group_id;
		retval = a->app_key;
		if (get_site_def() && retval.msgno != 1) {
			/* Not valid until after event horizon has been passed */
			synode_set_to_event_horizon(&retval);
		}
	}
	return retval;
}

void site_install_action(site_def *site)
{
	DBGOUT(FN; NDBG(get_nodeno(get_site_def()), u));
	if (synode_gt(site->start, max_synode))
		set_max_synode(site->start);
	site->nodeno = xcom_find_node_index(&site->nodes);
	push_site_def(site);
	DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_site_def(site)));
	set_group(get_group_id(site));
	if(get_maxnodes(get_site_def())){
		update_servers(site);
	}
	DBGOUT(FN; SYCEXP(site->start); SYCEXP(site->boot_key));
	DBGOUT(FN; NDBG(get_nodeno(site), u));
	DBGOUT(SYCEXP(site->start); SYCEXP(site->boot_key));
	DBGOUT(NDBG(get_nodeno(site), u));
}

static site_def *create_site_def_with_start(app_data_ptr a, synode_no start)
{
	site_def * site = new_site_def();
	MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)); );
	init_site_def(a->body.app_u_u.nodes.node_list_len,
				  a->body.app_u_u.nodes.node_list_val, site);
	site->start = start;
	site->boot_key = a->app_key;
	return site;
}


static site_def * install_ng_with_start(app_data_ptr a, synode_no start)
{
	if (a) {
		site_def *site = create_site_def_with_start(a, start);
		site_install_action(site);
		return site;
	}
	return 0;
}


site_def *install_node_group(app_data_ptr a)
{
	ADD_EVENTS(
	    add_event(string_arg("a->app_key")); 
	    add_synode_event(a->app_key); 
	    );
	if (a)
		return install_ng_with_start(a, getstart(a));
		else
		return 0;
}

/* purecov: begin deadcode */
int	is_real_recover(app_data_ptr a)
{
	return  a && a->body.c_t == xcom_recover && a->body.app_u_u.rep.msg_list.synode_no_array_len > 0;
}
/* purecov: end */

void	set_max_synode(synode_no synode)
{
	max_synode = synode; /* Track max synode number */
	MAY_DBG(FN; STRLIT("new "); SYCEXP(max_synode));
}

/* purecov: begin deadcode */
static void	learn_accepted_value(site_def const * site, pax_msg *p, synode_no synode)
{
	pax_msg * msg = pax_msg_new(synode, site);
	ref_msg(msg);
	copy_app_data(&msg->a, p->a);
	msg->start_type = p->start_type;
	set_learn_type(msg);
	MAY_DBG(FN; STRLIT("trying to learn known value "); SYCEXP(synode));
	send_to_all_site(site, msg, "learn_accepted_value");
	unref_msg(&msg);
}
/* purecov: end */

static int	is_busy(synode_no s)
{
	pax_machine * p = hash_get(s);
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

bool_t match_my_msg(pax_msg *learned, pax_msg *mine)
{
	MAY_DBG(FN; PTREXP(learned->a);
	    if (learned->a)
	    SYCEXP(learned->a->unique_id);
	    PTREXP(mine->a);
	    if (mine->a)
	    SYCEXP(mine->a->unique_id);
	    )		;
	if (learned->a && mine->a) { /* Both have app data, see if data is mine */
		return synode_eq(learned->a->unique_id, mine->a->unique_id);
	} else if (!(learned->a || mine->a)) { /* None have app data, anything goes */
		return TRUE;
	} else { /* Definitely mismatch */
		return FALSE;
	}
}


#if TASK_DBUG_ON
static void	dbg_reply_set(site_def const * site, const char *s, bit_set *bs)
{
	unsigned int	i = 0;
	unsigned int	n = get_maxnodes(site);
	GET_GOUT;
	STRLIT(s);
	for (i = 0; i <  n && i < bs->bits.bits_len * sizeof(*bs->bits.bits_val) * BITS_PER_BYTE; i++) {
		NPUT(BIT_ISSET(i, bs), d);
	}
	PRINT_GOUT;
	FREE_GOUT;
}
#endif


static void	propose_noop(synode_no find, pax_machine *p);

static inline int too_far(synode_no s)
{
	return s.msgno >= executed_msg.msgno + event_horizon;
}

#define GOTO(x) {DBGOUT(STRLIT("goto "); STRLIT(#x)); goto x; }

static inline int is_view(cargo_type x)
{
	return x == view_msg;
}

static inline int is_config(cargo_type x)
{
	return x == unified_boot_type || 
	    x == add_node_type || 
	    x == remove_node_type || 
	    x == force_config_type; 
}


/* Send messages by fetching from the input queue and trying to get it accepted
   by a Paxos instance */
static int	proposer_task(task_arg arg)
{
	DECL_ENV
	    int	self;       /* ID of this proposer task */
	pax_machine * p; /* Pointer to Paxos instance */
	msg_link * client_msg; /* The client message we are trying to push */
	synode_no msgno;
	pax_msg * prepare_msg;
	double	start_propose;
	double	start_push;
	double	delay;
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
		int	MY_ATTRIBUTE((unused)) lock = 0;
		/* Wait for client message */
		assert(!ep->client_msg);
		CHANNEL_GET(&prop_input_queue, &ep->client_msg, msg_link);
		MAY_DBG(FN; PTREXP(ep->client_msg->p->a); STRLIT("extracted "); SYCEXP(ep->client_msg->p->a->app_key));

		/* Grab rest of messages in queue as well, but never batch config messages, which need a unique number */

		if(!is_config(ep->client_msg->p->a->body.c_t) && !is_view(ep->client_msg->p->a->body.c_t)){
			ep->size = app_data_size(ep->client_msg->p->a);
		    while(AUTOBATCH && ep->size <= MAX_BATCH_SIZE &&
		    	! link_empty(&prop_input_queue.data)){  /* Batch payloads into single message */
		    	msg_link *tmp;
		    	app_data_ptr atmp;

				CHANNEL_GET(&prop_input_queue, &tmp, msg_link);
				atmp = tmp->p->a;
				ep->size += app_data_size(atmp);
				/* Abort batching if config or too big batch */
				if(is_config(atmp->body.c_t) || is_view(atmp->body.c_t) ||
				ep->size > MAX_BATCH_SIZE){
					channel_put_front(&prop_input_queue, &tmp->l);
					break;
				}
		    	ADD_T_EV(seconds(),__FILE__, __LINE__, "batching");
	
				tmp->p->a = 0; 		/* Steal this payload */
				msg_link_delete(&tmp);  /* Get rid of the empty message */
				atmp->next = ep->client_msg->p->a; /* Add to list of app_data */
				G_TRACE("Batching %s %s", cargo_type_to_str(ep->client_msg->p->a->body.c_t),
					cargo_type_to_str(atmp->body.c_t));
				ep->client_msg->p->a = atmp;
				MAY_DBG(FN; PTREXP(ep->client_msg->p->a); STRLIT("extracted "); SYCEXP(ep->client_msg->p->a->app_key));
		    }
	    }
		ep->start_propose = task_now();
		ep->delay = 0.0;


		assert(!(AUTOBATCH && ep->client_msg->p->a->chosen));

		/* See if value is known already (old message) */
		if (ep->client_msg->p->a->chosen) {
			DBGOUT(FN; PTREXP(ep->client_msg->p->a); STRLIT("pushing old "); SYCEXP(ep->client_msg->p->a->app_key));
			MAY_DBG(FN;
			    COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p));
			    );
			ep->msgno = ep->client_msg->p->a->app_key;
			ep->site = find_site_def(ep->msgno);
			if(!ep->site) /* Use current site if message is too old */
				ep->site = get_site_def();

			/* See if we can do anything with this message */
#if 0
	    	if (!ep->site || get_nodeno(ep->site) == VOID_NODE_NO) {
				/* Give up */
				deliver_to_app(NULL, ep->client_msg->p->a, delivery_failure);
				GOTO(next);
			}
#endif
retry_old:
			ep->p = get_cache(ep->msgno);
			assert(ep->p);
			lock = lock_pax_machine(ep->p);
			assert(!lock);

			/* Try to get a value accepted */
			learn_accepted_value(ep->site, ep->client_msg->p, ep->msgno);
			while (!finished(ep->p)) {
				/* Sleep here if value is not already chosen */
				TIMED_TASK_WAIT(&ep->p->rv, ep->delay = wakeup_delay(ep->delay));
				if (!synode_eq(ep->msgno, ep->p->synode)) {
					DBGOUT(FN; STRLIT("proposer_task detected stolen state machine, retry"); );
					/* unlock_pax_machine(ep->p); */
					GOTO(retry_old);
				}
				assert(synode_eq(ep->msgno, ep->p->synode));
				learn_accepted_value(ep->site, ep->client_msg->p, ep->msgno);
			}
			unlock_pax_machine(ep->p);
			msg_link_delete(&ep->client_msg);
			continue;
		}

		/* It is a new message */

		assert(!synode_eq(current_message, null_synode));

retry_new:
		/* Find a free slot */

		assert(!synode_eq(current_message, null_synode));
		ep->msgno = current_message;
		while (is_busy(ep->msgno)) {
			while (/* ! ep->client_msg->p->force_delivery &&  */ too_far(incr_msgno(ep->msgno))) { /* Too far ahead of executor */
				TIMED_TASK_WAIT(&exec_wait, 1.0);
				DBGOUT(FN; TIMECEXP(ep->start_propose); TIMECEXP(ep->client_msg->p->a->expiry_time); TIMECEXP(task_now());

				    NDBG(enough_live_nodes(ep->site), d));
			}
			ep->msgno = incr_msgno(ep->msgno);
		}
		assert(!synode_eq(ep->msgno, null_synode));
		proposer_site = find_site_def_rw(ep->msgno);

		ep->site = proposer_site;

		/* See if we can do anything with this message */
#if 1
    	if (!ep->site || get_nodeno(ep->site) == VOID_NODE_NO) {
			/* Give up */
			deliver_to_app(NULL, ep->client_msg->p->a, delivery_failure);
			GOTO(next);
		}
#endif
		DBGOHK(FN; STRLIT("changing current message"));
		set_current_message(ep->msgno);

		brand_client_msg(ep->client_msg->p, ep->msgno);
		ep->client_msg->p->a->lsn = ep->msgno.msgno;

		for(;;) { /* Loop until the client message has been learned */
			/* Get a Paxos instance to send the client message */
			ep->p = get_cache(ep->msgno);
			assert(ep->p);
			if(ep->client_msg->p->force_delivery)
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
			DBGOUT(FN; PTREXP(ep->client_msg->p->a); STRLIT("pushing "); SYCEXP(ep->msgno));
			MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_app_data(ep->prepare_msg->a)));

			if(threephase || ep->p->force_delivery){
				push_msg_3p(ep->site, ep->p, ep->prepare_msg, ep->msgno, normal);
			}else{
				push_msg_2p(ep->site, ep->p);
			}

			ep->start_push = task_now();

			while (!finished(ep->p)) { /* Try to get a value accepted */
				/* We will wake up periodically, and whenever a message arrives */
				TIMED_TASK_WAIT(&ep->p->rv, ep->delay = wakeup_delay(ep->delay));
				if (!synode_eq(ep->msgno, ep->p->synode) || ep->p->proposer.msg == NULL) {
					DBGOHK(FN; STRLIT("detected stolen state machine, retry"); );
					/* unlock_pax_machine(ep->p); */
					GOTO(retry_new); /* Need to break out of both loops,
										and we have no "exit named loop" construction */
				}
				assert(synode_eq(ep->msgno, ep->p->synode) &&  ep->p->proposer.msg);
				if (finished(ep->p))
					break;
				 {
					 double	now = task_now();
					if ((ep->start_push + ep->delay) <= now) {
						PAX_MSG_SANITY_CHECK(ep->p->proposer.msg);
						DBGOUT(FN; STRLIT("retry pushing "); SYCEXP(ep->msgno));
						MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_app_data(ep->prepare_msg->a));
						    );
						DBGOUT(BALCEXP(ep->p->proposer.bal);
						    BALCEXP(ep->p->acceptor.promise));
						MAY_DBG(FN; dbg_reply_set(ep->site, "prep_node_set", ep->p->proposer.prep_nodeset);
						    dbg_reply_set(ep->site, "prop_node_set", ep->p->proposer.prop_nodeset);
						    );
						push_msg_3p(ep->site, ep->p, ep->prepare_msg, ep->msgno, normal);
						ep->start_push = now;
					}
				}
			}
			/* When we get here, we know the value for this message number,
			   but it may not be the value we tried to push,
			   so loop until we have a successfull push. */
			unlock_pax_machine(ep->p);
			MAY_DBG(FN; STRLIT(" found finished message ");
			    SYCEXP(ep->msgno);
			    STRLIT("seconds since last push ");
			    NPUT(task_now() - ep->start_push, f);
			    STRLIT("ep->client_msg ");
			    COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p));
			    );
			MAY_DBG(FN; STRLIT("ep->p->learner.msg ");
			    COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p->learner.msg));
			    );
			if (match_my_msg(ep->p->learner.msg, ep->client_msg->p)){
				break;
			} else
				GOTO(retry_new);
		}
next:
		 {
			double	now = task_now();
			double	used = now -ep->start_propose;
			add_to_filter(used);
			DBGOUT(FN; STRLIT("completed ep->msgno ");
			    SYCEXP(ep->msgno); NDBG(used, f); NDBG(median_time(), f);
			    STRLIT("seconds since last push "); NDBG(now - ep->start_push, f); );
			MAY_DBG(FN; STRLIT("ep->client_msg ");
			    COPY_AND_FREE_GOUT(dbg_pax_msg(ep->client_msg->p));
			    );
			if (ep->p) {
				MAY_DBG(FN; STRLIT("ep->p->learner.msg ");
						COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p->learner.msg));
				    );
			}
			msg_link_delete(&ep->client_msg);
		}
	}
	FINALLY
	    MAY_DBG(FN; STRLIT("exit "); NDBG(ep->self, d); NDBG(task_now(), f));
	if (ep->p)
		unlock_pax_machine(ep->p);
	replace_pax_msg(&ep->prepare_msg, NULL);
	if (ep->client_msg) { /* If we get here with a client message, we have failed to deliver */
		deliver_to_app(ep->p, ep->client_msg->p->a, delivery_failure);
		msg_link_delete(&ep->client_msg);

	}
	TASK_END;
}


/* }}} */


/* {{{ Executor task */

static node_no	leader(site_def const *s)
{
	node_no	leader = 0;
	for (leader = 0; leader < get_maxnodes(s); leader++) {
		if (!may_be_dead(s->detected, leader, task_now()))
			return leader;
	}
	return 0;
}


int	iamthegreatest(site_def const *s)
{
	return leader(s) == s->nodeno;
}


void	execute_msg(site_def const * site, pax_machine *pma, pax_msg *p)
{
	app_data_ptr a = p->a;
	DBGOUT(FN;
	    COPY_AND_FREE_GOUT(dbg_pax_msg(p));
	    );
	if (a) {
		switch (p->a->body.c_t) {
		case unified_boot_type:
	    case add_node_type:
	    case remove_node_type:
      case force_config_type:

			check_tasks();
			break;
		case xcom_recover:
/* purecov: begin deadcode */
			break;
/* purecov: end */
		case app_type:
			MAY_DBG(FN; STRLIT(" learner.msg ");
			    COPY_AND_FREE_GOUT(dbg_pax_msg(pma->learner.msg));
			    );
			deliver_to_app(pma, a, delivery_ok);
			break;
		case view_msg:
			MAY_DBG(FN;
			    STRLIT(" learner.msg ");
			    COPY_AND_FREE_GOUT(dbg_pax_msg(pma->learner.msg)); );
			if(site && site->global_node_set.node_set_len == a->body.app_u_u.present.node_set_len){
 				assert(site->global_node_set.node_set_len == a->body.app_u_u.present.node_set_len);
				copy_node_set(&a->body.app_u_u.present, &(((site_def *)site)->global_node_set));
				deliver_global_view_msg(site, p->synode);
			}
			break;
#ifdef USE_EXIT_TYPE
		case exit_type:
			g_critical("Unable to get message, process will now exit. Please ensure that the process is restarted");
			exit(1);
			break;
#endif
		default:
			break;
		}
	}
	MAY_DBG(FN; SYCEXP(p->synode));
}



static void	read_missing_values(int n);
static void	propose_missing_values(int n);

static void	find_value(site_def const *site, unsigned int *wait, int n)
{
	DBGOHK(FN; NDBG(*wait, d));

	if(get_nodeno(site) == VOID_NODE_NO){
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

int	get_xcom_message(pax_machine **p, synode_no msgno, int n)
{
	DECL_ENV
	    unsigned int	wait;
	double	delay;
	END_ENV;

	TASK_BEGIN

	    ep->wait = 0;
	ep->delay = 0.0;
	*p = get_cache(msgno);

	while (!finished(*p)) {
		site_def const * site = find_site_def(msgno);
		DBGOHK(FN;
		    STRLIT(" not finished ");
		    SYCEXP(msgno); PTREXP(*p);
		    NDBG(ep->wait, u);
		    SYCEXP(msgno));
		if (get_maxnodes(site) > 1 && iamthegreatest(site) &&
		    site->global_node_set.node_set_val &&
		    !site->global_node_set.node_set_val[msgno.node] &&
		    may_be_dead(site->detected, msgno.node, task_now())){
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

synode_no set_executed_msg(synode_no msgno)
{
	DBGOUT(FN; STRLIT("changing executed_msg from "); SYCEXP(executed_msg); STRLIT(" to "); SYCEXP(msgno));
	if (synode_gt(msgno, current_message)) {
		DBGOHK(FN; STRLIT("changing current message"));
		set_current_message(first_free_synode(msgno));
	}

	if (msgno.msgno > executed_msg.msgno)
		task_wakeup(&exec_wait);

	executed_msg = msgno;
	executor_site = find_site_def(executed_msg);
	return executed_msg;
}


static synode_no first_free_synode(synode_no msgno)
{
	site_def const * site = find_site_def(msgno);
 	synode_no retval = msgno;
	if(get_group_id(site) == 0){
		DBGOUT(FN; PTREXP(site); SYCEXP(msgno));
		if(site){
            DBGOUT(FN; SYCEXP(site->boot_key); SYCEXP(site->start); COPY_AND_FREE_GOUT(dbg_site_def(site)));
		}
	}
	assert(get_group_id(site) != 0);
	assert(!synode_eq(msgno, null_synode));
	if (retval.msgno == 0)
		retval.msgno = 1;
	retval.node = get_nodeno(site);
	if (synode_lt(retval, msgno))
		return incr_msgno(retval);
	else
		return retval;
}



synode_no set_current_message(synode_no msgno)
{
	MAY_DBG(FN; STRLIT("changing current_message from "); SYCEXP(current_message); STRLIT(" to "); SYCEXP(msgno));
	return current_message = msgno;
}


static void	handle_learn(site_def const * site, pax_machine *p, pax_msg *m);
static void	update_max_synode(pax_msg *p);

#if TASK_DBUG_ON
static void perf_dbg(int *_n, int *_old_n, double *_old_t) MY_ATTRIBUTE((unused));
static void perf_dbg(int *_n, int *_old_n, double *_old_t)
{
	int	n = *_n;
	int	old_n = *_old_n;
	double	old_t = *_old_t;

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

static inline int	LOSER(synode_no x, site_def const *site)
{
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
  DBGOUT(NEXP(x.node,u); NEXP(site->global_node_set.node_set_val[(x).node],d));
  return
    /* ( n > maxnodes / 2 || (ARBITRATOR_HACK && (2 == maxnodes && 0 == get_nodeno(site)))) && */
    (!(site)->global_node_set.node_set_val[(x).node] );
}

#else
#define LOSER(x, site) 0
#endif

static void	debug_loser(synode_no x)  MY_ATTRIBUTE((unused));
#if defined(TASK_DBUG_ON) && TASK_DBUG_ON
static void	debug_loser(synode_no x)
{
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
static void	debug_loser(synode_no x MY_ATTRIBUTE((unused)))
{
}
/* purecov: end */
#endif

/* #define DBGFIX2(x){ GET_GOUT; ADD_F_GOUT("%f ",task_now()); x; PRINT_GOUT; FREE_GOUT; } */
#define DBGFIX2(x)
static void	send_value(site_def const *site, node_no to, synode_no synode)
{
	pax_machine * pm = get_cache(synode);
	if (pm && pm->learner.msg) {
		pax_msg * msg = clone_pax_msg(pm->learner.msg);
		ref_msg(msg);
		send_server_msg(site, to, msg);
		unref_msg(&msg);
	}
}

/* Peturn message number where it is safe for nodes in prev config to exit */
static synode_no compute_delay(synode_no start)
{
	start.msgno += event_horizon;
	return start;
}

/* Push messages to all nodes which were in the previous site, but not in this */
static void inform_removed(int index,int all)
{
	site_def * *sites = 0;
	uint32_t	site_count = 0;
	DBGFIX2(FN; NEXP(index, d));
	get_all_site_defs(&sites, &site_count);
	while (site_count > 1 && index >= 0 && (uint32_t)(index + 1) < site_count) {
		site_def * s = sites[index];
		site_def * ps = sites[index+1];

		/* Compute diff and push messages */
		DBGFIX2(FN; NDBG(index,d); PTREXP(s); if(s)SYCEXP(s->boot_key); PTREXP(ps); if(ps)SYCEXP(ps->boot_key));

		if (s && ps) {
			node_no i = 0;
			DBGFIX2(FN; SYCEXP(s->boot_key); SYCEXP(s->start);
			    SYCEXP(ps->boot_key); SYCEXP(ps->start));
			for (i = 0; i < ps->nodes.node_list_len; i++) { /* Loop over prev site */
				if (ps->nodeno != i && !node_exists(&ps->nodes.node_list_val[i], &s->nodes)) {
					synode_no synode = s->start;
					synode_no end = compute_delay(s->start);
					while (!synode_gt(synode, end)) { /* Loop over relevant messages */
						send_value(ps, i, synode);
						synode = incr_synode(synode);
					}
				}
			}
		}
		if(! all) /* Early exit if not all configs should be examined */
			break;
		index--;
	}
}

site_def *handle_add_node(app_data_ptr a)
{
	site_def * site = clone_site_def(get_site_def());
	DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)); );
	MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)); );
	ADD_EVENTS(
	    add_event(string_arg("a->app_key")); 
	    add_synode_event(a->app_key); 
	    );
	assert(get_site_def());
	assert(site);
	add_site_def(a->body.app_u_u.nodes.node_list_len,
	    a->body.app_u_u.nodes.node_list_val, site);
	site->start = getstart(a);
	site->boot_key = a->app_key;
	site_install_action(site);
	return site;
}

static void	terminate_and_exit()
{
	XCOM_FSM(xa_terminate, int_arg(0));	/* Tell xcom to stop */
	XCOM_FSM(xa_exit, int_arg(0));		/* Tell xcom to exit */
}

int	terminator_task(task_arg arg)
{
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

static void	delayed_terminate_and_exit(double t)
{
	task_new(terminator_task, double_arg(t), "terminator_task", XCOM_THREAD_DEBUG);
}

static inline int is_empty_site(site_def const *s)
{
	return s->nodes.node_list_len == 0;
}

site_def *handle_remove_node(app_data_ptr a)
{
	site_def * site = clone_site_def(get_site_def());
	DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&a->body.app_u_u.nodes)));
	ADD_EVENTS(
	    add_event(string_arg("a->app_key")); 
	    add_synode_event(a->app_key);
	    add_event(string_arg("nodeno"));
	    add_event(uint_arg(get_nodeno(site)));
	);
	remove_site_def(a->body.app_u_u.nodes.node_list_len,
	    a->body.app_u_u.nodes.node_list_val, site);
	site->start = getstart(a);
	site->boot_key = a->app_key;
	site_install_action(site);
	return site;
}

void handle_config(app_data_ptr a)
{
	while(a){
		switch (a->body.c_t) {
			case unified_boot_type:
				install_node_group(a);
				break;
			case add_node_type:
				handle_add_node(a);
				break;
			case remove_node_type:
				handle_remove_node(a);
                if(xcom_shutdown)
					return;
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

enum exec_state {
	FETCH = 0,
	EXECUTE = 1
};
typedef enum exec_state exec_state;

#define NEXTSTATE(x) ep->state = (x)

static 	synode_no delivered_msg;

synode_no get_delivered_msg()
{
	return delivered_msg;
}

static inline int is_member(site_def const *site)
{
	return site->nodeno != VOID_NODE_NO;
}

/*
Execute xcom message stream.

Beware of the exit logic in this task, which is both simple and not so simple.
Consider three configs C1 and C2. C1 has two nodes, A and B. C2 has only node B.
C3 is empty.
A config with message number N will be activated after a delay of (at least)
alpha messages, where alpha is the size of the pipeline (or the event horizon).

So, C1.start = C1+alpha,
and C2.start = C2+alpha. A, which is removed from C1, cannot exit until a majority
of nodes in the new config C2 (in this case B) has learned all the messages from
config C1, which means all messages less than C2.start. How can A know that a majority
of C2 has learned those messages?

If we denote the first message that is not yet decided (and executed) by E,
the proposers will not try to propose messages with number >= E+alpha,
and all incoming tcp messages with message number >= E+alpha will be ignored.
E is incremented by the executor task, so all messages < E are known.
This means that when the value of E+alpha is known, all messages up to
and including E are also known, although not all messages E+1..E+alpha-1
necessarily are known.

This leads to the requirement that a node which is removed (A) needs to wait until
it knows the value of C2.start+alpha, since by then it knows that a majority
of the nodes in C2 are ready to execute C2.start, which in turn implies that
a majority of nodes in C2 knows all the values from config C1. Note that the last
message that should be delivered to the application by a node that is leaving C1 is
C2.start-1, which is the last message of C1.

How does a node that is removed get to know values from the next config?
There are two ways, and we use both. First, the node that tries to exit can
simply ask for the message. get_xcom_message() will do this for all messages
<= max_synode, but it may take some time.
Second, the nodes of C2 can send the messages C2.start..C2.start+alpha
to the nodes that are removed (nodes that are in C1 but not in C2).
inform_removed() does this. We take care to handle the case where configs are close enough
that C0 < C1 <= C0+alpha by tracking the oldest config that contains nodes that are
leaving.

This takes care of nodes leaving C1. What about nodes that leave C2? C3 is empty,
so B, which is leaving C2, cannot wait for messages from C3. But since C3 is empty,
there is no need to wait. It can exit immediately after having executed C3.start-1, the
last message of C2. What if C3.start-1 < C2.start+alpha? This can happen if C2 and C3
are close. In that case, B will exit before A gets the chance to learn C2.start+alpha,
which will leave A hanging forever. Clearly, we need to impose an additional constraint,
that C3.start must be greater than C2.start+alpha. This is taken care of by the special
test for an empty config.

Complicated and confusing? Not really, but there is a clean and simple solution which has
not been implemented yet, since it requires more changes to the consensus logic.
If we require that for the messages C2..C2.start-1 we have a majority from both the nodes
in C1 and the nodes in C2, the nodes not in C2 can exit when they have executed message
C2.start-1, since we then know that a majority of the nodes of C2 has agreed on those messages
as well, so they do not depend on the nodes not in C2 any more. This holds even if C2 is empty.
Note that requiring a majority from both C1 and C2 is different from requiring a majority from
C1+C2, which means that the proposer logic needs to consider answers from two different sets of
acceptors for those messages. Since acceptors are identified by their node number, and the node
numbers need not be the same for both configs, we need to maintain a mapping between the nodes
numbers of any two consecutive configs. Alternatively, we could remove the node numbers altogether,
and always use a unique, unchanging ID for a node, like IP address + port.

*/

/* FIFO which tracks the message numbers where we should deliver queued messages or
inform the removed nodes */
#define FIFO_SIZE 1000
static struct {
	int n;
	int front;
	int rear;
	synode_no q[FIFO_SIZE];
}delay_fifo;

static inline int addone(int i)
{
	return ((i + 1) % FIFO_SIZE);
}

/* Is queue empty?  */
static inline int fifo_empty()
{
	return delay_fifo.n <= 0;
}

/* Is queue full?  */
static inline int fifo_full()
{
	return delay_fifo.n >= FIFO_SIZE;
}


/* Insert in queue  */
static inline void fifo_insert(synode_no s)
{
	if(! fifo_full()){
		delay_fifo.n++;
		delay_fifo.q[delay_fifo.rear] = s;
		delay_fifo.rear = addone(delay_fifo.rear);
	}
}

/* Extract first from queue  */
static inline synode_no fifo_extract()
{
	if(! fifo_empty()){
		synode_no ret = delay_fifo.q[delay_fifo.front];
		delay_fifo.front = addone(delay_fifo.front);
		delay_fifo.n--;
		return ret;
	}else{
		return null_synode;
	}
}

/* Return first in queue, but do not dequeue  */
static inline synode_no fifo_front()
{
	if(! fifo_empty()){
		return delay_fifo.q[delay_fifo.front];
	}else{
		return null_synode;
	}
}

static int	executor_task(task_arg arg MY_ATTRIBUTE((unused)))
{
	DECL_ENV
	    pax_machine * p;
	int	n ;
	int	old_n;
	double	old_t;
	synode_no exit_synode;
	exec_state state;
	enum {
		no_exit,
		not_member_exit,
		empty_exit
	} exit_type;
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

	if (executed_msg.msgno == 0)
		executed_msg.msgno = 1;
	delivered_msg = executed_msg;
	NEXTSTATE(FETCH);
	executor_site = find_site_def(executed_msg);

	while (!xcom_shutdown) {
		for (; ; ) {
			if (ep->state == FETCH) {
				if ( !LOSER(executed_msg, executor_site)) {
					TASK_CALL(get_xcom_message(&ep->p, executed_msg, FIND_MAX));
					DBGOUT(FN; STRLIT("got message "); SYCEXP(ep->p->synode); COPY_AND_FREE_GOUT(dbg_app_data(XAPP)));
					/* Execute unified_boot immediately, but do not deliver site message until we */
					/* are ready to execute messages from the new site definition. */
					/* At that point we can be certain that a majority have learned */
					/* everything from the old site. */

					if ((XAPP) && is_config((XAPP)->body.c_t) &&
						synode_gt(executed_msg, get_site_def()->boot_key)) /* Redo test */
					{
						site_def * site = 0;
						set_last_received_config(executed_msg);
						handle_config(XAPP);
						garbage_collect_site_defs(delivered_msg);
						check_tasks();
						site = get_site_def_rw();
						if (site == 0) {
							TERMINATE;
						}
						DBGFIX2(FN; STRLIT("new config "); SYCEXP(site->boot_key); ); /*SYCEXP(site->start); NEXP(get_nodeno(site),d); NEXP(ARBITRATOR_HACK,d);
								NEXP(ep->exit_type,d); SYCEXP(ep->exit_synode);
								SYCEXP(executed_msg); SYCEXP(max_synode)); */

						/* If site is empty, increase start to allow nodes to terminate before start */
						if (is_empty_site(site)) {
							site->start = compute_delay(compute_delay(site->start));
						}
						if (ep->exit_type == no_exit){/* We have not yet set the exit trigger */
							synode_no delay_until;
							if(is_member(site)){
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
									DBGFIX2(FN; SYCEXP(ep->exit_synode); SYCEXP(executed_msg); SYCEXP(max_synode));
								}else{
									/*
										If we are not a member of the new site, we should exit after having
										seen enough messages from the new site.
									 */
									ep->exit_synode = compute_delay(site->start);
									ep->exit_type = not_member_exit;
									if (!synode_lt(ep->exit_synode, max_synode)){
										/* We need messages from the next site, so set max_synode accordingly. */
										set_max_synode(incr_synode(ep->exit_synode));
									}
									delay_until = ep->exit_synode;
									DBGFIX2(FN; SYCEXP(delay_until); SYCEXP(executed_msg); SYCEXP(max_synode));
									DBGFIX2(FN; SYCEXP(ep->exit_synode); SYCEXP(executed_msg); SYCEXP(max_synode));
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
					DBGOUT(FN; debug_loser(executed_msg); PTREXP(executor_site);
						COPY_AND_FREE_GOUT(dbg_node_set(executor_site->global_node_set)));
				}
				DBGOUT(FN; NDBG(ep->state, d); SYCEXP(delivered_msg); SYCEXP(executed_msg);
					SYCEXP(ep->exit_synode); NDBG(ep->exit_type, d));

				/* See if we should exit when having seen this message */
				if (ep->exit_type == not_member_exit && synode_eq(executed_msg, ep->exit_synode)) {
					inform_removed(ep->inform_index, 1); /* Inform all removed nodes before we exit */
					delayed_terminate_and_exit(TERMINATE_DELAY);	/* Tell xcom to stop */
					TERMINATE;
				}

				if (fifo_empty()) {
					NEXTSTATE(EXECUTE);
				} else if (synode_eq(executed_msg, fifo_front())) {
					DBGFIX2(FN; SYCEXP(fifo_front()); SYCEXP(executed_msg);
							SYCEXP(ep->exit_synode); NDBG(ep->exit_type, d));
					while(synode_eq(executed_msg, fifo_front())){ /* More than one may match */
						inform_removed(ep->inform_index, 0);
						fifo_extract();
						ep->inform_index--;
					}
					garbage_collect_servers();
					NEXTSTATE(EXECUTE);
				}
				SET_EXECUTED_MSG(incr_synode(executed_msg));
				MAY_DBG(FN; NDBG(ep->state, d); SYCEXP(fifo_front()); SYCEXP(executed_msg));
				MAY_DBG(FN; NDBG(ep->state, d); SYCEXP(ep->exit_synode); SYCEXP(executed_msg));
			} else if (ep->state == EXECUTE) {
				site_def const * x_site = find_site_def(delivered_msg);

				DBGOUT(FN; NDBG(ep->state, d); SYCEXP(delivered_msg); SYCEXP(delivered_msg); SYCEXP(executed_msg);
					SYCEXP(ep->exit_synode); NDBG(ep->exit_type, d));
				ep->p = get_cache(delivered_msg);
				ADD_EVENTS(
					add_event(string_arg("executing message")); 
					add_synode_event(ep->p->synode); 
				);
				if (LOSER(delivered_msg, x_site)) {
#ifdef IGNORE_LOSERS
					DBGOUT(FN; debug_loser(delivered_msg); PTREXP(x_site); dbg_node_set(x_site->global_node_set));
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
				if (ep->exit_type == empty_exit && synode_eq(delivered_msg, ep->exit_synode)) {
					inform_removed(ep->inform_index, 1); /* Inform all removed nodes before we exit */
					delayed_terminate_and_exit(TERMINATE_DELAY);	/* Tell xcom to stop */
					TERMINATE;
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


static synode_no  get_sweep_start()
{
	synode_no    find = executed_msg;
	find.node = get_nodeno(find_site_def(find));
	if (find.node < executed_msg.node) {
		find = incr_msgno(find);
	}
	return find;
}


static int	sweeper_task(task_arg arg MY_ATTRIBUTE((unused)))
{
	DECL_ENV
	    synode_no find;
	END_ENV;

	TASK_BEGIN

	    ep->find = get_sweep_start();

	while (!xcom_shutdown) {
		ep->find.group_id = executed_msg.group_id; /* In case group id has changed */
#ifndef AGGRESSIVE_SWEEP
		while (!is_only_task()) {
			TASK_YIELD;
		}
#endif
		ADD_EVENTS(
			add_event(string_arg("sweeper ready")); 
			add_synode_event(executed_msg); 
		);
/* 		 DBGOUT(FN; STRLIT("ready to run ");   */
/*                         SYCEXP(executed_msg); SYCEXP(max_synode); SYCEXP(ep->find));  */
		 {
			while (synode_lt(ep->find, max_synode) && ! too_far(ep->find)) {
				/* pax_machine * pm = hash_get(ep->find); */
				pax_machine * pm = 0;
				ADD_EVENTS(
					add_event(string_arg("sweeper examining")); 
					add_synode_event(ep->find); 
				);
 				DBGOUT(FN; STRLIT("examining "); SYCEXP(ep->find));
				if (ep->find.node == VOID_NODE_NO) {
					if(synode_gt(executed_msg, ep->find)){
						ep->find = get_sweep_start();
					}
					if (ep->find.node == VOID_NODE_NO)
						goto deactivate;
				}
				pm = get_cache(ep->find);
				if (pm && !pm->force_delivery) { /* We want full 3 phase Paxos for forced messages */
					/* DBGOUT(FN; dbg_pax_machine(pm)); */
					if (!is_busy_machine(pm) && pm->acceptor.promise.cnt == 0 && ! pm->acceptor.msg && !finished(pm)) {
						pm->op = skip_op;
						ADD_EVENTS(
							add_event(string_arg("sweeper skipping")); 
							add_synode_event(ep->find);
							add_event(string_arg(pax_op_to_str(pm->op)));
						);
						skip_msg(pax_msg_new(ep->find, find_site_def(ep->find)));
 						MAY_DBG(FN; STRLIT("skipping "); SYCEXP(ep->find));
/* 						MAY_DBG(FN; dbg_pax_machine(pm)); */
					}
				}
				ep->find = incr_msgno(ep->find);
			}
		}
	deactivate:
		TASK_DEACTIVATE;
	}
	FINALLY
	    MAY_DBG(FN; STRLIT(" shutdown sweeper "); SYCEXP(executed_msg); NDBG(task_now(), f));
	TASK_END;
}


/* }}} */

#if 0
static double	wakeup_delay(double old)
{
	double	retval = 0.0;
	if (0.0 == old) {
		double	m = median_time();
		if (m == 0.0 || m > 1.0)
			m = 0.1;
		retval = 0.1 + 10.0 * m + m * my_drand48();
	} else {
		retval = old * 1.4142136; /* Exponential backoff */
	}
	while (retval > 10.0)
		retval /= 1.31415926;
	/* DBGOUT(FN; NDBG(retval,d)); */
	return retval;
}


#else
static double	wakeup_delay(double old)
{
	double	retval = 0.0;
	if (0.0 == old) {
		double	m = median_time();
		if (m == 0.0 || m > 0.3)
			m = 0.1;
		retval = 0.1 + 5.0 * m + m * my_drand48();
	} else {
		retval = old * 1.4142136; /* Exponential backoff */
	}
	while (retval > 3.0)
		retval /= 1.31415926;
	/* DBGOUT(FN; NDBG(retval,d)); */
	return retval;
}


#endif

static void	propose_noop(synode_no find, pax_machine *p)
{
	/* Prepare to send a noop */
   	site_def const *site = find_site_def(find);
	assert(! too_far(find));
	replace_pax_msg(&p->proposer.msg, pax_msg_new(find, site));
	assert(p->proposer.msg);
	create_noop(p->proposer.msg);
/*  	DBGOUT(FN; SYCEXP(find););  */
	push_msg_3p(site, p, clone_pax_msg(p->proposer.msg), find, no_op);
}


static void	send_read(synode_no find)
{
	/* Prepare to send a read_op */
	site_def const *site = find_site_def(find);
 	MAY_DBG(FN; NDBG(get_maxnodes(site),u); NDBG(get_nodeno(site),u););
	if (site && find.node != get_nodeno(site)) {
		pax_msg * pm = pax_msg_new(find, site);
		ref_msg(pm);
		create_read(site, pm);
  		MAY_DBG(FN; SYCEXP(find););

 		MAY_DBG(FN; NDBG(get_maxnodes(site),u); NDBG(get_nodeno(site),u); PTREXP(pm));
		/* send_server_msg(site, find.node, pm); */
#if 0
		send_to_others(site, pm, "send_read");
#else
		if(get_nodeno(site) == VOID_NODE_NO)
			send_to_others(site, pm, "send_read");
		else
			send_to_someone(site, pm, "send_read");
#endif
		unref_msg(&pm);
	}
}


/* }}} */

/* {{{ Find missing values */

static int	ok_to_propose(pax_machine *p)
{
#if 0
	site_def const *s = find_site_def(p->synode.group_id);
	int	retval = (p->synode.node == get_nodeno(s) || task_now() -p->last_modified > 5.0 || may_be_dead(s->detected, p->synode.node, task_now()))
	 && !recently_active(p) && !finished(p) && !is_busy_machine(p);
#else
	int	retval = !recently_active(p) && !finished(p) && !is_busy_machine(p);
#endif
	MAY_DBG(FN; NDBG(p->synode.node, u); NDBG(recently_active(p),d); NDBG(finished(p),d); NDBG(is_busy_machine(p),d); NDBG(retval, d));
	return retval;
}


static void	read_missing_values(int n)
{
	synode_no find = executed_msg;
	synode_no end = max_synode;
	int	i = 0;

  	MAY_DBG(FN; SYCEXP(find); SYCEXP(end));
	if (
	    synode_gt(executed_msg, max_synode) ||
	    synode_eq(executed_msg, null_synode))
		return;

	while (!synode_gt(find, end) && i < n && ! too_far(find)) {
		pax_machine * p = get_cache(find);
  		MAY_DBG(FN; SYCEXP(find); SYCEXP(end); NDBG(recently_active(p),d);  NDBG(finished(p),d); NDBG(is_busy_machine(p),d));

		if (!recently_active(p) && !finished(p) && !is_busy_machine(p)) {
			send_read(find);
		}
		find = incr_synode(find);
		i++;
	}
}


static void	propose_missing_values(int n)
{
	synode_no find = executed_msg;
	synode_no end = max_synode;
	int	i = 0;

	DBGOHK(FN; NDBG(get_maxnodes(get_site_def()), u); SYCEXP(find); SYCEXP(end));
	if (
	    synode_gt(executed_msg, max_synode) ||
	    synode_eq(executed_msg, null_synode))
		return;

	MAY_DBG(FN; SYCEXP(find); SYCEXP(end));
	i = 0;
	while (!synode_gt(find, end) && i < n && ! too_far(find)) {
		pax_machine * p = get_cache(find);
 		 DBGOHK(FN; NDBG(ok_to_propose(p),d);  TIMECEXP(task_now());  TIMECEXP(p->last_modified); SYCEXP(find))
 		    ;
		if(get_nodeno(find_site_def(find)) == VOID_NODE_NO)
			break;
		if (ok_to_propose(p)) {
			if (task_now() - BUILD_TIMEOUT > p->last_modified){
				propose_noop(find, p);
			}
		}
		find = incr_synode(find);
		i++;
	}
}


/* Propose a noop for the range find..end */
void	request_values(synode_no find, synode_no end)
{
	DBGOUT(FN; SYCEXP(find); SYCEXP(find); SYCEXP(end); );
	while (!synode_gt(find, end) && ! too_far(find)) {
		pax_machine * p = get_cache(find);
		site_def const *site = find_site_def(find);
		if(get_nodeno(site) == VOID_NODE_NO)
			break;
		if (!finished(p) && !is_busy_machine(p)) {
			/* Prepare to send a noop */
			replace_pax_msg(&p->proposer.msg, pax_msg_new(find, site));
			assert(p->proposer.msg);
			create_noop(p->proposer.msg);

			DBGOUT(FN; STRLIT("propose"); SYCEXP(find); );
			push_msg_3p(site, p, pax_msg_new(find, site), find, no_op);
		}
		find = incr_synode(find);
	}
}


/* }}} */

/* {{{ Message handlers */
#if 0
void	reply_msg(site_def const * site, pax_msg *m)
{
	MAY_DBG(FN; );
	if (get_server(s, m->from)) {
		send_server_msg(s, m->from, m);
	}
}

#else
#define reply_msg(m) \
{ \
  if(is_local_node((m)->from, site)){ \
    dispatch_op(site, m, NULL); \
  }else{ \
    if(node_no_exists((m)->from, site) && (m)->group_id == get_group_id(site) && get_server(site, (m)->from)){ \
			send_server_msg(site, (m)->from, m); \
		}else{ \
			link_into(&(msg_link_new((m), (m)->from)->l), reply_queue); \
		} \
	} \
}
#endif


#define CREATE_REPLY(x) pax_msg *reply = NULL; CLONE_PAX_MSG(reply, x)
#define SEND_REPLY reply_msg(reply); replace_pax_msg(&reply, NULL)

static void	teach_ignorant_node(site_def const * site, pax_machine *p, pax_msg *pm, synode_no synode, linkage *reply_queue)
{
	CREATE_REPLY(pm);
	DBGOUT(FN; SYCEXP(synode));
	reply->synode = synode;
	reply->proposal = p->learner.msg->proposal;
	reply->msg_type =  p->learner.msg->msg_type;
	copy_app_data(&reply->a, p->learner.msg->a);
	set_learn_type(reply);
	/* set_unique_id(reply, p->learner.msg->unique_id); */
	SEND_REPLY;
}


/* Handle incoming read */
static void	handle_read(site_def const * site, pax_machine *p, linkage *reply_queue, pax_msg *pm)
{
	DBGOUT(FN;
	    BALCEXP(pm->proposal);
	    BALCEXP(p->acceptor.promise);
	    if (p->acceptor.msg)
	    BALCEXP(p->acceptor.msg->proposal);
	    STRLIT("type "); STRLIT(pax_msg_type_to_str(pm->msg_type)))         ;

	if (finished(p)) { /* We have learned a value */
		teach_ignorant_node(site, p, pm, pm->synode, reply_queue);
	}
}


#ifdef USE_EXIT_TYPE
static void	miss_prepare(site_def const * site, pax_msg *pm, linkage *reply_queue)
{
	CREATE_REPLY(pm);
	DBGOUT(FN; SYCEXP(pm->synode));
	reply->msg_type = normal;
	reply->a = new_app_data();
	reply->a->body.c_t = exit_type;
	reply->op = ack_prepare_op;
	SEND_REPLY;
}


static void	miss_accept(site_def const * site, pax_msg *pm, linkage *reply_queue)
{
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

static void	handle_simple_prepare(site_def const * site, pax_machine *p, pax_msg *pm, synode_no synode, linkage *reply_queue)
{
	if (finished(p)) { /* We have learned a value */
		MAY_DBG(FN; SYCEXP(synode); BALCEXP(pm->proposal); NDBG(finished(p), d));
		teach_ignorant_node(site, p, pm, synode, reply_queue);
	} else {
		int	greater = gt_ballot(pm->proposal, p->acceptor.promise); /* Paxos acceptor phase 1 decision */
		MAY_DBG(FN; SYCEXP(synode); BALCEXP(pm->proposal); NDBG(greater, d));
		if (greater || noop_match(p, pm) ) {
			CREATE_REPLY(pm);
			reply->synode = synode;
			if (greater)
				p->acceptor.promise = pm->proposal; /* promise to not accept any less */
			if (accepted(p)) { /* We have accepted a value */
				reply->proposal = p->acceptor.msg->proposal;
				reply->msg_type =  p->acceptor.msg->msg_type;
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
static void	handle_prepare(site_def const * site, pax_machine *p, linkage *reply_queue, pax_msg *pm)
{
		ADD_EVENTS(
			add_synode_event(p->synode);
			add_event(string_arg("pm->from"));
			add_event(int_arg(pm->from));
			add_event(string_arg(pax_op_to_str(pm->op)));
		);
#if 0
	DBGOUT(FN;
	    NDBG(pm->from, d); NDBG(pm->to, d);
	    SYCEXP(pm->synode);
	    BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise));
#endif
	MAY_DBG(FN; BALCEXP(pm->proposal);
	    BALCEXP(p->acceptor.promise);
	    if (p->acceptor.msg)
	    BALCEXP(p->acceptor.msg->proposal);
	    STRLIT("type "); STRLIT(pax_msg_type_to_str(pm->msg_type)))         ;

	handle_simple_prepare(site, p, pm, pm->synode, reply_queue);
}


static void	check_propose(site_def const * site, pax_machine *p)
{
	MAY_DBG(FN; SYCEXP(p->synode);
	    COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(site)));
	    );
	PAX_MSG_SANITY_CHECK(p->proposer.msg);
	if (prep_majority(site, p)) {
		p->proposer.msg->proposal = p->proposer.bal;
		BIT_ZERO(p->proposer.prop_nodeset);
		p->proposer.msg->synode = p->synode;
		propose_msg(p->proposer.msg);
		p->proposer.sent_prop = p->proposer.bal;
	}
}


static void	check_learn(site_def const * site, pax_machine *p)
{
	MAY_DBG(FN; SYCEXP(p->synode);
	    COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(site)));
	    );
	PAX_MSG_SANITY_CHECK(p->proposer.msg);
	if (get_nodeno(site) != VOID_NODE_NO && prop_majority(site, p)) {
		p->proposer.msg->synode = p->synode;
		if (p->proposer.msg->receivers)
			free_bit_set(p->proposer.msg->receivers);
		p->proposer.msg->receivers = clone_bit_set(p->proposer.prep_nodeset);
		BIT_SET(get_nodeno(site), p->proposer.msg->receivers);
		if(no_duplicate_payload)
			tiny_learn_msg(site, p->proposer.msg);
 		else
			learn_msg(site, p->proposer.msg);
		p->proposer.sent_learn = p->proposer.bal;
	}
}


static void	do_learn(site_def const * site MY_ATTRIBUTE((unused)), pax_machine *p, pax_msg *m)
{
	ADD_EVENTS(
		add_synode_event(p->synode);
		add_event(string_arg("m->from"));
		add_event(int_arg(m->from));
		add_event(string_arg(pax_op_to_str(m->op)));
	);
	/* FN; SYCEXP(p->synode); SYCEXP(m->synode); STRLIT(NEWLINE); */
	MAY_DBG(FN; SYCEXP(p->synode); SYCEXP(m->synode);
	    dbg_bitset(m->receivers, get_maxnodes(site));
	    );
	if (m->a)
		m->a->chosen = TRUE;
	replace_pax_msg(&p->acceptor.msg, m);
	replace_pax_msg(&p->learner.msg, m);
	/* if(m->msg_type == no_op)lru_touch(p); */ /* Move to no_op lru if no_op */
}


static void	handle_simple_ack_prepare(site_def const * site MY_ATTRIBUTE((unused)), pax_machine *p, pax_msg *m)
{
	if(get_nodeno(site) != VOID_NODE_NO)
		BIT_SET(m->from, p->proposer.prep_nodeset);
}


/* Other node has already accepted a value */
static void	handle_ack_prepare(site_def const * site, pax_machine *p, pax_msg *m)
{
	ADD_EVENTS(
		add_synode_event(p->synode);
		add_event(string_arg("m->from"));
		add_event(int_arg(m->from));
		add_event(string_arg(pax_op_to_str(m->op)));
	);
#if 0
	DBGOUT(FN;
	    NDBG(pm->from, d); NDBG(pm->to, d);
	    SYCEXP(pm->synode);
	    BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise));
#endif
	assert(m);
	MAY_DBG(FN;
	    if (p->proposer.msg)
	    BALCEXP(p->proposer.msg->proposal);
	    BALCEXP(p->proposer.bal);
	    BALCEXP(m->reply_to);
	    BALCEXP(p->proposer.sent_prop);
	    SYCEXP(m->synode))		;
	if (m->from != VOID_NODE_NO && eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my prepare */
		handle_simple_ack_prepare(site, p, m);
		if (gt_ballot(m->proposal, p->proposer.msg->proposal)) { /* greater */
			replace_pax_msg(&p->proposer.msg, m);
			assert(p->proposer.msg);
		}
		if (gt_ballot(m->reply_to, p->proposer.sent_prop))
			check_propose(site, p);
	}
}


/* Other node has not already accepted a value */
static void	handle_ack_prepare_empty(site_def const * site, pax_machine *p, pax_msg *m)
{
	ADD_EVENTS(
		add_synode_event(p->synode);
		add_event(string_arg("m->from"));
		add_event(int_arg(m->from));
		add_event(string_arg(pax_op_to_str(m->op)));
	);
#if 0
	DBGOUT(FN;
	    NDBG(pm->from, d); NDBG(pm->to, d);
	    SYCEXP(pm->synode);
	    BALCEXP(pm->proposal); BALCEXP(p->acceptor.promise));
#endif
	MAY_DBG(FN;
	    if (p->proposer.msg)
	    BALCEXP(p->proposer.msg->proposal);
	    BALCEXP(p->proposer.bal);
	    BALCEXP(m->reply_to);
	    BALCEXP(p->proposer.sent_prop);
	    SYCEXP(m->synode))		;
	if (m->from != VOID_NODE_NO && eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my prepare */
		handle_simple_ack_prepare(site, p, m);
		if (gt_ballot(m->reply_to, p->proposer.sent_prop))
			check_propose(site, p);
	}
}


/* #define AUTO_MSG(p,synode) {if(!(p)){replace_pax_msg(&(p), pax_msg_new(synode, site));} */

static void	handle_simple_accept(site_def const * site, pax_machine *p, pax_msg *m, synode_no synode, linkage *reply_queue)
{
	if (finished(p)) { /* We have learned a value */
		teach_ignorant_node(site, p, m, synode, reply_queue);
	} else if (!gt_ballot(p->acceptor.promise, m->proposal) || /* Paxos acceptor phase 2 decision */
	noop_match(p, m) ) {
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
static void	handle_accept(site_def const * site, pax_machine *p, linkage *reply_queue, pax_msg *m)
{
	MAY_DBG(FN;
	    BALCEXP(p->acceptor.promise);
	    BALCEXP(m->proposal);
	    STREXP(pax_msg_type_to_str(m->msg_type)));
	PAX_MSG_SANITY_CHECK(m);
	ADD_EVENTS(
		add_synode_event(p->synode);
		add_event(string_arg("m->from"));
		add_event(int_arg(m->from));
		add_event(string_arg(pax_op_to_str(m->op)));
	);

	handle_simple_accept(site, p, m, m->synode, reply_queue);
}


/* Handle answer to accept */
static void	handle_ack_accept(site_def const * site, pax_machine *p, pax_msg *m)
{
	ADD_EVENTS(
		add_synode_event(p->synode);
		add_event(string_arg("m->from"));
		add_event(int_arg(m->from));
		add_event(string_arg(pax_op_to_str(m->op)));
	);
	MAY_DBG(FN; SYCEXP(m->synode); BALCEXP(p->proposer.bal); BALCEXP(p->proposer.sent_learn); BALCEXP(m->proposal); BALCEXP(m->reply_to);
	    );
	MAY_DBG(FN; SYCEXP(p->synode);
	    if (p->acceptor.msg)
	    BALCEXP(p->acceptor.msg->proposal);
	    BALCEXP(p->proposer.bal);
	    BALCEXP(m->reply_to);
	    )		;
	if (get_nodeno(site) != VOID_NODE_NO && m->from != VOID_NODE_NO &&
      eq_ballot(p->proposer.bal, m->reply_to)) { /* answer to my accept */
		BIT_SET(m->from, p->proposer.prop_nodeset);
		if (gt_ballot(m->proposal, p->proposer.sent_learn))
			check_learn(site, p);
	}
}

/* Configure all messages in interval start, end to be forced */
static void force_interval(synode_no start, synode_no end)
{
	while (synode_lt(start, end)) {
		pax_machine * p = get_cache(start);
		if(get_nodeno(find_site_def(start)) == VOID_NODE_NO)
			break;
		/* if(! finished(p)) */
		p->force_delivery = 1;
		/* Old nodesets are null and void */
		BIT_ZERO(p->proposer.prep_nodeset);
		BIT_ZERO(p->proposer.prep_nodeset);
		start = incr_synode(start);
	}
}

static void start_force_config(site_def *s)
{
	synode_no end = s->boot_key;

	synode_set_to_event_horizon(&end);

	DBGOUT(FN; SYCEXP(executed_msg); SYCEXP(end));
	if (synode_gt(end, max_synode))
		set_max_synode(end);

	free_site_def(forced_config);
	forced_config = s;
	force_interval(executed_msg, max_synode); /* Force everything in the pipeline */
}


/* Learn this value */
static void	handle_learn(site_def const * site, pax_machine *p, pax_msg *m)
{
	MAY_DBG(FN; STRLIT("proposer nodeset ");
	    dbg_bitset(p->proposer.prop_nodeset, get_maxnodes(site));
	    );
	MAY_DBG(FN; STRLIT("receivers ");
	    dbg_bitset(m->receivers, get_maxnodes(site));
	    );
	MAY_DBG(FN; NDBG(task_now(), f); SYCEXP(p->synode);
	    COPY_AND_FREE_GOUT(dbg_app_data(m->a));
	    );

	PAX_MSG_SANITY_CHECK(m);

	if (!finished(p)) { /* Avoid re-learn */
		do_learn(site, p, m);
		/* Check for special messages */
		if(m->a && m->a->body.c_t == unified_boot_type){
			DBGOUT(FN; STRLIT("Got unified_boot "); SYCEXP(p->synode); SYCEXP(m->synode); );
			XCOM_FSM(xa_net_boot, void_arg(m->a));
		}
		/* See if someone is forcing a new config */
		if(m->force_delivery && m->a){
			DBGOUT(FN; STRLIT("Got forced config "); SYCEXP(p->synode); SYCEXP(m->synode); );
			/* Configure all messages from executed_msg until start of new config
			   as forced messages so they will eventually be finished */
			/* Immediately install this new config */
			switch (m->a->body.c_t) {
			case add_node_type:
/* purecov: begin deadcode */
				start_force_config(clone_site_def(handle_add_node(m->a)));
				break;
/* purecov: end */
			case remove_node_type:
/* purecov: begin deadcode */
				start_force_config(clone_site_def(handle_remove_node(m->a)));
				break;
/* purecov: end */
			case force_config_type:
				start_force_config(clone_site_def(install_node_group(m->a)));
				break;
			default:
				break;
			}
			force_interval(executed_msg, getstart(m->a));
		}
	}

	task_wakeup(&p->rv);
}


/* Skip this value */
static void	handle_skip(site_def const * site, pax_machine *p, pax_msg *m)
{
	/*   MAY_DBG(FN;); */
	/*   MAY_DBG(FN; NDBG(task_now(),f); SYCEXP(p->msg->synode)); */
	if (!finished(p)) {
		skip_value(m);
		do_learn(site, p, m);
	}
	/*   MAY_DBG(FN; STRLIT("taskwakeup "); SYCEXP(p->msg->synode)); */
	task_wakeup(&p->rv);
}


static void	handle_client_msg(pax_msg *p)
{
	if (!p || p->a == NULL)
		/* discard invalid message */
		return;

	{
		msg_link * ml = msg_link_new(p, VOID_NODE_NO);

		/* Put it in the proposer queue */
		ADD_T_EV(task_now(), __FILE__, __LINE__, "handle_client_msg");
		channel_put(&prop_input_queue, &ml->l);
	}
}

/* Handle incoming alive message */
static double	sent_alive = 0.0;
static inline void	handle_alive(site_def const * site, linkage *reply_queue, pax_msg *pm)
{
	DBGOUT(FN; SYCEXP(pm->synode); NDBG(pm->from,u); NDBG(pm->to,u); );
	if (pm->from != pm->to && !client_boot_done && /* Already done? */
	!is_dead_site(pm->group_id)) { /* Avoid dealing with zombies */
		double	t = task_now();
		if (t - sent_alive > 1.0) {
			CREATE_REPLY(pm);
			reply->op = need_boot_op;
			SEND_REPLY;
			sent_alive = t;
			DBGOUT(FN; STRLIT("sent need_boot_op"); );
		}
	}
}


static void	update_max_synode(pax_msg *p)
{
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
	if (is_dead_site(p->group_id))
		return;
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
void	add_to_cache(app_data_ptr a, synode_no synode)
{
	pax_machine * pm = get_cache(synode);
	pax_msg * msg = pax_msg_new_0(synode);
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

static client_reply_code can_execute_cfgchange(pax_msg *p)
{
	app_data_ptr a = p->a;
	if (executed_msg.msgno <= 2)
		return REQUEST_RETRY;
	if (a && a->group_id != 0 && a->group_id != executed_msg.group_id)
		return REQUEST_FAIL;
	return REQUEST_OK;
}

static void activate_sweeper()
{
	if (sweeper) {
		ADD_EVENTS(
			add_event(string_arg("sweeper activated max_synode")); 
			add_synode_event(max_synode); 
		);
		task_activate(sweeper);
	}
}

pax_msg *dispatch_op(site_def const *site, pax_msg *p, linkage *reply_queue)
{
	pax_machine * pm = NULL;
	site_def * dsite = find_site_def_rw(p->synode);
	int	in_front = too_far(p->synode);

	if(p->force_delivery){
		/* Ensure that forced message can be processed */
		in_front = 0;
	}

	if (dsite && p->op != client_msg)
		note_detected(dsite, p->from);

	MAY_DBG(FN; STRLIT("incoming message ");
			COPY_AND_FREE_GOUT(dbg_pax_msg(p));
	    );
	ADD_EVENTS(
		add_synode_event(p->synode);
		add_event(string_arg("p->from"));
		add_event(int_arg(p->from));
		add_event(string_arg(pax_op_to_str(p->op)));
	);
	switch (p->op) {
	case client_msg:
		clicnt++;
		if (p->a && (p->a->body.c_t == enable_arbitrator)) {
			CREATE_REPLY(p);
			DBGOUT(FN; STRLIT("Got enable_arbitrator from client"); SYCEXP(p->synode); );
			ARBITRATOR_HACK = 1;
			reply->op = xcom_client_reply;
			reply->cli_err = REQUEST_OK;
			SEND_REPLY;
			break;
		}
		if (p->a && (p->a->body.c_t == disable_arbitrator)) {
			CREATE_REPLY(p);
			DBGOUT(FN; STRLIT("Got disable_arbitrator from client"); SYCEXP(p->synode); );
			ARBITRATOR_HACK = 0;
			reply->op = xcom_client_reply;
			reply->cli_err = REQUEST_OK;
			SEND_REPLY;
			break;
		}
		if (p->a && (p->a->body.c_t == x_terminate_and_exit)) {
			CREATE_REPLY(p);
			DBGOUT(FN; STRLIT("Got terminate_and_exit from client"); SYCEXP(p->synode); );
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
			DBGOUT(FN; STRLIT("Got unified_boot from client"); SYCEXP(p->synode); );
			DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)); );
			DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
			XCOM_FSM(xa_net_boot, void_arg(p->a));
		}
		if (p->a && p->a->body.c_t == add_node_type) {
			DBGOUT(FN; STRLIT("Got add_node from client"); SYCEXP(p->synode); );
			DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)); );
			DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
			assert(get_site_def());
		}
		if (p->a && p->a->body.c_t == remove_node_type) {
			DBGOUT(FN; STRLIT("Got remove_node from client"); SYCEXP(p->synode); );
			DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)); );
			DBGOUT(STRLIT("handle_client_msg "); NDBG(p->a->group_id, x));
			assert(get_site_def());
		}
		if (p->a && p->a->body.c_t == force_config_type) {
			DBGOUT(FN; STRLIT("Got new config from client"); SYCEXP(p->synode); );
			DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&p->a->body.app_u_u.nodes)); );
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
		handle_read(site, pm, reply_queue, p);
		break;
	case prepare_op:
		pm = get_cache(p->synode);
		assert(pm);
		if(p->force_delivery)
			pm->force_delivery = 1;
		pm->last_modified = task_now();
		handle_alive(site, reply_queue, p);
		handle_prepare(site, pm, reply_queue, p);
		break;
	case ack_prepare_op:
		if (in_front || !is_cached(p->synode))
			break;
		pm = get_cache(p->synode);
		if(p->force_delivery)
			pm->force_delivery = 1;
		if (!pm->proposer.msg)
			break;
		assert(pm && pm->proposer.msg);
		handle_ack_prepare(site, pm, p);
		break;
	case ack_prepare_empty_op:
		if (in_front || !is_cached(p->synode))
			break;
		pm = get_cache(p->synode);
		if(p->force_delivery)
			pm->force_delivery = 1;
		if (!pm->proposer.msg)
			break;
		assert(pm && pm->proposer.msg);
		handle_ack_prepare_empty(site, pm, p);
		break;
	case accept_op:
		pm = get_cache(p->synode);
		assert(pm);
		if(p->force_delivery)
			pm->force_delivery = 1;
		pm->last_modified = task_now();
		handle_alive(site, reply_queue, p);
		handle_accept(site, pm, reply_queue, p);
		break;
	case ack_accept_op:
		if (in_front || !is_cached(p->synode))
			break;
		pm = get_cache(p->synode);
		if(p->force_delivery)
			pm->force_delivery = 1;
		if (!pm->proposer.msg)
			break;
		assert(pm && pm->proposer.msg);
		handle_ack_accept(site, pm, p);
		break;
	case recover_learn_op:
		DBGOUT(FN; STRLIT("recover_learn_op receive "); SYCEXP(p->synode));
		pm = get_cache(p->synode);
		assert(pm);
		if(p->force_delivery)
			pm->force_delivery = 1;
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
		if(p->force_delivery)
			pm->force_delivery = 1;
		pm->last_modified = task_now();
		update_max_synode(p);
		activate_sweeper();
		handle_learn(site, pm, p);
		break;
	case tiny_learn_op:
		if (p->msg_type == no_op)
			goto learnop;
		pm = get_cache(p->synode);
		assert(pm);
		if(p->force_delivery)
			pm->force_delivery = 1;
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
		if(p->force_delivery)
			pm->force_delivery = 1;
		pm->last_modified = task_now();
		handle_skip(site, pm, p);
		break;
	case i_am_alive_op:
		/* handle_alive(site, reply_queue, p); */
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
			XCOM_FSM(xa_complete,int_arg(0));
		}
		break;
	case die_op:
		/* assert("die horribly" == "need coredump"); */
		 {
			GET_GOUT;
			FN;
			SYCEXP(executed_msg);
			PRINT_GOUT;
			FREE_GOUT;
		}
		g_critical("Node %u unable to get message, process will now exit. Please ensure that the process is restarted",
		    get_nodeno(site));
		exit(1);
	default:
		break;
	}
	return(p);
}



/* }}} */

/* {{{ Acceptor-learner task */
/* purecov: begin deadcode */
static void	send_die(site_def const * site, pax_msg *p)
{
	if (get_maxnodes(site) > 0) {
		pax_msg * np = NULL;
		synode_no synode = null_synode;
		synode.group_id = get_group_id(site);
		np = pax_msg_new(synode, site);
		ref_msg(np);
		np->op = die_op;
		DBGOUT(FN; STRLIT("sending die_op to node "); NDBG(p->from, d); SYCEXP(executed_msg); SYCEXP(max_synode));

		send_server_msg(site, p->from, np);
		unref_msg(&np);
	}
}
/* purecov: end */

int	acceptor_learner_task(task_arg arg)
{
	DECL_ENV
	    connection_descriptor rfd;
	srv_buf *in_buf;

	pax_msg * p;
	u_int	buflen;
	char	*buf;
	linkage reply_queue;
	int	errors;
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
			int	ret_ssl;
			int	err;
			ERR_clear_error();
			ret_ssl = SSL_accept(ep->rfd.ssl_fd);
			err = SSL_get_error(ep->rfd.ssl_fd, ret_ssl);

			while (ret_ssl != SSL_SUCCESS) {
				if (err == SSL_ERROR_WANT_READ){
					wait_io(stack, ep->rfd.fd, 'r');
				} else if (err == SSL_ERROR_WANT_WRITE){
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
	link_init(&ep->reply_queue,  type_hash("msg_link"));

	while (!xcom_shutdown) {
		int64_t	n;
		site_def const * site = 0;
		unchecked_replace_pax_msg(&ep->p, pax_msg_new_0(null_synode));

		if(use_buffered_read){
			TASK_CALL(buffered_read_msg(&ep->rfd, ep->in_buf, ep->p, &n));
		}else{
			TASK_CALL(read_msg(&ep->rfd, ep->p, &n));
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
		ep->p->refcnt = 1; /* Refcnt from other end is void here */
		MAY_DBG(FN;
				NDBG(ep->rfd.fd, d); NDBG(task_now(), f);
				COPY_AND_FREE_GOUT(dbg_pax_msg(ep->p));
				);
		receive_count[ep->p->op]++;
		receive_bytes[ep->p->op] += (uint64_t)n + MSG_HDR_SIZE;
		{
			gboolean behind = FALSE;
			if (get_maxnodes(site) > 0) {
				behind = ep->p->synode.msgno + (CACHED / get_maxnodes(site)) <= max_synode.msgno;
				/* behind = synode_lt(ep->p->synode, executed_msg); */
			}
			if (ep->p->msg_type == normal ||
			    ep->p->synode.msgno == 0 || /* Used by i-am-alive and so on */
				is_cached(ep->p->synode) || /* Already in cache */
				(!behind)) { /* Guard against cache pollution from other nodes */
				dispatch_op(site, ep->p, &ep->reply_queue);

				/* Send replies on same fd */
				while (!link_empty(&ep->reply_queue)) {
					msg_link * reply = (msg_link * )(link_extract_first(&ep->reply_queue));
					MAY_DBG(FN;
					    COPY_AND_FREE_GOUT(dbg_linkage(&ep->reply_queue));
					    COPY_AND_FREE_GOUT(dbg_msg_link(reply));
					    COPY_AND_FREE_GOUT(dbg_pax_msg(reply->p));
					    );
					assert(reply->p);
					assert(reply->p->refcnt > 0);
					reply->p->to = ep->p->from;
					reply->p->from = ep->p->to;
					serialize_msg(reply->p, ep->rfd.x_proto, &ep->buflen, &ep->buf);
					MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_msg_link(reply));
							COPY_AND_FREE_GOUT(dbg_pax_msg(reply->p)));
					msg_link_delete(&reply);
					if(ep->buflen){
						int64_t	sent;
						TASK_CALL(task_write(&ep->rfd , ep->buf, ep->buflen, &sent));
						send_count[ep->p->op]++;
						send_bytes[ep->p->op] += ep->buflen;
						X_FREE(ep->buf);
					}
					ep->buf = NULL;
				}
			} else {
				DBGOUT(FN; STRLIT("rejecting ");
					   STRLIT(pax_op_to_str(ep->p->op));
					   NDBG(ep->p->from, d); NDBG(ep->p->to, d);
					   SYCEXP(ep->p->synode);
					   BALCEXP(ep->p->proposal));
				if (xcom_booted() && behind) {
#ifdef USE_EXIT_TYPE
					if (ep->p->op == prepare_op) {
						miss_prepare(ep->p, &ep->reply_queue);
					} else if (ep->p->op == accept_op) {
						miss_accept(ep->p, &ep->reply_queue);
					}
#else
					if (ep->p->op == prepare_op) {
						DBGOUT(FN; STRLIT("send_die ");
							   STRLIT(pax_op_to_str(ep->p->op));
							   NDBG(ep->p->from, d); NDBG(ep->p->to, d);
							   SYCEXP(ep->p->synode);
							   BALCEXP(ep->p->proposal));
						send_die(site, ep->p); /* Missed the window */
					}
#endif
				}
			}
		}
		/* TASK_YIELD; */
	}

	FINALLY
	    MAY_DBG(FN; STRLIT(" shutdown "); NDBG(ep->rfd.fd, d ); NDBG(task_now(), f));
	if(ep->reply_queue.suc && !link_empty(&ep->reply_queue))
	    empty_msg_list(&ep->reply_queue);
	unchecked_replace_pax_msg(&ep->p, NULL);
	shutdown_connection(&ep->rfd);
	DBGOUT(FN; NDBG(xcom_shutdown, d));
	if (ep->buf)
		X_FREE(ep->buf);
	free(ep->in_buf);

	TASK_END;
}


/* }}} */

/* {{{ Reply handler task */
int const need_boot_special = 1;

static void	server_handle_need_snapshot(server *srv, site_def const *s, node_no node);

int	reply_handler_task(task_arg arg)
{
	DECL_ENV
	    server * s;
	pax_msg * reply;
	END_ENV;

	TASK_BEGIN

	ep->s = (server * )get_void_arg(arg);
	srv_ref(ep->s);
	ep->reply = NULL;

	for (; ; ) {
		while (!is_connected(&ep->s->con)) {
			MAY_DBG(FN; STRLIT("waiting for connection"));
			TASK_DELAY(1.000);
		}
		{
			int64_t	n;
			unchecked_replace_pax_msg(&ep->reply, pax_msg_new_0( null_synode));

			ADD_EVENTS(
				add_event(string_arg("ep->s->con.fd"));
				add_event(int_arg(ep->s->con.fd));
			);
			TASK_CALL(read_msg(&ep->s->con, ep->reply, &n));
			ADD_EVENTS(
				add_event(string_arg("ep->s->con.fd"));
				add_event(int_arg(ep->s->con.fd));
			);
			ep->reply->refcnt = 1; /* Refcnt from other end is void here */
			if (n <= 0) {
				shutdown_connection(&ep->s->con);
				continue;
			}
			receive_bytes[ep->reply->op] += (uint64_t)n + MSG_HDR_SIZE;
		}
		MAY_DBG(FN;
		    NDBG(ep->s->con.fd, d); NDBG(task_now(), f);
		    COPY_AND_FREE_GOUT(dbg_pax_msg(ep->reply));
		    );
		receive_count[ep->reply->op]++;

		/* Special test for need_snapshot, since node and site may not be consistent */
		if (need_boot_special && ep->reply->op == need_boot_op) {
			pax_msg * p = ep->reply;
			server_handle_need_snapshot(ep->s, get_site_def(), p->from);
		}else{
			dispatch_op(find_site_def(ep->reply->synode), ep->reply, NULL);
		}
		TASK_YIELD;
	}

	FINALLY
	    replace_pax_msg(&ep->reply, NULL);

	shutdown_connection(&ep->s->con);
	ep->s->reply_handler = NULL;
	MAY_DBG(FN; STRLIT(" shutdown "); NDBG(ep->s->con.fd, d); NDBG(task_now(), f));
	srv_unref(ep->s);

	TASK_END;
}


/* }}} */


/* purecov: begin deadcode */
static inline void xcom_sleep(unsigned int seconds)
{
#if defined (WIN32) || defined (WIN64)
 Sleep((DWORD)seconds*1000); /* windows sleep takes milliseconds */
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
long	get_unique_long(void)
{
#if defined(WIN32) || defined(WIN64)
	__time64_t ltime;

	_time64( &ltime );
	return (long) (ltime ^ GetCurrentProcessId());
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

#define CO_BEGIN switch(state){ default: assert(state == 0); case 0:
#define CO_END    }

#define CO_RETURN(x)                \
  {                                             \
    state = __LINE__;                          \
    return x;                                   \
  case __LINE__:;                               \
  }

#define HALT(x) while(1){ CO_RETURN(x);}

/* purecov: begin deadcode */
void send_app_data(app_data_ptr a)
{
	pax_msg * msg = pax_msg_new(null_synode, get_proposer_site());
	xcom_send(a, msg);
}

void	xcom_send_data(uint32_t size, char *data)
{
	app_data_ptr a = new_app_data();
	a->body.c_t = app_type;
	a->body.app_u_u.data.data_len = size;
	a->body.app_u_u.data.data_val = data;
	send_app_data(a);
}

app_data_ptr create_config(node_list *nl, cargo_type type)
{
	app_data_ptr a = new_app_data();
	a->body.c_t = type;
	init_node_list(nl->node_list_len, nl->node_list_val, &a->body.app_u_u.nodes);
	return a;
}
/* purecov: end */

app_data_ptr init_config_with_group(app_data *a, node_list *nl, cargo_type type,
                                    uint32_t group_id)
{
	init_app_data(a);
	a->app_key.group_id = a->group_id = group_id;
	a->body.c_t = type;
	init_node_list(nl->node_list_len, nl->node_list_val, &a->body.app_u_u.nodes);
	return a;
}

/* purecov: begin deadcode */
app_data_ptr create_config_with_group(node_list *nl, cargo_type type,
                                      uint32_t group_id)
{
	app_data_ptr a = new_app_data();
	return init_config_with_group(a, nl, type, group_id);
}

void send_boot(node_list *nl)
{	app_data_ptr a = create_config(nl, unified_boot_type);
	install_node_group(a); 	/* Cannot get consensus unless group is known */
	send_app_data(a);
}

void send_add_node(node_list *nl)
{
	send_app_data(create_config(nl, add_node_type));
}

void send_remove_node(node_list *nl)
{
	send_app_data(create_config(nl, remove_node_type));
}

void send_config(node_list *nl)
{
	send_app_data(create_config(nl, force_config_type));
}

void send_client_app_data(char *srv, xcom_port port, app_data_ptr a)
{
	pax_msg * msg = pax_msg_new(null_synode, 0);
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

void send_client_boot(char *srv, xcom_port port, node_list *nl)
{
	send_client_app_data(srv, port, create_config(nl, unified_boot_type));
}

void send_client_add_node(char *srv, xcom_port port, node_list *nl)
{
	send_client_app_data(srv, port, create_config(nl, add_node_type));
}

void send_client_remove_node(char *srv, xcom_port port, node_list *nl)
{
	send_client_app_data(srv, port, create_config(nl, remove_node_type));
}

void send_client_config(char *srv, xcom_port port, node_list *nl)
{
	send_client_app_data(srv, port, create_config(nl, force_config_type));
}
/* purecov: end */

static void	server_send_snapshot(server *srv, site_def const *s, gcs_snapshot *gcs_snap, node_no node)
{
	pax_msg * p = pax_msg_new(gcs_snap->log_start, get_site_def());
	ref_msg(p);
	p->op = gcs_snapshot_op;
	p->gcs_snap = gcs_snap;
	send_msg(srv, s->nodeno, node, get_group_id(s), p);
	unref_msg(&p);
}

/* purecov: begin deadcode */
static void	send_snapshot(site_def const *s, gcs_snapshot *gcs_snap, node_no node)
{
	assert(s->servers[node]);
	server_send_snapshot(s->servers[node], s, gcs_snap, node);
}
/* purecov: end */

static void	server_push_log(server *srv, synode_no push, node_no node)
{
	site_def const *s = get_site_def();
	while (!synode_gt(push, get_max_synode())) {
		if (is_cached(push)) {
			pax_machine * p = get_cache(push);
			if (pm_finished(p)) {
				pax_msg * pm = clone_pax_msg(p->learner.msg);
				ref_msg(pm);
				pm->op = recover_learn_op;
				send_msg(srv, s->nodeno, node, get_group_id(s), pm);
				unref_msg(&pm);
			}
		}
		push = incr_synode(push);
	}
}

/* purecov: begin deadcode */
static void	push_log(synode_no push, node_no node)
{
	site_def const * s = get_site_def();
	assert(s->servers[node]);
	server_push_log(s->servers[node], push, node);
}
/* purecov: end */

static app_snap_getter get_app_snap;
static app_snap_handler handle_app_snap;

/* purecov: begin deadcode */
static void	handle_need_snapshot(site_def const *s, node_no node)
{
	gcs_snapshot * gs = export_config();
	synode_no app_lsn = get_app_snap(&gs->app_snap);
	if (!synode_eq(null_synode, app_lsn) && synode_lt(app_lsn, gs->log_start))
		gs->log_start = app_lsn;
	send_snapshot(s, gs, node);
	push_log(gs->log_start, node);
}
/* purecov: end */

static void	server_handle_need_snapshot(server *srv, site_def const *s, node_no node)
{
	gcs_snapshot * gs = export_config();
	synode_no app_lsn = get_app_snap(&gs->app_snap);
	if (!synode_eq(null_synode, app_lsn) && synode_lt(app_lsn, gs->log_start)){
		gs->log_start = app_lsn;
	} else if (!synode_eq(null_synode, last_config_modification_id)){
		gs->log_start = last_config_modification_id;
	}

	server_send_snapshot(srv, s, gs, node);
	server_push_log(srv, gs->log_start, node);
}

#define X(b) #b,
const char *xcom_state_name[] = {
	x_state_list
};

const char *xcom_actions_name[] = {
	x_actions
};
#undef X

xcom_state xcom_fsm(xcom_actions action, task_arg fsmargs)
{
	static int	state = 0;
	G_MESSAGE("state %d action %s", state, xcom_actions_name[action]);
	switch (state) {
	default:
		assert(state == 0);
	case 0:
                /* Initialize basic xcom data */
                xcom_thread_init();
start:
		for (; ; ) {
			if (action == xa_init) {
				xcom_shutdown = 0;
				sent_alive = 0.0;
			}
			if (action == xa_u_boot) {
/* purecov: begin deadcode */
				node_list * nl = get_void_arg(fsmargs);
				app_data_ptr a = create_config(nl, unified_boot_type);
				install_node_group(a); 	/* Cannot get consensus unless group is known */
				send_app_data(a);
				set_executed_msg(incr_msgno(get_site_def()->start));
				goto run;
/* purecov: end */
			}
			if (action == xa_add) {
/* purecov: begin deadcode */
				add_args * a = get_void_arg(fsmargs);
				send_client_add_node(a->addr, a->port, a->nl);
/* purecov: end */
			}
			if (action == xa_net_boot) {
				app_data * a = get_void_arg(fsmargs);
				install_node_group(a);
				set_executed_msg(incr_msgno(get_site_def()->start));
				goto run;
			}
			if (action == xa_snapshot) {
				goto recover;
			}
			if (action == xa_exit) {
				/* Xcom is finished when we get here */
				bury_site(get_group_id(get_site_def()));
				task_terminate_all();    /* Kill, kill, kill, kill, kill, kill. This is the end. */

				init_xcom_base();        /* Reset shared variables */
				init_tasks();            /* Reset task variables */
				free_site_defs();
				free_forced_config_site_def();
				garbage_collect_servers();
				DBGOUT(FN; STRLIT("shutting down"));
				xcom_shutdown = 1;
				if(xcom_exit_cb)
					xcom_exit_cb(get_int_arg(fsmargs));
				G_MESSAGE("Exiting xcom thread");
			}
			CO_RETURN(x_start);
		}
recover:
		 {
			pax_msg * p = get_void_arg(fsmargs);
			import_config(p->gcs_snap);
			handle_app_snap(&p->gcs_snap->app_snap);
			set_executed_msg(p->gcs_snap->log_start);

			set_last_received_config(p->gcs_snap->log_start);

			DBGOUT(FN; SYCEXP(executed_msg); );
			for (; ; ) {
				if (action == xa_terminate) {
					goto start;
				}
				if (action == xa_complete) {
					goto run;
				}

				CO_RETURN(x_recover);
			}
		}
run:
		DBGOUT(FN; SYCEXP(executed_msg); );
		if(xcom_run_cb)
			xcom_run_cb(0);
		force_recover = 0;
		client_boot_done = 1;
		netboot_ok = 1;
		booting = 0;
		set_proposer_startpoint();
		create_proposers();
		set_task(&executor, task_new(executor_task, null_arg, "executor_task", XCOM_THREAD_DEBUG));
		set_task(&sweeper, task_new(sweeper_task, null_arg, "sweeper_task", XCOM_THREAD_DEBUG));
		set_task(&detector, task_new(detector_task, null_arg, "detector_task", XCOM_THREAD_DEBUG));
		set_task(&alive_t, task_new(alive_task, null_arg, "alive_task", XCOM_THREAD_DEBUG));

		for (; ; ) {
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

				init_xcom_base();             /* Reset shared variables */
				free_site_defs();
				free_forced_config_site_def();
				garbage_collect_servers();
				if(xcom_terminate_cb)
					xcom_terminate_cb(get_int_arg(fsmargs));
				goto start;
			}
			if (action == xa_need_snapshot) {
				pax_msg * p = get_void_arg(fsmargs);
				handle_need_snapshot(find_site_def(p->synode), p->from);
			}
			if (action == xa_force_config) {
				app_data * a = get_void_arg(fsmargs);
				site_def *s = create_site_def_with_start(a, executed_msg);
				s->boot_key = executed_msg;
				start_force_config(s);
			}
			CO_RETURN(x_run);
		}
	}
}

/* purecov: begin deadcode */
void xcom_add_node(char *addr, xcom_port port, node_list *nl)
{
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


void xcom_fsm_add_node(char *addr, node_list *nl)
{
	xcom_port	node_port = xcom_get_port(addr);
	char	*node_addr = xcom_get_name(addr);

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

void set_app_snap_handler(app_snap_handler x)
{
	handle_app_snap = x;
}

void set_app_snap_getter(app_snap_getter x)
{
	get_app_snap = x;
}

/* Initialize sockaddr based on server and port */
static int	init_sockaddr(char *server, struct sockaddr_in *sock_addr,
                          socklen_t *sock_size, xcom_port port)
{
	/* Get address of server */
	struct addrinfo *addr = 0;

	checked_getaddrinfo(server, 0, 0, &addr);

	if (addr == 0) {
		return 0;
	}

	/* Copy first address */
	memcpy(sock_addr, addr->ai_addr, addr->ai_addrlen);
	*sock_size = addr->ai_addrlen;
	sock_addr->sin_port = htons(port);
	freeaddrinfo(addr);

	return 1;
}


static result	checked_create_socket(int domain, int type, int protocol)
{
	result	retval = {0,0};
	int	retry = 1000;

	do {
		SET_OS_ERR(0);
		retval.val = socket(domain, type, protocol);
		retval.funerr = to_errno(GET_OS_ERR);
	} while (--retry && retval.val == -1 && (from_errno(retval.funerr) == SOCK_EAGAIN));

	if (retval.val == -1) {
		task_dump_err(retval.funerr);
#if defined (WIN32) || defined (WIN64)
		G_MESSAGE("Socket creation failed with error %d.",
			retval.funerr);
#else
		G_MESSAGE("Socket creation failed with error %d - %s.",
			retval.funerr, strerror(retval.funerr));
#endif
		abort();
	}
	return retval;
}

/* Read max n bytes from socket fd into buffer buf */
static result	socket_read(connection_descriptor*rfd, void *buf, int n)
{
	result 	ret = {0,0};

	assert(n >= 0);

	do {
	  ret = con_read(rfd, buf, n);
	  task_dump_err(ret.funerr);
	} while (ret.val < 0 && can_retry_read(ret.funerr));
	assert(!can_retry_read(ret.funerr));
	return ret;
}


/* Read exactly n bytes from socket fd into buffer buf */
static int64_t	socket_read_bytes(connection_descriptor *rfd, char *p, uint32_t n)
{
	uint32_t	left= n;
	char	*bytes= p;

	result	nread = {0,0};

	while (left > 0) {
		/*
		  socket_read just reads no more than INT_MAX bytes. We should not pass
		  a length more than INT_MAX to it.
		*/
		int r = (int) MIN(left, INT_MAX);

		nread = socket_read(rfd, bytes, r);
		if (nread.val == 0) {
			return 0;
		} else if (nread.val < 0) {
			return - 1;
		} else {
			bytes += nread.val;
			left -= (uint32_t)nread.val;
		}
	}
	assert(left == 0);
	return n;
}

/* Write n bytes from buffer buf to socket fd */
static int64_t	socket_write(connection_descriptor *wfd, void *_buf, uint32_t n)
{
	char	*buf = (char*) _buf;
	result 	ret = {0,0};

	uint32_t	total; /* Keeps track of number of bytes written so far */

	total = 0;
	while (total < n) {
		int w= (int) MIN(n - total, INT_MAX);

		while ((ret = con_write(wfd, buf + total, w)).val < 0 &&
			   can_retry_write(ret.funerr)) {
			task_dump_err(ret.funerr);
			DBGOUT(FN; STRLIT("retry "); NEXP(total, d); NEXP(n, d));
		}
		if (ret.val <= 0) { /* Something went wrong */
			task_dump_err(ret.funerr);
			return - 1;
		} else {
			total += (uint32_t)ret.val; /* Add number of bytes written to total */
		}
	}
	DBGOUT(FN; NEXP(total, u); NEXP(n, u));
	assert(total == n);
	return(total);
}

static inline result xcom_close_socket(int *sock)
{
	result res = {0,0};
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

static inline result xcom_shut_close_socket(int *sock)
{
	result res = {0,0};
	if (*sock >= 0) {
#if defined (WIN32) || defined (WIN64)
               static LPFN_DISCONNECTEX DisconnectEx = NULL;
               if (DisconnectEx == NULL)
               {
                       DWORD dwBytesReturned;
                       GUID guidDisconnectEx = WSAID_DISCONNECTEX;
                       WSAIoctl(*sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                               &guidDisconnectEx, sizeof(GUID),
                               &DisconnectEx, sizeof(DisconnectEx),
                               &dwBytesReturned, NULL, NULL);
               }
               if (DisconnectEx != NULL)
               {
                       (DisconnectEx(*sock, (LPOVERLAPPED)NULL,
                               (DWORD)0, (DWORD)0) == TRUE) ? 0 : -1;
               }
               else
#endif
		shutdown(*sock, _SHUT_RDWR);
		res = xcom_close_socket(sock);
	}
	return res;
}

static int timed_connect(int fd, sockaddr *sock_addr, socklen_t sock_size)
{
  struct timeval timeout;
  fd_set rfds, wfds, efds;
  int res;

  timeout.tv_sec=  10;
  timeout.tv_usec= 0;

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&efds);
  FD_SET(fd, &rfds);
  FD_SET(fd, &wfds);
  FD_SET(fd, &efds);

  /* Set non-blocking */
  if(unblock_fd(fd) < 0)
    return -1;

  /* Trying to connect with timeout */
  res= connect(fd, sock_addr, sock_size);

#if defined (WIN32) || defined (WIN64)
  if (res == SOCKET_ERROR)
  {
    res= WSAGetLastError();
    /* If the error is WSAEWOULDBLOCK, wait. */
    if (res == WSAEWOULDBLOCK)
    {
      MAY_DBG(FN; STRLIT("connect - error=WSAEWOULDBLOCK. Invoking select..."); );
#else
  if (res < 0)
  {
    if (errno == EINPROGRESS)
    {
      MAY_DBG(FN; STRLIT("connect - errno=EINPROGRESS. Invoking select..."); );
#endif
      res= select(fd + 1, &rfds, &wfds, &efds, &timeout);
      MAY_DBG(FN; STRLIT("select - Finished. "); NEXP(res, d));
      if (res == 0)
      {
        G_MESSAGE("Timed out while waiting for connection to be established! "
                  "Cancelling connection attempt. (socket= %d, error=%d)",
                  fd, res);
        G_WARNING("select - Timeout! Cancelling connection...");
        return -1;
      }
#if defined (WIN32) || defined (WIN64)
      else if (res == SOCKET_ERROR)
      {
        G_WARNING("select - Error while connecting! "
                  "(socket= %d, error=%d)",
                  fd, WSAGetLastError());
#else
      else if (res < 0)
      {
        G_WARNING("select - Error while connecting! "
                  "(socket= %d, error=%d, error msg='%s')",
                  fd, errno, strerror(errno));
#endif
        return -1;
      }
      else
      {
        if (FD_ISSET(fd, &wfds) || FD_ISSET(fd, &rfds))
        {
          MAY_DBG(FN; STRLIT("select - Socket ready!"); );
        }

        if (FD_ISSET(fd, &efds))
        {
          /*
            This is a non-blocking socket, so one needs to
            find the issue that triggered the exception.
           */
          int socket_errno= 0;
          socklen_t socket_errno_len= sizeof(errno);
          if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_errno,
                         &socket_errno_len))
          {
            G_WARNING("Connection to socket %d failed. Unable to sort out the "
                      "connection error!", fd);
          }
          else
          {
#if defined (WIN32) || defined (WIN64)
            G_WARNING("Connection to socket %d failed with error %d.",
                      fd, socket_errno);
#else
            G_WARNING("Connection to socket %d failed with error %d - %s.",
                      fd, socket_errno, strerror(socket_errno));
#endif
          }
          return -1;
        }
      }
    }
    else
    {
#if defined (WIN32) || defined (WIN64)
      G_WARNING("connect - Error connecting "
                "(socket=%d, error=%d).",
                fd, WSAGetLastError());
#else
      G_WARNING("connect - Error connecting "
                "(socket=%d, error=%d, error message='%s').",
                fd, errno, strerror(errno));
#endif
      return -1;
    }
  }
  else
  {
    MAY_DBG(FN; STRLIT("connect - Connected to socket without waiting!"); );
  }

  /* Set blocking */
  if(block_fd(fd) < 0)
  {
#if defined (WIN32) || defined (WIN64)
    G_WARNING("Unable to set socket back to blocking state. "
              "(socket=%d, error=%d).",
              fd, WSAGetLastError());
#else
    G_WARNING("Unable to set socket back to blocking state. "
              "(socket=%d, error=%d, error message='%s').",
              fd, errno, strerror(errno));
#endif
    return -1;
  }

  return fd;
}


/* Connect to server on given port */
static connection_descriptor*	connect_xcom(char *server, xcom_port port)
{
	result fd = {0,0};
	result ret = {0,0};
	struct sockaddr_in sock_addr;
	socklen_t sock_size;

	DBGOUT(FN; STREXP(server); NEXP(port, d));
	G_MESSAGE("connecting to %s %d", server, port);
	/* Create socket */
	if ((fd = checked_create_socket(AF_INET, SOCK_STREAM, 0)).val < 0) {
		G_MESSAGE("Error creating sockets.");
		return NULL;
	}

	/* Get address of server */
	if (!init_sockaddr(server, &sock_addr, &sock_size, port)) {
		xcom_close_socket(&fd.val);
		G_MESSAGE("Error initializing socket addresses.");
		return NULL;
	}

	/* Connect socket to address */

	SET_OS_ERR(0);
	if (timed_connect(fd.val, (struct sockaddr *)&sock_addr, sock_size) == -1) {
		fd.funerr = to_errno(GET_OS_ERR);
#if defined (WIN32) || defined (WIN64)
		G_MESSAGE("Connecting socket to address %s in port %d failed with error %d.",
				server, port, fd.funerr);
#else
		G_MESSAGE("Connecting socket to address %s in port %d failed with error %d - %s.",
				server, port, fd.funerr, strerror(fd.funerr));
#endif
		xcom_close_socket(&fd.val);
		return NULL;
	}

	{
		int	peer = 0;
		/* Sanity check before return */
		SET_OS_ERR(0);
		ret.val = peer = getpeername(fd.val, (struct sockaddr *)&sock_addr,
		                             &sock_size);
		ret.funerr = to_errno(GET_OS_ERR);
		if (peer >= 0) {
			ret = set_nodelay(fd.val);
			if(ret.val < 0){
				task_dump_err(ret.funerr);
				xcom_shut_close_socket(&fd.val);
#if defined (WIN32) || defined (WIN64)
				G_MESSAGE("Setting node delay failed  while connecting to %s with error %d.",
						server, ret.funerr);
#else
				G_MESSAGE("Setting node delay failed  while connecting to %s with error %d - %s.",
						server, ret.funerr, strerror(ret.funerr));
#endif
				return NULL;
			}
			G_MESSAGE("client connected to %s %d fd %d", server, port, fd.val);
		} else {
			/* Something is wrong */
			socklen_t errlen = sizeof(ret.funerr);
			DBGOUT(FN; STRLIT("getpeername failed"); );
			if (ret.funerr) {
				DBGOUT(FN; NEXP(from_errno(ret.funerr), d);
					   STRLIT(strerror(from_errno(ret.funerr))));
			}
			getsockopt(fd.val, SOL_SOCKET, SO_ERROR, (void *) & ret.funerr, &errlen);
			if (ret.funerr == 0) {
				ret.funerr = to_errno(SOCK_ECONNREFUSED);
			}
			xcom_shut_close_socket(&fd.val);
#if defined (WIN32) || defined (WIN64)
			G_MESSAGE("Getting the peer name failed while connecting to server %s with error %d.",
					server, ret.funerr);
#else
			G_MESSAGE("Getting the peer name failed while connecting to server %s with error %d -%s.",
					server, ret.funerr, strerror(ret.funerr));
#endif
			return NULL;
		}

#ifdef XCOM_HAVE_OPENSSL
		if (xcom_use_ssl()) {
			connection_descriptor *cd = 0;
			SSL * ssl = SSL_new(client_ctx);
			G_MESSAGE("Trying to connect using SSL.")
			SSL_set_fd(ssl, fd.val);

			ERR_clear_error();
			ret.val = SSL_connect(ssl);
			ret.funerr = to_ssl_err(SSL_get_error(ssl, ret.val));

			if (ret.val != SSL_SUCCESS) {
				G_MESSAGE("Error connecting using SSL %d %d.",
					  ret.funerr, SSL_get_error(ssl, ret.val));
				task_dump_err(ret.funerr);
				SSL_shutdown(ssl);
				SSL_free(ssl);
				xcom_shut_close_socket(&fd.val);
				return NULL;
			}
			DBGOUT(FN; STRLIT("ssl connected to "); STRLIT(server); NDBG(port,d); NDBG(fd.val, d); PTREXP(ssl));

			if (ssl_verify_server_cert(ssl, server))
			{
				G_MESSAGE("Error validating certificate and peer.");
				task_dump_err(ret.funerr);
				SSL_shutdown(ssl);
				SSL_free(ssl);
				xcom_shut_close_socket(&fd.val);
				return NULL;
			}

			cd = new_connection(fd.val, ssl);
			set_connected(cd, CON_FD);
			G_MESSAGE("Success connecting using SSL.")
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

connection_descriptor*	xcom_open_client_connection(char *server, xcom_port port)
{
	return connect_xcom(server, port);
}

/* Send a protocol negotiation message on connection con */
static int	xcom_send_proto(connection_descriptor *con, xcom_proto x_proto, x_msg_type x_type, unsigned int tag)
{
	char	buf[MSG_HDR_SIZE];
        memset(buf, 0, MSG_HDR_SIZE);

	if (con->fd >= 0) {
		con->snd_tag = tag;
		write_protoversion(VERS_PTR((unsigned char*) buf), x_proto);
		put_header_1_0((unsigned char*) buf, 0, x_type, tag);
		{
			int	sent;
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

static int	xcom_recv_proto(connection_descriptor * rfd, xcom_proto *x_proto, x_msg_type *x_type, unsigned int *tag)
{
	int	n;
	unsigned char	header_buf[MSG_HDR_SIZE];
	uint32_t	msgsize;

	/* Read length field, protocol version, and checksum */
	n = (int)socket_read_bytes(rfd, (char*)header_buf, MSG_HDR_SIZE);

	if (n != MSG_HDR_SIZE) {
		DBGOUT(FN; NDBG(n, d));
		return -1;
	}

	*x_proto = read_protoversion(VERS_PTR(header_buf));
	get_header_1_0(header_buf, &msgsize, x_type, tag);

	return n;
}

#define TAG_START 313

static int64_t	xcom_send_client_app_data(connection_descriptor *fd, app_data_ptr a, int force)
{
	pax_msg * msg = pax_msg_new(null_synode, 0);
	uint32_t	buflen = 0;
	char	*buf = 0;
	int64_t retval= 0;

	if(! proto_done(fd)){
		xcom_proto x_proto;
		x_msg_type x_type;
		unsigned int tag;
		retval = xcom_send_proto(fd, my_xcom_version, x_version_req, TAG_START);
		G_DEBUG("client sent negotiation request for protocol %d",my_xcom_version);
		if(retval < 0)
			goto end;
		retval = xcom_recv_proto(fd, &x_proto, &x_type, &tag);
		if(retval < 0)
			goto end;
		if(tag != TAG_START)
		{
			retval = -1;
			goto end;
		}
		if(x_type != x_version_reply)
		{
			retval = -1;
			goto end;
		}

		if(x_proto == x_unknown_proto){
			G_DEBUG("no common protocol, returning error");
			retval = -1;
			goto end;
		}
		G_DEBUG("client connection will use protocol version %d",x_proto);
		DBGOUT(STRLIT("client connection will use protocol version ");
			   NDBG(x_proto,u); STRLIT(xcom_proto_to_str(x_proto)));
		fd->x_proto = x_proto;
		set_connected(fd, CON_PROTO);
	}
	msg->a = a;
	msg->to = VOID_NODE_NO;
	msg->op = client_msg;
	msg->force_delivery = force;

	serialize_msg(msg, fd->x_proto, &buflen, &buf);
	if(buflen){
		retval = socket_write(fd, buf, buflen);
		if (buflen != retval) {
			DBGOUT(FN; STRLIT("write failed "); NDBG(fd->fd, d);
				   NDBG(buflen, d); NDBG(retval, d));
		}
		X_FREE(buf);
	}
end:
	msg->a = 0; /* Do not deallocate a */
	XCOM_XDR_FREE(xdr_pax_msg, msg);
	return retval;
}

int64_t	xcom_client_send_data(uint32_t size, char *data, connection_descriptor *fd)
{
	app_data a;
	int64_t retval = 0;
	init_app_data(&a);
	a.body.c_t = app_type;
	a.body.app_u_u.data.data_len = size;
	a.body.app_u_u.data.data_val = data;
	retval = xcom_send_client_app_data(fd, &a, 0);
	my_xdr_free((xdrproc_t) xdr_app_data, (char*)&a);
	return retval;
}

static pax_msg *	socket_read_msg(connection_descriptor *rfd, pax_msg *p)
/* Should buffer reads as well */
{
	int64_t	n;
	char	*bytes;
	unsigned char header_buf[MSG_HDR_SIZE];
	xcom_proto x_version;
	uint32_t	msgsize;
	x_msg_type x_type;
	unsigned int tag;
	int deserialize_ok = 0;

	bytes = NULL;

	/* Read version, length, type, and tag */
	n = socket_read_bytes(rfd, (char*)header_buf, MSG_HDR_SIZE);

	if (n <= 0) {
		DBGOUT(FN; NDBG(n, ll));
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

	get_header_1_0(header_buf, &msgsize, & x_type, &tag);

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
		DBGOUT(FN; NDBG(n, ll));
		return 0;
	}
	return(p);
}

int	xcom_close_client_connection(connection_descriptor *connection)
{
	int	retval = 0;

#ifdef XCOM_HAVE_OPENSSL
	if (connection->ssl_fd) {
		SSL_shutdown(connection->ssl_fd);
		ssl_free_con(connection);
	}
#endif
	retval =  xcom_shut_close_socket(&connection->fd).val;
	free(connection);
	return retval;
}

int	xcom_client_boot(connection_descriptor *fd, node_list *nl, uint32_t group_id)
{
	app_data a;
	int retval = 0;
	retval =  (int)xcom_send_client_app_data(fd, init_config_with_group(&a, nl, unified_boot_type, group_id), 0);
	my_xdr_free((xdrproc_t) xdr_app_data, (char*)&a);
	return retval;
}


int xcom_send_app_wait(connection_descriptor *fd, app_data *a, int force)
{
	int retval = 0;
	pax_msg p;
	pax_msg *rp = 0;

	for(;;){
		retval = (int)xcom_send_client_app_data(fd, a, force);
		if(retval < 0)
			return 0;
		memset(&p, 0, sizeof(p));
		rp = socket_read_msg(fd, &p);
		if(rp){
			client_reply_code cli_err = rp->cli_err;
			my_xdr_free((xdrproc_t)xdr_pax_msg, (char*)&p);
			switch(cli_err){
				case REQUEST_OK:
					return 1;
				case REQUEST_FAIL:
                                        G_MESSAGE("cli_err %d",cli_err);
					return 0;
				case REQUEST_RETRY:
			                G_MESSAGE("cli_err %d",cli_err);
					xcom_sleep(1);
					break;
				default:
					G_WARNING("client protocol botched");
					return 0;
			}
		}else{
			G_WARNING("read failed");
			return 0;
		}
	}
}

int xcom_send_cfg_wait(connection_descriptor * fd, node_list *nl,
                       uint32_t group_id, cargo_type ct, int force)
{
	app_data a;
	int retval = 0;
	DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(nl)););
	retval = xcom_send_app_wait(fd, init_config_with_group(&a, nl, ct, group_id), force);
	my_xdr_free((xdrproc_t) xdr_app_data, (char*)&a);
	return retval;
}

int	xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                         uint32_t group_id)
{
	return xcom_send_cfg_wait(fd, nl, group_id, add_node_type, 0);
}

int	xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                            uint32_t group_id)
{
	return xcom_send_cfg_wait(fd, nl, group_id, remove_node_type, 0);
}

#ifdef NOTDEF
/* Not completely implemented, need to be handled properly
   when received as a client message in dispatch_op.
   Should have separate opcode from normal add/remove,
   like force config_type */
int xcom_client_force_add_node(connection_descriptor *, node_list *nl,
                               uint32_t group_id)
{
	return xcom_send_cfg_wait(fd, nl, group_id, add_node_type, 1);
}

int xcom_client_force_remove_node(connection_descriptor *, node_list *nl,
                                  uint32_t group_id)
{
	return xcom_send_cfg_wait(fd, nl, group_id, remove_node_type, 1);
}
#endif

int xcom_client_force_config(connection_descriptor *fd, node_list *nl,
                             uint32_t group_id)
{
	return xcom_send_cfg_wait(fd, nl, group_id, force_config_type, 1);
}

int	xcom_client_enable_arbitrator(connection_descriptor *fd)
{
	app_data a;
	int retval = 0;
	init_app_data(&a);
	a.body.c_t = enable_arbitrator;
	retval = xcom_send_app_wait(fd, &a, 0);
	my_xdr_free((xdrproc_t) xdr_app_data, (char*)&a);
	return retval;
}


int	xcom_client_disable_arbitrator(connection_descriptor *fd)
{
	app_data a;
	int retval = 0;
	init_app_data(&a);
	a.body.c_t = disable_arbitrator;
	retval = xcom_send_app_wait(fd, &a, 0);
	my_xdr_free((xdrproc_t) xdr_app_data, (char*)&a);
	return retval;
}

int	xcom_client_terminate_and_exit(connection_descriptor *fd)
{
	app_data a;
	int retval = 0;
	init_app_data(&a);
	a.body.c_t = x_terminate_and_exit;
	retval = xcom_send_app_wait(fd, &a, 0);
	my_xdr_free((xdrproc_t) xdr_app_data, (char*)&a);
	return retval;
}




