/******************************************************
Data dictionary creation and booting

(c) 1996 Innobase Oy

Created 4/18/1996 Heikki Tuuri
*******************************************************/

#include "dict0boot.h"

#ifdef UNIV_NONINL
#include "dict0boot.ic"
#endif

#include "dict0crea.h"
#include "btr0btr.h"
#include "dict0load.h"
#include "dict0load.h"
#include "trx0trx.h"
#include "srv0srv.h"
#include "ibuf0ibuf.h"
#include "buf0flu.h"
#include "log0recv.h"
#include "os0file.h"

/**************************************************************************
Gets a pointer to the dictionary header and x-latches its page. */

dict_hdr_t*
dict_hdr_get(
/*=========*/
			/* out: pointer to the dictionary header, 
			page x-latched */
	mtr_t*	mtr)	/* in: mtr */
{
	dict_hdr_t*	header;

	ut_ad(mtr);
	
	header = DICT_HDR + buf_page_get(DICT_HDR_SPACE, DICT_HDR_PAGE_NO,
							RW_X_LATCH, mtr);
	buf_page_dbg_add_level(header, SYNC_DICT_HEADER);

	return(header);
}

/**************************************************************************
Returns a new table, index, or tree id. */

dulint
dict_hdr_get_new_id(
/*================*/
			/* out: the new id */
	ulint	type)	/* in: DICT_HDR_ROW_ID, ... */
{
	dict_hdr_t*	dict_hdr;
	dulint		id;
	mtr_t		mtr;

	ut_ad((type == DICT_HDR_TABLE_ID) || (type == DICT_HDR_INDEX_ID)
	      || (type == DICT_HDR_MIX_ID));

	mtr_start(&mtr);

	dict_hdr = dict_hdr_get(&mtr);

	id = mtr_read_dulint(dict_hdr + type, MLOG_8BYTES, &mtr); 

	/* Add some dummy code here because otherwise pgcc seems to
	compile wrong */

	if (0 == ut_dulint_cmp(id, ut_dulint_max)) {
		printf("Max id\n");
	}

	id = ut_dulint_add(id, 1);
	
	mlog_write_dulint(dict_hdr + type, id, MLOG_8BYTES, &mtr); 

	mtr_commit(&mtr);

	return(id);
}				

/**************************************************************************
Writes the current value of the row id counter to the dictionary header file
page. */

void
dict_hdr_flush_row_id(void)
/*=======================*/
{
	dict_hdr_t*	dict_hdr;
	dulint		id;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	id = dict_sys->row_id;

	mtr_start(&mtr);

	dict_hdr = dict_hdr_get(&mtr);
	
	mlog_write_dulint(dict_hdr + DICT_HDR_ROW_ID, id, MLOG_8BYTES, &mtr); 

	mtr_commit(&mtr);
}				

/*********************************************************************
Creates the file page for the dictionary header. This function is
called only at the database creation. */
static
ibool
dict_hdr_create(
/*============*/
			/* out: TRUE if succeed */
	mtr_t*	mtr)	/* in: mtr */
{
	dict_hdr_t*	dict_header;
	ulint		hdr_page_no;
	ulint		root_page_no;
	page_t*		page;
	
	ut_ad(mtr);

	/* Create the dictionary header file block in a new, allocated file
	segment in the system tablespace */
	page = fseg_create(DICT_HDR_SPACE, 0,
				  DICT_HDR + DICT_HDR_FSEG_HEADER, mtr);

	hdr_page_no = buf_frame_get_page_no(page);
	
	ut_a(DICT_HDR_PAGE_NO == hdr_page_no);

	dict_header = dict_hdr_get(mtr);

	/* Start counting row, table, index, and tree ids from
	DICT_HDR_FIRST_ID */
	mlog_write_dulint(dict_header + DICT_HDR_ROW_ID,
				ut_dulint_create(0, DICT_HDR_FIRST_ID),
				MLOG_8BYTES, mtr);

	mlog_write_dulint(dict_header + DICT_HDR_TABLE_ID,
				ut_dulint_create(0, DICT_HDR_FIRST_ID),
				MLOG_8BYTES, mtr);

	mlog_write_dulint(dict_header + DICT_HDR_INDEX_ID,
				ut_dulint_create(0, DICT_HDR_FIRST_ID),
				MLOG_8BYTES, mtr);

	mlog_write_dulint(dict_header + DICT_HDR_MIX_ID,
				ut_dulint_create(0, DICT_HDR_FIRST_ID),
				MLOG_8BYTES, mtr);

	/* Create the B-tree roots for the clustered indexes of the basic
	system tables */

	/*--------------------------*/	
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				DICT_HDR_SPACE, DICT_TABLES_ID, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_TABLES, root_page_no,
							MLOG_4BYTES, mtr);
	/*--------------------------*/	
	root_page_no = btr_create(DICT_UNIQUE, DICT_HDR_SPACE,
						DICT_TABLE_IDS_ID, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_TABLE_IDS, root_page_no,
							MLOG_4BYTES, mtr);
	/*--------------------------*/	
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				DICT_HDR_SPACE, DICT_COLUMNS_ID, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_COLUMNS, root_page_no,
							MLOG_4BYTES, mtr);
	/*--------------------------*/	
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				DICT_HDR_SPACE, DICT_INDEXES_ID, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_INDEXES, root_page_no,
							MLOG_4BYTES, mtr);
	/*--------------------------*/	
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				DICT_HDR_SPACE, DICT_FIELDS_ID, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_FIELDS, root_page_no,
							MLOG_4BYTES, mtr);
	/*--------------------------*/	

	return(TRUE);
}

/*********************************************************************
Initializes the data dictionary memory structures when the database is
started. This function is also called when the data dictionary is created. */

void
dict_boot(void)
/*===========*/
{
	dict_table_t*	table;
	dict_index_t*	index;
	dict_hdr_t*	dict_hdr;
	mtr_t		mtr;

	mtr_start(&mtr);
	
	/* Create the hash tables etc. */
	dict_init();

	mutex_enter(&(dict_sys->mutex));
	
	/* Get the dictionary header */
	dict_hdr = dict_hdr_get(&mtr);

	/* Because we only write new row ids to disk-based data structure
	(dictionary header) when it is divisible by
	DICT_HDR_ROW_ID_WRITE_MARGIN, in recovery we will not recover
	the latest value of the row id counter. Therefore we advance
	the counter at the database startup to avoid overlapping values.
	Note that when a user after database startup first time asks for
	a new row id, then because the counter is now divisible by
	..._MARGIN, it will immediately be updated to the disk-based
	header. */

	dict_sys->row_id = ut_dulint_add(
			     ut_dulint_align_up(
				mtr_read_dulint(dict_hdr + DICT_HDR_ROW_ID,
							MLOG_8BYTES, &mtr),
				DICT_HDR_ROW_ID_WRITE_MARGIN),
			     DICT_HDR_ROW_ID_WRITE_MARGIN);

	/* Insert into the dictionary cache the descriptions of the basic
	system tables */
	/*-------------------------*/
	table = dict_mem_table_create((char *) "SYS_TABLES", DICT_HDR_SPACE,8);

	dict_mem_table_add_col(table, (char *) "NAME", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, (char *) "ID", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, (char *) "N_COLS", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "TYPE", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "MIX_ID", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, (char *) "MIX_LEN", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "CLUSTER_NAME", DATA_BINARY, 
			       0, 0, 0);
	dict_mem_table_add_col(table, (char *) "SPACE", DATA_INT, 0, 4, 0);

	table->id = DICT_TABLES_ID;
	
	dict_table_add_to_cache(table);
	dict_sys->sys_tables = table;
	
	index = dict_mem_index_create((char *) "SYS_TABLES", (char *) 
				      "CLUST_IND",
				      DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 1);

	dict_mem_index_add_field(index, (char *) "NAME", 0);

	index->page_no = mtr_read_ulint(dict_hdr + DICT_HDR_TABLES,
							MLOG_4BYTES, &mtr);
	index->id = DICT_TABLES_ID;

	ut_a(dict_index_add_to_cache(table, index));
	/*-------------------------*/
	index = dict_mem_index_create((char *) "SYS_TABLES",
				      (char *) "ID_IND", DICT_HDR_SPACE,
				      DICT_UNIQUE, 1);
	dict_mem_index_add_field(index, (char *) "ID", 0);

	index->page_no = mtr_read_ulint(dict_hdr + DICT_HDR_TABLE_IDS,
							MLOG_4BYTES, &mtr);
	index->id = DICT_TABLE_IDS_ID;
	ut_a(dict_index_add_to_cache(table, index));
	/*-------------------------*/
	table = dict_mem_table_create((char *) "SYS_COLUMNS",DICT_HDR_SPACE,7);

	dict_mem_table_add_col(table, (char *) "TABLE_ID", DATA_BINARY,0,0,0);
	dict_mem_table_add_col(table, (char *) "POS", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "NAME", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, (char *) "MTYPE", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "PRTYPE", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "LEN", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "PREC", DATA_INT, 0, 4, 0);
	
	table->id = DICT_COLUMNS_ID;

	dict_table_add_to_cache(table);
	dict_sys->sys_columns = table;

	index = dict_mem_index_create((char *) "SYS_COLUMNS",
				      (char *) "CLUST_IND", DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, (char *) "TABLE_ID", 0);
	dict_mem_index_add_field(index, (char *) "POS", 0);

	index->page_no = mtr_read_ulint(dict_hdr + DICT_HDR_COLUMNS,
							MLOG_4BYTES, &mtr);
	index->id = DICT_COLUMNS_ID;
	ut_a(dict_index_add_to_cache(table, index));
	/*-------------------------*/
	table = dict_mem_table_create((char *) "SYS_INDEXES",DICT_HDR_SPACE,7);

	dict_mem_table_add_col(table, (char *) "TABLE_ID", DATA_BINARY, 0,0,0);
	dict_mem_table_add_col(table, (char *) "ID", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, (char *) "NAME", DATA_BINARY, 0, 0, 0);
	dict_mem_table_add_col(table, (char *) "N_FIELDS", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "TYPE", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "SPACE", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "PAGE_NO", DATA_INT, 0, 4, 0);

	/* The '+ 2' below comes from the 2 system fields */
	ut_ad(DICT_SYS_INDEXES_PAGE_NO_FIELD == 6 + 2);
	ut_ad(DICT_SYS_INDEXES_SPACE_NO_FIELD == 5 + 2); 

	table->id = DICT_INDEXES_ID;
	dict_table_add_to_cache(table);
	dict_sys->sys_indexes = table;

	index = dict_mem_index_create((char *) "SYS_INDEXES",
				      (char *) "CLUST_IND", DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, (char *) "TABLE_ID", 0);
	dict_mem_index_add_field(index, (char *) "ID", 0);

	index->page_no = mtr_read_ulint(dict_hdr + DICT_HDR_INDEXES,
							MLOG_4BYTES, &mtr);
	index->id = DICT_INDEXES_ID;
	ut_a(dict_index_add_to_cache(table, index));
	/*-------------------------*/
	table = dict_mem_table_create((char *) "SYS_FIELDS", DICT_HDR_SPACE,3);

	dict_mem_table_add_col(table, (char *) "INDEX_ID", DATA_BINARY, 0,0,0);
	dict_mem_table_add_col(table, (char *) "POS", DATA_INT, 0, 4, 0);
	dict_mem_table_add_col(table, (char *) "COL_NAME", DATA_BINARY, 0,0,0);

	table->id = DICT_FIELDS_ID;
	dict_table_add_to_cache(table);
	dict_sys->sys_fields = table;

	index = dict_mem_index_create((char *) "SYS_FIELDS", 
				      (char *) "CLUST_IND", DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, (char *) "INDEX_ID", 0);
	dict_mem_index_add_field(index, (char *) "POS", 0);

	index->page_no = mtr_read_ulint(dict_hdr + DICT_HDR_FIELDS,
						MLOG_4BYTES, &mtr);
	index->id = DICT_FIELDS_ID;
	ut_a(dict_index_add_to_cache(table, index));

	mtr_commit(&mtr);
	/*-------------------------*/

	/* Initialize the insert buffer table and index for each tablespace */

	ibuf_init_at_db_start();

	/* Load definitions of other indexes on system tables */

	dict_load_sys_table(dict_sys->sys_tables);
	dict_load_sys_table(dict_sys->sys_columns);
	dict_load_sys_table(dict_sys->sys_indexes);
	dict_load_sys_table(dict_sys->sys_fields);
	
	mutex_exit(&(dict_sys->mutex));
}

/*********************************************************************
Inserts the basic system table data into themselves in the database
creation. */
static
void
dict_insert_initial_data(void)
/*==========================*/
{
	/* Does nothing yet */
}

/*********************************************************************
Creates and initializes the data dictionary at the database creation. */

void
dict_create(void)
/*=============*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	dict_hdr_create(&mtr);

	mtr_commit(&mtr);
	
	dict_boot();

	dict_insert_initial_data();

	sync_order_checks_on = TRUE;
}
