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

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include "xcom/xcom_msg_queue.h"

#include <assert.h>
#include <stdlib.h>

#include "xcom/pax_msg.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_struct.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xdr_gen/xcom_vp.h"

/* extern void replace_pax_msg(pax_msg **target, pax_msg *p); */

/* Paxos message queue */

/* Free list to speed up allocation and deallocation */
static linkage msg_link_list = {0, &msg_link_list, &msg_link_list};

void init_link_list() { link_init(&msg_link_list, TYPE_HASH("msg_link")); }

/* Initialize msg_link */
static void msg_link_init(msg_link *link, pax_msg *p, node_no to) {
  link_init(&link->l, TYPE_HASH("msg_link"));
  link->to = to;
  replace_pax_msg(&link->p, p);
}

/* purecov: begin deadcode */
char *dbg_msg_link(msg_link *link) {
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
msg_link *msg_link_new(pax_msg *p, node_no to) {
  msg_link *ret;
  if (link_empty(&msg_link_list)) {
    ret = (msg_link *)xcom_calloc((size_t)1, sizeof(msg_link));
    msg_link_init(ret, p, to);
    /* IFDBG(D_NONE, FN; STRLIT(" allocate "); PTREXP(ret);
    dbg_linkage(&ret->l)); */
  } else {
    /* IFDBG(D_NONE, FN; STRLIT("get from free list ");
     * dbg_linkage(&msg_link_list));*/
    ret = (msg_link *)link_extract_first(&msg_link_list);
    /* IFDBG(D_NONE, FN; STRLIT("msg_link_new from free list "); PTREXP(ret););
     */
    assert(!ret->p);
    msg_link_init(ret, p, to);
  }
  return ret;
}

/* Put msg_link in free list */
void msg_link_delete(msg_link **link_p) {
  msg_link *link = *link_p;
  /* IFDBG(D_NONE, FN; PTREXP(link);); */
  link_into(link_out(&link->l), &msg_link_list);
  replace_pax_msg(&link->p, nullptr);
  *link_p = nullptr;
  /* IFDBG(D_NONE, FN; STRLIT("insert in free list ");
   * dbg_linkage(&msg_link_list));
   */
}

/* Deallocate msg_link */
static void msg_link_free(msg_link **link_p) {
  msg_link *link = *link_p;
  /* IFDBG(D_NONE, FN; STRLIT("msg_link_free link %p",(void*)link);); */
  link_out(&link->l);
  replace_pax_msg(&link->p, nullptr);
  free(link);
  *link_p = nullptr;
}

void empty_msg_list(linkage *l) {
  while (!link_empty(l)) {
    msg_link *link = (msg_link *)link_extract_first(l);
    msg_link_delete(&link);
  }
}

#if 0
/* purecov: begin deadcode */
void shrink_msg_list(linkage *l, int n) {
  int count = cardinal(l);
  while (!link_empty(l) && count > n) {
    msg_link *link = (msg_link *)link_extract_first(l);
    msg_link_delete(&link);
    count--;
  }
}
/* purecov: end */
#endif

void empty_msg_channel(channel *c) {
  IFDBG(D_NONE, FN;);
  task_wakeup(&c->queue);   /* Wake up all tasks in queue */
  empty_msg_list(&c->data); /* Empty the queue */
}

#if 0
/* purecov: begin deadcode */
void shrink_msg_channel(channel *c, int n) {
  IFDBG(D_NONE, FN;);
  task_wakeup(&c->queue);       /* Wake up all tasks in queue */
  shrink_msg_list(&c->data, n); /* Empty the queue */
}
/* purecov: end */
#endif

/* Empty the free list */
void empty_link_free_list() {
  IFDBG(D_NONE, FN; STRLIT("empty_link_free_list"););
  while (!link_empty(&msg_link_list)) {
    msg_link *link = (msg_link *)link_extract_first(&msg_link_list);
    msg_link_free(&link);
  }
}
