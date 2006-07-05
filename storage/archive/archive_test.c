#include "azlib.h"
#include <stdio.h>

#define TEST_STRING "This is a test"
#define BUFFER_LEN 1024

int main(int argc __attribute__((unused)), char *argv[])
{
  int ret;
  azio_stream foo, foo1;
  char buffer[BUFFER_LEN];

  MY_INIT(argv[0]);

  if (!(ret= azopen(&foo, "test", O_CREAT|O_WRONLY|O_TRUNC|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }
  azwrite(&foo, TEST_STRING, sizeof(TEST_STRING));
  azflush(&foo, Z_FINISH);

  if (!(ret= azopen(&foo1, "test", O_RDONLY|O_BINARY)))
  {
    printf("Could not open test file\n");
    return 0;
  }
  ret= azread(&foo1, buffer, BUFFER_LEN);
  printf("Read %d bytes\n", ret);
  printf("%s\n", buffer);
  azrewind(&foo1);
  azclose(&foo);
  if (!(ret= azopen(&foo, "test", O_APPEND|O_WRONLY|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }
  azwrite(&foo, TEST_STRING, sizeof(TEST_STRING));
  azflush(&foo, Z_FINISH);
  ret= azread(&foo1, buffer, BUFFER_LEN);
  printf("Read %d bytes\n", ret);
  printf("%s\n", buffer);
  azclose(&foo);
  azclose(&foo1);

  /* unlink("test"); */
  return 0;
}
