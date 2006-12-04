#include "azlib.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define TEST_FILENAME "test.az"
#define TEST_STRING "YOU don't know about me without you have read a book by the name of The Adventures of Tom Sawyer; but that ain't no matter.  That book was made by Mr. Mark Twain, and he told the truth, mainly.  There was things which he stretched, but mainly he told the truth.  That is nothing.  I never seen anybody but lied one time or another, without it was Aunt Polly, or the widow, or maybe Mary.  Aunt Polly--Tom's Aunt Polly, she is--and Mary, and the Widow Douglas is all told about in that book, which is mostly a true book, with some stretchers, as I said before.  Now the way that the book winds up is this:  Tom and me found the money that the robbers hid in the cave, and it made us rich.  We got six thousand dollars apiece--all gold.  It was an awful sight of money when it was piled up.  Well, Judge Thatcher he took it and put it out at interest, and it fetched us a dollar a day apiece all the year round --more than a body could tell what to do with.  The Widow Douglas she took me for her son, and allowed she would..."
#define BUFFER_LEN 1024
#define TWOGIG 2147483648
#define FOURGIG 4294967296

int main(int argc __attribute__((unused)), char *argv[])
{
  unsigned long ret;
  int error;
  azio_stream writer_handle, reader_handle;
  char buffer[BUFFER_LEN];
  unsigned long write_length;
  unsigned long read_length= 0;

  MY_INIT(argv[0]);

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }
  ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
  assert(ret == BUFFER_LEN);
  azflush(&writer_handle, Z_FINISH);

  if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY|O_BINARY)))
  {
    printf("Could not open test file\n");
    return 0;
  }
  ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
  printf("Read %lu bytes, expected %d\n", ret, BUFFER_LEN);

  azrewind(&reader_handle);
  azclose(&writer_handle);

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_APPEND|O_WRONLY|O_BINARY)))
  {
    printf("Could not open file (%s) for appending\n", TEST_FILENAME);
    return 0;
  }
  ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
  assert(ret == BUFFER_LEN);
  azflush(&writer_handle, Z_FINISH);

  /* Read the original data */
  ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
  printf("Read %lu bytes, expected %d\n", ret, BUFFER_LEN);
  assert(ret == BUFFER_LEN);
  assert(!error);

  /* Read the new data */
  ret= azread(&reader_handle, buffer, BUFFER_LEN, &error);
  printf("Read %lu bytes, expected %d\n", ret, BUFFER_LEN);
  assert(ret == BUFFER_LEN);
  assert(!error);

  azclose(&writer_handle);
  azclose(&reader_handle);
  unlink(TEST_FILENAME);

  /* Start size tests */
  printf("About to run 2gig and 4gig test now, you may want to hit CTRL-C\n");

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }

  for (write_length= 0; write_length < TWOGIG ; write_length+= ret)
  {
    ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
    assert(!error);
    if (ret != BUFFER_LEN)
    {
      printf("Size %lu\n", ret);
      assert(ret != BUFFER_LEN);
    }
  }
  assert(write_length == TWOGIG);
  printf("Read %lu bytes, expected %lu\n", write_length, TWOGIG);
  azflush(&writer_handle, Z_FINISH);

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
      printf("Size %lu\n", ret);
      assert(ret != BUFFER_LEN);
    }
  }

  assert(read_length == TWOGIG);
  azclose(&writer_handle);
  azclose(&reader_handle);
  unlink(TEST_FILENAME);

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY)))
  {
    printf("Could not create test file\n");
    return 0;
  }

  for (write_length= 0; write_length < FOURGIG ; write_length+= ret)
  {
    ret= azwrite(&writer_handle, TEST_STRING, BUFFER_LEN);
    assert(ret == BUFFER_LEN);
  }
  assert(write_length == FOURGIG);
  printf("Read %lu bytes, expected %lu\n", write_length, FOURGIG);
  azclose(&writer_handle);
  azclose(&reader_handle);
  unlink(TEST_FILENAME);

  return 0;
}
