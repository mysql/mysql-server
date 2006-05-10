
#include "my_config.h"

#include <stdlib.h>
#include <tap.h>

int main()
{
  plan(4);
  ok(1, NULL);
  ok(1, NULL);
  /*
    Tests in the todo region is expected to fail. If they don't,
    something is strange.
  */
  todo_start("Need to fix these");
  ok(0, NULL);
  ok(0, NULL);
  todo_end();
  return exit_status();
}
