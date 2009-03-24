#include <pthread.h>

int
main(int argc, char** argv)
{
	pthread_t	x1;
	pthread_t	x2;
	pthread_t	x3;

	__sync_bool_compare_and_swap(&x1, x2, x3);

	return(0);
}
