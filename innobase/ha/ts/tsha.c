/************************************************************************
The test module for hash table

(c) 1994, 1995 Innobase Oy

Created 1/25/1994 Heikki Tuuri
*************************************************************************/

#include "ut0ut.h"
#include "ha0ha.h"
#include "mem0mem.h"
#include "sync0sync.h"

ulint	ulint_array[200000];

void 
test1(void)
{
	hash_table_t*	table1;
	ulint		i;
	ulint		n313			= 313;
	ulint		n414			= 414;
	
	printf("------------------------------------------------\n");
	printf("TEST 1. BASIC TEST\n");

	table1 = ha_create(50000);
	
	ha_insert_for_fold(table1, 313, &n313);
	
	ha_insert_for_fold(table1, 313, &n414);

	ut_a(ha_validate(table1));

	ha_delete(table1, 313, &n313);
	ha_delete(table1, 313, &n414);

	ut_a(ha_validate(table1));

	printf("------------------------------------------------\n");
	printf("TEST 2. TEST OF MASSIVE INSERTS AND DELETES\n");

	table1 = ha_create(10000);

	for (i = 0; i < 200000; i++) {
		ulint_array[i] = i;
	}

	for (i = 0; i < 50000; i++) {
		ha_insert_for_fold(table1, i * 7, ulint_array + i);
	}
	
	ut_a(ha_validate(table1));

	for (i = 0; i < 50000; i++) {
		ha_delete(table1, i * 7, ulint_array + i);
	}

	ut_a(ha_validate(table1));
}

void 
test2(void)
{
	hash_table_t*	table1;
	ulint		i;
	ulint		oldtm, tm;
	ha_node_t*	node;

	printf("------------------------------------------------\n");
	printf("TEST 3. SPEED TEST\n");

	table1 = ha_create(300000);
	
	oldtm = ut_clock();
	
	for (i = 0; i < 200000; i++) {
		ha_insert_for_fold(table1, i * 27877, ulint_array + i);
	}

	tm = ut_clock();

	printf("Wall clock time for %lu inserts %lu millisecs\n",
		i, tm - oldtm);
	
	oldtm = ut_clock();
	
	for (i = 0; i < 200000; i++) {
		node = ha_search(table1, i * 27877);
	}
	
	tm = ut_clock();

	printf("Wall clock time for %lu searches %lu millisecs\n",
		i, tm - oldtm);

	oldtm = ut_clock();

	for (i = 0; i < 200000; i++) {
		ha_delete(table1, i * 27877, ulint_array + i);
	}

	tm = ut_clock();

	printf("Wall clock time for %lu deletes %lu millisecs\n",
		i, tm - oldtm);
}

void 
main(void) 
{
	sync_init();
	mem_init(1000000);

	test1();

	test2();

	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
