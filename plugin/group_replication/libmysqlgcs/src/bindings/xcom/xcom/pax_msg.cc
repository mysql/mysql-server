/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
#ifdef _MSC_VER
#include <stdint.h>
#endif
#include <rpc/rpc.h>
#include <stdlib.h>

#include "xcom/app_data.h"
#include "xcom/bitset.h"
#include "xcom/node_no.h"
#include "xcom/pax_msg.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_def.h"
#include "xcom/site_struct.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_vp_str.h"
#include "xdr_gen/xcom_vp.h"

/* Paxos messages */

/* Initialize a message */
static pax_msg *init_pax_msg(pax_msg *p, int refcnt, synode_no synode,
                             site_def const *site) {
  node_no nodeno = VOID_NODE_NO;
  if (site) nodeno = get_nodeno(site);
  p->refcnt = refcnt;
  p->group_id = 0;
  p->max_synode = null_synode;
  p->from = nodeno;
  p->to = VOID_NODE_NO;
  p->op = initial_op;
  init_ballot(&p->reply_to, 0, nodeno);
  /*
   -1 ensures ballot (-1,nodeno) is less than any ballot used by any proposer.
   Leader will use reserved ballot (0,_) for its initial 2-phase Paxos round.
   Remaining rounds will use ballot (1+,_) and the vanilla 3-phase Paxos.
   */
  init_ballot(&p->proposal, -1, nodeno);
  p->synode = synode;
  p->msg_type = normal;
  p->receivers = nullptr;
  p->a = nullptr;
  p->force_delivery = 0;
  p->event_horizon = EVENT_HORIZON_MIN;
  return p;
}

pax_msg *pax_msg_new(synode_no synode, site_def const *site) {
  pax_msg *p = (pax_msg *)xcom_calloc((size_t)1, sizeof(pax_msg));
  IFDBG(D_NONE, FN; PTREXP(p));
  return init_pax_msg(p, 0, synode, site);
}

pax_msg *pax_msg_new_0(synode_no synode) {
  pax_msg *p = (pax_msg *)xcom_calloc((size_t)1, sizeof(pax_msg));
  IFDBG(D_NONE, FN; PTREXP(p));
  return init_pax_msg(p, 0, synode, nullptr);
}

pax_msg *clone_pax_msg_no_app(pax_msg *msg) {
  pax_msg *p = (pax_msg *)xcom_calloc((size_t)1, sizeof(pax_msg));
  IFDBG(D_NONE, FN; STRLIT("clone_pax_msg"); PTREXP(p));
  *p = *msg;
  p->refcnt = 0;
  p->receivers = clone_bit_set(msg->receivers);
  p->a = nullptr; /* Or copy_app_data will be confused */
  p->snap = nullptr;
  p->gcs_snap = nullptr;
  return p;
}

pax_msg *clone_pax_msg(pax_msg *msg) {
  pax_msg *p = clone_pax_msg_no_app(msg);
  /*
    Need to increase the refcnt so that the msg is deleted
    in safe_app_data_copy if there is a failure.
   */
  p->refcnt = 1;
  safe_app_data_copy(&p, msg->a);
  if (p) p->refcnt = 0;
  return p;
}

void delete_pax_msg(pax_msg *p) {
  IFDBG(D_NONE, FN; STRLIT("delete_pax_msg"); PTREXP(p));
  XCOM_XDR_FREE(xdr_pax_msg, p);
}

int ref_msg(pax_msg *p) {
  if (p->refcnt < 0) return p->refcnt;
  p->refcnt++;
  return p->refcnt;
}

int unref_msg(pax_msg **pp) {
  pax_msg *p = *pp;
  if (!p) return 0;
  if (p->refcnt < 0) return p->refcnt;
  p->refcnt--;
  if (p->refcnt == 0) {
    delete_pax_msg(p);
    return 0;
  }
  *pp = nullptr;
  return p->refcnt;
}

void unchecked_replace_pax_msg(pax_msg **target, pax_msg *p) {
  if (p) {
    ref_msg(p);
  }
  if (*target) {
    unref_msg(target);
  }
  *target = p;
}

/* purecov: begin deadcode */
#if TASK_DBUG_ON
/* Debug a message */
char *dbg_pax_msg(pax_msg const *p) {
  GET_NEW_GOUT;
  if (!p) {
    STRLIT("p == 0 ");
    RET_GOUT;
  }
  STRLIT("pax_msg");
  PTREXP(p);
  NDBG(p->force_delivery, d);
  NDBG(p->group_id, u);
  SYCEXP(p->max_synode);
  NDBG(p->from, d);
  NDBG(p->to, d);
  STREXP(pax_op_to_str(p->op));
  BALCEXP(p->reply_to);
  BALCEXP(p->proposal);
  SYCEXP(p->synode);
  STREXP(pax_msg_type_to_str(p->msg_type));
  STRLIT("receivers ");
  COPY_AND_FREE_GOUT(
      dbg_bitset(p->receivers, get_maxnodes(find_site_def(p->synode))));
  RET_GOUT;
}
#endif
/* purecov: end */

/* Ballot definition */

/* Initialize a ballot */
ballot *init_ballot(ballot *bal, int cnt, node_no node) {
  bal->cnt = cnt;
  bal->node = node;
  return bal;
}

int eq_ballot(ballot x, ballot y) { return x.cnt == y.cnt && x.node == y.node; }

int gt_ballot(ballot x, ballot y) {
  return x.cnt > y.cnt || (x.cnt == y.cnt && x.node > y.node);
}

#ifdef TASK_EVENT_TRACE
/* purecov: begin inspected */
void add_ballot_event(ballot const bal) {
  add_event(0, string_arg("{"));
  add_event(EVENT_DUMP_PAD, int_arg(bal.cnt));
  add_event(0, uint_arg(bal.node));
  add_event(EVENT_DUMP_PAD, string_arg("}"));
}
/* purecov: end */
#endif
