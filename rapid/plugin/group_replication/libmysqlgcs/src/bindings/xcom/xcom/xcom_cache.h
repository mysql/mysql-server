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

#ifndef XCOM_CACHE_H
#define XCOM_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
We require that the number of elements in the cache is big enough enough that
it is always possible to find instances that are not busy.
Under normal circumstances the number of busy instances will be
less than event_horizon, since the proposers only considers
instances which belong to the local node.
A node may start proposing no_ops for instances belonging
to other nodes, meaning that event_horizon * NSERVERS instances may be
involved. However, for the time being, proposing a no_op for an instance
will not mark it as busy. This may change in the future, so a safe upper
limit on the number of nodes marked as busy is event_horizon * NSERVERS.
*/
#define CACHED 50000

#define is_cached(x) (hash_get(x) != NULL)

struct lru_machine ;
typedef struct lru_machine lru_machine;

struct pax_machine ;
typedef struct pax_machine pax_machine;

/* Definition of a Paxos instance */
struct pax_machine {
	linkage hash_link;
	lru_machine * lru;
	synode_no synode;
	double	last_modified;            /* Start time */
	linkage rv;           /* Tasks may sleep on this until something interesting happens */

	struct {
		ballot bal;          /* The current ballot we are working on */
		bit_set * prep_nodeset; /* Nodes which have answered my prepare */
		ballot sent_prop;
		bit_set * prop_nodeset; /* Nodes which have answered my propose */
		pax_msg * msg;         /* The value we are trying to push */
		ballot sent_learn;
	} proposer;

	struct {
		ballot promise;      /* Promise to not accept any proposals less than this */
		pax_msg * msg;         /* The value we have accepted */
	} acceptor;

	struct {
		pax_msg *msg;         /* The value we have learned */
	} learner;
	int	lock;               /* Busy ? */
	pax_op op;
	int force_delivery;
};


int	is_busy_machine(pax_machine *p);
int	lock_pax_machine(pax_machine *p);
pax_machine *get_cache_no_touch(synode_no synode);
pax_machine *get_cache(synode_no synode);
pax_machine *hash_get(synode_no synode);
char *dbg_machine_nodeset(pax_machine *p, u_int nodes);
char *dbg_pax_machine(pax_machine *p);
void init_cache();
void deinit_cache();
void unlock_pax_machine(pax_machine *p);
void xcom_cache_var_init();
void shrink_cache();
size_t pax_machine_size(pax_machine const *p);

void init_cache_size();
size_t add_cache_size(size_t x);
size_t sub_cache_size(size_t x);
int above_cache_limit();
size_t set_max_cache_size(size_t x);
int	was_removed_from_cache(synode_no x);

#ifdef __cplusplus
}
#endif

#endif

