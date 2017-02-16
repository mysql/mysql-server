/*
  this is modified version of the original example main.c
  fixed so that it could compile and run in MySQL source tree
*/

#include <my_global.h>	/* This includes dbug.h */
#include <my_sys.h>
#include <my_pthread.h>

int main (argc, argv)
int argc;
char *argv[];
{
  register int result, ix;
  extern int factorial(int);
  MY_INIT(argv[0]);

  {
    DBUG_ENTER ("main");
    DBUG_PROCESS (argv[0]);
    for (ix = 1; ix < argc && argv[ix][0] == '-'; ix++) {
      switch (argv[ix][1]) {
      case '#':
	DBUG_PUSH (&(argv[ix][2]));
	break;
      }
    }
    for (; ix < argc; ix++) {
      DBUG_PRINT ("args", ("argv[%d] = %s", ix, argv[ix]));
      result = factorial (atoi(argv[ix]));
      printf ("%d\n", result);
    }
    DBUG_LEAVE;
  }
  my_end(0);
  exit(0);
}
