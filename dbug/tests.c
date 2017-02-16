/*
  A program to test DBUG features. Used by tests-t.pl
*/

char *push1=0;

#include <my_global.h>  /* This includes dbug.h */
#include <my_sys.h>
#include <my_pthread.h>
#include <string.h>

const char *func3()
{
  DBUG_ENTER("func3");
  DBUG_RETURN(DBUG_EVALUATE("ret3", "ok", "ko"));
}

void func2()
{
  const char *s __attribute__((unused));
  DBUG_ENTER("func2");
  s=func3();
  DBUG_PRINT("info", ("s=%s", s));
  DBUG_VOID_RETURN;
}

int func1()
{
  DBUG_ENTER("func1");
  func2();
  if (push1)
  {
    DBUG_PUSH(push1);
    fprintf(DBUG_FILE, "=> push1\n");
  }
  DBUG_RETURN(10);
}

int main (int argc __attribute__((unused)),
          char *argv[] __attribute__((unused)))
{
#ifdef DBUG_OFF
  return 1;
#else
  int i;
  if (argc == 1)
    return 0;

  MY_INIT("dbug-tests");

  dup2(1, 2);
  for (i = 1; i < argc; i++)
  {
    if (strncmp(argv[i], "--push1=", 8) == 0)
      push1=argv[i]+8;
    else
      DBUG_PUSH (argv[i]);
  }
  {
    DBUG_ENTER ("main");
    func1();
    DBUG_EXECUTE_IF("dump",
    {
      char s[1000];
      DBUG_EXPLAIN(s, sizeof(s)-1);
      DBUG_DUMP("dump", (uchar*)s, strlen(s));
    });
    DBUG_EXECUTE_IF("push",  DBUG_PUSH("+t"); );
    DBUG_EXECUTE("execute", fprintf(DBUG_FILE, "=> execute\n"); );
    DBUG_EXECUTE_IF("set",  DBUG_SET("+F"); );
    fprintf(DBUG_FILE, "=> evaluate: %s\n",
            DBUG_EVALUATE("evaluate", "ON", "OFF"));
    fprintf(DBUG_FILE, "=> evaluate_if: %s\n",
            DBUG_EVALUATE_IF("evaluate_if", "ON", "OFF"));
    DBUG_EXECUTE_IF("pop",  DBUG_POP(); );
    {
      char s[1000] __attribute__((unused));
      DBUG_EXPLAIN(s, sizeof(s)-1);
      DBUG_PRINT("explain", ("dbug explained: %s", s));
    }
    func2();
    DBUG_LEAVE;
  }
  DBUG_SET(""); /* to not have my_end() in the traces */
  my_end(0);
  return 0;
#endif /* DBUG_OFF */
}
