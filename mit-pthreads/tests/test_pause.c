#include <stdio.h>
#include <signal.h>

foo(int sig) 
{
	return;
}

main()
{
	sigset_t all;

	signal (1, foo);
	sigfillset(&all);
	sigprocmask(SIG_BLOCK, &all, NULL);
	printf("Begin pause\n");
	pause();
	printf("Done pause\n");
}
