/************************************************************************
The test module for communication

(c) 1995 Innobase Oy

Created 9/26/1995 Heikki Tuuri
*************************************************************************/

#include "../com0com.h"
#include "../com0shm.h"
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
	ulint			i;
	
	ep = com_endpoint_create(COM_SHM);

	ut_a(ep);

	size = 8192;
	
	ret = com_endpoint_set_option(ep, COM_OPT_MAX_DGRAM_SIZE,
				      (byte*)&size, 0);

	ut_a(ret == 0);

	ret = com_bind(ep, "SRV", 3);

	ut_a(ret == 0);

	printf("Server endpoint created!\n");

     for (i = 0; i < 50000; i++) {

	ret = com_recvfrom(ep, buf, 10000, &len, addr, 150, &addr_len);

	ut_a(ret == 0);

	buf[len] = '\0';
	addr[addr_len] = '\0';
/*
	printf(
	"Message of len %lu\n%s \nreceived from address %s of len %lu\n",
		len, buf, addr, addr_len);
*/
	ret = com_sendto(ep, (byte*)"Hello from server!\n", 18, "CLI", 3);

	ut_a(ret == 0);
     }

	ret = com_endpoint_free(ep);

	ut_ad(ret == 0);

	printf("Count of extra system calls in com_shm  %lu\n",
			com_shm_system_call_count);
	printf("Count of extra system calls in ip_mutex  %lu\n",
			ip_mutex_system_call_count);
}

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	sync_init();
	mem_init();
	
	oldtm = ut_clock();
	
	test1();

	ut_ad(mem_all_freed());
	
	tm = ut_clock();
	printf("Wall clock time for test %ld milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
