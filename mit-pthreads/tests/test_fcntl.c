#include <stdio.h>
#include <fcntl.h>

main()
{
	int flags, child;

	if ((flags = fcntl(0, F_GETFL)) < 0) {
		perror("fcntl 1st GETFL");
	}
	printf ("flags = %x\n", flags);

	switch(child = fork()) {
	case -1:
		printf("error during fork\n");
		break;
	case 0: /* child */
		execlp("test_create", "test_create", NULL);
		break;
	default: /* parent */
		wait(NULL);
		break;
	}
		
	while(1){
	if ((flags = fcntl(0, F_GETFL)) < 0) {
		perror("fcntl parent GETFL");
	}
	printf ("parent %d flags = %x\n", child, flags);
	sleep(1);
	}
}
