#include <ndb_global.h>

void require_failed(int exitcode, RequirePrinter printer,
                    const char* expr, const char* file, int line)
{
#define FMT "%s:%d: require(%s) failed\n", file, line, expr
  if (!printer)
  {
    fprintf(stderr, FMT);
    fflush(stderr);
  }
  else
  {
    printer(FMT);
  }
#ifdef _WIN32
  DebugBreak();
#endif
  if(exitcode)
  {
    exit(exitcode);
  }
  abort();
}

#ifdef TEST
int main()
{
  require(1);
  require(0);
}
#endif
