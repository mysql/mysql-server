/******************************************************
Transaction system

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include "trx0sys.h"

#ifdef UNIV_NONINL
#include "trx0sys.ic"
#endif

#include "fsp0fsp.h"
#include "mtr0mtr.h"
#include "trx0trx.h"
#include "trx0rseg.h"
#include "trx0undo.h"
#include "srv0srv.h"
#include "trx0purge.h"
#include "log0log.h"
#include "os0file.h"

/* The transaction system */
trx_sys_t*		trx_sys 	= NULL;
trx_doublewrite_t*	trx_doublewrite = NULL;

/* The following is set to TRUE when we are upgrading from the old format data
files to the new >= 4.1.x format multiple tablespaces format data files */

ibool			trx_doublewrite_must_reset_space_ids	= FALSE;

/* The following is TRUE when we are using the database in the new format,
i.e., we have successfully upgraded, or have created a new database
installation */

ibool			trx_sys_multiple_tablespace_format	= FALSE;

/* In a MySQL replication slave, in crash recovery we store the master log
file name and position here. We have successfully got the updates to InnoDB
up to this position. If .._pos is -1, it means no crash recovery was needed,
or there was no master log position info inside InnoDB. */

char 		trx_sys_mysql_master_log_name[TRX_SYS_MYSQL_LOG_NAME_LEN];
ib_longlong	trx_sys_mysql_master_log_pos	= -1;

/* If this MySQL server uses binary logging, after InnoDB has been inited
and if it has done a crash recovery, we store the binlog file name and position
here. If .._pos is -1, it means there was no binlog position info inside
InnoDB. */

char 		trx_sys_mysql_bin_log_name[TRX_SYS_MYSQL_LOG_NAME_LEN];
ib_longlong	trx_sys_mysql_bin_log_pos	= -1;


/********************************************************************
Determines if a page number is located inside the doublewrite buffer. */

ibool
trx_doublewrite_page_inside(
/*========================*/
				/* out: TRUE if the location is inside
				the two blocks of the doublewrite buffer */
	ulint	page_no)	/* in: page number */
{
	if (trx_doublewrite == NULL) {

		return(FALSE);
	}

	if (page_no >= trx_doublewrite->block1
	    && page_no < trx_doublewrite->block1
					+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		return(TRUE);
	}

	if (page_no >= trx_doublewrite->block2
	    && page_no < trx_doublewrite->block2
					+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		return(TRUE);
	}

	return(FALSE);
}

/********************************************************************
Creates or initialializes the doublewrite buffer at a database start. */
static
void
trx_doublewrite_init(
/*=================*/
	byte*	doublewrite)	/* in: pointer to the doublewrite buf
				header on trx sys page */
{
	trx_doublewrite = mem_alloc(sizeof(trx_doublewrite_t));

	/* Since we now start to use the doublewrite buffer, no need to call
	fsync() after every write to a data file */

	os_do_not_call_flush_at_each_write = TRUE;

	mutex_create(&(trx_doublewrite->mutex));
	mutex_set_level(&(trx_doublewrite->mutex), SYNC_DOUBLEWRITE);

	trx_doublewrite->first_free = 0;

	trx_doublewrite->block1 = mach_read_from_4(
						doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK1);
	trx_doublewrite->block2 = mach_read_from_4(
						doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK2);
	trx_doublewrite->write_buf_unaligned = 
				ut_malloc(
				(1 + 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
				* UNIV_PAGE_SIZE);
						
	trx_doublewrite->write_buf = ut_align(
					trx_doublewrite->write_buf_unaligned,
					UNIV_PAGE_SIZE);
	trx_doublewrite->buf_block_arr = mem_alloc(
					2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
					* sizeof(void*));
}

/********************************************************************
Marks the trx sys header when we have successfully upgraded to the >= 4.1.x
multiple tablespace format. */

void
trx_sys_mark_upgraded_to_multiple_tablespaces(void)
/*===============================================*/
{
	page_t*	page;
	byte*	doublewrite;
	mtr_t	mtr;

	/* We upgraded to 4.1.x and reset the space id fields in the
	doublewrite buffer. Let us mark to the trx_sys header that the upgrade
	has been done. */

	mtr_start(&mtr);

	page = buf_page_get(TRX_SYS_SPACE, TRX_SYS_PAGE_NO, RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

	doublewrite = page + TRX_SYS_DOUBLEWRITE;

	mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED,
				TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N,
				MLOG_4BYTES, &mtr);
	mtr_commit(&mtr);
		
	/* Flush the modified pages to disk and make a checkpoint */
	log_make_checkpoint_at(ut_dulint_max, TRUE);

	trx_sys_multiple_tablespace_format = TRUE;
}

/********************************************************************
Creates the doublewrite buffer to a new InnoDB installation. The header of the
doublewrite buffer is placed on the trx system header page. */

void
trx_sys_create_doublewrite_buf(void)
/*================================*/
{
	page_t*	page;
	page_t*	page2;
	page_t*	new_page;
	byte*	doublewrite;
	byte*	fseg_header;
	ulint	page_no;
	ulint	prev_page_no;
	ulint	i;
	mtr_t	mtr;

	if (trx_doublewrite) {
		/* Already inited */

		return;
	}

start_again:	
	mtr_start(&mtr);

	page = buf_page_get(TRX_SYS_SPACE, TRX_SYS_PAGE_NO, RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

	doublewrite = page + TRX_SYS_DOUBLEWRITE;
	
	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC)
					== TRX_SYS_DOUBLEWRITE_MAGIC_N) {
		/* The doublewrite buffer has already been created:
		just read in some numbers */

		trx_doublewrite_init(doublewrite);

		mtr_commit(&mtr);
	} else {
		fprintf(stderr,
		"InnoDB: Doublewrite buffer not found: creating new\n");

		if (buf_pool_get_curr_size() <
					(2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
						+ FSP_EXTENT_SIZE / 2 + 100)
					* UNIV_PAGE_SIZE) {
			fprintf(stderr,
			"InnoDB: Cannot create doublewrite buffer: you must\n"
			"InnoDB: increase your buffer pool size.\n"
			"InnoDB: Cannot continue operation.\n");

			exit(1);
		}
	
		page2 = fseg_create(TRX_SYS_SPACE, TRX_SYS_PAGE_NO,
			TRX_SYS_DOUBLEWRITE + TRX_SYS_DOUBLEWRITE_FSEG, &mtr);

		/* fseg_create acquires a second latch on the page,
		therefore we must declare it: */

#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page2, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

		if (page2 == NULL) {
			fprintf(stderr,
			"InnoDB: Cannot create doublewrite buffer: you must\n"
			"InnoDB: increase your tablespace size.\n"
			"InnoDB: Cannot continue operation.\n");

			/* We exit without committing the mtr to prevent
			its modifications to the database getting to disk */
			
			exit(1);
		}

		fseg_header = page + TRX_SYS_DOUBLEWRITE
						+ TRX_SYS_DOUBLEWRITE_FSEG;
		prev_page_no = 0;

		for (i = 0; i < 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
						+ FSP_EXTENT_SIZE / 2; i++) {
			page_no = fseg_alloc_free_page(fseg_header,
			 				prev_page_no + 1,
							FSP_UP, &mtr);
			if (page_no == FIL_NULL) {
				fprintf(stderr,
			"InnoDB: Cannot create doublewrite buffer: you must\n"
			"InnoDB: increase your tablespace size.\n"
			"InnoDB: Cannot continue operation.\n");

				exit(1);
			}

			/* We read the allocated pages to the buffer pool;
			when they are written to disk in a flush, the space
			id and page number fields are also written to the
			pages. When we at database startup read pages
			from the doublewrite buffer, we know that if the
			space id and page number in them are the same as
			the page position in the tablespace, then the page
			has not been written to in doublewrite. */
			
			new_page = buf_page_get(TRX_SYS_SPACE, page_no,
							RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
			buf_page_dbg_add_level(new_page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

			/* Make a dummy change to the page to ensure it will
			be written to disk in a flush */

			mlog_write_ulint(new_page + FIL_PAGE_DATA,
					TRX_SYS_DOUBLEWRITE_MAGIC_N,
					MLOG_4BYTES, &mtr);

			if (i == FSP_EXTENT_SIZE / 2) {
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK1,
						page_no, MLOG_4BYTES, &mtr);
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_REPEAT
						+ TRX_SYS_DOUBLEWRITE_BLOCK1,
						page_no, MLOG_4BYTES, &mtr);
			} else if (i == FSP_EXTENT_SIZE / 2
					+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_BLOCK2,
						page_no, MLOG_4BYTES, &mtr);
				mlog_write_ulint(doublewrite
						+ TRX_SYS_DOUBLEWRITE_REPEAT
						+ TRX_SYS_DOUBLEWRITE_BLOCK2,
						page_no, MLOG_4BYTES, &mtr);
			} else if (i > FSP_EXTENT_SIZE / 2) {
				ut_a(page_no == prev_page_no + 1);
			}

			prev_page_no = page_no;
		}

		mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC,
			TRX_SYS_DOUBLEWRITE_MAGIC_N, MLOG_4BYTES, &mtr);
		mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC
						+ TRX_SYS_DOUBLEWRITE_REPEAT,
			TRX_SYS_DOUBLEWRITE_MAGIC_N, MLOG_4BYTES, &mtr);

		mlog_write_ulint(doublewrite
				+ TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED,
				TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N,
				MLOG_4BYTES, &mtr);
		mtr_commit(&mtr);
		
		/* Flush the modified pages to disk and make a checkpoint */
		log_make_checkpoint_at(ut_dulint_max, TRUE);

		fprintf(stderr, "InnoDB: Doublewrite buffer created\n");

		trx_sys_multiple_tablespace_format = TRUE;

		goto start_again;
	}
}

/********************************************************************
At a database startup initializes the doublewrite buffer memory structure if
we already have a doublewrite buffer created in the data files. If we are
upgrading to an InnoDB version which supports multiple tablespaces, then this
function performs the necessary update operations. If we are in a crash
recovery, this function uses a possible doublewrite buffer to restore
half-written pages in the data files. */

void
trx_sys_doublewrite_init_or_restore_pages(
/*======================================*/
	ibool	restore_corrupt_pages)
{
	byte*	buf;
	byte*	read_buf;
	byte*	unaligned_read_buf;
	ulint	block1;
	ulint	block2;
	ulint	source_page_no;
	byte*	page;
	byte*	doublewrite;
	ulint	space_id;
	ulint	page_no;
	ulint	i;
	
	/* We do the file i/o past the buffer pool */

	unaligned_read_buf = ut_malloc(2 * UNIV_PAGE_SIZE);
	read_buf = ut_align(unaligned_read_buf, UNIV_PAGE_SIZE);	

	/* Read the trx sys header to check if we are using the doublewrite
	buffer */

	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, TRX_SYS_PAGE_NO, 0,
					UNIV_PAGE_SIZE, read_buf, NULL);
	doublewrite = read_buf + TRX_SYS_DOUBLEWRITE;

	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC)
					== TRX_SYS_DOUBLEWRITE_MAGIC_N) {
		/* The doublewrite buffer has been created */
		
		trx_doublewrite_init(doublewrite);

		block1 = trx_doublewrite->block1;
		block2 = trx_doublewrite->block2;

		buf = trx_doublewrite->write_buf;
	} else {
		goto leave_func;
	}

	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED)
	    != TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N) {
		        
	        /* We are upgrading from a version < 4.1.x to a version where
		multiple tablespaces are supported. We must reset the space id
		field in the pages in the doublewrite buffer because starting
		from this version the space id is stored to
		FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID. */

		trx_doublewrite_must_reset_space_ids = TRUE;

		fprintf(stderr,
"InnoDB: Resetting space id's in the doublewrite buffer\n");
	} else {
		trx_sys_multiple_tablespace_format = TRUE;
	}

	/* Read the pages from the doublewrite buffer to memory */

	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, block1, 0,
			TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
			buf, NULL);
	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, block2, 0,
			TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
			buf + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
			NULL);
	/* Check if any of these pages is half-written in data files, in the
	intended position */

	page = buf;
	
	for (i = 0; i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 2; i++) {
		
		page_no = mach_read_from_4(page + FIL_PAGE_OFFSET);

		if (trx_doublewrite_must_reset_space_ids) {

		        space_id = 0;
			mach_write_to_4(page
					+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 0);
			/* We do not need to calculate new checksums for the
			pages because the field .._SPACE_ID does not affect
			them. Write the page back to where we read it from. */

			if (i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
			        source_page_no = block1 + i;
			} else {
				source_page_no = block2
					+ i - TRX_SYS_DOUBLEWRITE_BLOCK_SIZE;
			}

			fil_io(OS_FILE_WRITE, TRUE, 0, source_page_no, 0,
					      UNIV_PAGE_SIZE, page, NULL);
			/* printf("Resetting space id in page %lu\n",
						   source_page_no); */
		} else {
		        space_id = mach_read_from_4(
				page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
		}

		if (!restore_corrupt_pages) {
			/* The database was shut down gracefully: no need to
			restore pages */

		} else if (!fil_tablespace_exists_in_mem(space_id)) {
			/* Maybe we have dropped the single-table tablespace
			and this page once belonged to it: do nothing */

		} else if (!fil_check_adress_in_tablespace(space_id,
								page_no)) {
		  	fprintf(stderr,
"InnoDB: Warning: a page in the doublewrite buffer is not within space\n"
"InnoDB: bounds; space id %lu page number %lu, page %lu in doublewrite buf.\n",
				(ulong) space_id, (ulong) page_no, (ulong) i);
		
		} else if (space_id == TRX_SYS_SPACE
		    && (  (page_no >= block1
			   && page_no
				< block1 + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
		        || (page_no >= block2
			   && page_no
				< block2 + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE))) {

			/* It is an unwritten doublewrite buffer page:
			do nothing */
		} else {
			/* Read in the actual page from the data files */
			
			fil_io(OS_FILE_READ, TRUE, space_id, page_no, 0,
					UNIV_PAGE_SIZE, read_buf, NULL);
			/* Check if the page is corrupt */

			if (buf_page_is_corrupted(read_buf)) {

		  		fprintf(stderr,
		"InnoDB: Warning: database page corruption or a failed\n"
		"InnoDB: file read of page %lu.\n", (ulong) page_no);
		  		fprintf(stderr,
		"InnoDB: Trying to recover it from the doublewrite buffer.\n");
				
				if (buf_page_is_corrupted(page)) {
					fprintf(stderr,
		"InnoDB: Dump of the page:\n");
					buf_page_print(read_buf);
					fprintf(stderr,
		"InnoDB: Dump of corresponding page in doublewrite buffer:\n");
					buf_page_print(page);

		  			fprintf(stderr,
		"InnoDB: Also the page in the doublewrite buffer is corrupt.\n"
		"InnoDB: Cannot continue operation.\n"
		"InnoDB: You can try to recover the database with the my.cnf\n"
		"InnoDB: option:\n"
		"InnoDB: set-variable=innodb_force_recovery=6\n");
					exit(1);
				}

				/* Write the good page from the
				doublewrite buffer to the intended
				position */

				fil_io(OS_FILE_WRITE, TRUE, space_id,
					page_no, 0,
					UNIV_PAGE_SIZE, page, NULL);
		  		fprintf(stderr,
		"InnoDB: Recovered the page from the doublewrite buffer.\n");
			}
		}

		page += UNIV_PAGE_SIZE;
	}

	fil_flush_file_spaces(FIL_TABLESPACE);
	
leave_func:
	ut_free(unaligned_read_buf);
}

/********************************************************************
Checks that trx is in the trx list. */

ibool
trx_in_trx_list(
/*============*/
			/* out: TRUE if is in */
	trx_t*	in_trx)	/* in: trx */
{
	trx_t*	trx;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(kernel_mutex)));
#endif /* UNIV_SYNC_DEBUG */

	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx != NULL) {

		if (trx == in_trx) {

			return(TRUE);
		}

		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	return(FALSE);
}

/*********************************************************************
Writes the value of max_trx_id to the file based trx system header. */

void
trx_sys_flush_max_trx_id(void)
/*==========================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	mlog_write_dulint(sys_header + TRX_SYS_TRX_ID_STORE,
				trx_sys->max_trx_id, &mtr);
	mtr_commit(&mtr);
}

/*********************************************************************
Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction just being committed. In a MySQL
replication slave updates the latest master binlog position up to which
replication has proceeded. */

void
trx_sys_update_mysql_binlog_offset(
/*===============================*/
	const char*	file_name,/* in: MySQL log file name */
	ib_longlong	offset,	/* in: position in that log file */
	ulint		field,	/* in: offset of the MySQL log info field in
				the trx sys header */
	mtr_t*		mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;

	if (ut_strlen(file_name) >= TRX_SYS_MYSQL_LOG_NAME_LEN) {

		/* We cannot fit the name to the 512 bytes we have reserved */

		return;
	}

	sys_header = trx_sysf_get(mtr);

	if (mach_read_from_4(sys_header + field
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	   != TRX_SYS_MYSQL_LOG_MAGIC_N) {

	   	mlog_write_ulint(sys_header + field
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD,
				TRX_SYS_MYSQL_LOG_MAGIC_N,
				MLOG_4BYTES, mtr);
	}

	if (0 != strcmp((char*) (sys_header + field + TRX_SYS_MYSQL_LOG_NAME), file_name)) {

		mlog_write_string(sys_header + field
					+ TRX_SYS_MYSQL_LOG_NAME,
			(byte*) file_name, 1 + ut_strlen(file_name), mtr);
	}

	if (mach_read_from_4(sys_header + field
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH) > 0
	   || (offset >> 32) > 0) {
				
		mlog_write_ulint(sys_header + field
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH,
				(ulint)(offset >> 32),
				MLOG_4BYTES, mtr);
	}

	mlog_write_ulint(sys_header + field
					+ TRX_SYS_MYSQL_LOG_OFFSET_LOW,
				(ulint)(offset & 0xFFFFFFFFUL),
				MLOG_4BYTES, mtr);
}

/*********************************************************************
Prints to stderr the MySQL binlog info in the system header if the
magic number shows it valid. */

void
trx_sys_print_mysql_binlog_offset_from_page(
/*========================================*/
	byte*	page)	/* in: buffer containing the trx system header page,
			i.e., page number TRX_SYS_PAGE_NO in the tablespace */
{
	trx_sysf_t*	sys_header;

	sys_header = page + TRX_SYS;
	
	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	   == TRX_SYS_MYSQL_LOG_MAGIC_N) {

		fprintf(stderr,
	"ibbackup: Last MySQL binlog file position %lu %lu, file name %s\n",
		(ulong) mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH),
		(ulong) mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_LOW),
		sys_header + TRX_SYS_MYSQL_LOG_INFO + TRX_SYS_MYSQL_LOG_NAME);
	}
}

/*********************************************************************
Stores the MySQL binlog offset info in the trx system header if
the magic number shows it valid, and print the info to stderr */

void
trx_sys_print_mysql_binlog_offset(void)
/*===================================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;
	ulong           trx_sys_mysql_bin_log_pos_high, trx_sys_mysql_bin_log_pos_low;

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	   != TRX_SYS_MYSQL_LOG_MAGIC_N) {

		mtr_commit(&mtr);

		return;
	}

        trx_sys_mysql_bin_log_pos_high = mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
                                                    + TRX_SYS_MYSQL_LOG_OFFSET_HIGH);
        trx_sys_mysql_bin_log_pos_low  = mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
                                                    + TRX_SYS_MYSQL_LOG_OFFSET_LOW);

        trx_sys_mysql_bin_log_pos      =  (((ib_longlong)trx_sys_mysql_bin_log_pos_high) << 32) +
          (ib_longlong)trx_sys_mysql_bin_log_pos_low;

        ut_memcpy(trx_sys_mysql_bin_log_name, sys_header + TRX_SYS_MYSQL_LOG_INFO +
                  TRX_SYS_MYSQL_LOG_NAME, TRX_SYS_MYSQL_LOG_NAME_LEN);

        fprintf(stderr,
                "InnoDB: Last MySQL binlog file position %lu %lu, file name %s\n",
                trx_sys_mysql_bin_log_pos_high, trx_sys_mysql_bin_log_pos_low,
                trx_sys_mysql_bin_log_name);
        
	mtr_commit(&mtr);
}

/*********************************************************************
Prints to stderr the MySQL master log offset info in the trx system header if
the magic number shows it valid. */

void
trx_sys_print_mysql_master_log_pos(void)
/*====================================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;
	
	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	   != TRX_SYS_MYSQL_LOG_MAGIC_N) {

		mtr_commit(&mtr);

		return;
	}

	fprintf(stderr,
"InnoDB: In a MySQL replication slave the last master binlog file\n"
"InnoDB: position %lu %lu, file name %s\n",
		(ulong) mach_read_from_4(sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH),
		(ulong) mach_read_from_4(sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_LOW),
		sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
						+ TRX_SYS_MYSQL_LOG_NAME);
	/* Copy the master log position info to global variables we can
	use in ha_innobase.cc to initialize glob_mi to right values */

	ut_memcpy(trx_sys_mysql_master_log_name,
		sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
						+ TRX_SYS_MYSQL_LOG_NAME,
			TRX_SYS_MYSQL_LOG_NAME_LEN);

	trx_sys_mysql_master_log_pos = 
		(((ib_longlong)mach_read_from_4(
			sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH))
		<< 32)
		+ (ib_longlong)
		mach_read_from_4(sys_header + TRX_SYS_MYSQL_MASTER_LOG_INFO
					+ TRX_SYS_MYSQL_LOG_OFFSET_LOW);
	mtr_commit(&mtr);
}

/********************************************************************
Looks for a free slot for a rollback segment in the trx system file copy. */

ulint
trx_sysf_rseg_find_free(
/*====================*/
			/* out: slot index or ULINT_UNDEFINED if not found */
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		page_no;
	ulint		i;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(kernel_mutex)));
#endif /* UNIV_SYNC_DEBUG */

	sys_header = trx_sysf_get(mtr);

	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no == FIL_NULL) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}
	
/*********************************************************************
Creates the file page for the transaction system. This function is called only
at the database creation, before trx_sys_init. */
static
void
trx_sysf_create(
/*============*/
	mtr_t*	mtr)	/* in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		slot_no;
	page_t*		page;
	ulint		page_no;
	ulint		i;
	
	ut_ad(mtr);

	/* Note that below we first reserve the file space x-latch, and
	then enter the kernel: we must do it in this order to conform
	to the latching order rules. */

	mtr_x_lock(fil_space_get_latch(TRX_SYS_SPACE), mtr);
	mutex_enter(&kernel_mutex);

	/* Create the trx sys file block in a new allocated file segment */
	page = fseg_create(TRX_SYS_SPACE, 0, TRX_SYS + TRX_SYS_FSEG_HEADER,
					    				mtr);
	ut_a(buf_frame_get_page_no(page) == TRX_SYS_PAGE_NO);

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_TRX_SYS_HEADER);
#endif /* UNIV_SYNC_DEBUG */

	sys_header = trx_sysf_get(mtr);

	/* Start counting transaction ids from number 1 up */
	mlog_write_dulint(sys_header + TRX_SYS_TRX_ID_STORE,
				ut_dulint_create(0, 1), mtr);

	/* Reset the rollback segment slots */
	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {

		trx_sysf_rseg_set_page_no(sys_header, i, FIL_NULL, mtr);
	}

	/* Create the first rollback segment in the SYSTEM tablespace */
	page_no = trx_rseg_header_create(TRX_SYS_SPACE, ULINT_MAX, &slot_no,
									mtr);
	ut_a(slot_no == TRX_SYS_SYSTEM_RSEG_ID);
	ut_a(page_no != FIL_NULL);

	mutex_exit(&kernel_mutex);
}

/*********************************************************************
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started. */

void
trx_sys_init_at_db_start(void)
/*==========================*/
{
	trx_sysf_t*	sys_header;
	ib_longlong	rows_to_undo	= 0;
	const char*	unit		= "";
	trx_t*		trx;
	mtr_t		mtr;

	mtr_start(&mtr);
	
	ut_ad(trx_sys == NULL);

	mutex_enter(&kernel_mutex);

	trx_sys = mem_alloc(sizeof(trx_sys_t));
	
	sys_header = trx_sysf_get(&mtr);

	trx_rseg_list_and_array_init(sys_header, &mtr);
	
	trx_sys->latest_rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	/* VERY important: after the database is started, max_trx_id value is
	divisible by TRX_SYS_TRX_ID_WRITE_MARGIN, and the 'if' in
	trx_sys_get_new_trx_id will evaluate to TRUE when the function
	is first time called, and the value for trx id will be written
	to the disk-based header! Thus trx id values will not overlap when
	the database is repeatedly started! */

	trx_sys->max_trx_id = ut_dulint_add(
			      	ut_dulint_align_up(
					mtr_read_dulint(sys_header
						+ TRX_SYS_TRX_ID_STORE, &mtr),
					TRX_SYS_TRX_ID_WRITE_MARGIN),
				2 * TRX_SYS_TRX_ID_WRITE_MARGIN);

	UT_LIST_INIT(trx_sys->mysql_trx_list);				
	trx_lists_init_at_db_start();

	if (UT_LIST_GET_LEN(trx_sys->trx_list) > 0) {
		trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

		for (;;) {

			if ( trx->conc_state != TRX_PREPARED) {
			    rows_to_undo +=
				ut_conv_dulint_to_longlong(trx->undo_no);
			}

			trx = UT_LIST_GET_NEXT(trx_list, trx);

			if (!trx) {
				break;
			}
		}
	
		if (rows_to_undo > 1000000000) {
			unit = "M";
			rows_to_undo = rows_to_undo / 1000000;
		}

		fprintf(stderr,
"InnoDB: %lu transaction(s) which must be rolled back or cleaned up\n"
"InnoDB: in total %lu%s row operations to undo\n",
				(ulong) UT_LIST_GET_LEN(trx_sys->trx_list),
				(ulong) rows_to_undo, unit);

		fprintf(stderr, "InnoDB: Trx id counter is %lu %lu\n", 
			(ulong) ut_dulint_get_high(trx_sys->max_trx_id),
			(ulong) ut_dulint_get_low(trx_sys->max_trx_id));
	}

	UT_LIST_INIT(trx_sys->view_list);

	trx_purge_sys_create();

	mutex_exit(&kernel_mutex);

	mtr_commit(&mtr);
}

/*********************************************************************
Creates and initializes the transaction system at the database creation. */

void
trx_sys_create(void)
/*================*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	trx_sysf_create(&mtr);

	mtr_commit(&mtr);

	trx_sys_init_at_db_start();
}
