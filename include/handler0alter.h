/******************************************************
Smart ALTER TABLE

(c) 2005-2007 Innobase Oy
*******************************************************/

/*****************************************************************
Copies an InnoDB record to table->record[0]. */
UNIV_INTERN
void
innobase_rec_to_mysql(
/*==================*/
	TABLE*			table,		/* in/out: MySQL table */
	const rec_t*		rec,		/* in: record */
	const dict_index_t*	index,		/* in: index */
	const ulint*		offsets);	/* in: rec_get_offsets(
						rec, index, ...) */

/*****************************************************************
Resets table->record[0]. */
UNIV_INTERN
void
innobase_rec_reset(
/*===============*/
	TABLE*			table);		/* in/out: MySQL table */
