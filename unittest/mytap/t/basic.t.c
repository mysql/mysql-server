
#include "my_config.h"

#include <stdlib.h>
#include "../tap.h"

int main() {
  plan(5);
  ok(1 == 1, "testing basic functions");
  ok(2 == 2, "");
  ok(3 == 3, NULL);
  if (1 == 1)
    skip(2, "Sensa fragoli");
  else {
    ok(1 == 2, "Should not be run at all");
    ok(1, "This one neither");
  }
  return exit_status();
}
