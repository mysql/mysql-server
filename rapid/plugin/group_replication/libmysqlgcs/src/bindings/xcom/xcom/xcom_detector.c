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
#include "xcom_transport.h"
#include "xcom_base.h"
#include "node_set.h"
#include "app_data.h"
#include "pax_msg.h"
#include "synode_no.h"
#include "site_def.h"
#include "node_list.h"
#include "x_platform.h"

extern task_env *detector;
extern int	xcom_shutdown;

/* static double	detected[NSERVERS]; */

/* See if node has been suspiciously still for some time */
int	may_be_dead(detector_state const ds, node_no i, double seconds)
{
  /* DBGOUT(FN; NDBG(i,u); NDBG(ds[i] < seconds - 4.0, d)); */
  return ds[i] < seconds - 4.0;
}

void init_detector(detector_state ds)
{
  int	i = 0;
  for (i = 0; i < NSERVERS; i++) {
    ds[i] = 0.0;
  }
}

void note_detected(site_def const *site, node_no node)
{
/*   DBGOUT(FN; NDBG(node,d);); */

  /* site->servers's size is NSERVERS. */
  assert(site->nodes.node_list_len <= NSERVERS);

  if (site && node < site->nodes.node_list_len) {
    site->servers[node]->detected = task_now();
  }
}

static void reset_detected(site_def const *site, u_int node)
{
/*   DBGOUT(FN; NDBG(node,d);); */
  /* site->servers's size is NSERVERS. */
  assert(site->nodes.node_list_len <= NSERVERS);
  if (site && node < site->nodes.node_list_len) {
    site->servers[node]->detected = 0.0;
  }
}

static void reset_disjunct_servers(site_def const *old_site, site_def const *new_site)
{
  u_int node;

  if (old_site && new_site){

    /* Reset nodes which are not in new site (removed) */
    for(node = 0; node < old_site->nodes.node_list_len; node++) {
      if(!node_exists(&old_site->nodes.node_list_val[node], &new_site->nodes))
        reset_detected(old_site, node);
    }

    /* Reset nodes which are not in old site (added) */
    for(node = 0; node < new_site->nodes.node_list_len; node++) {
      if(!node_exists(&new_site->nodes.node_list_val[node], &old_site->nodes))
        reset_detected(new_site, node);
    }
  }
}

void update_detected(site_def *site)
{
/*   DBGOUT(FN; NDBG(node,d);); */
  u_int node;

  if (site){
    /* site->servers's size is NSERVERS. */
    assert(site->nodes.node_list_len <= NSERVERS);
    for(node = 0; node < site->nodes.node_list_len; node++) {
      site->detected[node] = site->servers[node]->detected;
    }
  }
  site->detector_updated = 1;
}

int	enough_live_nodes(site_def const *site)
{
  node_no i = 0;
  double	t = task_now();
  node_no n = 0;
  node_no maxnodes = get_maxnodes(site);
  node_no self = get_nodeno(site);

  if(site && !site->detector_updated){
	update_detected((site_def*)site);
  }

  /* DBGOUT(FN; NDBG(maxnodes,d); );*/
  if (maxnodes == 0)
    return 0;
  for (i = 0; i < maxnodes; i++) {
    if (i == self || t - site->detected[i] < DETECTOR_LIVE_TIMEOUT) {
      n++;
    }
  }
  /* DBGOUT(FN; NDBG(maxnodes,d); NDBG(n,d);); */
  return
    maxnodes > 0 &&
    ( n > maxnodes / 2 || (ARBITRATOR_HACK && (2 == maxnodes)));
}

static void send_my_view(site_def const *site);

#define DETECT(site) (i == get_nodeno(site)) || (site->detected[i] + DETECTOR_LIVE_TIMEOUT > task_now())

static void	update_global_count(site_def *site)
{
	u_int i;
	u_int nodes = get_maxnodes(site);

	site->global_node_count = 0;
	for (i = 0; i < nodes && i < site->global_node_set.node_set_len; i++) {
		if (site->global_node_set.node_set_val[i])
			site->global_node_count++;
	}
}

#if 0
/*
  This code seems to be dead.
  TODO: validate this with OHK and then remove.
*/
static void    update_global_node_set(site_def *site)
{
       u_int i;
       u_int nodes = get_maxnodes(site);
       node_no count = 0;

       for (i = 0; i < nodes && i < site->global_node_set.node_set_len; i++) {
               site->global_node_set.node_set_val[i] = DETECT(site);
       }
}
#endif

static void	check_global_node_set(site_def *site, int *notify)
{
	u_int i;
	u_int nodes = get_maxnodes(site);

	site->global_node_count = 0;
	for (i = 0; i < nodes && i < site->global_node_set.node_set_len; i++) {
		int	detect = DETECT(site);
		DBGOUT(FN; NDBG(i,d); NDBG(detect,d); NDBG(site->detected[i],f));
		if (site->global_node_set.node_set_val[i])
			site->global_node_count++;
		if (site->global_node_set.node_set_val[i] !=  detect) {
			*notify = 1;
		}
		DBGOHK(FN; NDBG(i,u); NDBG(*notify, d));
	}
}

static void	check_local_node_set(site_def *site, int *notify)
{
	u_int i;
	u_int nodes = get_maxnodes(site);

	for (i = 0; i < nodes && i < site->global_node_set.node_set_len; i++) {
		int	detect = DETECT(site);
		if (site->local_node_set.node_set_val[i] !=  detect) {
			site->local_node_set.node_set_val[i] =  detect;
			*notify = 1;
		}
		DBGOHK(FN; NDBG(i,u); NDBG(*notify, d));
	}
}

#if 0
/*
  This code seems to be dead.
  TODO: validate this with OHK and then remove.
*/
static void    update_local_node_set(site_def *site)
{
       u_int i;
       u_int nodes = get_maxnodes(site);
       node_no count = 0;

       for (i = 0; i < nodes && i < site->global_node_set.node_set_len; i++) {
               site->local_node_set.node_set_val[i] = DETECT(site);
       }
}
#endif

static node_no	leader(site_def const *s)
{
	node_no	leader = 0;
	for (leader = 0; leader < get_maxnodes(s); leader++) {
		if (!may_be_dead(s->detected, leader, task_now()) &&
			is_set(s->global_node_set, leader))
			return leader;
	}
	return 0;
}


int	iamtheleader(site_def const *s)
{
	return leader(s) == s->nodeno;
}

extern synode_no executed_msg;
extern synode_no max_synode;

static site_def * last_p_site= 0;
static site_def * last_x_site= 0;

void invalidate_detector_sites(site_def *site)
{
  if(last_p_site == site)
  {
    last_p_site = NULL;
  }

  if(last_x_site == site)
  {
    last_x_site = NULL;
  }
}

/* Notify others about our current view */
int	detector_task(task_arg arg MY_ATTRIBUTE((unused)))
{
	DECL_ENV
		int notify;
		int local_notify;
	END_ENV;

	TASK_BEGIN
	last_p_site = 0;
	last_x_site = 0;
	ep->notify = 1;
	ep->local_notify = 1;
	DBGOHK(FN; );
	while (!xcom_shutdown) {
		site_def * p_site = (site_def * )get_proposer_site();
		site_def * x_site = (site_def * )get_executor_site();

		if (!p_site)
			p_site = (site_def * )get_site_def();
		DBGOHK(FN; SYCEXP(executed_msg); SYCEXP(max_synode));
		DBGOHK(FN; PTREXP(p_site); NDBG(get_nodeno(p_site), u));
		DBGOHK(FN; PTREXP(x_site); NDBG(get_nodeno(x_site), u));

		if (x_site && get_nodeno(x_site) != VOID_NODE_NO) {

			if (x_site != last_x_site) {
				reset_disjunct_servers(last_x_site, x_site);
			}
			update_detected(x_site);
			if (x_site != last_x_site) {
				last_x_site = x_site;
				ep->notify = 1;
				ep->local_notify = 1;
			}


			DBGOHK(FN; PTREXP(x_site); NDBG(get_nodeno(x_site), u));
			DBGOHK(FN; COPY_AND_FREE_GOUT(dbg_node_set(x_site->global_node_set)));
			DBGOHK(FN; COPY_AND_FREE_GOUT(dbg_node_set(x_site->local_node_set)));
			check_global_node_set(x_site, &ep->notify);
			update_global_count(x_site);
			DBGOHK(FN; NDBG(iamtheleader(x_site), d); NDBG(enough_live_nodes(x_site), d); );
			/* Send xcom message if node has changed state */
			DBGOHK(FN; NDBG(ep->notify,d));
			if (ep->notify && iamtheleader(x_site) && enough_live_nodes(x_site)) {
				ep->notify = 0;
				send_my_view(x_site);
			}
		}

		if (x_site && get_nodeno(x_site) != VOID_NODE_NO) {
			DBGOHK(FN; PTREXP(x_site); NDBG(get_nodeno(x_site), u));
			DBGOHK(FN; COPY_AND_FREE_GOUT(dbg_node_set(x_site->global_node_set)));
			DBGOHK(FN; COPY_AND_FREE_GOUT(dbg_node_set(x_site->local_node_set)));
			update_global_count(x_site);
			check_local_node_set(x_site, &ep->local_notify);
			DBGOHK(FN; NDBG(ep->local_notify,d));
			if (ep->local_notify) {
				ep->local_notify = 0;
				deliver_view_msg(x_site); /* To application */
			}
		}
		TASK_DELAY(1.0);
	}

	FINALLY
	TASK_END;
}

node_set detector_node_set(site_def const *site)
{
  node_set new_set;
  new_set.node_set_len = 0;
  new_set.node_set_val = 0;
  if (site) {
    u_int nodes = get_maxnodes(site);
    alloc_node_set(&new_set, nodes);
    {
      u_int i = 0;
      for (i = 0; i < nodes; i++) {
        new_set.node_set_val[i] = DETECT(site);
      }
    }
  }
  return new_set;
}

static void send_my_view(site_def const *site)
{
  app_data_ptr a = new_app_data();
  pax_msg * msg = pax_msg_new(null_synode, site);
  DBGOHK(FN;);
  a->body.c_t = view_msg;
  a->body.app_u_u.present = detector_node_set(site);
  xcom_send(a, msg);
}


/* {{{ Alive task */

/* Send alive messages periodically */
int	alive_task(task_arg arg MY_ATTRIBUTE((unused)))
{
	DECL_ENV
	    pax_msg * i_p;
	pax_msg * you_p;
	END_ENV;
	TASK_BEGIN

	    ep->i_p = ep->you_p = NULL;

	while (!xcom_shutdown) {
		double	sec = task_now();
		synode_no alive_synode = get_current_message();
		site_def const * site = find_site_def(alive_synode);
		if (site && get_nodeno(site) != VOID_NODE_NO) {
			/* Send alive if we have not been active for some time */
			if (server_active(site, get_nodeno(site)) < sec - 0.5) {
				replace_pax_msg(&ep->i_p, pax_msg_new(alive_synode, site));
				ep->i_p->op = i_am_alive_op;
				send_to_all_site(site, ep->i_p, "alive_task");
			}

			/* Ping nodes which seem absent */
			 {
				node_no i;
				for (i = 0; i < get_maxnodes(site); i++) {
					if (i != get_nodeno(site) && may_be_dead(site->detected, i, sec)) {
						replace_pax_msg(&ep->you_p, pax_msg_new(alive_synode, site));
						ep->you_p->op = are_you_alive_op;
						ep->you_p->a = new_app_data();
						ep->you_p->a->app_key.group_id = ep->you_p->a->group_id = get_group_id(site);
						ep->you_p->a->body.c_t = xcom_boot_type;
						init_node_list(1, &site->nodes.node_list_val[i], &ep->you_p->a->body.app_u_u.nodes);
						DBGOUT(FN; COPY_AND_FREE_GOUT(dbg_list(&ep->you_p->a->body.app_u_u.nodes)););
						send_server_msg(site, i, ep->you_p);
					}
				}
			}
		}
		TASK_DELAY(1.0);
	}
	FINALLY
	    replace_pax_msg(&ep->i_p, NULL);
	replace_pax_msg(&ep->you_p, NULL);
	TASK_END;
}


