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

#ifndef NODE_SET_H
#define NODE_SET_H

#ifdef __cplusplus
extern "C" {
#endif

#define dbg_node_set(s) _dbg_node_set(s, #s)
#define g_dbg_node_set(s) _g_dbg_node_set(s, #s)
bool_t equal_node_set(node_set x, node_set y);
bool_t is_empty_node_set(node_set set);
bool_t is_full_node_set(node_set set);
bool_t is_set(node_set set, node_no i);
u_int node_count(node_set set);
node_set *alloc_node_set(node_set *set, u_int n);
node_set *init_node_set(node_set *set, u_int n);
node_set *reset_node_set(node_set *set);
node_set *set_node_set(node_set *set);
node_set bit_set_to_node_set(bit_set *set, u_int n);
node_set clone_node_set(node_set set);
char *_dbg_node_set(node_set set, const char *name);
void _g_dbg_node_set(node_set set, const char *name);
void add_node(node_set set, node_no node);
void and_node_set(node_set *x, node_set const *y);
void copy_node_set(node_set const *from, node_set *to);
void copy_node_set(node_set const *from, node_set *to);
void dump_node_set(node_set set);
void free_node_set(node_set *set);
void not_node_set(node_set *x, node_set const *y);
void or_node_set(node_set *x, node_set const *y);
void remove_node(node_set set, node_no node);
void xor_node_set(node_set *x, node_set const *y);
node_set *realloc_node_set(node_set *set, u_int n);

#ifdef __cplusplus
}
#endif

#endif
