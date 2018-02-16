#include <dbug.h>

int main(argc, argv) int argc;
char *argv[];
{
  int result, ix;
  extern int factorial(int);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);
  for (ix = 1; ix < argc && argv[ix][0] == '-'; ix++) {
    switch (argv[ix][1]) {
      case '#':
        DBUG_PUSH(&(argv[ix][2]));
        break;
    }
  }
  for (; ix < argc; ix++) {
    DBUG_PRINT("args", ("argv[%d] = %s", ix, argv[ix]));
    result = factorial(atoi(argv[ix]));
    printf("%d\n", result);
  }
  DBUG_RETURN(0);
}
