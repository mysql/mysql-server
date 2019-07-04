/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

synode_no cache_get_last_removed() { return last_removed_cache; }

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

static size_t length_increment = INCREMENT;
static size_t size_decrement = INCREMENT / 10;

#define BUCKETS length_increment

typedef struct stack_machine {
  linkage stack_link;
  uint64_t start_msgno;
  uint occupation;
  linkage *pax_hash;
} stack_machine;

static linkage hash_stack = {0, &hash_stack,
                             &hash_stack}; /* Head of the hash stack */
static linkage protected_lru = {
    0, &protected_lru, &protected_lru}; /* Head of LRU chain of cache hits */
static linkage probation_lru = {
    0, &probation_lru, &probation_lru}; /* Head of LRU chain of cache misses */

static void hash_init(stack_machine *hash_bucket) {
  hash_bucket->pax_hash = malloc(sizeof(linkage) * BUCKETS);
  for (size_t i = 0; i < BUCKETS; i++) {
    link_init(&hash_bucket->pax_hash[i], type_hash("pax_machine"));
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

static uint64_t highest_msgno = 0;
static uint64_t cache_length = 0;
static uint64_t occupation = 0;

static void do_increment_step();

static pax_machine *hash_in(pax_machine *pm) {
  synode_no synode = pm->synode;
  MAY_DBG(FN; PTREXP(pm); SYCEXP(synode););

  if (highest_msgno < synode.msgno) highest_msgno = synode.msgno;

  FWD_ITER(&hash_stack, stack_machine, {
    if (link_iter->start_msgno < synode.msgno || link_iter->start_msgno == 0) {
      link_into(&pm->hash_link, &link_iter->pax_hash[synode_hash(synode)]);
      pm->stack_link = link_iter;
      link_iter->occupation++;
      occupation++;
      if (occupation == cache_length) {
        do_increment_step();
      }
      break;
    }
  })

  return pm;
}

static pax_machine *hash_out(pax_machine *p) {
  MAY_DBG(FN; PTREXP(p); SYCEXP(p->synode););
  if (!link_empty(&p->hash_link)) {
    occupation--;
    p->stack_link->occupation--;
  }
  return (pax_machine *)link_out(&p->hash_link);
}

pax_machine *hash_get(synode_no synode) {
  /* static pax_machine *cached_machine = NULL; */
  stack_machine *hash_table = NULL;

  /* if(cached_machine && synode_eq(synode, cached_machine->synode)) */
  /*   return cached_machine; */

  FWD_ITER(&hash_stack, stack_machine, {
    if (link_iter->start_msgno < synode.msgno || link_iter->start_msgno == 0) {
      hash_table = link_iter;
      break;
    }
  })

  linkage *bucket = &hash_table->pax_hash[synode_hash(synode)];

  FWD_ITER(bucket, pax_machine, {
    if (synode_eq(link_iter->synode, synode)) {
      /* cached_machine = link_iter; */
      return link_iter;
    }
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

static lru_machine *lru_get(bool_t force) {
  lru_machine *retval = NULL;
  lru_machine *force_retval = NULL;
  if (!link_empty(&probation_lru)) {
    retval = (lru_machine *)link_first(&probation_lru);
  } else {
    /* Find the first non-busy instance in the LRU */
    FWD_ITER(&protected_lru, lru_machine,
             if (!is_busy_machine(&link_iter->pax)) {
               if (was_machine_executed(&link_iter->pax)) {
                 retval = link_iter;
                 break;
               } else if (force && !force_retval) {
                 force_retval = link_iter;
               }
             })

    if (!retval && force) retval = force_retval;

    /* Since this machine is in the cache, we need to update
       last_removed_cache */
    if (retval) last_removed_cache = retval->pax.synode;
  }
  return retval;
}

static lru_machine *lru_touch_hit(pax_machine *p) {
  lru_machine *lru = p->lru;
  link_into(link_out(&lru->lru_link), &protected_lru);
  return lru;
}

/* Resets cache structures */
static void reset_cache() {
  link_init(&protected_lru, type_hash("lru_machine"));
  link_init(&probation_lru, type_hash("lru_machine"));
  link_init(&hash_stack, type_hash("stack_machine"));
  init_cache_size(); /* After cache has been intialized, size is 0 */
  last_removed_cache = null_synode;
  highest_msgno = 0;
}

static void add_stack_machine(uint64_t start_msgno);
static void expand_lru();

/*
  Initialize the message cache.
  The cache_manager_task is initialized in xcom_base to avoid memory
  leaks in tests.
*/
void init_cache() {
  reset_cache();
  // Init LRU
  expand_lru();
  // Init first hash_table
  add_stack_machine(0);
}

static void deinit_pax_machine(pax_machine *p, lru_machine *l) {
  init_pax_machine(&l->pax, NULL, null_synode);
  if (p->proposer.prep_nodeset) {
    free_bit_set(p->proposer.prep_nodeset);
    p->proposer.prep_nodeset = NULL;
  }
  if (p->proposer.prop_nodeset) {
    free_bit_set(p->proposer.prop_nodeset);
    p->proposer.prop_nodeset = NULL;
  }
}

static void free_lru_machine(lru_machine *link_iter) {
  link_out(&link_iter->lru_link);
  deinit_pax_machine(&link_iter->pax, link_iter);
  free(link_iter);
  cache_length--;
}

void deinit_cache() {
  FWD_ITER(&probation_lru, lru_machine, { free_lru_machine(link_iter); })

  FWD_ITER(&protected_lru, lru_machine, {
    hash_out(&link_iter->pax);
    free_lru_machine(link_iter);
  })

  FWD_ITER(&hash_stack, stack_machine, {
    free(link_iter->pax_hash);
    free(link_iter);
  })

  reset_cache();
  psi_report_cache_shutdown();
}

/* static synode_no log_tail; */

pax_machine *get_cache_no_touch(synode_no synode, bool_t force) {
  pax_machine *retval = hash_get(synode);
  /* DBGOUT(FN; SYCEXP(synode); STREXP(task_name())); */
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  if (!retval) {
    lru_machine *l =
        lru_get(force); /* Need to know when it is safe to re-use... */
    if (!l) return NULL;
    MAY_DBG(FN; PTREXP(l); COPY_AND_FREE_GOUT(dbg_pax_machine(&l->pax)););
    /*     assert(l->pax.synode > log_tail); */

    retval = hash_out(&l->pax);          /* Remove from hash table */
    init_pax_machine(retval, l, synode); /* Initialize */
    hash_in(retval);                     /* Insert in hash table again */
  }
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  return retval;
}

pax_machine *force_get_cache(synode_no synode) {
  pax_machine *retval = get_cache_no_touch(synode, TRUE);
  lru_touch_hit(retval); /* Insert in protected_lru */
  MAY_DBG(FN; SYCEXP(synode); PTREXP(retval));
  return retval;
}

pax_machine *get_cache(synode_no synode) {
  pax_machine *retval = get_cache_no_touch(synode, FALSE);
  if (retval) lru_touch_hit(retval); /* Insert in protected_lru */
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

static uint64_t cache_size = 0;

/*
  Loop through the LRU (protected_lru) and deallocate objects until the size
  of the cache is below the limit. The freshly initialized objects are put
  into the probation_lru, so we can always start scanning at the end of
  protected_lru. lru_get will always look in probation_lru first.
*/
size_t shrink_cache() {
  size_t shrunk = 0;
  FWD_ITER(&protected_lru, lru_machine, {
    if (above_cache_limit() && can_deallocate(link_iter)) {
      last_removed_cache = link_iter->pax.synode;
      hash_out(&link_iter->pax); /* Remove from hash table */
      link_into(link_out(&link_iter->lru_link),
                &probation_lru); /* Put in probation lru */
      init_pax_machine(&link_iter->pax, link_iter, null_synode);
      if (shrunk++ == size_decrement) {
        break;
      }
    } else {
      break;
    }
  });
  return shrunk;
}

void xcom_cache_var_init() {}

/* }}} */

/* {{{ Paxos machine */

/* Initialize a Paxos instance */
pax_machine *init_pax_machine(pax_machine *p, lru_machine *lru,
                              synode_no synode) {
  sub_cache_size(p);
  link_init(&p->hash_link, type_hash("pax_machine"));
  p->lru = lru;
  p->stack_link = NULL;
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
/* purecov: end */

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
/* }}} */

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
  return the_app_xcom_cfg && cache_size > the_app_xcom_cfg->m_cache_limit;
}

/* If cfg object exits, set max cache size */
size_t set_max_cache_size(uint64_t x) {
  size_t ret = 0;
  if (the_app_xcom_cfg) {
    G_DEBUG("Changing max cache size to %llu. Previous value was %llu.",
            (unsigned long long)x,
            (unsigned long long)the_app_xcom_cfg->m_cache_limit);
    ret = the_app_xcom_cfg->m_cache_limit = x;
    if (above_cache_limit()) shrink_cache();
  }
  return ret;
}

static void expand_lru() {
  for (size_t i = 0; i < BUCKETS; i++) {
    lru_machine *l = (lru_machine *)calloc(1, sizeof(lru_machine));
    link_init(&l->lru_link, type_hash("lru_machine"));
    link_into(&l->lru_link, &probation_lru);
    init_pax_machine(&l->pax, l, null_synode);
    cache_length++;
  }
}

static void add_stack_machine(uint64_t start_msgno) {
  stack_machine *hash_bucket = (stack_machine *)malloc(sizeof(stack_machine));
  link_init(&hash_bucket->stack_link, type_hash("stack_machine"));
  hash_bucket->occupation = 0;
  hash_bucket->start_msgno = start_msgno;
  hash_init(hash_bucket);
  link_follow(&hash_bucket->stack_link, &hash_stack);
}

static void do_increment_step() {
  expand_lru();
  add_stack_machine(highest_msgno);
}

static void do_decrement_step() {
  uint count = 0;
  FWD_ITER(&probation_lru, lru_machine, {
    free_lru_machine(link_iter);
    if (++count == BUCKETS) break;
  })

  free(((stack_machine *)link_last(&hash_stack))->pax_hash);
  free(link_out(link_last(&hash_stack)));
  ((stack_machine *)link_last(&hash_stack))->start_msgno = 0;
}

// Use vars instead of defines for unit testing
static uint64_t dec_threshold_length = DEC_THRESHOLD_LENGTH;
static float min_target_occupation = MIN_TARGET_OCCUPATION;
static float dec_threshold_size = DEC_THRESHOLD_SIZE;
static float min_length_threshold = MIN_LENGTH_THRESHOLD;

uint16_t check_decrease() {
  // Do not decrease before 500k length
  if ((cache_length <= dec_threshold_length)) return 1;
  // Oldest hash item is empty
  if (((stack_machine *)link_last(&hash_stack))->occupation != 0) return 2;
  // Low occupation
  if (occupation >= cache_length * min_target_occupation) return 3;
  // Resulting length high enough
  if ((cache_length - BUCKETS) * min_length_threshold <= occupation) return 4;
  // Skip if cache is (likely) still increasing.
  if ((cache_size <= the_app_xcom_cfg->m_cache_limit * dec_threshold_size)) {
    return 5;
  }
  do_decrement_step();
  return 0;
}

extern int xcom_shutdown;
void do_cache_maintenance() {
  if (above_cache_limit()) {
    shrink_cache();
  } else {
    check_decrease();
  }
}

int cache_manager_task(task_arg arg MY_ATTRIBUTE((unused))) {
  DECL_ENV
  int dummy;
  END_ENV;

  TASK_BEGIN

  while (!xcom_shutdown) {
    do_cache_maintenance();
    TASK_DELAY(0.1);
  }
  FINALLY
  TASK_END;
}

// Unit testing
/* purecov: begin deadcode */
uint64_t get_xcom_cache_occupation() { return occupation; }
uint64_t get_xcom_cache_length() { return cache_length; }
uint64_t get_xcom_cache_size() { return cache_size; }

void set_length_increment(size_t increment) { length_increment = increment; }

void set_size_decrement(size_t decrement) { size_decrement = decrement; }

void set_dec_threshold_length(uint64_t threshold) {
  dec_threshold_length = threshold;
}

void set_min_target_occupation(float threshold) {
  min_target_occupation = threshold;
}

void set_dec_threshold_size(float t_hold) { dec_threshold_size = t_hold; }

void set_min_length_threshold(float threshold) {
  min_length_threshold = threshold;
}
/* purecov: end */
