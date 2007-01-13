#include "azlib.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define BUFFER_LEN 1024

int main(int argc, char *argv[])
{
  unsigned int ret;
  azio_stream reader_handle;

  MY_INIT(argv[0]);

  if (argc < 2)
  {
    printf("No file specified. \n");
    return 0;
  }

  if (!(ret= azopen(&reader_handle, argv[1], O_RDONLY|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }

  printf("Version :%u\n", reader_handle.version);
  printf("Start position :%llu\n", (unsigned long long)reader_handle.start);
  if (reader_handle.version > 2)
  {
    printf("Block size :%u\n", reader_handle.block_size);
    printf("Rows: %llu\n", reader_handle.rows);
    printf("Autoincrement: %llu\n", reader_handle.auto_increment);
    printf("Check Point: %llu\n", reader_handle.check_point);
    printf("Forced Flushes: %llu\n", reader_handle.forced_flushes);
    printf("State: %s\n", ( reader_handle.dirty ? "dirty" : "clean"));
  }

  azclose(&reader_handle);

  return 0;
}
