/************************************************************************
The test module for the data dictionary

(c) 1996 Innobase Oy

Created 1/13/1996 Heikki Tuuri
*************************************************************************/

#include "sync0sync.h"
#include "mem0mem.h"
#include "buf0buf.h"
#include "data0type.h"
#include "..\dict0dict.h"

/************************************************************************
Basic test of data dictionary. */

void
test1(void)
/*=======*/
{
	dict_table_t*	table;
	dict_index_t*	index;

	table = dict_table_create("TS_TABLE1", 3);

	dict_table_add_col(table, "COL1", DATA_INT, 3, 4, 5);
	dict_table_add_col(table, "COL2", DATA_INT, 3, 4, 5);
	dict_table_add_col(table, "COL3", DATA_INT, 3, 4, 5);

	ut_a(0 == dict_table_publish(table));

	index = dict_index_create("TS_TABLE1", "IND1",
			DICT_UNIQUE | DICT_CLUSTERED | DICT_MIX, 2, 1);

	dict_index_add_field(index, "COL2", DICT_DESCEND);
	dict_index_add_field(index, "COL1", 0);

	ut_a(0 == dict_index_publish(index));

	dict_table_print(table);

	dict_table_free(table);

	ut_a(dict_all_freed());

	dict_free_all();

	ut_a(dict_all_freed());
}
	
/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	oldtm = ut_clock();
	
	sync_init();
	mem_init();
	buf_pool_init(100, 100);
	dict_init();	

	test1();

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
