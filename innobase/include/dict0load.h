/******************************************************
Loads to the memory cache database object definitions
from dictionary tables

(c) 1996 Innobase Oy

Created 4/24/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0load_h
#define dict0load_h

#include "univ.i"
#include "dict0types.h"
#include "ut0byte.h"

/************************************************************************
Loads a table definition and also all its index definitions, and also
the cluster definition, if the table is a member in a cluster. */

dict_table_t*
dict_load_table(
/*============*/
			/* out: table, NULL if does not exist */
	char*	name);	/* in: table name */
/***************************************************************************
Loads a table object based on the table id. */

dict_table_t*
dict_load_table_on_id(
/*==================*/
				/* out: table; NULL if table does not exist */
	dulint	table_id);	/* in: table id */	
/************************************************************************
This function is called when the database is booted.
Loads system table index definitions except for the clustered index which
is added to the dictionary cache at booting before calling this function. */

void
dict_load_sys_table(
/*================*/
	dict_table_t*	table);	/* in: system table */


#ifndef UNIV_NONINL
#include "dict0load.ic"
#endif

#endif
