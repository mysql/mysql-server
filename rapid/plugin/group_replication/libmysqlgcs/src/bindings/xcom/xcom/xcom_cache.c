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

#include <rpc/rpc.h>
#include <assert.h>

#include <stdlib.h>

#include "xcom_common.h"
#include "simset.h"
#include "xcom_vp.h"
#include "xcom_cache.h"
#include "task.h"
#include "node_no.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "xcom_transport.h"
#include "xcom_base.h"
#include "pax_msg.h"
#include "xcom_vp_str.h"
#include "synode_no.h"
#include "task.h"
#include "task_debug.h"
#include "site_def.h"
#include "bitset.h"

/* {{{ Paxos machine cache */

struct lru_machine {
  linkage lru_link;
  pax_machine pax;
};

#define BUCKETS (CACHED)

static linkage pax_hash[BUCKETS];  /* Hash link table */
static lru_machine cache[CACHED]; /* The Paxos instances, plus a link for the LRU chain */
static linkage protected_lru = {0,&protected_lru, &protected_lru};           /* Head of LRU chain of cache hits */
static linkage probation_lru = {0,&probation_lru, &probation_lru};           /* Head of LRU chain of cache misses */

static pax_machine *init_pax_machine(pax_machine *p, lru_machine *lru, synode_no synode);

static void hash_init()
{
  unsigned int	i = 0;
  for (i = 0; i < BUCKETS; i++) {
    link_init(&pax_hash[i], type_hash("pax_machine"));
  }
}

extern void hexdump(void *p, long length);

#if 0
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

static unsigned int	synode_hash(synode_no synode)
{
  /* Need to hash three fields separately, since struct may contain padding with
     undefined values */
  return fnv_hash((unsigned char *) & synode.node, sizeof(synode.node),
                  fnv_hash((unsigned char *) & synode.group_id, sizeof(synode.group_id),
                           fnv_hash((unsigned char *) & synode.msgno, sizeof(synode.msgno), FNVSTART)))

    % BUCKETS;
}
#else
static unsigned int	synode_hash(synode_no synode)
{
  /* Need to hash three fields separately, since struct may contain padding with
     undefined values */
	return (unsigned int)
	  (4711 * synode.node + 5 * synode.group_id + synode.msgno) % BUCKETS;
}

#endif

static pax_machine *hash_in(pax_machine *p)
{
  MAY_DBG(FN; PTREXP(p);
  SYCEXP(p->synode);
  );
  link_into(&p->hash_link, &pax_hash[synode_hash(p->synode)]);
  return p;
}

static pax_machine *hash_out(pax_machine *p)
{
  MAY_DBG(FN; PTREXP(p);
  SYCEXP(p->synode);
  );
  return (pax_machine * )link_out(&p->hash_link);
}

pax_machine *hash_get(synode_no synode)
{
  /* static pax_machine *cached_machine = NULL; */
  linkage * bucket = &pax_hash[synode_hash(synode)];

  /* if(cached_machine && synode_eq(synode, cached_machine->synode)) */
  /*   return cached_machine; */

  FWD_ITER(bucket, pax_machine,
           if (synode_eq(link_iter->synode, synode)){
             /* cached_machine = link_iter; */
             return link_iter;
           }
           )
    ;
  return NULL;
}

#if 0
static int	is_noop(synode_no synode)
{
  if (is_cached(synode)) {
    pax_machine * m = get_cache(synode);
    return m->learner.msg && m->learner.msg->msg_type == no_op;
  } else {
    return 0;
  }
}
#endif

static lru_machine *lru_get()
{
  lru_machine *retval = NULL;
  if (!link_empty(&probation_lru))
    retval = (lru_machine * ) link_first(&probation_lru);
  else
    retval = (lru_machine * ) link_first(&protected_lru);
  assert(!is_busy_machine(&retval->pax));
  return retval;
}

#if 0
static lru_machine *lru_touch(pax_machine *p)
{
  lru_machine * lru = p->lru;
  if (0 && p->learner.msg && p->learner.msg->op == no_op)
    link_into(link_out(&lru->lru_link), &probation_lru);
  else
    link_into(link_out(&lru->lru_link), &protected_lru);
  return lru;
}
#endif
static lru_machine *lru_touch_miss(pax_machine *p)
{
  lru_machine * lru = p->lru;
  link_into(link_out(&lru->lru_link), &probation_lru);
  return lru;
}
#if 0
static lru_machine *lru_touch_hit(pax_machine *p)
{
  lru_machine * lru = p->lru;
  link_into(link_out(&lru->lru_link), &protected_lru);
  return lru;
}
#endif

/* Initialize the message cache */
void init_cache()
{
  unsigned int	i = 0;
  link_init(&protected_lru, type_hash("lru_machine"));
  link_init(&probation_lru, type_hash("lru_machine"));
  hash_init();
  for (i = 0; i < CACHED; i++) {
    lru_machine * l = &cache[i];
    link_init(&l->lru_link, type_hash("lru_machine"));
    if (1)
      link_into(&l->lru_link, &probation_lru);
    else
      link_into(&l->lru_link, &protected_lru);
    init_pax_machine(&l->pax, l, null_synode);
  }
}

void deinit_cache()
{
  int i= 0;
  /*
    We reset the memory structures before claiming back memory.
    Since deiniting the cache happens rarely - mostly when the
    XCom thread terminates we are ok with doing it like this,
    i.e., at the cost an additional loop and potential extra
    allocations - before deallocating.

    We do this to not clutter the execution flow and improve
    readability and maintaintability by keeping the source code
    for the deactivation routine simple and straightforward.
  */
  init_cache();
  for (i= 0; i < CACHED; i++)
  {
    lru_machine *l= &cache[i];
    pax_machine *p= &l->pax;
    if (p->proposer.prep_nodeset)
    {
      free_bit_set(p->proposer.prep_nodeset);
      p->proposer.prep_nodeset= NULL;
    }
    if (p->proposer.prop_nodeset)
    {
      free_bit_set(p->proposer.prop_nodeset);
      p->proposer.prop_nodeset= NULL;
    }
  }
}

/* static synode_no log_tail; */

pax_machine *get_cache(synode_no synode)
{
  pax_machine * retval = hash_get(synode);
  /* DBGOUT(FN; SYCEXP(synode); STREXP(task_name())); */
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  if (!retval) {
    lru_machine * l = lru_get(); /* Need to know when it is safe to re-use... */
    MAY_DBG(FN; PTREXP(l);
    COPY_AND_FREE_GOUT(dbg_pax_machine(&l->pax));
    );
    /*     assert(l->pax.synode > log_tail); */

    retval = hash_out(&l->pax); /* Remove from hash table */
    init_pax_machine(retval, l, synode); /* Initialize */
    hash_in(retval);            /* Insert in hash table again */
    lru_touch_miss(retval);
  } else {
    lru_touch_miss(retval);
  }
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  return retval;
}

void xcom_cache_var_init()
{
}

/* }}} */


/* {{{ Paxos machine */

/* Initialize a Paxos instance */
static pax_machine *init_pax_machine(pax_machine *p, lru_machine *lru, synode_no synode)
{
  link_init(&p->hash_link, type_hash("pax_machine"));
  p->lru = lru;
  p->synode = synode;
  p->last_modified = 0.0;
  link_init(&p->rv, type_hash("task_env"));
  init_ballot(&p->proposer.bal, 0, 0);
  init_ballot(&p->proposer.sent_prop, 0, 0);
  init_ballot(&p->proposer.sent_learn, -1, 0);
  if (!p->proposer.prep_nodeset)
    p->proposer.prep_nodeset = new_bit_set(NSERVERS);
  BIT_ZERO(p->proposer.prep_nodeset);
  if (!p->proposer.prop_nodeset)
    p->proposer.prop_nodeset = new_bit_set(NSERVERS);
  BIT_ZERO(p->proposer.prop_nodeset);
  replace_pax_msg(&p->proposer.msg, NULL);
  init_ballot(&p->acceptor.promise, 0, 0);
  replace_pax_msg(&p->acceptor.msg, NULL);
  replace_pax_msg(&p->learner.msg, NULL);
  p->lock = 0;
  p->op = initial_op;
  p->force_delivery = 0;
  return p;
}

int	lock_pax_machine(pax_machine *p)
{
  int	old = p->lock;
  if (!p->lock)
    p->lock = 1;
  return old;
}

void unlock_pax_machine(pax_machine *p)
{
  p->lock = 0;
}

int	is_busy_machine(pax_machine *p)
{
  return p->lock;
}

/* purecov: begin deadcode */
/* Debug nodesets of Paxos instance */
char *dbg_machine_nodeset(pax_machine *p, u_int nodes)
{
	GET_NEW_GOUT;
	STRLIT("proposer.prep_nodeset ");
	COPY_AND_FREE_GOUT(dbg_bitset(p->proposer.prep_nodeset, nodes));
	STRLIT("proposer.prop_nodeset ");
	COPY_AND_FREE_GOUT(dbg_bitset(p->proposer.prop_nodeset, nodes));
	RET_GOUT;
}


/* Debug a Paxos instance */
char *dbg_pax_machine(pax_machine *p)
{
	GET_NEW_GOUT;
	if (!p) {
		STRLIT("p == 0 ");
		RET_GOUT;
	}
	PTREXP(p);
	COPY_AND_FREE_GOUT(dbg_machine_nodeset(p, get_maxnodes(find_site_def(p->synode))));
	BALCEXP(p->proposer.bal);
	BALCEXP(p->proposer.sent_prop);
	BALCEXP(p->proposer.sent_learn);
	BALCEXP(p->acceptor.promise);
	STRLIT("proposer.msg ");
	COPY_AND_FREE_GOUT(dbg_pax_msg(p->proposer.msg));
	STRLIT("acceptor.msg ");
	COPY_AND_FREE_GOUT(dbg_pax_msg(p->acceptor.msg));
	STRLIT("learner.msg ");
	COPY_AND_FREE_GOUT(dbg_pax_msg(p->learner.msg));
	NDBG(p->last_modified,f);
	NDBG(p->lock,d);
	STREXP(pax_op_to_str(p->op));
	RET_GOUT;
}

/* purecov: end */
/* }}} */
