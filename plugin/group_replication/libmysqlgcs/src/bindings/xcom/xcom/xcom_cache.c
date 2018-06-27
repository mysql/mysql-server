/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cache.h"

#include <assert.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/bitset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/pax_msg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cfg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_vp_str.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#define DBG_CACHE_SIZE 0

/* Protect at least MIN_CACHED * (number of nodes) pax_machine objects from
 * deallocation by shrink_cache */
#define MIN_CACHED 10

/* {{{ Paxos machine cache */

struct lru_machine {
  linkage lru_link;
  pax_machine pax;
};

static synode_no last_removed_cache;

int was_removed_from_cache(synode_no x) {
  ADD_EVENTS(add_event(string_arg("x ")); add_synode_event(x);
             add_event(string_arg("last_removed_cache "));
             add_synode_event(last_removed_cache););
  /*
  What to do with requests from nodes that have a different group ID?
  Should we just ignore them, as we do with the current code,
  or should we do something about it?
  */
  return last_removed_cache.group_id == x.group_id &&
         !synode_gt(x, last_removed_cache);
}

#define BUCKETS (CACHED)

static linkage pax_hash[BUCKETS]; /* Hash link table */
static lru_machine
    cache[CACHED]; /* The Paxos instances, plus a link for the LRU chain */
static linkage protected_lru = {
    0, &protected_lru, &protected_lru}; /* Head of LRU chain of cache hits */
static linkage probation_lru = {
    0, &probation_lru, &probation_lru}; /* Head of LRU chain of cache misses */

static pax_machine *init_pax_machine(pax_machine *p, lru_machine *lru,
                                     synode_no synode);

static void hash_init() {
  unsigned int i = 0;
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
static unsigned int synode_hash(synode_no synode) {
  /* Need to hash three fields separately, since struct may contain padding with
     undefined values */
  return (unsigned int)(4711 * synode.node + 5 * synode.group_id +
                        synode.msgno) %
         BUCKETS;
}

#endif

static pax_machine *hash_in(pax_machine *p) {
  MAY_DBG(FN; PTREXP(p); SYCEXP(p->synode););
  link_into(&p->hash_link, &pax_hash[synode_hash(p->synode)]);
  return p;
}

static pax_machine *hash_out(pax_machine *p) {
  MAY_DBG(FN; PTREXP(p); SYCEXP(p->synode););
  return (pax_machine *)link_out(&p->hash_link);
}

pax_machine *hash_get(synode_no synode) {
  /* static pax_machine *cached_machine = NULL; */
  linkage *bucket = &pax_hash[synode_hash(synode)];

  /* if(cached_machine && synode_eq(synode, cached_machine->synode)) */
  /*   return cached_machine; */

  FWD_ITER(bucket, pax_machine, if (synode_eq(link_iter->synode, synode)) {
    /* cached_machine = link_iter; */
    return link_iter;
  });
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

static int was_machine_executed(pax_machine *p) {
  int const not_yet_functional = synode_eq(null_synode, get_delivered_msg());
  int const already_executed = synode_lt(p->synode, get_delivered_msg());
  return not_yet_functional || already_executed;
}

/*
Get a machine for (re)use.
The machines are statically allocated, and organized in two lists.
probation_lru is the free list.
protected_lru tracks the machines that are currently in the cache in
lest recently used order.
*/

static lru_machine *lru_get() {
  lru_machine *retval = NULL;
  if (!link_empty(&probation_lru)) {
    retval = (lru_machine *)link_first(&probation_lru);
  } else {
    /* Find the first non-busy instance in the LRU */
    FWD_ITER(&protected_lru, lru_machine,
             if (!is_busy_machine(&link_iter->pax) &&
                 was_machine_executed(&link_iter->pax)) {
               retval = link_iter;
               /* Since this machine is in in the cache, we need to update
               last_removed_cache */
               last_removed_cache = retval->pax.synode;
               break;
             })
  }
  assert(retval && !is_busy_machine(&retval->pax) &&
         was_machine_executed(&retval->pax));
  return retval;
}

static lru_machine *lru_touch_hit(pax_machine *p) {
  lru_machine *lru = p->lru;
  link_into(link_out(&lru->lru_link), &protected_lru);
  return lru;
}

/* Initialize the message cache */
void init_cache() {
  unsigned int i = 0;
  link_init(&protected_lru, type_hash("lru_machine"));
  link_init(&probation_lru, type_hash("lru_machine"));
  hash_init();
  for (i = 0; i < CACHED; i++) {
    lru_machine *l = &cache[i];
    link_init(&l->lru_link, type_hash("lru_machine"));
    link_into(&l->lru_link, &probation_lru);
    init_pax_machine(&l->pax, l, null_synode);
  }
  init_cache_size(); /* After cache has been intialized, size is 0 */
  last_removed_cache = null_synode;
}

void deinit_cache() {
  int i = 0;
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
  psi_report_cache_shutdown();
  for (i = 0; i < CACHED; i++) {
    lru_machine *l = &cache[i];
    pax_machine *p = &l->pax;
    if (p->proposer.prep_nodeset) {
      free_bit_set(p->proposer.prep_nodeset);
      p->proposer.prep_nodeset = NULL;
    }
    if (p->proposer.prop_nodeset) {
      free_bit_set(p->proposer.prop_nodeset);
      p->proposer.prop_nodeset = NULL;
    }
  }
}

/* static synode_no log_tail; */

pax_machine *get_cache_no_touch(synode_no synode) {
  pax_machine *retval = hash_get(synode);
  /* DBGOUT(FN; SYCEXP(synode); STREXP(task_name())); */
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  if (!retval) {
    lru_machine *l = lru_get(); /* Need to know when it is safe to re-use... */
    MAY_DBG(FN; PTREXP(l); COPY_AND_FREE_GOUT(dbg_pax_machine(&l->pax)););
    /*     assert(l->pax.synode > log_tail); */

    retval = hash_out(&l->pax);          /* Remove from hash table */
    init_pax_machine(retval, l, synode); /* Initialize */
    hash_in(retval);                     /* Insert in hash table again */
  }
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  return retval;
}

pax_machine *get_cache(synode_no synode) {
  pax_machine *retval = get_cache_no_touch(synode);
  lru_touch_hit(retval); /* Insert in protected_lru */
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  return retval;
}

static inline int can_deallocate(lru_machine *link_iter) {
  synode_no delivered_msg;
  site_def const *site = get_site_def();
  site_def const *dealloc_site = find_site_def(link_iter->pax.synode);

  /* If we have no site, or site was just installed, refuse deallocation */
  if (site == 0) return 0;
  /*
          With the patch that was put in to ensure that nodes always see  a
          global  view  message when it joins, the node that joins may need
          messages which are significantly behind the point where the  node
          joins  (effectively starting with the latest config). So there is
          a very real risk that a node which joined might find  that  those
          messages had been removed, since all the other nodes had executed
          past that point. This test effectively stops  garbage  collection
          of  old  messages until the joining node has got a chance to tell
          the others about its low water mark. If  it  has  not  done  that
          within  DETECTOR_LIVE_TIMEOUT,  it will be considered dead by the
          other nodes anyway, and expelled.
  */
  if ((site->install_time + DETECTOR_LIVE_TIMEOUT) > task_now()) return 0;
  if (dealloc_site == 0) /* Synode does not match any site, OK to deallocate */
    return 1;
  delivered_msg = get_min_delivered_msg(site);
  if (synode_eq(delivered_msg,
                null_synode)) /* Missing info from some node, not OK */
    return 0;
  return link_iter->pax.synode.group_id != delivered_msg.group_id ||
         (link_iter->pax.synode.msgno + MIN_CACHED) < delivered_msg.msgno;
}

/*
        Loop through the LRU (protected_lru) and deallocate objects until the
   size of
        the cache is below the limit.
        The freshly initialized objects are put into the probation_lru, so we
   can always start
        scanning at the end of protected_lru.
        lru_get will always look in probation_lru first.
*/
void shrink_cache() {
  FWD_ITER(&protected_lru, lru_machine,
           if (above_cache_limit() && can_deallocate(link_iter)) {
             last_removed_cache = link_iter->pax.synode;
             hash_out(&link_iter->pax); /* Remove from hash table */
             link_into(link_out(&link_iter->lru_link),
                       &probation_lru); /* Put in probation lru */
             init_pax_machine(&link_iter->pax, link_iter, null_synode);
           } else { return; });
}

void xcom_cache_var_init() {}

/* }}} */

/* {{{ Paxos machine */

/* Initialize a Paxos instance */
static pax_machine *init_pax_machine(pax_machine *p, lru_machine *lru,
                                     synode_no synode) {
  sub_cache_size(p);
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
  p->enforcer = 0;
  return p;
}

int lock_pax_machine(pax_machine *p) {
  int old = p->lock;
  if (!p->lock) p->lock = 1;
  return old;
}

void unlock_pax_machine(pax_machine *p) { p->lock = 0; }

int is_busy_machine(pax_machine *p) { return p->lock; }

/* purecov: begin deadcode */
/* Debug nodesets of Paxos instance */
char *dbg_machine_nodeset(pax_machine *p, u_int nodes) {
  GET_NEW_GOUT;
  STRLIT("proposer.prep_nodeset ");
  COPY_AND_FREE_GOUT(dbg_bitset(p->proposer.prep_nodeset, nodes));
  STRLIT("proposer.prop_nodeset ");
  COPY_AND_FREE_GOUT(dbg_bitset(p->proposer.prop_nodeset, nodes));
  RET_GOUT;
}

/* Debug a Paxos instance */
char *dbg_pax_machine(pax_machine *p) {
  GET_NEW_GOUT;
  if (!p) {
    STRLIT("p == 0 ");
    RET_GOUT;
  }
  PTREXP(p);
  COPY_AND_FREE_GOUT(
      dbg_machine_nodeset(p, get_maxnodes(find_site_def(p->synode))));
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
  NDBG(p->last_modified, f);
  NDBG(p->lock, d);
  STREXP(pax_op_to_str(p->op));
  RET_GOUT;
}

/*
  Return the size of a pax_msg. Counts only the pax_msg struct itself
  and the size of the app_data.
*/
static inline size_t get_app_msg_size(pax_msg const *p) {
  if (!p)
    return (size_t)0;
  else
    return sizeof(pax_msg) + app_data_list_size(p->a);
}

/*
  Return the size of the messages referenced by a pax_machine.
  The pax_machine itself is statically allocated, so we do
  not count this when computing the cache size.
*/
size_t pax_machine_size(pax_machine const *p) {
  size_t size = get_app_msg_size(p->proposer.msg);

  if (p->acceptor.msg && p->proposer.msg != p->acceptor.msg)
    size += get_app_msg_size(p->acceptor.msg);

  if (p->learner.msg && p->acceptor.msg != p->learner.msg &&
      p->proposer.msg != p->learner.msg)
    size += get_app_msg_size(p->learner.msg);
  return size;
}

static size_t cache_size = 0;

/* The cache itself is statically allocated, set size of dynamically allocted
 * data to 0 */
void init_cache_size() { cache_size = 0; }

/* Add to cache size */

size_t add_cache_size(pax_machine *p) {
  size_t x = pax_machine_size(p);
  cache_size += x;
  if (DBG_CACHE_SIZE && x) {
    G_DEBUG("%f %s:%d cache_size %lu x %lu", seconds(), __FILE__, __LINE__,
            (long unsigned int)cache_size, (long unsigned int)x);
  }
#ifndef XCOM_STANDALONE
  p->is_instrumented = psi_report_mem_alloc(x);
#endif
  return cache_size;
}

/* Subtract from cache size */
size_t sub_cache_size(pax_machine *p) {
  size_t x = pax_machine_size(p);
  cache_size -= x;
  if (DBG_CACHE_SIZE && x) {
    G_DEBUG("%f %s:%d cache_size %lu x %lu", seconds(), __FILE__, __LINE__,
            (long unsigned int)cache_size, (long unsigned int)x);
  }
#ifndef XCOM_STANDALONE
  psi_report_mem_free(x, p->is_instrumented);
  p->is_instrumented = 0;
#endif
  return cache_size;
}

/* See if cache is above limit */
int above_cache_limit() {
  return the_app_xcom_cfg && cache_size > the_app_xcom_cfg->cache_limit;
}

/* If cfg object exits, set max cache size */
size_t set_max_cache_size(size_t x) {
  if (the_app_xcom_cfg)
    return the_app_xcom_cfg->cache_limit = x;
  else
    return 0;
}
/* purecov: end */
/* }}} */
