#include "mysys_priv.h"
#include "my_dir.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <pagecache.h>
#include "test_file.h"


/*
  Check that file contance correspond to descriptor

  SYNOPSIS
    test_file()
    file                 File to test
    file_name            Path (and name) of file which is tested
    size                 size of file
    buff_size            size of buffer which is enought to check the file
    desc                 file descriptor to check with

  RETURN
    1 file if OK
    0 error
*/

int test_file(PAGECACHE_FILE file, char *file_name,
              off_t size, size_t buff_size, struct file_desc *desc)
{
  MY_STAT stat_buff, *stat;
  unsigned char *buffr= malloc(buff_size);
  off_t pos= 0;
  size_t byte;
  int step= 0;

  if ((stat= my_stat(file_name, &stat_buff, MYF(0))) == NULL)
  {
    diag("Can't stat() %s (errno: %d)\n", file_name, errno);
    return 0;
  }
  if (stat->st_size != size)
  {
    diag("file %s size is %lu (should be %lu)\n",
         file_name, (ulong) stat->st_size, (ulong) size);
    return 0;
  }
  /* check content */
  my_seek(file.file, 0, SEEK_SET, MYF(0));
  while (desc[step].length != 0)
  {
    if (my_read(file.file, (char*)buffr, desc[step].length, MYF(0)) !=
        desc[step].length)
    {
      diag("Can't read %u bytes from %s (errno: %d)\n",
           (uint)desc[step].length, file_name, errno);
      return 0;
    }
    for (byte= 0; byte < desc[step].length; byte++)
    {
      if (buffr[byte] != desc[step].content)
      {
        diag("content of %s mismatch 0x%x in position %lu instead of 0x%x\n",
             file_name, (uint) buffr[byte], (ulong) (pos + byte),
             desc[step].content);
        return 0;
      }
    }
    pos+= desc[step].length;
    step++;
  }
  return 1;
}
