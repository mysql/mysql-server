#include <setjmp.h>

main()
{
jmp_buf foo;

if (setjmp(foo)) {
	exit(0);
}
printf("Hi mom\n");
longjmp(foo, 1);
printf("Should never reach here\n");
}
