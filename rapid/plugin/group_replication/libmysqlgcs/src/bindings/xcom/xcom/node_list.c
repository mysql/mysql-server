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

#include "x_platform.h"

#include <rpc/rpc.h>

#include <stdlib.h>
#include <assert.h>

#ifndef WIN
#include <strings.h>
#endif

#include "xcom_common.h"
#include "task_debug.h"

#include "xcom_vp.h"
#include "node_list.h"
#include "node_address.h"

extern xcom_port xcom_get_port(char *a);

/**
   Debug a node list.
 */
/* purecov: begin deadcode */
char *dbg_list(node_list const *nodes)
{
  u_int i;
  GET_NEW_GOUT;
  PTREXP(nodes);  NDBG(nodes->node_list_len, u); PTREXP(nodes->node_list_val);
  for(i = 0; i < nodes->node_list_len; i++){
    COPY_AND_FREE_GOUT(dbg_node_address(nodes->node_list_val[i]));
  }
  RET_GOUT;
}
/* purecov: end */

/* {{{ Clone a node list */

node_list clone_node_list(node_list list)
{
  node_list retval;
  init_node_list(list.node_list_len, list.node_list_val, &retval);
  return retval;
}

/* }}} */

/* OHKFIX Do something more intelligent than strcmp */
int match_node(node_address *n1, node_address *n2)
{
	return n1 != 0 && n2 != 0 && (xcom_get_port(n1->address) == xcom_get_port(n2->address) && strcmp(n1->address, n2->address) == 0);
}

int match_node_list(node_address *n1, node_address *n2, u_int len2)
{
	u_int	i;
	for (i = 0; i < len2; i++) {
		if (match_node(n2+i, n1))
			return 1;
	}
	return 0;
}

static int	exists(node_address *name, node_list const *nodes)
{
	return match_node_list(name, nodes->node_list_val, nodes->node_list_len);
}

int	node_exists(node_address *name, node_list const *nodes)
{
	return exists(name, nodes);
}

static u_int	added_nodes(u_int n, node_address *names, node_list *nodes)
{
	u_int	i;
	u_int	added = n;
	if (nodes->node_list_val) {
		for (i = 0; i < n; i++) {
			if (exists(&names[i], nodes)) {
				added--;
			}
		}
	}
	return added;
}

static void init_proto_range(x_proto_range *r)
{
	r->min_proto = my_min_xcom_version;
	r->max_proto = my_xcom_version;
}

/* Add nodes to node list, avoid duplicate entries */
void	add_node_list(u_int n, node_address *names, node_list *nodes)
{
	/* Find new nodes */
	if (n && names) {
		u_int	added = added_nodes(n, names, nodes);

		if (added) {
			node_address * np = 0;
			u_int	i;

			/* Expand node list and add new nodes */
			nodes->node_list_val = realloc(nodes->node_list_val, (added + nodes->node_list_len) * sizeof(node_address));
			np = &nodes->node_list_val[nodes->node_list_len];
			for (i = 0; i < n; i++) {
				/* 			DBGOUT(FN; STREXP(names[i])); */
				if (!exists(&names[i], nodes)) {
					np->address = strdup(names[i].address);
					np->uuid.data.data_len = names[i].uuid.data.data_len;
					if(np->uuid.data.data_len){
						np->uuid.data.data_val = calloc(1, np->uuid.data.data_len);
						memcpy(np->uuid.data.data_val, names[i].uuid.data.data_val, np->uuid.data.data_len);
					} else {
						np->uuid.data.data_val = 0;
					}
					np->proto = names[i].proto;
					np++;
					/* Update length here so next iteration will check for duplicates against newly a
					    dded node
									     */
					nodes->node_list_len++;
				}
			}
		}
	}
}


/* Remove nodes from node list, ignore missing nodes */
void	remove_node_list(u_int n, node_address *names, node_list *nodes)
{
	node_address * np = 0;
	u_int	i;
	u_int	new_len = nodes->node_list_len;

	np = nodes->node_list_val;
	for (i = 0; i < nodes->node_list_len; i++) {
                if (match_node_list(&nodes->node_list_val[i], names, n)) {
			free(nodes->node_list_val[i].address);
                        nodes->node_list_val[i].address= 0;
			free(nodes->node_list_val[i].uuid.data.data_val);
                        nodes->node_list_val[i].uuid.data.data_val= 0;
			new_len--;
		} else {
			*np = nodes->node_list_val[i];
			np++;
		}
	}
	nodes->node_list_len = new_len;
}


/* {{{ Initialize a node list from array of string pointers */

void init_node_list(u_int n, node_address *names, node_list *nodes)
{
  nodes->node_list_len = 0;
  nodes->node_list_val = 0;
  add_node_list(n, names, nodes);
}

node_list *empty_node_list()
{
  return calloc(1, sizeof(node_list));
}

/* }}} */

node_address *init_node_address(node_address *na, u_int n, char *names[])
{
  u_int i;
  for(i = 0; i < n; i++){
    na[i].address = strdup(names[i]);
    init_proto_range(&na[i].proto);
    assert(na[i].uuid.data.data_len == 0 && na[i].uuid.data.data_val == 0);
  }
  return na;
}

node_address *new_node_address(u_int n, char *names[])
{
  node_address *na = calloc(n, sizeof(node_address));
  return init_node_address(na, n, names);
}

void delete_node_address(u_int n, node_address *na)
{
  u_int i;
  for(i = 0; i < n; i++){
    free(na[i].address);
    na[i].address= 0;
    free(na[i].uuid.data.data_val);
    na[i].uuid.data.data_val= 0;
  }
  free(na);
  na= 0;
}


