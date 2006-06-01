
#include <tap.h>

unsigned int gcs(unsigned int a, unsigned int b)
{
  if (b > a) {
    unsigned int t = a;
    a = b;
    b = t;
  }

  while (b != 0) {
    unsigned int m = a % b;
    a = b;
    b = m;
  }
  return a;
}

int main() {
  unsigned int a,b;
  unsigned int failed;
  plan(1);
  diag("Testing basic functions");
  failed = 0;
  for (a = 1 ; a < 2000 ; ++a)
    for (b = 1 ; b < 2000 ; ++b)
    {
      unsigned int d = gcs(a, b);
      if (a % d != 0 || b % d != 0) {
        ++failed;
        diag("Failed for gcs(%4u,%4u)", a, b);
      }
    }
  ok(failed == 0, "Testing gcs()");
  return exit_status();
}

