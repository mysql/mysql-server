/******************************************************
Row operation global types

(c) 1996 Innobase Oy

Created 12/27/1996 Heikki Tuuri
*******************************************************/

#ifndef row0types_h
#define row0types_h

typedef struct plan_struct plan_t;

typedef	struct upd_struct upd_t;

typedef struct upd_field_struct upd_field_t;

typedef	struct upd_node_struct upd_node_t;

typedef	struct del_node_struct del_node_t;

typedef	struct ins_node_struct ins_node_t;

typedef struct sel_node_struct	sel_node_t;

typedef struct open_node_struct	open_node_t;

typedef struct fetch_node_struct fetch_node_t;

typedef struct row_printf_node_struct 	row_printf_node_t;
typedef struct sel_buf_struct	sel_buf_t;

typedef	struct undo_node_struct undo_node_t;

typedef	struct purge_node_struct purge_node_t;

#endif
