/************************************************************************
The test module for file space management

(c) 1996 Innobase Oy

Created 1/4/1996 Heikki Tuuri
*************************************************************************/

#include "string.h"

#include "os0thread.h"
#include "os0file.h"
#include "ut0ut.h"
#include "ut0byte.h"
#include "sync0sync.h"
#include "mem0mem.h"
#include "fil0fil.h"
#include "mach0data.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "log0log.h"
#include "fut0lst.h"
#include "fut0fut.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "..\fsp0fsp.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;
ulint		page_nos[10000];

#define N_SPACES	1
#define N_FILES		2
#define FILE_SIZE	1000 	/* must be > 512 */
#define POOL_SIZE	1000
#define	COUNTER_OFFSET	1500

#define LOOP_SIZE	150
#define	N_THREADS	5


ulint zero = 0;

buf_block_t*	bl_arr[POOL_SIZE];

/************************************************************************
Io-handler thread function. */

ulint
handler_thread(
/*===========*/
	void*	arg)
{
	ulint	segment;
	void*	mess;
	ulint	i;
	bool	ret;
	
	segment = *((ulint*)arg);

	printf("Io handler thread %lu starts\n", segment);

	for (i = 0;; i++) {
		ret = fil_aio_wait(segment, &mess);
		ut_a(ret);

		buf_page_io_complete((buf_block_t*)mess);
		
		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
		
	}

	return(0);
}

/*************************************************************************
Creates the files for the file system test and inserts them to
the file system. */

void
create_files(void)
/*==============*/
{
	bool		ret;
	ulint		i, k;
	char		name[20];
	os_thread_t	thr[5];
	os_thread_id_t	id[5];

	printf("--------------------------------------------------------\n");
	printf("Create or open database files\n");

	strcpy(name, "tsfile00");

	for (k = 0; k < N_SPACES; k++) {
	for (i = 0; i < N_FILES; i++) {

		name[6] = (char)((ulint)'0' + k);
		name[7] = (char)((ulint)'0' + i);
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
					OS_FILE_TABLESPACE, &ret);

		if (ret == FALSE) {
			ut_a(os_file_get_last_error() ==
						OS_FILE_ALREADY_EXISTS);
	
			files[i] = os_file_create(
				name, OS_FILE_OPEN,
						OS_FILE_TABLESPACE, &ret);

			ut_a(ret);
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, k, OS_FILE_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, FILE_SIZE, k);
	}
	}

	ios = 0;

	mutex_create(&ios_mutex);
	
	for (i = 0; i < 5; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}
}

/************************************************************************
Creates the test database files. */

void 
create_db(void)
/*===========*/
{
	ulint			i;
	buf_block_t*		block;
	byte*			frame;
	ulint			j;
	ulint			tm, oldtm;
	mtr_t			mtr;
	
	printf("--------------------------------------------------------\n");
	printf("Write database pages\n");

	oldtm = ut_clock();

	for (i = 0; i < N_SPACES; i++) {
		for (j = 0; j < FILE_SIZE * N_FILES; j++) {
			mtr_start(&mtr);
		
			block = buf_page_create(i, j, &mtr);

			frame = buf_block_get_frame(block);

			buf_page_x_lock(block, &mtr);

			if (j > FILE_SIZE * N_FILES
				- 64 * 2 - 1) {
				mlog_write_ulint(frame + FIL_PAGE_PREV, j - 5,
						MLOG_4BYTES, &mtr);
				mlog_write_ulint(frame + FIL_PAGE_NEXT, j - 7,
						MLOG_4BYTES, &mtr);
			} else {
				mlog_write_ulint(frame + FIL_PAGE_PREV, j - 1,
						MLOG_4BYTES, &mtr);
				mlog_write_ulint(frame + FIL_PAGE_NEXT, j + 1,
						MLOG_4BYTES, &mtr);
			}
					
			mlog_write_ulint(frame + FIL_PAGE_OFFSET, j,
					MLOG_4BYTES, &mtr);
			mlog_write_ulint(frame + FIL_PAGE_SPACE, i,
					MLOG_4BYTES, &mtr);
			mlog_write_ulint(frame + COUNTER_OFFSET, 0,
					MLOG_4BYTES, &mtr);

			mtr_commit(&mtr);			
		}
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
}
	
/************************************************************************
Reads the test database files. */

void 
test1(void)
/*=======*/
{
	ulint			i, j, k;
	buf_block_t*		block;
	byte*			frame;
	ulint			tm, oldtm;
	mtr_t			mtr;
	
	printf("--------------------------------------------------------\n");
	printf("TEST 1. Read linearly database files\n");

	oldtm = ut_clock();
	
	for (k = 0; k < 1; k++) {
	for (i = 0; i < N_SPACES; i++) {
		for (j = 0; j < N_FILES * FILE_SIZE; j++) {
			mtr_start(&mtr);
		
			block = buf_page_get(i, j, &mtr);

			frame = buf_block_get_frame(block);

			buf_page_s_lock(block, &mtr);

			ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr)
					== j);
			ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);
			
			mtr_commit(&mtr);
		}
	}
	}

	tm = ut_clock();
	printf("Wall clock time for %lu pages %lu milliseconds\n",
			k * i * j, tm - oldtm);
	buf_validate();
}

/************************************************************************
Test for file-based lists. */

void
test2(void)
/*=======*/
{
	mtr_t			mtr;
	buf_frame_t*		frame;
	buf_block_t*		block;
	ulint			i;
	flst_base_node_t*	base1;
	fil_addr_t		base_addr1;
	flst_base_node_t*	base2;
	fil_addr_t		base_addr2;
	flst_node_t*		node;
	fil_addr_t		node_addr;
	flst_node_t*		node2;
	fil_addr_t		node_addr2;
	flst_node_t*		node3;
	fil_addr_t		node_addr3;

#define	BPAGE	10
#define BASE1	300
#define BASE2	500
#define	NODE1	800
#define	NODE2	900
#define	NODE3	1000
#define	NODE4	1100
#define INDEX	30
	
	buf_validate();

	mtr_start(&mtr);
		
	block = buf_page_get(0, BPAGE, &mtr);
	frame = buf_block_get_frame(block);

	flst_init(frame + BASE1, &mtr);
	flst_init(frame + BASE2, &mtr);
	
	mtr_commit(&mtr);

	printf("-------------------------------------------\n");
	printf("TEST 2. Test of file-based two-way lists \n");
	
	base_addr1.page = BPAGE;
	base_addr1.boffset = BASE1;

	base_addr2.page = BPAGE;
	base_addr2.boffset = BASE2;

	printf(
   "Add 1000 elements in list1 in reversed order and in list2 in order\n");
	for (i = 0; i < 1000; i++) {
		mtr_start(&mtr);

		base1 = fut_get_ptr(0, base_addr1, &mtr);
		base2 = fut_get_ptr(0, base_addr2, &mtr);

		block = buf_page_get(0, i, &mtr);
		frame = buf_block_get_frame(block);

		buf_page_x_lock(block, &mtr);

		flst_add_first(base1, frame + NODE1, &mtr);
		mlog_write_ulint(frame + NODE1 + INDEX, i,
				MLOG_4BYTES, &mtr);
		flst_add_last(base2, frame + NODE2, &mtr);
		mlog_write_ulint(frame + NODE2 + INDEX, i,
				MLOG_4BYTES, &mtr);

		mtr_commit(&mtr);
	}

	mtr_start(&mtr);

	base1 = fut_get_ptr(0, base_addr1, &mtr);
	base2 = fut_get_ptr(0, base_addr2, &mtr);

	flst_validate(base1, &mtr);
	flst_validate(base2, &mtr);

	flst_print(base1, &mtr);
	flst_print(base2, &mtr);

	mtr_commit(&mtr);

	mtr_start(&mtr);

	base1 = fut_get_ptr_s_lock(0, base_addr1, &mtr);

	node_addr = flst_get_first(base1, &mtr);

	mtr_commit(&mtr);

	printf("Check order of elements in list1\n");
	for (i = 0; i < 1000; i++) {
		mtr_start(&mtr);
		ut_a(!fil_addr_is_null(node_addr));
		
		node = fut_get_ptr_x_lock(0, node_addr, &mtr);
	
		ut_a(mtr_read_ulint(node + INDEX, MLOG_4BYTES, &mtr) ==
			999 - i);

		node_addr = flst_get_next_addr(node, &mtr);
		mtr_commit(&mtr);
	}

	ut_a(fil_addr_is_null(node_addr));

	mtr_start(&mtr);

	base2 = fut_get_ptr_s_lock(0, base_addr2, &mtr);

	node_addr = flst_get_first(base2, &mtr);

	mtr_commit(&mtr);

	printf("Check order of elements in list2\n");
	for (i = 0; i < 1000; i++) {
		mtr_start(&mtr);
		ut_a(!fil_addr_is_null(node_addr));
		
		node = fut_get_ptr_x_lock(0, node_addr, &mtr);
	
		ut_a(mtr_read_ulint(node + INDEX, MLOG_4BYTES, &mtr)
			== i);

		node_addr = flst_get_next_addr(node, &mtr);
		mtr_commit(&mtr);
	}

	ut_a(fil_addr_is_null(node_addr));

	mtr_start(&mtr);

	base1 = fut_get_ptr(0, base_addr1, &mtr);
	base2 = fut_get_ptr(0, base_addr2, &mtr);

	flst_validate(base1, &mtr);
	flst_validate(base2, &mtr);

	mtr_commit(&mtr);

	mtr_start(&mtr);

	base1 = fut_get_ptr_s_lock(0, base_addr1, &mtr);

	node_addr = flst_get_first(base1, &mtr);

	mtr_commit(&mtr);

	for (i = 0; i < 500; i++) {
		mtr_start(&mtr);
		ut_a(!fil_addr_is_null(node_addr));
		
		node = fut_get_ptr_x_lock(0, node_addr, &mtr);

		node_addr = flst_get_next_addr(node, &mtr);
		mtr_commit(&mtr);
	}

	printf("Add 200 elements to the middle of list1\n");	
	for (i = 0; i < 100; i++) {
		mtr_start(&mtr);
		
		node = fut_get_ptr_x_lock(0, node_addr, &mtr);

		node_addr2.page = i;
		node_addr2.boffset = NODE3;
		node2 = fut_get_ptr_x_lock(0, node_addr2, &mtr);

		node_addr3.page = i;
		node_addr3.boffset = NODE4;
		node3 = fut_get_ptr_x_lock(0, node_addr3, &mtr);

		mlog_write_ulint(node2 + INDEX, 99 - i, MLOG_4BYTES, &mtr);

		block = buf_page_get(0, BPAGE, &mtr);
		frame = buf_block_get_frame(block);

		base1 = frame + BASE1;
		
		flst_insert_after(base1, node, node2, &mtr);
		flst_insert_before(base1, node3, node, &mtr);

		if (i % 17 == 0) {
			flst_validate(base1, &mtr);
		}
		mtr_commit(&mtr);
	}

	printf("Check that 100 of the inserted nodes are in order\n");
	mtr_start(&mtr);
	ut_a(!fil_addr_is_null(node_addr));
		
	node = fut_get_ptr_x_lock(0, node_addr, &mtr);

	node_addr = flst_get_next_addr(node, &mtr);
	mtr_commit(&mtr);

	for (i = 0; i < 100; i++) {
		mtr_start(&mtr);
		ut_a(!fil_addr_is_null(node_addr));
		
		node = fut_get_ptr_x_lock(0, node_addr, &mtr);
	
		ut_a(mtr_read_ulint(node + INDEX, MLOG_4BYTES, &mtr)
			== i);

		node_addr = flst_get_next_addr(node, &mtr);
		mtr_commit(&mtr);
	}

	printf("Remove 899 elements from the middle of list1\n");
	mtr_start(&mtr);

	base1 = fut_get_ptr_x_lock(0, base_addr1, &mtr);

	node_addr = flst_get_first(base1, &mtr);

	flst_print(base1, &mtr);
	mtr_commit(&mtr);

	for (i = 0; i < 300; i++) {
		mtr_start(&mtr);
		ut_a(!fil_addr_is_null(node_addr));
		
		node = fut_get_ptr_x_lock(0, node_addr, &mtr);

		node_addr = flst_get_next_addr(node, &mtr);
		mtr_commit(&mtr);
	}

	for (i = 0; i < 899; i++) {

		mtr_start(&mtr);

		base1 = fut_get_ptr_x_lock(0, base_addr1, &mtr);

		node_addr = flst_get_first(base1, &mtr);

		node = fut_get_ptr_x_lock(0, node_addr, &mtr);

		node_addr = flst_get_next_addr(node, &mtr);

		ut_a(!fil_addr_is_null(node_addr));

		node2 = fut_get_ptr_x_lock(0, node_addr, &mtr);

		flst_remove(base1, node2, &mtr);

		if (i % 17 == 0) {
			flst_validate(base1, &mtr);
		}
		
		mtr_commit(&mtr);
	}

	printf("Remove 301 elements from the start of list1\n");
	for (i = 0; i < 301; i++) {

		mtr_start(&mtr);

		base1 = fut_get_ptr_x_lock(0, base_addr1, &mtr);

		node_addr = flst_get_first(base1, &mtr);

		node = fut_get_ptr_x_lock(0, node_addr, &mtr);

		flst_remove(base1, node, &mtr);

		if (i % 17 == 0) {
			flst_validate(base1, &mtr);
		}

		mtr_commit(&mtr);
	}

	mtr_start(&mtr);

	base1 = fut_get_ptr_x_lock(0, base_addr1, &mtr);

	ut_a(flst_get_len(base1, &mtr) == 0);
	flst_print(base1, &mtr);

	mtr_commit(&mtr);
}

/************************************************************************
Inits space header of space 0. */

void
init_space(void)
/*============*/
{
	mtr_t		mtr;

	printf("Init space header\n");
	
	mtr_start(&mtr);

	fsp_header_init(0, FILE_SIZE * N_FILES, &mtr);		

	mtr_commit(&mtr);
}

/************************************************************************
Test for file space management. */

void
test5(void)
/*=======*/
{
	mtr_t		mtr;
	ulint		seg_page;
	ulint		new_page;
	ulint		seg_page2;
	ulint		new_page2;
	ulint		seg_page3;
	buf_block_t*	block;
	bool		finished;
	ulint		i;
	ulint		reserved;
	ulint		used;
	ulint		tm, oldtm;
	
	buf_validate();

	fsp_validate(0);
	fsp_print(0);
	
	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, &mtr);

	mtr_commit(&mtr);

	fsp_validate(0);

	buf_validate();
	printf("Segment created: header page %lu, byte offset %lu\n",
						seg_page, 1000);
	fsp_print(0);

	mtr_start(&mtr);

	block = buf_page_get(0, seg_page, &mtr);
	
	new_page = fseg_alloc_free_page(buf_block_get_frame(block) + 1000,
					2, FSP_UP, &mtr);	
	
	mtr_commit(&mtr);

	fsp_print(0);
	fsp_validate(0);
	buf_validate();
	printf("Segment page allocated %lu\n", new_page);


	mtr_start(&mtr);

	block = buf_page_get(0, seg_page, &mtr);
	
	fseg_free_page(buf_block_get_frame(block) + 1000,
					0, new_page, &mtr);	
	
	mtr_commit(&mtr);

	fsp_validate(0);
	printf("Segment page freed %lu\n", new_page);
	
	finished = FALSE;

	while (!finished) {
	
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		finished = fseg_free_step(
			buf_block_get_frame(block) + 1000, &mtr);	
	
		mtr_commit(&mtr);
	}
	fsp_validate(0);

	/***********************************************/
	buf_validate();
	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, &mtr);

	mtr_commit(&mtr);

	ut_a(seg_page == 2);
	
	printf("Segment created: header page %lu\n", seg_page);

	new_page = seg_page;
	for (i = 0; i < 511; i++) {

		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		new_page = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page + 1, FSP_UP, &mtr);
		printf("%lu %lu; ", i, new_page);
		if (i % 10 == 0) {
			printf("\n");
		}
	
		mtr_commit(&mtr);

		if (i % 117 == 0) {
			fsp_validate(0);
		}
	}

	fsp_validate(0);
	buf_validate();

	mtr_start(&mtr);

	block = buf_page_get(0, seg_page, &mtr);

	reserved = fseg_n_reserved_pages(buf_block_get_frame(block) + 1000,
					&used, &mtr);

	ut_a(used == 512);	
	ut_a(reserved >= 512);	

	printf("Pages used in segment %lu reserved by segment %lu \n",
		used, reserved);
	
	mtr_commit(&mtr);

	finished = FALSE;

	while (!finished) {
		i++;
		
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		finished = fseg_free_step(
			buf_block_get_frame(block) + 1000, &mtr);	
	
		mtr_commit(&mtr);

		if (i % 117 == 0) {
			fsp_validate(0);
		}
	}

	fsp_validate(0);
	buf_validate();

	/***********************************************/

	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, &mtr);

	mtr_commit(&mtr);

	ut_a(seg_page == 2);

	mtr_start(&mtr);

	seg_page2 = fseg_create(0, 0, 1000, &mtr);

	mtr_commit(&mtr);

	ut_a(seg_page2 == 3);
	
	new_page = seg_page;
	new_page2 = seg_page2;

	for (;;) {

		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		new_page = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page + 1, FSP_UP, &mtr);

		printf("1:%lu %lu; ", i, new_page);
		if (i % 10 == 0) {
			printf("\n");
		}
	
		new_page = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page + 1, FSP_UP, &mtr);

		printf("1:%lu %lu; ", i, new_page);
		if (i % 10 == 0) {
			printf("\n");
		}
		
		mtr_commit(&mtr);

		i++;
		if (i % 217 == 0) {
			fsp_validate(0);
		}
		
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page2, &mtr);
	
		new_page2 = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page2 + 1, FSP_DOWN, &mtr);

		printf("2:%lu %lu; ", i, new_page2);
		if (i % 10 == 0) {
			printf("\n");
		}
	
		mtr_commit(&mtr);

		if (new_page2 == FIL_NULL) {
			break;
		}
	}

	mtr_start(&mtr);

	block = buf_page_get(0, seg_page, &mtr);

	reserved = fseg_n_reserved_pages(buf_block_get_frame(block) + 1000,
					&used, &mtr);

	printf("Pages used in segment 1 %lu, reserved by segment %lu \n",
		used, reserved);
	
	mtr_commit(&mtr);
	fsp_validate(0);

	mtr_start(&mtr);

	block = buf_page_get(0, seg_page2, &mtr);

	reserved = fseg_n_reserved_pages(buf_block_get_frame(block) + 1000,
					&used, &mtr);

	printf("Pages used in segment 2 %lu, reserved by segment %lu \n",
		used, reserved);
	
	mtr_commit(&mtr);

	fsp_print(0);
	
	for (;;) {

		i++;
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		block = buf_page_get(0, seg_page2, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);

		mtr_commit(&mtr);

		if (finished) {
			break;
		}

		if (i % 117 == 0) {
			fsp_validate(0);
		}
	}

	fsp_validate(0);

	mtr_start(&mtr);

	seg_page3 = fseg_create(0, 0, 1000, &mtr);
	page_nos[0] = seg_page3;
	new_page2 = seg_page3;

	mtr_commit(&mtr);

	for (i = 1; i < 250; i++) {
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page3, &mtr);
	
		new_page2 = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page2 + 1, FSP_UP, &mtr);
		page_nos[i] = new_page2;

		mtr_commit(&mtr);
	}

	/*************************************************/

	mtr_start(&mtr);

	fseg_create(0, seg_page3, 1500, &mtr);

	mtr_commit(&mtr);

	for (i = 0; i < 250; i++) {
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page3, &mtr);
	
		new_page2 = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1500,
					new_page2 + 1, FSP_UP, &mtr);
		page_nos[i] = new_page2;

		mtr_commit(&mtr);
	}

	printf("---------------------------------------------------------\n");
	printf("TEST 5A13. Test free_step.\n");

	fseg_free(0, seg_page3, 1500);
	
	printf("---------------------------------------------------------\n");
	printf("TEST 5A3. Test free_step.\n");

	for (;;) {

		mtr_start(&mtr);

		block = buf_page_get(0, seg_page3, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		mtr_commit(&mtr);

		if (finished) {
			break;
		}
	}

	/***************************************************/	

	mtr_start(&mtr);

	seg_page2 = fseg_create(0, 0, 1000, &mtr);
	page_nos[0] = seg_page2;
	new_page2 = seg_page2;

	mtr_commit(&mtr);

	i = 1;
	for (;;) {
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page2, &mtr);
	
		new_page2 = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page2 + 1, FSP_UP, &mtr);
		page_nos[i] = new_page2;
/*
		printf("%lu %lu; ", i, new_page2);
*/
		mtr_commit(&mtr);

		if (new_page2 == FIL_NULL) {
			break;
		}
		i++;
	}

	printf("---------------------------------------------------------\n");
	printf("TEST 5D. Test free_step.\n");

	for (;;) {

		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		mtr_commit(&mtr);

		if (finished) {
			break;
		}
	}

	for (;;) {

		mtr_start(&mtr);

		block = buf_page_get(0, seg_page2, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		mtr_commit(&mtr);

		if (finished) {
			break;
		}
	}

	
	/***************************************/
	
	oldtm = ut_clock();

	fsp_validate(0);

    for (i = 0; i < 10; i++) {	
	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, &mtr);

	mtr_commit(&mtr);

	mtr_start(&mtr);

	block = buf_page_get(0, seg_page, &mtr);
	
	new_page = fseg_alloc_free_page(buf_block_get_frame(block) + 1000,
					3, FSP_UP, &mtr);	
	
	mtr_commit(&mtr);

	finished = FALSE;

	while (!finished) {
	
		mtr_start(&mtr);

		block = buf_page_get(0, seg_page, &mtr);
	
		finished = fseg_free_step(
			buf_block_get_frame(block) + 1000, &mtr);	
	
		mtr_commit(&mtr);
	}
    }

	tm = ut_clock();
	printf("Wall clock time for %lu seg crea+free %lu millisecs\n",
			i, tm - oldtm);

	buf_validate();
	fsp_validate(0);
	fsp_print(0);

	buf_flush_batch(BUF_FLUSH_LIST, 2000);
	os_thread_sleep(3000000);

/*	buf_print(); */
	buf_all_freed();
}	

/************************************************************************
Random test thread function. */

ulint
random_thread(
/*===========*/
	void*	arg)
{
	ulint		n;
	ulint		i, j, t, p, sp, d, b;
	ulint		s;
	ulint		arr[FILE_SIZE * N_FILES];
	ulint		seg_page;
	fseg_header_t*	seg_header;
	fil_addr_t	seg_addr;
	byte		dir;
	ulint		k;
	mtr_t		mtr;
	bool		finished;
	ulint		used;
	ulint		reserved;
	
	n = *((ulint*)arg);
	n = os_thread_get_curr_id();

	printf("Random test thread %lu starts\n", n);

	for (i = 0; i < 30; i++) {
	   t = ut_rnd_gen_ulint() % 10;
	   s = ut_rnd_gen_ulint() % FILE_SIZE * N_FILES;
	   p = 0;
	   sp = ut_rnd_gen_ulint() % N_SPACES;
	   d = ut_rnd_gen_ulint() % 3;
	   b = ut_rnd_gen_ulint() % 3;
	   
	   if (i % 10 == 0) {
	   	printf("Thr %lu round %lu starts\n", n, i);
	   }
	   ut_a(buf_validate());

	   if (t != 0) {
		do {
		mtr_start(&mtr);
		seg_page = fseg_create(sp, p, 1000, &mtr);
		mtr_commit(&mtr);
		} while (seg_page == FIL_NULL);
		
		seg_addr.page = seg_page;
		seg_addr.boffset = 1000;

		k = 0;
		j = 0;
	   	while (j < s) {
	   		j++;
	   		if (d == 0) {
	   			dir = FSP_DOWN;
	   		} else if (d == 1) {
	   			dir = FSP_NO_DIR;
	   		} else {
	   			dir = FSP_UP;
	   		}
			mtr_start(&mtr);
			seg_header = fut_get_ptr(sp, seg_addr, &mtr);

			if (b != 0) {
	   			arr[k] = fseg_alloc_free_page(seg_header,
						p, dir, &mtr);
	   			k++;
	   		} else if (k > 0) {
				fseg_free_page(seg_header, sp, arr[k - 1],
						&mtr);
				k--;
			} 	

	   		mtr_commit(&mtr);
	   		if ((k > 0) && (arr[k - 1] == FIL_NULL)) {
	   			k--;
				break;
			}
			if (p > 0) {
				p = arr[k - 1] + dir - 1;
			}
			if (j % 577 == 0) {
				if (k > 0) {
					p = arr[k / 2] + 1;
				} else {
					p = 0;
				}
				d = ut_rnd_gen_ulint() % 3;
	   			b = ut_rnd_gen_ulint() % 3;
			}	
	   	}
		finished = FALSE;
		mtr_start(&mtr);

		seg_header = fut_get_ptr(sp, seg_addr, &mtr);
	
		reserved = fseg_n_reserved_pages(seg_header,
					&used, &mtr);

		printf("Pages used in segment %lu reserved by segment %lu \n",
			used, reserved);
	
		mtr_commit(&mtr);

		printf("Thread %lu starts releasing seg %lu size %lu\n", n,
				seg_addr.page, k);
		while (!finished) {
			mtr_start(&mtr);
			seg_header = fut_get_ptr(sp, seg_addr, &mtr);
	
			finished = fseg_free_step(seg_header, &mtr);	
			mtr_commit(&mtr);
		}
	   } else {
		fsp_print(sp);
		printf("Thread %lu validates fsp\n", n);
	   	fsp_validate(sp);
	   	buf_validate();
	   }
	} /* for i */
	printf("\nRandom test thread %lu exits\n", os_thread_get_curr_id());
	return(0);
}

/*************************************************************************
Performs random operations on the buffer with several threads. */

void
test6(void)
/*=======*/
{
	ulint		i;
	os_thread_t	thr[N_THREADS + 1];
	os_thread_id_t	id[N_THREADS + 1];
	ulint		n[N_THREADS + 1];
	
	printf("--------------------------------------------------------\n");
	printf("TEST 6. Random multi-thread test on the buffer \n");

	incs = 0;
	mutex_create(&incs_mutex);
	
	for (i = 0; i < N_THREADS; i++) {
		n[i] = i;

		thr[i] = os_thread_create(random_thread, n + i, id + i);
	}

	for (i = 0; i < N_THREADS; i++) {
		os_thread_wait(thr[i]);
	}
}

/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	oldtm = ut_clock();
	
	os_aio_init(160, 5);
	sync_init();
	mem_init();
	fil_init(26);	/* Allow 25 open files at a time */
	buf_pool_init(POOL_SIZE, POOL_SIZE);
	log_init();
	fsp_init();
	
	buf_validate();

	ut_a(fil_validate());
	
	create_files();

	create_db();

	buf_validate();

/*	test1(); */
/*	buf_validate(); */
/*
	test2();
	buf_validate();
*/
	init_space();

	test5();
	buf_validate();

/*	test6(); */

	buf_flush_batch(BUF_FLUSH_LIST, POOL_SIZE + 1);
/*	buf_print(); */
	buf_validate();

	os_thread_sleep(1000000);
	
/*	buf_print(); */
	buf_all_freed();
	
	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
