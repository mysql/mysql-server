/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <assert.h>
#include <stdlib.h>

#include "xcom_common.h"
#include "simset.h"
#include "xcom_vp.h"
#include "task.h"
#include "task_debug.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "pax_msg.h"
#include "xcom_msg_queue.h"

/* extern void replace_pax_msg(pax_msg **target, pax_msg *p); */

/* {{{ Paxos message queue */


/* Free list to speed up allocation and deallocation */
static linkage msg_link_list = {0,&msg_link_list,&msg_link_list};

void init_link_list()
{
	link_init(&msg_link_list, type_hash("msg_link"));
}


/* Initialize msg_link */
static void msg_link_init(msg_link *link, pax_msg *p, node_no to)
{
	link_init(&link->l, type_hash("msg_link"));
	link->to = to;
	replace_pax_msg(&link->p, p);
}

/* purecov: begin deadcode */
char *dbg_msg_link(msg_link *link)
{
	GET_NEW_GOUT;
	if (!link) {
		STRLIT("link == 0 ");
	} else {
		PTREXP(link);
		PTREXP(link->p);
	}
	RET_GOUT;
}
/* purecov: end */

/* Get msg_link from free list if not empty, else allocate */
msg_link *msg_link_new(pax_msg *p, node_no to)
{
	msg_link * ret;
	if (link_empty(&msg_link_list)) {
		ret = calloc(1, sizeof(msg_link));
		msg_link_init(ret, p, to);
		/* DBGOUT(FN; STRLIT(" allocate "); PTREXP(ret);
		dbg_linkage(&ret->l)); */
	} else {
		/* MAY_DBG(FN; STRLIT("get from free list "); dbg_linkage(&msg_link_list));*/
		ret = (msg_link * )link_extract_first(&msg_link_list);
		/* DBGOUT(FN; STRLIT("msg_link_new from free list "); PTREXP(ret);); */
		assert(!ret->p);
		msg_link_init(ret, p, to);
	}
	return ret;
}


/* Put msg_link in free list */
void msg_link_delete(msg_link **link_p)
{
	msg_link * link = *link_p;
	/* DBGOUT(FN; PTREXP(link);); */
	link_into(link_out(&link->l), &msg_link_list);
	replace_pax_msg(&link->p, NULL);
	*link_p = NULL;
	/* MAY_DBG(FN; STRLIT("insert in free list "); dbg_linkage(&msg_link_list)); */
}


/* Deallocate msg_link */
static void msg_link_free(msg_link **link_p)
{
	msg_link * link = *link_p;
	/* DBGOUT(FN; STRLIT("msg_link_free link %p",(void*)link);); */
	link_out(&link->l);
	replace_pax_msg(&link->p, NULL);
	free(link);
	*link_p = NULL;
}


void empty_msg_list(linkage *l)
{
	while (!link_empty(l)) {
		msg_link * link = (msg_link * )link_extract_first(l);
		msg_link_delete(&link);
	}
}


void empty_msg_channel(channel *c)
{
	DBGOUT(FN;);
	task_wakeup(&c->queue); /* Wake up all tasks in queue */
	empty_msg_list(&c->data); /* Empty the queue */
}


/* Empty the free list */
void empty_link_free_list()
{
	DBGOUT(FN; STRLIT("empty_link_free_list"); );
	while (!link_empty(&msg_link_list)) {
		msg_link * link = (msg_link * )link_extract_first(&msg_link_list);
		msg_link_free(&link);
	}
}


/* }}} */
