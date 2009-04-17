#include <pthread.h>
#include <string.h>

int
main(int argc, char** argv)
{
	pthread_t	x1;
	pthread_t	x2;
	pthread_t	x3;

	memset(&x1, 0x0, sizeof(x1));
	memset(&x2, 0x0, sizeof(x2));
	memset(&x3, 0x0, sizeof(x3));

	__sync_bool_compare_and_swap(&x1, x2, x3);

	return(0);
}
