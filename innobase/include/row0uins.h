/******************************************************
Fresh insert undo

(c) 1996 Innobase Oy

Created 2/25/1997 Heikki Tuuri
*******************************************************/

#ifndef row0uins_h
#define row0uins_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "row0types.h"
#include "mtr0mtr.h"
	
/***************************************************************
Undoes a fresh insert of a row to a table. A fresh insert means that
the same clustered index unique key did not have any record, even delete
marked, at the time of the insert. */

ulint
row_undo_ins(
/*=========*/
				/* out: DB_SUCCESS */
	undo_node_t*	node);	/* in: row undo node */


#ifndef UNIV_NONINL
#include "row0uins.ic"
#endif

#endif 
