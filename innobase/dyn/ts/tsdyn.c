/************************************************************************
The test module for dynamic array

(c) 1996 Innobase Oy

Created 2/5/1996 Heikki Tuuri
*************************************************************************/

#include "../dyn0dyn.h"
#include "sync0sync.h"
#include "mem0mem.h"

/****************************************************************
Basic test. */

void
test1(void)
/*=======*/
{
	dyn_array_t	dyn;
	ulint		i;
	ulint*		ulint_ptr;

	printf("-------------------------------------------\n");
	printf("TEST 1. Basic test\n");

	dyn_array_create(&dyn);

	for (i = 0; i < 1000; i++) {
		ulint_ptr = dyn_array_push(&dyn, sizeof(ulint));
		*ulint_ptr = i;
	}

	ut_a(dyn_array_get_n_elements(&dyn) == 1000);
 
	for (i = 0; i < 1000; i++) {
		ulint_ptr = dyn_array_get_nth_element(&dyn, i, sizeof(ulint));
		ut_a(*ulint_ptr == i);
	}

	dyn_array_free(&dyn);
}

void 
main(void) 
{
	sync_init();
	mem_init();

	test1();
	
	ut_ad(sync_all_freed());

	ut_ad(mem_all_freed());
	
	printf("TEST SUCCESSFULLY COMPLETED!\n");
} 
