%/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.
%
%   This program is free software; you can redistribute it and/or modify
%   it under the terms of the GNU General Public License, version 2.0,
%   as published by the Free Software Foundation.
%
%   This program is designed to work with certain software (including
%   but not limited to OpenSSL) that is licensed under separate terms,
%   as designated in a particular file or component or in included license
%   documentation.  The authors of MySQL hereby grant you an additional
%   permission to link the program and your derivative works with the
%   separately licensed software that they have either included with
%   the program or referenced in the documentation.
%
%   This program is distributed in the hope that it will be useful,
%   but WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%   GNU General Public License, version 2.0, for more details.
%
%   You should have received a copy of the GNU General Public License
%   along with this program; if not, write to the Free Software
%   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
%


%#include "xcom/xcom_vp_platform.h"

%#include "xcom/xcom_limits.h"
%#include "xcom/xcom_profile.h"
#ifdef RPC_XDR
%extern synode_no const null_synode;
%extern synode_no get_delivered_msg();
%#ifndef _WIN32
%#include <strings.h> /* For bzero */
%#endif
#endif

/*
The xcom protocol version numbers.

Zero is not used, so a zero protocol version indicates an error.
To add a new protocol version, add a new value to this enum.
To change an existing struct, add the new member with an #ifdef
guard corresponding to the protocol version.
For example, to add a member corresponding to protocol version
x_1_7, the definition would look like this:

#if (XCOM_PROTO_VERS > 107)
	new_member_t new_member;
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%	 new_member = suitable_default_value;
%  }
%END
#endif
#endif

In this example, 107 corresponds to x_1_7.
The code in the BEGIN..END block will be inserted immediately before the
final return in the generated xdr function. Members which are not in
earlier protocol versions are not serialized, since they are excluded
by the #if guard. When deserializing, the code in the BEGIN..END block
takes care of insering a suitable value instead of actually reading
the value from the serialized struct, since the earlier protocol
version does not contain the new member.

After adding a new protocol version, set MY_XCOM_PROTO to this version in xcom_transport.cc (xcom_transport.cc:/MY_XCOM_PROTO)
In addition, the xdr_pax_msg, in this case xdr_pax_msg_1_7 must be added to the dispatch table pax_msg_func in xcom_transport.cc (xcom_transport.cc:/pax_msg_func)

For conversion of the new enum value to a string, add an entry in xcom_proto_to_str (xcom_vp_str.cc:/xcom_proto_to_str)

To actually generate the xdr functions for the new protocol version, see comments in rpcgen.cmake
*/

enum xcom_proto {
  x_unknown_proto = 0,
  x_1_0 = 1,
  x_1_1 = 2,
  x_1_2 = 3,
  x_1_3 = 4,
  x_1_4 = 5,
  x_1_5 = 6,
  x_1_6 = 7,
  x_1_7 = 8,
  x_1_8 = 9,
  x_1_9 = 10
};

enum delivery_status {
  delivery_ok = 0,
  delivery_failure = 1
};

/* Consensus type */
enum cons_type {
  cons_majority = 0          /* Plain majority */,
  cons_all = 1               /* Everyone must agree */
/*   cons_none = 2 */             /* NOT USED */
};

enum cargo_type {
  unified_boot_type = 0,
  xcom_boot_type = 1,
  xcom_set_group = 2,
/*   xcom_recover = 3, */
  app_type = 4,
/*   query_type = 5, */
/*   query_next_log = 6, */
  exit_type = 7,
  reset_type = 8,
  begin_trans = 9,
  prepared_trans = 10,
  abort_trans = 11,
  view_msg = 12,
  remove_reset_type = 13,
  add_node_type = 14,
  remove_node_type = 15,
  enable_arbitrator = 16,
  disable_arbitrator = 17,
  force_config_type = 18,
  x_terminate_and_exit = 19,
  set_cache_limit = 20,
  get_event_horizon_type = 21,
  set_event_horizon_type = 22,
  get_synode_app_data_type = 23,
  convert_into_local_server_type = 24,
  set_max_leaders = 25,
  set_leaders_type = 26,
  get_leaders_type = 27
};

enum recover_action {
  rec_block = 0,
  rec_delay = 1,
  rec_send = 2
};

enum pax_op {
  client_msg = 0,
  initial_op = 1,
  prepare_op = 2,
  ack_prepare_op = 3,
  ack_prepare_empty_op = 4,
  accept_op = 5,
  ack_accept_op = 6,
  learn_op = 7,
  recover_learn_op = 8,
  multi_prepare_op = 9,
  multi_ack_prepare_empty_op = 10,
  multi_accept_op = 11,
  multi_ack_accept_op = 12,
  multi_learn_op = 13,
  skip_op = 14,
  i_am_alive_op = 15,
  are_you_alive_op = 16,
  need_boot_op = 17,
  snapshot_op = 18,
  die_op = 19,
  read_op = 20,
  gcs_snapshot_op = 21,
  xcom_client_reply = 22,
  tiny_learn_op = 23,
  synode_request = 24,
  synode_allocated = 25,
  LAST_OP
};

enum pax_msg_type {
  normal = 0,
  no_op = 1,
  multi_no_op = 2
};

enum client_reply_code {
  REQUEST_OK = 0,      /* Everything OK */
  REQUEST_FAIL = 1,    /* Definitely a failure */
  REQUEST_RETRY = 2,   /* Retry */
  REQUEST_REDIRECT = 3 /* Try another xcom node */
};

enum start_t {
     IDLE = 0,
     BOOT = 1,
     RECOVER = 2
};

typedef uint32_t xcom_event_horizon;

typedef uint32_t node_no;

typedef bool node_set<NSERVERS>;

/* A portable bit set */

typedef uint32_t bit_mask;

struct bit_set {
  bit_mask bits<NSERVERS>;
};

#ifdef RPC_HDR
%#define	BITS_PER_BYTE 8
%#define	MASK_BITS	((bit_mask)(sizeof (bit_mask) * BITS_PER_BYTE))	/* bits per mask */
%#define	howmany_words(x, y)	(((x)+((y)-1))/(y))
%

%#define BIT_OP(__n, __p, __op, __inv) ((__p)->bits.bits_val[(__n)/MASK_BITS] __op  __inv (1u << ((__n) % MASK_BITS)))
%#define BIT_XOR(__n, __p) BIT_OP(__n, __p, ^=,(bit_mask))
%#define BIT_SET(__n, __p) BIT_OP(__n, __p, |=,(bit_mask))
%#define BIT_CLR(__n, __p) BIT_OP(__n, __p, &=,(bit_mask) ~)
%#define BIT_ISSET(__n, __p) (BIT_OP(__n, __p, &,(bit_mask)) != 0ul)
%#define BIT_ZERO(__p) memset((__p)->bits.bits_val, 0, (__p)->bits.bits_len * sizeof(*(__p)->bits.bits_val))

%extern bit_set *new_bit_set(uint32_t bits);
%extern bit_set *clone_bit_set(bit_set *orig);
%extern void free_bit_set(bit_set *bs);

%#ifndef CHECKED_DATA
%#define CHECKED_DATA
%typedef struct {
%	u_int data_len;
%	char *data_val;
%} checked_data;
%extern  bool_t xdr_checked_data (XDR *, checked_data*);
%#endif

#endif

struct blob {
	opaque data<MAXBLOB>;
};

struct x_proto_range {
	xcom_proto min_proto;
	xcom_proto max_proto;
};

/* Message number will wrap in 5.8E5 years if we run at 1000000 messages per second */
/* Change to circular hyper int if this is not desirable */

struct synode_no {
  uint32_t group_id; /* The group this synode belongs to */
  uint64_t msgno; /* Monotonically increasing number */
  node_no node;         /* Node number */
};

struct trans_id{
  synode_no cfg;
  uint32_t pc;
};

enum paxos_role { P_PROP = 1, P_ACC = 2, P_LEARN = 4 };

struct node_address{
	string address<MAXNAME>;
	blob  uuid;
#if (XCOM_PROTO_VERS > 100)
	x_proto_range proto; /* Supported protocols */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%	 objp->proto.min_proto = x_1_0;
%	 objp->proto.max_proto = x_1_0;
%  }
%END
#endif
#endif
#if (XCOM_PROTO_VERS > 108)
	uint32_t services; /* Services provided by this node */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%	 objp->services = P_PROP | P_ACC | P_LEARN;
%  }
%END
#endif
#endif
};

typedef node_address node_list<NSERVERS>;

typedef node_no node_no_array<NSERVERS>;
typedef synode_no synode_no_array<MAX_SYNODE_ARRAY>;

struct uncommitted_list{
  uint32_t active;
  synode_no_array vers;
};

struct repository {
  synode_no vers;
  synode_no_array msg_list;
  uncommitted_list u_list;
};

struct x_error
{
  int32_t nodeid;
  int32_t code;
  string message<MAXERROR>;
};

struct trans_data{
  trans_id tid;
  int32_t pc;
  string cluster_name<MAXNAME>;
  x_error errmsg;
};

struct leader{
	string address<MAXNAME>;
};

typedef leader leader_array<NSERVERS>;

/* Application-specific data */
union app_u switch(cargo_type c_t){
 case unified_boot_type:
 case add_node_type:
 case remove_node_type:
 case force_config_type:
 case xcom_boot_type:
 case xcom_set_group:
   node_list nodes;
 case app_type:
   checked_data data;
 case exit_type:
 case reset_type:
   void;
 case remove_reset_type:
   void;
 case begin_trans:
   void;
 case prepared_trans:
 case abort_trans:
   trans_data td;
 case view_msg:
   node_set present;
 case set_cache_limit:
   uint64_t cache_limit;
 case get_event_horizon_type:
   void;
 case set_event_horizon_type:
   xcom_event_horizon event_horizon;
 case get_synode_app_data_type:
   synode_no_array synodes;
 case convert_into_local_server_type:
   void;
 case set_max_leaders:
   node_no max_leaders;
 case set_leaders_type:
   leader_array leaders;
 default:
   void;
};

struct app_data{
  synode_no unique_id; /* Unique id of message */
  uint32_t group_id; /* Unique ID shared by our group */
  uint64_t lsn; /* Local sequence number */
  synode_no app_key;   /* Typically message number/log sequence number, but could be object ID  */
  cons_type consensus; /* Type of consensus needed for delivery of this message */
  double expiry_time; /* How long to wait before delivery fails */
  bool notused; /* not used  */
  bool log_it; /* Put this message in the log */
  bool chosen; /* Finished phase 3, may be executed */
  recover_action recover; /* Sent as part of recovery */
  app_u body;
  app_data *next; /* Link to next in list */
};

typedef app_data *app_data_ptr;
typedef app_data_ptr app_data_ptr_array<MAX_APP_PTR_ARRAY>;
typedef app_data_ptr *app_data_list;

struct key_range{
    synode_no k1;
    synode_no k2;
};

/* Ballot defined by count and node number */
struct ballot{
  int32_t cnt;
  node_no node;
};

struct snapshot{
  synode_no vers;
  app_data_ptr_array snap;
  uncommitted_list u_list;
};

struct config{
	synode_no start; 	/* Config is active from this message number */
	synode_no boot_key; /* The message number of the original unified_boot */
	node_list nodes;	/* Set of nodes in this config */
#if (XCOM_PROTO_VERS == 103) || (XCOM_PROTO_VERS > 106)
	node_set global_node_set; /* The global node set for this site */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%    objp->global_node_set.node_set_len = 0;
%    objp->global_node_set.node_set_val = 0;
%  }
%END
#endif
#endif
#if (XCOM_PROTO_VERS > 103)
	xcom_event_horizon event_horizon;
#else
#ifdef RPC_XDR
%BEGIN
%      if (xdrs->x_op == XDR_DECODE) {
%        objp->event_horizon = EVENT_HORIZON_MIN;
%      }
%END
#endif
#endif
#if (XCOM_PROTO_VERS > 108)
    node_no max_active_leaders; /* How many leaders can there be? >= 1 and <= number of nodes */
    leader_array leaders;
#else
#ifdef RPC_XDR
%BEGIN
%      if (xdrs->x_op == XDR_DECODE) {
%        objp->max_active_leaders = 0;  /* Set active leaders to all as default */
%        synthesize_leaders(&objp->leaders); /* Install all nodes as leaders */
%      }
%END
#endif
#endif
};

typedef config *config_ptr;
typedef config_ptr configs<MAX_SITE_DEFS>;

struct gcs_snapshot{
  synode_no log_start;
#if (XCOM_PROTO_VERS == 103) || (XCOM_PROTO_VERS > 106)
  synode_no log_end;
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%    objp->log_end = null_synode;
%  }
%END
#endif
#endif
  configs cfg;
  blob app_snap;
};

struct synode_app_data {
   synode_no synode;
   checked_data data;
#if (XCOM_PROTO_VERS > 108)
	 synode_no origin; /* node that actually sent the message. */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%	   objp->origin = objp->synode;
%  }
%END
#endif
#endif
};
typedef synode_app_data synode_app_data_array<MAX_SYNODE_ARRAY>;

/*
  protocol x_1_2 and x_1_3 differ only in the definition of gcs_snapshot,
  which is taken care of by xdr_gcs_snapshot
*/

/*
  pax_msg_1_5 is identical to pax_msg_1_4,
  but nodes running protocol version 1_5 or greater support IPv6.
  xdr_pax_msg for protocol x_1_6 and greater must grok the incompatible
  gcs_snapshot and config versions of x_1_3 and x_1_4.
*/

enum reply_type {
  leader_info
};

struct leader_info_data {
  node_no max_nr_leaders;
  leader_array preferred_leaders;
  leader_array actual_leaders;
};

union reply_data switch(reply_type rt){
 case leader_info:
   leader_info_data leaders;
 default:
   void;
};


struct pax_msg{
  node_no to;             /* To node */
  node_no from;           /* From node */
  uint32_t group_id; /* Unique ID shared by our group */
  synode_no max_synode; /* Gossip about the max real synode */
  start_t start_type; /* Boot or recovery? DEPRECATED */
  ballot reply_to;    /* Reply to which ballot */
  ballot proposal;    /* Proposal number */
  pax_op op;          /* Opcode: prepare, propose, learn, etc */
  synode_no synode;   /* The message number */
  pax_msg_type msg_type; /* normal, noop, or multi_noop */
  bit_set *receivers;
  app_data *a;      /* Payload */
  snapshot *snap;	/* Not used */
  gcs_snapshot *gcs_snap; /* gcs_snapshot if op == gcs_snapshot_op */
  client_reply_code cli_err;
  bool force_delivery; /* Deliver this message even if we do not have majority */
  int32_t refcnt;
#if (XCOM_PROTO_VERS > 101)
  synode_no delivered_msg; /* Gossip about the last delivered message */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%    objp->delivered_msg = get_delivered_msg(); /* Use our own minimum */
%  }
%END
#endif
#endif
#if (XCOM_PROTO_VERS > 103)
  xcom_event_horizon event_horizon; /* Group's event horizon */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%    objp->event_horizon = 0;
%  }
%END
#endif
#endif
#if (XCOM_PROTO_VERS > 105)
  synode_app_data_array requested_synode_app_data; /* The decided data for the requested synodes */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%        objp->requested_synode_app_data.synode_app_data_array_len = 0;
%        objp->requested_synode_app_data.synode_app_data_array_val = NULL;
%  }
%END
#endif
#endif
#if (XCOM_PROTO_VERS > 108)
  reply_data *rd; /* Reply from xcom to client */
#else
#ifdef RPC_XDR
%BEGIN
%  if (xdrs->x_op == XDR_DECODE) {
%        objp->rd = NULL;
%  }
%END
#endif
#endif
};

#ifdef RPC_HDR
/* xdr functions for old protocol versions, must match enum xcom_proto */
%bool_t xdr_pax_msg_1_0(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_1(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_2(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_3(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_4(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_5(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_6(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_7(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_8(XDR *, pax_msg *);
%bool_t xdr_pax_msg_1_9(XDR *, pax_msg *);

/* Extern function for default initialization of leaders */
%#ifdef __cplusplus
%extern "C" void synthesize_leaders(leader_array *leaders);
%extern "C" synode_no get_delivered_msg();
%#else
%extern void synthesize_leaders(leader_array *leaders);
%extern synode_no get_delivered_msg();
%#endif
#endif

