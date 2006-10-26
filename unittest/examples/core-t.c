
#include "my_config.h"

#include <stdlib.h>
#include <tap.h>

/*
  This is a simple test to demonstrate what happens if a signal that
  generates a core is raised.

  Note that this test will stop all further testing!
 */

int main() {
  plan(3);
  ok(1, "First test");
  abort();
  return exit_status();
}
