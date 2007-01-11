#include "azlib.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define TEST_FILENAME "test.az"
#define TEST_STRING "YOU don't know about me without you have read a book by the name of The Adventures of Tom Sawyer; but that ain't no matter.  That book was made by Mr. Mark Twain, and he told the truth, mainly.  There was things which he stretched, but mainly he told the truth.  That is nothing.  I never seen anybody but lied one time or another, without it was Aunt Polly, or the widow, or maybe Mary.  Aunt Polly--Tom's Aunt Polly, she is--and Mary, and the Widow Douglas is all told about in that book, which is mostly a true book, with some stretchers, as I said before.  Now the way that the book winds up is this:  Tom and me found the money that the robbers hid in the cave, and it made us rich.  We got six thousand dollars apiece--all gold.  It was an awful sight of money when it was piled up.  Well, Judge Thatcher he took it and put it out at interest, and it fetched us a dollar a day apiece all the year round --more than a body could tell what to do with.  The Widow Douglas she took me for her son, and allowed she would..."
#define TEST_LOOP_NUM 100
#define BUFFER_LEN 1024
#define TWOGIG 2147483648
#define FOURGIG 4294967296
#define EIGHTGIG 8589934592

/* prototypes */
int size_test(unsigned long long length, unsigned long long rows_to_test_for);


int main(int argc, char *argv[])
{
  unsigned int ret;

  int error;
  unsigned int x;
  int written_rows= 0;
  azio_stream writer_handle, reader_handle;
  char buffer[BUFFER_LEN];

  unlink(TEST_FILENAME);

  if (argc > 1)
    return 0;

  MY_INIT(argv[0]);

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_RDWR|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }

  if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY|O_BINARY)))
  {
    printf("Could not open test file\n");
    return 0;
  }

  assert(reader_handle.rows == 0);
  assert(reader_handle.auto_increment == 0);
  assert(reader_handle.check_point == 0);
  assert(reader_handle.forced_flushes == 0);
  assert(reader_handle.dirty == 1);

  for (x= 0; x < TEST_LOOP_NUM; x++)
  {
    ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
    assert(ret == BUFFER_LEN);
    written_rows++;
  }
  azflush(&writer_handle,  Z_SYNC_FLUSH);

  /* Lets test that our internal stats are good */
  assert(writer_handle.rows == TEST_LOOP_NUM);

  /* Reader needs to be flushed to make sure it is up to date */
  azflush(&reader_handle,  Z_SYNC_FLUSH);
  assert(reader_handle.rows == TEST_LOOP_NUM);
  assert(reader_handle.auto_increment == 0);
  assert(reader_handle.check_point == 0);
  assert(reader_handle.forced_flushes == 1);
  assert(reader_handle.dirty == 1);

  writer_handle.auto_increment= 4;
  azflush(&writer_handle, Z_SYNC_FLUSH);
  assert(writer_handle.rows == TEST_LOOP_NUM);
  assert(writer_handle.auto_increment == 4);
  assert(writer_handle.check_point == 0);
  assert(writer_handle.forced_flushes == 2);
  assert(writer_handle.dirty == 1);

  if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY|O_BINARY)))
  {
    printf("Could not open test file\n");
    return 0;
  }

  /* Read the original data */
  for (x= 0; x < writer_handle.rows; x++)
  {
    ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
    assert(!error);
    assert(ret == BUFFER_LEN);
    assert(!memcmp(buffer, TEST_STRING, ret));
  }
  assert(writer_handle.rows == TEST_LOOP_NUM);

  /* Test here for falling off the planet */

  /* Final Write before closing */
  ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
  assert(ret == BUFFER_LEN);

  /* We don't use FINISH, but I want to have it tested */
  azflush(&writer_handle,  Z_FINISH);

  assert(writer_handle.rows == TEST_LOOP_NUM+1);

  /* Read final write */
  azrewind(&reader_handle);
  for (x= 0; x < writer_handle.rows; x++)
  {
    ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
    assert(ret == BUFFER_LEN);
    assert(!error);
    assert(!memcmp(buffer, TEST_STRING, ret));
  }


  azclose(&writer_handle);

  /* Rewind and full test */
  azrewind(&reader_handle);
  for (x= 0; x < writer_handle.rows; x++)
  {
    ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
    assert(ret == BUFFER_LEN);
    assert(!error);
    assert(!memcmp(buffer, TEST_STRING, ret));
  }

  printf("Finished reading\n");

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_RDWR|O_BINARY)))
  {
    printf("Could not open file (%s) for appending\n", TEST_FILENAME);
    return 0;
  }
  ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
  assert(ret == BUFFER_LEN);
  azflush(&writer_handle,  Z_SYNC_FLUSH);

  /* Rewind and full test */
  azrewind(&reader_handle);
  for (x= 0; x < writer_handle.rows; x++)
  {
    ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
    assert(!error);
    assert(ret == BUFFER_LEN);
    assert(!memcmp(buffer, TEST_STRING, ret));
  }

  azclose(&writer_handle);
  azclose(&reader_handle);
  unlink(TEST_FILENAME);

  /* Start size tests */
  printf("About to run 2/4/8 gig tests now, you may want to hit CTRL-C\n");
  size_test(TWOGIG, 2097152);
  size_test(FOURGIG, 4194304);
  size_test(EIGHTGIG, 8388608);

  return 0;
}

int size_test(unsigned long long length, unsigned long long rows_to_test_for)
{
  azio_stream writer_handle, reader_handle;
  unsigned long long write_length;
  unsigned long long read_length= 0;
  unsigned int ret;
  char buffer[BUFFER_LEN];
  int error;

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_RDWR|O_TRUNC|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }

  for (write_length= 0; write_length < length ; write_length+= ret)
  {
    ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
    if (ret != BUFFER_LEN)
    {
      printf("Size %u\n", ret);
      assert(ret != BUFFER_LEN);
    }
    if ((write_length % 14031) == 0)
    {
      azflush(&writer_handle,  Z_SYNC_FLUSH);
    }
  }
  assert(write_length == length);
  azflush(&writer_handle,  Z_SYNC_FLUSH);

  printf("Reading back data\n");

  if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY|O_BINARY)))
  {
    printf("Could not open test file\n");
    return 0;
  }

  while ((ret= azread(&reader_handle, buffer, BUFFER_LEN, &error)))
  {
    read_length+= ret;
    assert(!memcmp(buffer, TEST_STRING, ret));
    if (ret != BUFFER_LEN)
    {
      printf("Size %u\n", ret);
      assert(ret != BUFFER_LEN);
    }
  }

  assert(read_length == length);
  assert(writer_handle.rows == rows_to_test_for);
  azclose(&writer_handle);
  azclose(&reader_handle);
  unlink(TEST_FILENAME);

  return 0;
}
