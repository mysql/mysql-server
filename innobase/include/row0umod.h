/******************************************************
Undo modify of a row

(c) 1997 Innobase Oy

Created 2/27/1997 Heikki Tuuri
*******************************************************/

#ifndef row0umod_h
#define row0umod_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"
#include "mtr0mtr.h"
	
/***************************************************************
Undoes a modify operation on a row of a table. */

ulint
row_undo_mod(
/*=========*/
				/* out: DB_SUCCESS or error code */
	undo_node_t*	node,	/* in: row undo node */
	que_thr_t*	thr);	/* in: query thread */


#ifndef UNIV_NONINL
#include "row0umod.ic"
#endif

#endif 
