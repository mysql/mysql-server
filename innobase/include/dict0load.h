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
Finds the first table name in the given database. */

char*
dict_get_first_table_name_in_db(
/*============================*/
			/* out, own: table name, NULL if does not exist;
			the caller must free the memory in the string! */
	char*	name);	/* in: database name which ends to '/' */
/************************************************************************
Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table. */

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
/***************************************************************************
Loads foreign key constraints where the table is either the foreign key
holder or where the table is referenced by a foreign key. Adds these
constraints to the data dictionary. Note that we know that the dictionary
cache already contains all constraints where the other relevant table is
already in the dictionary cache. */

ulint
dict_load_foreigns(
/*===============*/
				/* out: DB_SUCCESS or error code */
	char*	table_name);	/* in: table name */
/************************************************************************
Prints to the standard output information on all tables found in the data
dictionary system table. */

void
dict_print(void);
/*============*/


#ifndef UNIV_NONINL
#include "dict0load.ic"
#endif

#endif
