/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <rpc/rpc.h>
#include <assert.h>
#include <stdlib.h>

#include "xcom_common.h"
#include "simset.h"
#include "xcom_vp.h"
#include "task.h"
#include "task_debug.h"
#include "node_no.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "xcom_base.h"
#include "xcom_transport.h"
#include "pax_msg.h"
#include "app_data.h"
#include "xcom_memory.h"
#include "xcom_vp_str.h"
#include "synode_no.h"
#include "site_def.h"
#include "bitset.h"

/* {{{ Paxos messages */

/* Initialize a message */
static pax_msg *init_pax_msg(pax_msg *p, int refcnt, synode_no synode, site_def const * site)
{
	node_no nodeno = VOID_NODE_NO;
	if(site)
		nodeno = get_nodeno(site);
	p->refcnt = refcnt;
	p->group_id = 0;
	p->max_synode = null_synode;
	p->start_type = IDLE;
	p->from = nodeno;
	p->to = VOID_NODE_NO;
	p->op = initial_op;
	init_ballot(&p->reply_to, 0, nodeno);
	/*
	 -1 ensures ballot (-1,nodeno) is less than any ballot used by any
	 proposer.
	 Leader will use reserved ballot (0,_) for its initial 2-phase Paxos
	 round.
	 Remaining rounds will use ballot (1+,_) and the vanilla 3-phase Paxos.
	 */
	init_ballot(&p->proposal, -1, nodeno);
	p->synode = synode;
	p->msg_type = normal;
	p->receivers = NULL;
	p->a = NULL;
	p->force_delivery = 0;
	return p;
}


pax_msg *pax_msg_new(synode_no synode, site_def const * site)
{
	pax_msg * p = calloc(1, sizeof(pax_msg));
	MAY_DBG(FN; PTREXP(p));
	return init_pax_msg(p, 0, synode, site);
}


pax_msg *pax_msg_new_0(synode_no synode)
{
	pax_msg * p = calloc(1, sizeof(pax_msg));
	MAY_DBG(FN; PTREXP(p));
	return init_pax_msg(p, 0, synode, 0);
}

pax_msg *clone_pax_msg_no_app(pax_msg *msg)
{
	pax_msg * p = calloc(1, sizeof(pax_msg));
	MAY_DBG(FN; STRLIT("clone_pax_msg"); PTREXP(p));
	*p = *msg;
	p->refcnt = 0;
	p->receivers = clone_bit_set(msg->receivers);
	p->a = NULL; /* Or copy_app_data will be confused */
	p->snap = NULL;
	p->gcs_snap = NULL;
	return p;
}

pax_msg *clone_pax_msg(pax_msg *msg)
{
	pax_msg * p = clone_pax_msg_no_app(msg);
	copy_app_data(&p->a, msg->a);
	return p;
}


void delete_pax_msg(pax_msg *p)
{
	MAY_DBG(FN; STRLIT("delete_pax_msg"); PTREXP(p));
	XCOM_XDR_FREE(xdr_pax_msg, p);
}


int	ref_msg(pax_msg *p)
{
	if (p->refcnt <0)
		return p->refcnt;
	p->refcnt++;
	return p->refcnt;
}


int	unref_msg(pax_msg **pp)
{
	pax_msg * p = *pp;
	if (!p)
		return 0;
	if (p->refcnt <0)
		return p->refcnt;
	p->refcnt--;
	if (p->refcnt == 0) {
		delete_pax_msg(p);
		return 0;
	}
	*pp = NULL;
	return p->refcnt;
}


void unchecked_replace_pax_msg(pax_msg **target, pax_msg *p)
{
	if (p) {
		ref_msg(p);
	}
	if (*target) {
		unref_msg(target);
	}
	*target = p;
}

#if 0
void replace_pax_msg(pax_msg **target, pax_msg *p)
{
	PAX_MSG_SANITY_CHECK(p);
	unchecked_replace_pax_msg(target, p);
}
#endif
/* purecov: begin deadcode */
/* Debug a message */
char *dbg_pax_msg(pax_msg const *p)
{
	GET_NEW_GOUT;
	if (!p) {
		STRLIT("p == 0 ");
		RET_GOUT;
	}
	STRLIT("pax_msg");
	PTREXP(p);
	NDBG(p->force_delivery,d);
	NDBG(p->group_id,u);
	SYCEXP(p->max_synode);
	STREXP(start_t_to_str(p->start_type));
	NDBG(p->from, d);
	NDBG(p->to, d);
	STREXP(pax_op_to_str(p->op));
	BALCEXP(p->reply_to);
	BALCEXP(p->proposal);
	SYCEXP(p->synode);
	STREXP(pax_msg_type_to_str(p->msg_type));
	STRLIT("receivers ");
	COPY_AND_FREE_GOUT(dbg_bitset(p->receivers, get_maxnodes(find_site_def(p->synode))));
	RET_GOUT;
}
/* purecov: end */
/* }}} */

/* {{{ Ballot definition */

/* Initialize a ballot */
ballot *init_ballot(ballot *bal, int cnt, node_no node)
{
	bal->cnt = cnt;
	bal->node = node;
	return bal;
}


int	eq_ballot(ballot x, ballot y)
{
	return x.cnt == y.cnt && x.node == y.node;
}


int	gt_ballot(ballot x, ballot y)
{
	return 
	    x.cnt > y.cnt || 
	    (x.cnt == y.cnt && x.node > y.node);
}

/* }}} */
