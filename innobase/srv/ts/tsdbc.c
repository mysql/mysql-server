/************************************************************************
Database client test program

(c) 1995 Innobase Oy

Created 10/10/1995 Heikki Tuuri
*************************************************************************/

#include "com0com.h"
#include "com0shm.h"
#include "ut0ut.h"
#include "mem0mem.h"
#include "os0thread.h"
#include "sync0ipm.h"
#include "sync0sync.h"

byte	buf[10000];
char	addr[150];

void
test1(void)
/*=======*/
{
	com_endpoint_t*		ep;
	ulint			ret;
	ulint			size;
	ulint			len;
	ulint			addr_len;
	ulint			i, j;
	ulint			tm, oldtm;
	

	oldtm = ut_clock();

     	for (i = 0; i < 10000; i++) {

		ut_delay(100);
     	}

     	for (j = 0; j < i / 10; j++) {

		ut_delay(200);
     	}
	
	tm = ut_clock();
	printf("Wall clock time for test without server %ld milliseconds\n",
			tm - oldtm);
	printf("%lu rounds\n", i);
	
	ep = com_endpoint_create(COM_SHM);

	ut_a(ep);

	size = 8192;
	
	ret = com_endpoint_set_option(ep, COM_OPT_MAX_DGRAM_SIZE,
				      (byte*)&size, 0);

	ut_a(ret == 0);

	ret = com_bind(ep, "CLI", 3);

	ut_a(ret == 0);

	printf("Client endpoint created!\n");

	oldtm = ut_clock();

     	for (i = 0; i < 50000; i++) {

		ret = com_sendto(ep, (byte*)"Hello from client!\n", 18, "ibsrv", 5);

		ut_a(ret == 0);

		ret = com_recvfrom(ep, buf, 10000, &len, addr, 150, &addr_len);

		ut_a(ret == 0);

		buf[len] = '\0';
		addr[addr_len] = '\0';
/*
		printf(
	"Message of len %lu\n%s \nreceived from address %s of len %lu\n",
		len, buf, addr, addr_len);
*/		
     	}

	
	tm = ut_clock();
	printf("Wall clock time for test %ld milliseconds\n", tm - oldtm);
	printf("%lu message pairs\n", i);


	printf("System calls in com_shm %lu ip_mutex %lu mutex %lu\n",
		com_shm_system_call_count,
		ip_mutex_system_call_count,
		mutex_system_call_count);
		

	ret = com_endpoint_free(ep);

	ut_ad(ret == 0);
}

void 
main(void) 
/*======*/
{



	sync_init();
	mem_init();
	
	test1();

	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
