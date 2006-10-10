
#include <pagecache.h>

/*
  File content descriptor
*/
struct file_desc
{
  unsigned int length;
  unsigned char content;
};

int test_file(PAGECACHE_FILE file, char *file_name,
              off_t size, size_t buff_size, struct file_desc *desc);
