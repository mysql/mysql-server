/******************************************************
SQL parser global types

(c) 1997 Innobase Oy

Created 1/11/1998 Heikki Tuuri
*******************************************************/

#ifndef pars0types_h
#define pars0types_h

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
typedef struct return_node_struct	return_node_t;
typedef struct assign_node_struct	assign_node_t;
typedef struct col_assign_node_struct	col_assign_node_t;

typedef UT_LIST_BASE_NODE_T(sym_node_t)	sym_node_list_t;

#endif 
