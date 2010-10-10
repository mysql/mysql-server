/* Copyright (C) 2006-2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  TODO: use pthread_join instead of wait_for_thread_count_to_be_zero, like in
  my_atomic-t.c (see BUG#22320).
  Use diag() instead of fprintf(stderr).
*/
#include <tap.h>
#include <my_sys.h>
#include <m_string.h>
#include "test_file.h"
#include <tap.h>

#define PCACHE_SIZE (TEST_PAGE_SIZE*1024*10)

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif

#ifndef BIG
#undef SKIP_BIG_TESTS
#define SKIP_BIG_TESTS(X) /* no-op */
#endif

static char *file1_name= (char*)"page_cache_test_file_1";
static char *file2_name= (char*)"page_cache_test_file_2";
static PAGECACHE_FILE file1;
static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count;
static PAGECACHE pagecache;

/*
  File contance descriptors
*/
static struct file_desc simple_read_write_test_file[]=
{
  { TEST_PAGE_SIZE, '\1'},
  {0, 0}
};
static struct file_desc simple_read_change_write_read_test_file[]=
{
  { TEST_PAGE_SIZE/2, '\65'},
  { TEST_PAGE_SIZE/2, '\1'},
  {0, 0}
};
static struct file_desc simple_pin_test_file1[]=
{
  { TEST_PAGE_SIZE*2, '\1'},
  {0, 0}
};
static struct file_desc simple_pin_test_file2[]=
{
  { TEST_PAGE_SIZE/2, '\1'},
  { TEST_PAGE_SIZE/2, (unsigned char)129},
  { TEST_PAGE_SIZE, '\1'},
  {0, 0}
};
static struct file_desc simple_pin_no_lock_test_file1[]=
{
  { TEST_PAGE_SIZE, '\4'},
  {0, 0}
};
static struct file_desc simple_pin_no_lock_test_file2[]=
{
  { TEST_PAGE_SIZE, '\5'},
  {0, 0}
};
static struct file_desc simple_pin_no_lock_test_file3[]=
{
  { TEST_PAGE_SIZE, '\6'},
  {0, 0}
};
static struct file_desc simple_delete_forget_test_file[]=
{
  { TEST_PAGE_SIZE, '\1'},
  {0, 0}
};
static struct file_desc simple_delete_flush_test_file[]=
{
  { TEST_PAGE_SIZE, '\2'},
  {0, 0}
};


/**
  @brief Dummy pagecache callback.
*/

static my_bool
dummy_callback(uchar *page __attribute__((unused)),
               pgcache_page_no_t page_no __attribute__((unused)),
               uchar* data_ptr __attribute__((unused)))
{
  return 0;
}


/**
  @brief Dummy pagecache callback.
*/

static void
dummy_fail_callback(uchar* data_ptr __attribute__((unused)))
{
  return;
}


/*
  Recreate and reopen a file for test

  SYNOPSIS
    reset_file()
    file                 File to reset
    file_name            Path (and name) of file which should be reset
*/

void reset_file(PAGECACHE_FILE *file, const char *file_name)
{
  flush_pagecache_blocks(&pagecache, file, FLUSH_RELEASE);
  if (my_close(file->file, MYF(MY_WME)))
    exit(1);
  my_delete(file_name, MYF(MY_WME));
  if ((file->file= my_open(file_name,
                           O_CREAT | O_TRUNC | O_RDWR, MYF(0))) == -1)
  {
    diag("Got error during %s creation from open() (errno: %d)\n",
         file_name, my_errno);
    exit(1);
  }
}

/*
  Write then read page, check file on disk
*/

int simple_read_write_test()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  unsigned char *buffr= malloc(TEST_PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_read_write_test");
  bfill(buffw, TEST_PAGE_SIZE, '\1');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  pagecache_read(&pagecache, &file1, 0, 3, buffr,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_LEFT_UNLOCKED,
                 0);
  ok((res= test(memcmp(buffr, buffw, TEST_PAGE_SIZE) == 0)),
     "Simple write-read page ");
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error during flushing pagecache\n");
    exit(1);
  }
  ok((res&= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                           simple_read_write_test_file))),
     "Simple write-read page file");
  if (res)
    reset_file(&file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}


/*
  Prepare page, then read (and lock), change (write new value and unlock),
  then check the page in the cache and on the disk
*/
int simple_read_change_write_read_test()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  unsigned char *buffr= malloc(TEST_PAGE_SIZE);
  int res, res2;
  DBUG_ENTER("simple_read_change_write_read_test");

  /* prepare the file */
  bfill(buffw, TEST_PAGE_SIZE, '\1');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error during flushing pagecache\n");
    exit(1);
  }
  /* test */
  pagecache_read(&pagecache, &file1, 0, 3, buffw,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_WRITE,
                 0);
  bfill(buffw, TEST_PAGE_SIZE/2, '\65');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE_UNLOCK,
                  PAGECACHE_UNPIN,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);

  pagecache_read(&pagecache, &file1, 0, 3, buffr,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_LEFT_UNLOCKED,
                 0);
  ok((res= test(memcmp(buffr, buffw, TEST_PAGE_SIZE) == 0)),
     "Simple read-change-write-read page ");
  DBUG_ASSERT(pagecache.blocks_changed == 1);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error during flushing pagecache\n");
    exit(1);
  }
  DBUG_ASSERT(pagecache.blocks_changed == 0);
  ok((res2= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                           simple_read_change_write_read_test_file))),
     "Simple read-change-write-read page file");
  if (res && res2)
    reset_file(&file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res && res2);
}


/*
  Prepare page, read page 0 (and pin) then write page 1 and page 0.
  Flush the file (should flush only page 1 and return 1 (page 0 is
  still pinned).
  Check file on the disk.
  Unpin and flush.
  Check file on the disk.
*/
int simple_pin_test()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_pin_test");
  /* prepare the file */
  bfill(buffw, TEST_PAGE_SIZE, '\1');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  /* test */
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error during flushing pagecache\n");
    exit(1);
  }
  pagecache_read(&pagecache, &file1, 0, 3, buffw,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_WRITE,
                 0);
  pagecache_write(&pagecache, &file1, 1, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  bfill(buffw + TEST_PAGE_SIZE/2, TEST_PAGE_SIZE/2, ((unsigned char) 129));
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_WRITELOCKED,
                  PAGECACHE_PIN_LEFT_PINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  /*
    We have to get error because one page of the file is pinned,
    other page should be flushed
  */
  if (!flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Did not get error in flush_pagecache_blocks\n");
    res= 0;
    goto err;
  }
  ok((res= test(test_file(file1, file1_name, TEST_PAGE_SIZE*2, TEST_PAGE_SIZE*2,
                           simple_pin_test_file1))),
     "Simple pin page file with pin");
  pagecache_unlock(&pagecache,
                   &file1,
                   0,
                   PAGECACHE_LOCK_WRITE_UNLOCK,
                   PAGECACHE_UNPIN,
                   0, 0, 0);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error in flush_pagecache_blocks\n");
    res= 0;
    goto err;
  }
  ok((res&= test(test_file(file1, file1_name, TEST_PAGE_SIZE*2, TEST_PAGE_SIZE,
                           simple_pin_test_file2))),
     "Simple pin page result file");
  if (res)
    reset_file(&file1, file1_name);
err:
  free(buffw);
  DBUG_RETURN(res);
}

/*
  Prepare page, read page 0 (and pin) then write page 1 and page 0.
  Flush the file (should flush only page 1 and return 1 (page 0 is
  still pinned).
  Check file on the disk.
  Unpin and flush.
  Check file on the disk.
*/
int simple_pin_test2()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_pin_test2");
  /* prepare the file */
  bfill(buffw, TEST_PAGE_SIZE, '\1');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  /* test */
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error during flushing pagecache\n");
    exit(1);
  }
  pagecache_read(&pagecache, &file1, 0, 3, buffw,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_WRITE,
                 0);
  pagecache_write(&pagecache, &file1, 1, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  bfill(buffw + TEST_PAGE_SIZE/2, TEST_PAGE_SIZE/2, ((unsigned char) 129));
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE_TO_READ,
                  PAGECACHE_PIN_LEFT_PINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  /*
    We have to get error because one page of the file is pinned,
    other page should be flushed
  */
  if (!flush_pagecache_blocks(&pagecache, &file1, FLUSH_KEEP_LAZY))
  {
    diag("Did not get error in flush_pagecache_blocks 2\n");
    res= 0;
    goto err;
  }
  ok((res= test(test_file(file1, file1_name, TEST_PAGE_SIZE*2, TEST_PAGE_SIZE*2,
                           simple_pin_test_file1))),
     "Simple pin page file with pin 2");

  /* Test that a normal flush goes through */
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error in flush_pagecache_blocks 3\n");
    res= 0;
    goto err;
  }
  pagecache_unlock(&pagecache,
                   &file1,
                   0,
                   PAGECACHE_LOCK_READ_UNLOCK,
                   PAGECACHE_UNPIN,
                   0, 0, 0);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error in flush_pagecache_blocks 4\n");
    res= 0;
    goto err;
  }
  ok((res&= test(test_file(file1, file1_name, TEST_PAGE_SIZE*2, TEST_PAGE_SIZE,
                           simple_pin_test_file2))),
     "Simple pin page result file 2");
  if (res)
    reset_file(&file1, file1_name);
err:
  free(buffw);
  DBUG_RETURN(res);
}

/*
  Checks pins without lock.
*/
int simple_pin_no_lock_test()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  PAGECACHE_BLOCK_LINK *link;
  int res;
  DBUG_ENTER("simple_pin_no_lock_test");
  /* prepare the file */
  bfill(buffw, TEST_PAGE_SIZE, '\4');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  /* test */
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error during flushing pagecache 2\n");
    exit(1);
  }
  bfill(buffw, TEST_PAGE_SIZE, '\5');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  /*
    We have to get error because one page of the file is pinned,
    other page should be flushed
  */
  if (!flush_pagecache_blocks(&pagecache, &file1, FLUSH_KEEP_LAZY))
  {
    diag("Did not get error in flush_pagecache_blocks 2\n");
    res= 0;
    goto err;
  }
  ok((res= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                           simple_pin_no_lock_test_file1))),
     "Simple pin (no lock) page file with pin 2");
  pagecache_unlock(&pagecache,
                   &file1,
                   0,
                   PAGECACHE_LOCK_LEFT_UNLOCKED,
                   PAGECACHE_UNPIN,
                   0, 0, 0);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error in flush_pagecache_blocks 2\n");
    res= 0;
    goto err;
  }
  ok((res&= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                           simple_pin_no_lock_test_file2))),
     "Simple pin (no lock) page result file 2");

  bfill(buffw, TEST_PAGE_SIZE, '\6');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE,
                  PAGECACHE_PIN,
                  PAGECACHE_WRITE_DELAY,
                  &link, LSN_IMPOSSIBLE);
  pagecache_unlock_by_link(&pagecache, link,
                           PAGECACHE_LOCK_WRITE_UNLOCK,
                           PAGECACHE_PIN_LEFT_PINNED, 0, 0, 1, FALSE);
  if (!flush_pagecache_blocks(&pagecache, &file1, FLUSH_KEEP_LAZY))
  {
    diag("Did not get error in flush_pagecache_blocks 3\n");
    res= 0;
    goto err;
  }
  ok((res= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                           simple_pin_no_lock_test_file2))),
     "Simple pin (no lock) page file with pin 3");
  pagecache_unpin_by_link(&pagecache, link, 0);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error in flush_pagecache_blocks 3\n");
    res= 0;
    goto err;
  }
  ok((res&= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                           simple_pin_no_lock_test_file3))),
     "Simple pin (no lock) page result file 3");
  if (res)
    reset_file(&file1, file1_name);
err:
  free(buffw);
  DBUG_RETURN(res);
}
/*
  Prepare page, write new value, then delete page from cache without flush,
  on the disk should be page with old content written during preparation
*/

int simple_delete_forget_test()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  unsigned char *buffr= malloc(TEST_PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_delete_forget_test");
  /* prepare the file */
  bfill(buffw, TEST_PAGE_SIZE, '\1');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  /* test */
  bfill(buffw, TEST_PAGE_SIZE, '\2');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  pagecache_delete(&pagecache, &file1, 0,
                   PAGECACHE_LOCK_WRITE, 0);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  ok((res= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                          simple_delete_forget_test_file))),
     "Simple delete-forget page file");
  if (res)
    reset_file(&file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}

/*
  Prepare page with locking, write new content to the page,
  delete page with flush and on existing lock,
  check that page on disk contain new value.
*/

int simple_delete_flush_test()
{
  unsigned char *buffw= malloc(TEST_PAGE_SIZE);
  unsigned char *buffr= malloc(TEST_PAGE_SIZE);
  PAGECACHE_BLOCK_LINK *link;
  int res;
  DBUG_ENTER("simple_delete_flush_test");
  /* prepare the file */
  bfill(buffw, TEST_PAGE_SIZE, '\1');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE,
                  PAGECACHE_PIN,
                  PAGECACHE_WRITE_DELAY,
                  &link, LSN_IMPOSSIBLE);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  /* test */
  bfill(buffw, TEST_PAGE_SIZE, '\2');
  pagecache_write(&pagecache, &file1, 0, 3, buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_WRITELOCKED,
                  PAGECACHE_PIN_LEFT_PINNED,
                  PAGECACHE_WRITE_DELAY,
                  0, LSN_IMPOSSIBLE);
  if (pagecache_delete_by_link(&pagecache, link,
			       PAGECACHE_LOCK_LEFT_WRITELOCKED, 1))
  {
    diag("simple_delete_flush_test: error during delete");
    exit(1);
  }
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  ok((res= test(test_file(file1, file1_name, TEST_PAGE_SIZE, TEST_PAGE_SIZE,
                          simple_delete_flush_test_file))),
     "Simple delete flush (link) page file");
  if (res)
    reset_file(&file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}


/*
  write then read file bigger then cache
*/

int simple_big_test()
{
  unsigned char *buffw= (unsigned char *) my_malloc(TEST_PAGE_SIZE, MYF(MY_WME));
  unsigned char *buffr= (unsigned char *) my_malloc(TEST_PAGE_SIZE, MYF(MY_WME));
  struct file_desc *desc= ((struct file_desc *)
                           my_malloc((PCACHE_SIZE/(TEST_PAGE_SIZE/2) + 1) *
                                     sizeof(struct file_desc), MYF(MY_WME)));
  int res, i;
  DBUG_ENTER("simple_big_test");

  /* prepare the file twice larger then cache */
  for (i= 0; i < PCACHE_SIZE/(TEST_PAGE_SIZE/2); i++)
  {
    bfill(buffw, TEST_PAGE_SIZE, (unsigned char) (i & 0xff));
    desc[i].length= TEST_PAGE_SIZE;
    desc[i].content= (i & 0xff);
    pagecache_write(&pagecache, &file1, i, 3, buffw,
                    PAGECACHE_PLAIN_PAGE,
                    PAGECACHE_LOCK_LEFT_UNLOCKED,
                    PAGECACHE_PIN_LEFT_UNPINNED,
                    PAGECACHE_WRITE_DELAY,
                    0, LSN_IMPOSSIBLE);
  }
  desc[i].length= 0;
  desc[i].content= '\0';
  ok(1, "Simple big file write");
  /* check written pages sequentally read */
  for (i= 0; i < PCACHE_SIZE/(TEST_PAGE_SIZE/2); i++)
  {
    int j;
    pagecache_read(&pagecache, &file1, i, 3, buffr,
                   PAGECACHE_PLAIN_PAGE,
                   PAGECACHE_LOCK_LEFT_UNLOCKED,
                   0);
    for(j= 0; j < TEST_PAGE_SIZE; j++)
    {
      if (buffr[j] != (i & 0xff))
      {
        diag("simple_big_test seq: page %u byte %u mismatch\n", i, j);
        res= 0;
        goto err;
      }
    }
  }
  ok(1, "Simple big file sequential read");
  /* chack random reads */
  for (i= 0; i < PCACHE_SIZE/(TEST_PAGE_SIZE); i++)
  {
    int j, page;
    page= rand() % (PCACHE_SIZE/(TEST_PAGE_SIZE/2));
    pagecache_read(&pagecache, &file1, page, 3, buffr,
                   PAGECACHE_PLAIN_PAGE,
                   PAGECACHE_LOCK_LEFT_UNLOCKED,
                   0);
    for(j= 0; j < TEST_PAGE_SIZE; j++)
    {
      if (buffr[j] != (page & 0xff))
      {
        diag("simple_big_test rnd: page %u byte %u mismatch\n", page, j);
        res= 0;
        goto err;
      }
    }
  }
  ok(1, "Simple big file random read");
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);

  ok((res= test(test_file(file1, file1_name, PCACHE_SIZE*2, TEST_PAGE_SIZE,
                          desc))),
     "Simple big file");
  if (res)
    reset_file(&file1, file1_name);

err:
  my_free(buffw, 0);
  my_free(buffr, 0);
  my_free(desc, 0);
  DBUG_RETURN(res);
}


/*
  Thread function
*/

static void *test_thread(void *arg)
{
#ifndef DBUG_OFF
  int param= *((int*) arg);
#endif

  my_thread_init();
  {
  DBUG_ENTER("test_thread");
  DBUG_PRINT("enter", ("param: %d", param));

  if (!simple_read_write_test() ||
      !simple_read_change_write_read_test() ||
      !simple_pin_test() ||
      !simple_pin_test2() ||
      !simple_pin_no_lock_test() ||
      !simple_delete_forget_test() ||
      !simple_delete_flush_test())
    exit(1);

  SKIP_BIG_TESTS(4)
  {
    if (!simple_big_test())
      exit(1);
  }

  DBUG_PRINT("info", ("Thread %s ended\n", my_thread_name()));
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((uchar*) arg);
  my_thread_end();
  DBUG_RETURN(0);
  }
}


int main(int argc __attribute__((unused)),
         char **argv __attribute__((unused)))
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int *param, error, pagen;
  File tmp_file;
  MY_INIT(argv[0]);

#ifndef DBUG_OFF
#if defined(__WIN__)
  default_dbug_option= "d:t:i:O,\\test_pagecache_single.trace";
#else
  default_dbug_option= "d:t:i:o,/tmp/test_pagecache_single.trace";
#endif
  if (argc > 1)
  {
    DBUG_SET(default_dbug_option);
    DBUG_SET_INITIAL(default_dbug_option);
  }
#endif
  {
  DBUG_ENTER("main");
  DBUG_PRINT("info", ("Main thread: %s\n", my_thread_name()));

  plan(18);
  SKIP_BIG_TESTS(18)
  {

  if ((tmp_file= my_open(file2_name, O_CREAT | O_TRUNC | O_RDWR,
                         MYF(MY_WME))) < 0)
    exit(1);

  if ((file1.file= my_open(file1_name,
                           O_CREAT | O_TRUNC | O_RDWR, MYF(0))) == -1)
  {
    fprintf(stderr, "Got error during file1 creation from open() (errno: %d)\n",
	    errno);
    exit(1);
  }
  pagecache_file_init(file1, &dummy_callback, &dummy_callback,
                      &dummy_fail_callback, &dummy_callback, NULL);
  my_close(tmp_file, MYF(0));
  my_delete(file2_name, MYF(0));

  DBUG_PRINT("info", ("file1: %d", file1.file));
  if (my_chmod(file1_name, S_IRWXU | S_IRWXG | S_IRWXO, MYF(MY_WME)))
    exit(1);
  my_pwrite(file1.file, (const uchar*)"test file", 9, 0, MYF(MY_WME));

  if ((error= pthread_cond_init(&COND_thread_count, NULL)))
  {
    fprintf(stderr, "Got error: %d from pthread_cond_init (errno: %d)\n",
	    error, errno);
    exit(1);
  }
  if ((error= pthread_mutex_init(&LOCK_thread_count, MY_MUTEX_INIT_FAST)))
  {
    fprintf(stderr, "Got error: %d from pthread_cond_init (errno: %d)\n",
	    error, errno);
    exit(1);
  }

  if ((error= pthread_attr_init(&thr_attr)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_init (errno: %d)\n",
	    error,errno);
    exit(1);
  }
  if ((error= pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED)))
  {
    fprintf(stderr,
	    "Got error: %d from pthread_attr_setdetachstate (errno: %d)\n",
	    error,errno);
    exit(1);
  }

#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(2));
#endif

  if ((pagen= init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                             TEST_PAGE_SIZE, MYF(MY_WME))) == 0)
  {
    fprintf(stderr,"Got error: init_pagecache() (errno: %d)\n",
            errno);
    exit(1);
  }
  DBUG_PRINT("info", ("Page cache %d pages", pagen));

  pthread_mutex_lock(&LOCK_thread_count);
  param=(int*) malloc(sizeof(int));
  *param= 1;
  if ((error= pthread_create(&tid, &thr_attr, test_thread, (void*) param)))
  {
    fprintf(stderr,"Got error: %d from pthread_create (errno: %d)\n",
            error,errno);
    exit(1);
  }
  thread_count++;
  DBUG_PRINT("info", ("Thread started"));
  pthread_mutex_unlock(&LOCK_thread_count);

  pthread_attr_destroy(&thr_attr);

  pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    if ((error= pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"Got error: %d from pthread_cond_wait\n",error);
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("info", ("thread ended"));

  end_pagecache(&pagecache, 1);
  DBUG_PRINT("info", ("Page cache ended"));

  if (my_close(file1.file, MYF(MY_WME)))
    exit(1);

  my_delete(file1_name, MYF(0));

  } /* SKIP_BIG_TESTS */
  DBUG_PRINT("info", ("file1 (%d) closed", file1.file));
  DBUG_PRINT("info", ("Program end"));

  my_end(0);

  }
  return exit_status();
}
