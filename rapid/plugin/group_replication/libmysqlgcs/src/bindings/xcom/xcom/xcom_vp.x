%/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved. 
%
%   This program is free software; you can redistribute it and/or modify
%   it under the terms of the GNU General Public License, version 2.0,
%   as published by the Free Software Foundation.
%
%   This program is also distributed with certain software (including
%   but not limited to OpenSSL) that is licensed under separate terms,
%   as designated in a particular file or component or in included license
%   documentation.  The authors of MySQL hereby grant you an additional
%   permission to link the program and your derivative works with the
%   separately licensed software that they have included with MySQL.
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


%#include "xcom_vp_platform.h"

#include "xcom_proto_enum.h"
#include "xcom_limits.h"

struct x_proto_range {
	xcom_proto min_proto;
	xcom_proto max_proto;
};

  enum delivery_status {
    delivery_ok,
    delivery_failure
  };

/* Consensus type */
enum cons_type {
  cons_majority,          /* Plain majority */
  cons_all,               /* Everyone must agree */
  cons_none              /* NOT USED */
};


/* VP Number will wrap in 5.8E5 years if we run at 1000000 VP builds per second */
/* Change to circular hyper int if this is not desirable */

typedef uint32_t node_no;

struct synode_no {
  uint32_t group_id; /* The group this synode belongs to */
  uint64_t msgno; /* Monotonically increasing number */
  node_no node;         /* Node number */
};

struct trans_id{
  synode_no cfg;
  uint32_t pc;
};

typedef bool node_set<NSERVERS>;

struct blob {
	opaque data<MAXBLOB>;
};

struct node_address_1_0 {
	string address<MAXNAME>;
	blob  uuid;
};
typedef node_address_1_0 node_list_1_0<NSERVERS>;


struct node_address {
	string address<MAXNAME>;
	blob  uuid;
	x_proto_range proto; /* Supported protocols */
};
typedef node_address node_list<NSERVERS>;

%/*
%	Custom xdr functions to coerce rpcgen into handling different protocol versions.
%	Protocol version is passed in an extended XDR object.
%*/
%
%#ifndef VERSION_CONTROL
%#define VERSION_CONTROL
%
%extern xcom_proto const my_min_xcom_version; /* The minimum protocol version I am able to understand */
%extern xcom_proto const my_xcom_version; /* The maximum protocol version I am able to understand */
%
%typedef node_list node_list_1_1; /* Alias for real type to avoid generating xdr call to xdr_node_list */
%
%extern  bool_t xdr_node_list_1_1 (XDR *, node_list_1_1*);
%
%#endif

%#ifndef CHECKED_DATA
%#define CHECKED_DATA
%typedef struct {
%	u_int data_len;
%	char *data_val;
%} checked_data;
%extern  bool_t xdr_checked_data (XDR *, checked_data*);
%#endif

enum cargo_type {
  unified_boot_type,
  xcom_boot_type,
  xcom_set_group,
  xcom_recover,
  app_type,
  query_type,
  query_next_log,
  exit_type,
  reset_type,
  begin_trans,
  prepared_trans,
  abort_trans,
  view_msg,
  remove_reset_type,
  add_node_type,
  remove_node_type,
  enable_arbitrator,
  disable_arbitrator,
  force_config_type,
  x_terminate_and_exit,
  set_cache_limit
};

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

/* Application-specific data */
union app_u switch(cargo_type c_t){
 case unified_boot_type:
 case add_node_type:
 case remove_node_type:
 case force_config_type:
 case xcom_boot_type:
 case xcom_set_group:
   node_list_1_1 nodes;
 case xcom_recover:
   repository rep;
 case app_type:
   checked_data data;
 case query_type:
   void;
 case query_next_log:
   void;
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
 default:
   void;
};

enum recover_action {
  rec_block,
  rec_delay,
  rec_send
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

/* A portable bit set */

typedef uint32_t bit_mask;
%
%#define	BITS_PER_BYTE 8
%#define	MASK_BITS	((uint32_t)(sizeof (bit_mask) * BITS_PER_BYTE))	/* bits per mask */
%#define	howmany_words(x, y)	(((x)+((y)-1))/(y))
%

struct bit_set {
  bit_mask bits<NSERVERS>;
};

%#define BIT_OP(__n, __p, __op, __inv) ((__p)->bits.bits_val[(__n)/MASK_BITS] __op  __inv (1u << ((__n) % MASK_BITS)))
%#define BIT_XOR(__n, __p) BIT_OP(__n, __p, ^=, )
%#define BIT_SET(__n, __p) BIT_OP(__n, __p, |=, )
%#define BIT_CLR(__n, __p) BIT_OP(__n, __p, &=, ~)
%#define BIT_ISSET(__n, __p) (BIT_OP(__n, __p, &, ) != 0ul)
%#define BIT_ZERO(__p) memset((__p)->bits.bits_val, 0, (__p)->bits.bits_len * sizeof(*(__p)->bits.bits_val))

%extern bit_set *new_bit_set(uint32_t bits);
%extern bit_set *clone_bit_set(bit_set *orig);
%extern void free_bit_set(bit_set *bs);

/* Ballot defined by count and node number */
struct ballot{
  int32_t cnt;
  node_no node;
};

enum pax_op {
  client_msg,
  initial_op,
  prepare_op,
  ack_prepare_op,
  ack_prepare_empty_op,
  accept_op,
  ack_accept_op,
  learn_op,
  recover_learn_op,
  multi_prepare_op,
  multi_ack_prepare_empty_op,
  multi_accept_op,
  multi_ack_accept_op,
  multi_learn_op,
  skip_op,
  i_am_alive_op,
  are_you_alive_op,
  need_boot_op,
  snapshot_op,
  die_op,
  read_op,
  gcs_snapshot_op,
  xcom_client_reply,
  tiny_learn_op,
  LAST_OP
};

enum pax_msg_type {
  normal,
  no_op,
  multi_no_op
};

enum start_t {
     IDLE,
     BOOT,
     RECOVER
};

struct snapshot{
  synode_no vers;
  app_data_ptr_array snap;
  uncommitted_list u_list;
};

struct config{
	synode_no start; 	/* Config is active from this message number */
	synode_no boot_key; /* The message number of the original unified_boot */
	node_list_1_1 nodes;	/* Set of nodes in this config */
};

typedef config *config_ptr;
typedef config_ptr configs<NSERVERS>;

struct gcs_snapshot{
  synode_no log_start;
  configs cfg;
  blob app_snap;
};

enum client_reply_code {
     REQUEST_OK,
     REQUEST_FAIL,
     REQUEST_RETRY
};

struct pax_msg_1_1{
  node_no to;             /* To node */
  node_no from;           /* From node */
  uint32_t group_id; /* Unique ID shared by our group */
  synode_no max_synode; /* Gossip about the max real synode */
  start_t start_type; /* Boot or recovery? */
  ballot reply_to;    /* Reply to which ballot */
  ballot proposal;    /* Proposal number */
  pax_op op;          /* Opcode: prepare, propose, learn, etc */
  synode_no synode;   /* The message number */
  pax_msg_type msg_type; /* normal, noop, or multi_noop */
  bit_set *receivers;
  /* synode_no unique_id;  */   /* Local, unique ID used to see which message was sent */
  app_data *a;      /* Payload */
  snapshot *snap;	/* Snapshot if op == snapshot_op */
  gcs_snapshot *gcs_snap; /* gcs_snapshot if op == gcs_snapshot_op */
  client_reply_code cli_err;
  bool force_delivery; /* Deliver this message even if we do not have majority */
  int32_t refcnt;
 };


struct pax_msg_1_2{
  node_no to;             /* To node */
  node_no from;           /* From node */
  uint32_t group_id; /* Unique ID shared by our group */
  synode_no max_synode; /* Gossip about the max real synode */
  start_t start_type; /* Boot or recovery? */
  ballot reply_to;    /* Reply to which ballot */
  ballot proposal;    /* Proposal number */
  pax_op op;          /* Opcode: prepare, propose, learn, etc */
  synode_no synode;   /* The message number */
  pax_msg_type msg_type; /* normal, noop, or multi_noop */
  bit_set *receivers;
  /* synode_no unique_id;  */   /* Local, unique ID used to see which message was sent */
  app_data *a;      /* Payload */
  snapshot *snap;	/* Snapshot if op == snapshot_op */
  gcs_snapshot *gcs_snap; /* gcs_snapshot if op == gcs_snapshot_op */
  client_reply_code cli_err;
  bool force_delivery; /* Deliver this message even if we do not have majority */
  int32_t refcnt;
  synode_no delivered_msg; /* Gossip about the last delivered message */
 };

%#ifndef PAX_MSG_TYPEDEF
%#define PAX_MSG_TYPEDEF
%typedef pax_msg_1_2 pax_msg;
%extern  bool_t xdr_pax_msg (XDR *, pax_msg*);
%#endif

