/************************************************************************
The test module for the operating system interface
Auxiliary executable run alongside tsos.exe to test
process switching speed

(c) 1995 Innobase Oy

Created 9/27/1995 Heikki Tuuri
*************************************************************************/


#include "../os0thread.h"
#include "../os0shm.h"
#include "../os0proc.h"
#include "ut0ut.h"

/*********************************************************************
Test for shared memory and process switching through yield. */

void 
test2(void)
/*=======*/
{
	os_shm_t		shm;
	ulint			tm, oldtm;
	ulint*			pr_no;
	ulint			count;
	ulint			i;
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 2. Test of process switching through yield\n");


	shm = os_shm_create(1000, "TSOS_SHM");

	pr_no = os_shm_map(shm);

	count = 0;

	printf("Process 2 starts test!\n");
	
	oldtm = ut_clock();

	for (i = 0; i < 100000; i++) {
		if (*pr_no != 2) {
			count++;
			*pr_no = 2;
		}
		os_thread_yield();
	}

	tm = ut_clock();

	printf("Process 2 finishes test: %lu process switches noticed\n",
		count);
	
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);


	os_shm_unmap(shm);

	os_shm_free(shm);
}

/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	oldtm = ut_clock();

	test2();

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
	
	os_process_exit(0);
} 
