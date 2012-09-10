/*****************************************************************************

Copyright (c) 1998, 2009, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/pars0types.h
SQL parser global types

Created 1/11/1998 Heikki Tuuri
*******************************************************/

#ifndef pars0types_h
#define pars0types_h

struct pars_info_t;
struct pars_user_func_t;
struct pars_bound_lit_t;
struct pars_bound_id_t;
struct sym_node_t;
struct sym_tab_t;
struct pars_res_word_t;
struct func_node_t;
struct order_node_t;
struct proc_node_t;
struct elsif_node_t;
struct if_node_t;
struct while_node_t;
struct for_node_t;
struct exit_node_t;
struct return_node_t;
struct assign_node_t;
struct col_assign_node_t;

typedef UT_LIST_BASE_NODE_T(sym_node_t)	sym_node_list_t;

#endif
