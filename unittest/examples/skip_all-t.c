
#include "my_config.h"

#include <stdlib.h>
#include <tap.h>

int has_feature() {
  return 0;
}

/*
  In some cases, an entire test file does not make sense because there
  some feature is missing.  In that case, the entire test case can be
  skipped in the following manner.
 */
int main() {
  if (!has_feature())
    skip_all("Example of skipping an entire test");
  plan(4);
  ok(1, NULL);
  ok(1, NULL);
  ok(1, NULL);
  ok(1, NULL);
  return exit_status();
}
