/******************************************************
Data dictionary global types

(c) 1996 Innobase Oy

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0types_h
#define dict0types_h

#include "ut0list.h"

typedef struct dict_sys_struct		dict_sys_t;
typedef struct dict_col_struct		dict_col_t;
typedef struct dict_field_struct	dict_field_t;
typedef struct dict_index_struct	dict_index_t;
typedef struct dict_table_struct	dict_table_t;
typedef struct dict_foreign_struct	dict_foreign_t;

/* A cluster object is a table object with the type field set to
DICT_CLUSTERED */

typedef dict_table_t			dict_cluster_t;

typedef struct ind_node_struct		ind_node_t;
typedef struct tab_node_struct		tab_node_t;

/* Data types for dict_undo */
union dict_undo_data_union {

	dict_index_t*	index;		/* The index to be dropped */

	struct	{
	dict_table_t*	old_table;	/* All fields are required only for*/
	dict_table_t*	tmp_table;	/*RENAME, for CREATE and DROP we */
	dict_table_t*	new_table;	/*use only old_table */
	}		table;
};

typedef union dict_undo_data_union dict_undo_data_t;

/* During recovery these are the operations that need to be undone */
struct dict_undo_struct {
	ulint		op_type;	/* Discriminator one of :
					TRX_UNDO_INDEX_CREATE_REC,
					TRX_UNDO_TABLE_DROP_REC,
					TRX_UNDO_TABLE_CREATE_REC,
					TRX_UNDO_TABLE_RENAME_REC.*/
	dict_undo_data_t
			data;		/* Data required for UNDO */

	UT_LIST_NODE_T(struct dict_undo_struct)
			node;		/* UNDO list node */
};

typedef	struct dict_undo_struct		dict_undo_t;
typedef UT_LIST_BASE_NODE_T(dict_undo_t) dict_undo_list_t;

/* TODO: Currently this data structure is a place holder for indexes
created by a transaction.* The REDO is a misnomer*/
struct dict_redo_struct {
	ulint		op_type;	/* Discriminator one of :
					TRX_UNDO_INDEX_CREATE_REC.*/
	dict_index_t*	index;		/* The index created.*/

	UT_LIST_NODE_T(struct dict_redo_struct)
			node;		/* REDO list node */
};

typedef	struct dict_redo_struct		dict_redo_t;
typedef UT_LIST_BASE_NODE_T(dict_redo_t) dict_redo_list_t;
#endif
