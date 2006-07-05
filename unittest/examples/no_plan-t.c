
#include "my_config.h"

#include <stdlib.h>
#include <tap.h>

/*
  Sometimes, the number of tests is not known beforehand. In those
  cases, the plan can be omitted and will instead be written at the
  end of the test (inside exit_status()).

  Use this sparingly, it is a last resort: planning how many tests you
  are going to run will help you catch that offending case when some
  tests are skipped for an unknown reason.
*/
int main() {
  ok(1, " ");
  ok(1, " ");
  ok(1, " ");
  return exit_status();
}
