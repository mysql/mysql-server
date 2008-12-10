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
*/

#include <tap.h>
#include <my_sys.h>
#include <m_string.h>
#include "test_file.h"
#include <tap.h>

#define PCACHE_SIZE (TEST_PAGE_SIZE*1024*8)

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif


#define SLEEP my_sleep(5)

static char *file1_name= (char*)"page_cache_test_file_1";
static PAGECACHE_FILE file1;
static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count= 0;
static PAGECACHE pagecache;

static uint number_of_readers= 5;
static uint number_of_writers= 5;
static uint number_of_read_tests= 2000;
static uint number_of_write_tests= 1000;
static uint read_sleep_limit= 3;
static uint report_divisor= 50;

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


/**
  @brief Checks page consistency

  @param buff            pointer to the page content
  @param task            task ID
*/
void check_page(uchar *buff, int task)
{
  uint i;
  DBUG_ENTER("check_page");

  for (i= 1; i < TEST_PAGE_SIZE; i++)
  {
    if (buff[0] != buff[i])
      goto err;
  }
  DBUG_VOID_RETURN;
err:
  diag("Task %d char #%u '%u' != '%u'", task, i, (uint) buff[0],
       (uint) buff[i]);
  DBUG_PRINT("err", ("try to flush"));
  exit(1);
}



void reader(int num)
{
  unsigned char *buff;
  uint i;
  PAGECACHE_BLOCK_LINK *link;

  for (i= 0; i < number_of_read_tests; i++)
  {
    if (i % report_divisor == 0)
      diag("Reader %d - %u", num, i);
    buff= pagecache_read(&pagecache, &file1, 0, 3, NULL,
                         PAGECACHE_PLAIN_PAGE,
                         PAGECACHE_LOCK_READ,
                         &link);
    check_page(buff, num);
    pagecache_unlock_by_link(&pagecache, link,
                             PAGECACHE_LOCK_READ_UNLOCK,
                             PAGECACHE_UNPIN, 0, 0, 0, FALSE);
    {
      int lim= rand() % read_sleep_limit;
      int j;
      for (j= 0; j < lim; j++)
        SLEEP;
    }
  }
}


void writer(int num)
{
  uint i;
  uchar *buff;
  PAGECACHE_BLOCK_LINK *link;

  for (i= 0; i < number_of_write_tests; i++)
  {
    uchar c= (uchar) rand() % 256;

    if (i % report_divisor == 0)
      diag("Writer %d - %u", num, i);
    buff= pagecache_read(&pagecache, &file1, 0, 3, NULL,
                         PAGECACHE_PLAIN_PAGE,
                         PAGECACHE_LOCK_WRITE,
                         &link);

    check_page(buff, num);
    bfill(buff, TEST_PAGE_SIZE / 2, c);
    SLEEP;
    bfill(buff + TEST_PAGE_SIZE/2, TEST_PAGE_SIZE / 2, c);
    check_page(buff, num);
    pagecache_unlock_by_link(&pagecache, link,
                             PAGECACHE_LOCK_WRITE_UNLOCK,
                             PAGECACHE_UNPIN, 0, 0, 1, FALSE);
    SLEEP;
  }
}


static void *test_thread_reader(void *arg)
{
  int param=*((int*) arg);
  my_thread_init();
  {
    DBUG_ENTER("test_reader");

    DBUG_PRINT("enter", ("param: %d", param));

    reader(param);

    DBUG_PRINT("info", ("Thread %s ended", my_thread_name()));
    pthread_mutex_lock(&LOCK_thread_count);
    ok(1, "reader%d: done", param);
    thread_count--;
    VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
    pthread_mutex_unlock(&LOCK_thread_count);
    free((uchar*) arg);
    my_thread_end();
  }
  return 0;
}


static void *test_thread_writer(void *arg)
{
  int param=*((int*) arg);
  my_thread_init();
  {
    DBUG_ENTER("test_writer");

    writer(param);

    DBUG_PRINT("info", ("Thread %s ended", my_thread_name()));
    pthread_mutex_lock(&LOCK_thread_count);
    ok(1, "writer%d: done", param);
    thread_count--;
    VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
    pthread_mutex_unlock(&LOCK_thread_count);
    free((uchar*) arg);
    my_thread_end();
  }
  return 0;
}


int main(int argc __attribute__((unused)),
         char **argv __attribute__((unused)))
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int *param, error, pagen;

  MY_INIT(argv[0]);

#ifndef DBUG_OFF
#if defined(__WIN__)
  default_dbug_option= "d:t:i:O,\\test_pagecache_consist.trace";
#else
  default_dbug_option= "d:t:i:O,/tmp/test_pagecache_consist.trace";
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
  plan(number_of_writers + number_of_readers);
  SKIP_BIG_TESTS(number_of_writers + number_of_readers)
  {

  if ((file1.file= my_open(file1_name,
                           O_CREAT | O_TRUNC | O_RDWR, MYF(0))) == -1)
  {
    diag( "Got error during file1 creation from open() (errno: %d)\n",
	    errno);
    exit(1);
  }
  pagecache_file_init(file1, &dummy_callback, &dummy_callback,
                      &dummy_fail_callback, &dummy_callback, NULL);
  DBUG_PRINT("info", ("file1: %d", file1.file));
  if (my_chmod(file1_name, S_IRWXU | S_IRWXG | S_IRWXO, MYF(MY_WME)))
    exit(1);
  my_pwrite(file1.file, (const uchar*) "test file", 9, 0, MYF(0));

  if ((error= pthread_cond_init(&COND_thread_count, NULL)))
  {
    diag( "COND_thread_count: %d from pthread_cond_init (errno: %d)\n",
	    error, errno);
    exit(1);
  }
  if ((error= pthread_mutex_init(&LOCK_thread_count, MY_MUTEX_INIT_FAST)))
  {
    diag( "LOCK_thread_count: %d from pthread_cond_init (errno: %d)\n",
	    error, errno);
    exit(1);
  }

  if ((error= pthread_attr_init(&thr_attr)))
  {
    diag("Got error: %d from pthread_attr_init (errno: %d)\n",
	    error,errno);
    exit(1);
  }
  if ((error= pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED)))
  {
    diag(
	    "Got error: %d from pthread_attr_setdetachstate (errno: %d)\n",
	    error,errno);
    exit(1);
  }

#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(2));
#endif

  if ((pagen= init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                             TEST_PAGE_SIZE, 0)) == 0)
  {
    diag("Got error: init_pagecache() (errno: %d)\n",
            errno);
    exit(1);
  }
  DBUG_PRINT("info", ("Page cache %d pages", pagen));
  {
    unsigned char *buffr= malloc(TEST_PAGE_SIZE);
    memset(buffr, '\0', TEST_PAGE_SIZE);
    pagecache_write(&pagecache, &file1, 0, 3, buffr,
                    PAGECACHE_PLAIN_PAGE,
                    PAGECACHE_LOCK_LEFT_UNLOCKED,
                    PAGECACHE_PIN_LEFT_UNPINNED,
                    PAGECACHE_WRITE_DELAY,
                    0, LSN_IMPOSSIBLE);
  }
  pthread_mutex_lock(&LOCK_thread_count);

  while (number_of_readers != 0 || number_of_writers != 0)
  {
    if (number_of_readers != 0)
    {
      param=(int*) malloc(sizeof(int));
      *param= number_of_readers + number_of_writers;
      if ((error= pthread_create(&tid, &thr_attr, test_thread_reader,
                                 (void*) param)))
      {
        diag("Got error: %d from pthread_create (errno: %d)\n",
                error,errno);
        exit(1);
      }
      thread_count++;
      number_of_readers--;
    }
    if (number_of_writers != 0)
    {
      param=(int*) malloc(sizeof(int));
      *param= number_of_writers + number_of_readers;
      if ((error= pthread_create(&tid, &thr_attr, test_thread_writer,
                                 (void*) param)))
      {
        diag("Got error: %d from pthread_create (errno: %d)\n",
                error,errno);
        exit(1);
      }
      thread_count++;
      number_of_writers--;
    }
  }
  DBUG_PRINT("info", ("Thread started"));
  pthread_mutex_unlock(&LOCK_thread_count);

  pthread_attr_destroy(&thr_attr);

  /* wait finishing */
  pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    if ((error= pthread_cond_wait(&COND_thread_count, &LOCK_thread_count)))
      diag("COND_thread_count: %d from pthread_cond_wait\n", error);
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("info", ("thread ended"));

  end_pagecache(&pagecache, 1);
  DBUG_PRINT("info", ("Page cache ended"));

  if (my_close(file1.file, MYF(0)) != 0)
  {
    diag( "Got error during file1 closing from close() (errno: %d)\n",
	    errno);
    exit(1);
  }
  my_delete(file1_name, MYF(0));

  DBUG_PRINT("info", ("file1 (%d) closed", file1.file));
  DBUG_PRINT("info", ("Program end"));
  } /* SKIP_BIG_TESTS */
  my_end(0);

  return exit_status();
  }
}
