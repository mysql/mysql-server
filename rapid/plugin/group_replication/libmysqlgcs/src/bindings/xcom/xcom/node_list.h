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

#ifndef NODE_LIST_H
#define NODE_LIST_H

#ifdef __cplusplus
extern "C" {
#endif


node_list clone_node_list(node_list list);
char *dbg_list(node_list const *nodes);
void init_node_list(u_int n, node_address *names, node_list *nodes);
int match_node(node_address *n1, node_address *n2, u_int with_uid);
void	add_node_list(u_int n, node_address *names, node_list *nodes);
int	node_exists(node_address *name,  node_list const *nodes);
int	node_exists_with_uid(node_address *name,  node_list const *nodes);
node_address *new_node_address(u_int n, char *names[]);
node_address *new_node_address_uuid(u_int n, char *names[], blob uuid[]);
void delete_node_address(u_int n, node_address *na);
void	remove_node_list(u_int n, node_address *names, node_list *nodes);
node_address *init_node_address(node_address *na, u_int n, char *names[]);
node_list *empty_node_list();


#ifdef __cplusplus
}
#endif

#endif

