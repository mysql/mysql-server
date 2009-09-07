/*****************************************************************************

Copyright (c) 1998, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/pars0types.h
SQL parser global types

Created 1/11/1998 Heikki Tuuri
*******************************************************/

#ifndef pars0types_h
#define pars0types_h

typedef struct pars_info_struct		pars_info_t;
typedef struct pars_user_func_struct	pars_user_func_t;
typedef struct pars_bound_lit_struct	pars_bound_lit_t;
typedef struct pars_bound_id_struct	pars_bound_id_t;
typedef struct sym_node_struct		sym_node_t;
typedef struct sym_tab_struct		sym_tab_t;
typedef struct pars_res_word_struct	pars_res_word_t;
typedef struct func_node_struct		func_node_t;
typedef struct order_node_struct	order_node_t;
typedef struct proc_node_struct		proc_node_t;
typedef struct elsif_node_struct	elsif_node_t;
typedef struct if_node_struct		if_node_t;
typedef struct while_node_struct	while_node_t;
typedef struct for_node_struct		for_node_t;
typedef struct exit_node_struct		exit_node_t;
typedef struct return_node_struct	return_node_t;
typedef struct assign_node_struct	assign_node_t;
typedef struct col_assign_node_struct	col_assign_node_t;

typedef UT_LIST_BASE_NODE_T(sym_node_t)	sym_node_list_t;

#endif
