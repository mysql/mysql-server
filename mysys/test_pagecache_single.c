#include "mysys_priv.h"
#include "../include/my_pthread.h"
#include "../include/pagecache.h"
#include "my_dir.h"
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "../unittest/mytap/tap.h"
#include "test_file.h"

/* #define PAGE_SIZE 1024 */
#define PCACHE_SIZE (PAGE_SIZE*1024*10)

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif

static char *file1_name= (char*)"page_cache_test_file_1";
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
  {PAGE_SIZE, '\1'},
  { 0, 0}
};
static struct file_desc simple_read_change_write_read_test_file[]=
{
  {PAGE_SIZE/2, '\65'},
  {PAGE_SIZE/2, '\1'},
  { 0, 0}
};
static struct file_desc simple_pin_test_file1[]=
{
  {PAGE_SIZE*2, '\1'},
  { 0, 0}
};
static struct file_desc simple_pin_test_file2[]=
{
  {PAGE_SIZE/2, '\1'},
  {PAGE_SIZE/2, (unsigned char)129},
  {PAGE_SIZE, '\1'},
  { 0, 0}
};
static struct file_desc  simple_delete_forget_test_file[]=
{
  {PAGE_SIZE, '\1'},
  { 0, 0}
};
static struct file_desc  simple_delete_flush_test_file[]=
{
  {PAGE_SIZE, '\2'},
  { 0, 0}
};


/*
  Recreate and reopen a file for test

  SYNOPSIS
    reset_file()
    file                 File to reset
    file_name            Path (and name) of file which should be reset
*/

void reset_file(PAGECACHE_FILE file, char *file_name)
{
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_RELEASE);
  if (my_close(file1.file, MYF(0)) != 0)
  {
    diag("Got error during %s closing from close() (errno: %d)\n",
         file_name, errno);
    exit(1);
  }
  my_delete(file_name, MYF(0));
  if ((file.file= my_open(file_name,
                          O_CREAT | O_TRUNC | O_RDWR, MYF(0))) == -1)
  {
    diag("Got error during %s creation from open() (errno: %d)\n",
         file_name, errno);
    exit(1);
  }
}

/*
  Write then read page, check file on disk
*/

int simple_read_write_test()
{
  unsigned char *buffw= malloc(PAGE_SIZE);
  unsigned char *buffr= malloc(PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_read_write_test");
  memset(buffw, '\1', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  pagecache_read(&pagecache, &file1, 0, 3, (char*)buffr,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_LEFT_UNLOCKED,
                 0);
  ok((res= test(memcmp(buffr, buffw, PAGE_SIZE) == 0)),
     "Simple write-read page ");
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  ok((res&= test(test_file(file1, file1_name, PAGE_SIZE, PAGE_SIZE,
                           simple_read_write_test_file))),
     "Simple write-read page file");
  if (res)
    reset_file(file1, file1_name);
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
  unsigned char *buffw= malloc(PAGE_SIZE);
  unsigned char *buffr= malloc(PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_read_change_write_read_test");
  /* prepare the file */
  memset(buffw, '\1', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  /* test */
  pagecache_read(&pagecache, &file1, 0, 3, (char*)buffw,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_WRITE,
                 0);
  memset(buffw, '\65', PAGE_SIZE/2);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE_UNLOCK,
                  PAGECACHE_UNPIN,
                  PAGECACHE_WRITE_DELAY,
                  0);

  pagecache_read(&pagecache, &file1, 0, 3, (char*)buffr,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_LEFT_UNLOCKED,
                 0);
  ok((res= test(memcmp(buffr, buffw, PAGE_SIZE) == 0)),
     "Simple read-change-write-read page ");
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  ok((res&= test(test_file(file1, file1_name, PAGE_SIZE, PAGE_SIZE,
                           simple_read_change_write_read_test_file))),
     "Simple read-change-write-read page file");
  if (res)
    reset_file(file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}


/*
  Prepare page, read page 0 (and pin) then write page 1 and page 0.
  Flush the file (shold flush only page 1 and return 1 (page 0 is
  still pinned).
  Check file on the disk.
  Unpin and flush.
  Check file on the disk.
*/
int simple_pin_test()
{
  unsigned char *buffw= malloc(PAGE_SIZE);
  unsigned char *buffr= malloc(PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_pin_test");
  /* prepare the file */
  memset(buffw, '\1', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  /* test */
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("error in flush_pagecache_blocks\n");
    exit(1);
  }
  pagecache_read(&pagecache, &file1, 0, 3, (char*)buffw,
                 PAGECACHE_PLAIN_PAGE,
                 PAGECACHE_LOCK_WRITE,
                 0);
  pagecache_write(&pagecache, &file1, 1, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  memset(buffw + PAGE_SIZE/2, ((unsigned char) 129), PAGE_SIZE/2);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE_TO_READ,
                  PAGECACHE_PIN_LEFT_PINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
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
  ok((res= test(test_file(file1, file1_name, PAGE_SIZE*2, PAGE_SIZE*2,
                           simple_pin_test_file1))),
     "Simple pin page file with pin");
  pagecache_unlock_page(&pagecache,
                        &file1,
                        0,
                        PAGECACHE_LOCK_READ_UNLOCK,
                        PAGECACHE_UNPIN,
                        0, 0);
  if (flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE))
  {
    diag("Got error in flush_pagecache_blocks\n");
    res= 0;
    goto err;
  }
  ok((res&= test(test_file(file1, file1_name, PAGE_SIZE*2, PAGE_SIZE,
                           simple_pin_test_file2))),
     "Simple pin page result file");
  if (res)
    reset_file(file1, file1_name);
err:
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}

/*
  Prepare page, write new value, then delete page from cache without flush,
  on the disk should be page with old content written during preparation
*/

int simple_delete_forget_test()
{
  unsigned char *buffw= malloc(PAGE_SIZE);
  unsigned char *buffr= malloc(PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_delete_forget_test");
  /* prepare the file */
  memset(buffw, '\1', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  /* test */
  memset(buffw, '\2', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_UNLOCKED,
                  PAGECACHE_PIN_LEFT_UNPINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  pagecache_delete_page(&pagecache, &file1, 0,
                        PAGECACHE_LOCK_WRITE, 0);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  ok((res= test(test_file(file1, file1_name, PAGE_SIZE, PAGE_SIZE,
                          simple_delete_forget_test_file))),
     "Simple delete-forget page file");
  if (res)
    reset_file(file1, file1_name);
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
  unsigned char *buffw= malloc(PAGE_SIZE);
  unsigned char *buffr= malloc(PAGE_SIZE);
  int res;
  DBUG_ENTER("simple_delete_flush_test");
  /* prepare the file */
  memset(buffw, '\1', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_WRITE,
                  PAGECACHE_PIN,
                  PAGECACHE_WRITE_DELAY,
                  0);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  /* test */
  memset(buffw, '\2', PAGE_SIZE);
  pagecache_write(&pagecache, &file1, 0, 3, (char*)buffw,
                  PAGECACHE_PLAIN_PAGE,
                  PAGECACHE_LOCK_LEFT_WRITELOCKED,
                  PAGECACHE_PIN_LEFT_PINNED,
                  PAGECACHE_WRITE_DELAY,
                  0);
  pagecache_delete_page(&pagecache, &file1, 0,
                        PAGECACHE_LOCK_LEFT_WRITELOCKED, 1);
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  ok((res= test(test_file(file1, file1_name, PAGE_SIZE, PAGE_SIZE,
                          simple_delete_flush_test_file))),
     "Simple delete-forget page file");
  if (res)
    reset_file(file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}


/*
  write then read file bigger then cache
*/

int simple_big_test()
{
  unsigned char *buffw= (unsigned char *)malloc(PAGE_SIZE);
  unsigned char *buffr= (unsigned char *)malloc(PAGE_SIZE);
  struct file_desc *desc=
    (struct file_desc *)malloc((PCACHE_SIZE/(PAGE_SIZE/2)) *
                               sizeof(struct file_desc));
  int res, i;
  DBUG_ENTER("simple_big_test");
  /* prepare the file twice larger then cache */
  for (i= 0; i < PCACHE_SIZE/(PAGE_SIZE/2); i++)
  {
    memset(buffw, (unsigned char) (i & 0xff), PAGE_SIZE);
    desc[i].length= PAGE_SIZE;
    desc[i].content= (i & 0xff);
    pagecache_write(&pagecache, &file1, i, 3, (char*)buffw,
                    PAGECACHE_PLAIN_PAGE,
                    PAGECACHE_LOCK_LEFT_UNLOCKED,
                    PAGECACHE_PIN_LEFT_UNPINNED,
                    PAGECACHE_WRITE_DELAY,
                    0);
  }
  ok(1, "Simple big file write");
  /* check written pages sequentally read */
  for (i= 0; i < PCACHE_SIZE/(PAGE_SIZE/2); i++)
  {
    int j;
    pagecache_read(&pagecache, &file1, i, 3, (char*)buffr,
                   PAGECACHE_PLAIN_PAGE,
                   PAGECACHE_LOCK_LEFT_UNLOCKED,
                   0);
    for(j= 0; j < PAGE_SIZE; j++)
    {
      if (buffr[j] != (i & 0xff))
      {
        diag("simple_big_test seq: page %u byte %u mismatch\n", i, j);
        return 0;
      }
    }
  }
  ok(1, "simple big file sequentally read");
  /* chack random reads */
  for (i= 0; i < PCACHE_SIZE/(PAGE_SIZE); i++)
  {
    int j, page;
    page= rand() % (PCACHE_SIZE/(PAGE_SIZE/2));
    pagecache_read(&pagecache, &file1, page, 3, (char*)buffr,
                   PAGECACHE_PLAIN_PAGE,
                   PAGECACHE_LOCK_LEFT_UNLOCKED,
                   0);
    for(j= 0; j < PAGE_SIZE; j++)
    {
      if (buffr[j] != (page & 0xff))
      {
        diag("simple_big_test rnd: page %u byte %u mismatch\n", page, j);
        return 0;
      }
    }
  }
  ok(1, "simple big file random read");
  flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);

  ok((res= test(test_file(file1, file1_name, PCACHE_SIZE*2, PAGE_SIZE,
                          desc))),
     "Simple big file");
  if (res)
    reset_file(file1, file1_name);
  free(buffw);
  free(buffr);
  DBUG_RETURN(res);
}
/*
  Thread function
*/

static void *test_thread(void *arg)
{
  int param=*((int*) arg);

  my_thread_init();
  DBUG_ENTER("test_thread");

  DBUG_PRINT("enter", ("param: %d", param));

  if (!simple_read_write_test() ||
      !simple_read_change_write_read_test() ||
      !simple_pin_test() ||
      !simple_delete_forget_test() ||
      !simple_delete_flush_test() ||
      !simple_big_test())
    exit(1);

  DBUG_PRINT("info", ("Thread %s ended\n", my_thread_name()));
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((gptr) arg);
  my_thread_end();
  DBUG_RETURN(0);
}


int main(int argc, char **argv __attribute__((unused)))
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int *param, error, pagen;

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


  DBUG_ENTER("main");
  DBUG_PRINT("info", ("Main thread: %s\n", my_thread_name()));
  if ((file1.file= my_open(file1_name,
                           O_CREAT | O_TRUNC | O_RDWR, MYF(0))) == -1)
  {
    fprintf(stderr, "Got error during file1 creation from open() (errno: %d)\n",
	    errno);
    exit(1);
  }
  DBUG_PRINT("info", ("file1: %d", file1.file));
  if (chmod(file1_name, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
  {
    fprintf(stderr, "Got error during file1 chmod() (errno: %d)\n",
	    errno);
    exit(1);
  }
  my_pwrite(file1.file, "test file", 9, 0, MYF(0));

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

#ifndef pthread_attr_setstacksize		/* void return value */
  if ((error= pthread_attr_setstacksize(&thr_attr, 65536L)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_setstacksize (errno: %d)\n",
	    error,errno);
    exit(1);
  }
#endif
#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(2));
#endif

  my_thread_global_init();


  if ((pagen= init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                             PAGE_SIZE, 0)) == 0)
  {
    fprintf(stderr,"Got error: init_pagecache() (errno: %d)\n",
            errno);
    exit(1);
  }
  DBUG_PRINT("info", ("Page cache %d pages", pagen));

  if ((error=pthread_mutex_lock(&LOCK_thread_count)))
  {
    fprintf(stderr,"Got error: %d from pthread_mutex_lock (errno: %d)\n",
            error,errno);
    exit(1);
  }
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

  if ((error= pthread_mutex_lock(&LOCK_thread_count)))
    fprintf(stderr,"Got error: %d from pthread_mutex_lock\n",error);
  while (thread_count)
  {
    if ((error= pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"Got error: %d from pthread_cond_wait\n",error);
  }
  if ((error= pthread_mutex_unlock(&LOCK_thread_count)))
    fprintf(stderr,"Got error: %d from pthread_mutex_unlock\n",error);
  DBUG_PRINT("info", ("thread ended"));

  end_pagecache(&pagecache, 1);
  DBUG_PRINT("info", ("Page cache ended"));

  if (my_close(file1.file, MYF(0)) != 0)
  {
    fprintf(stderr, "Got error during file1 closing from close() (errno: %d)\n",
	    errno);
    exit(1);
  }
  /*my_delete(file1_name, MYF(0));*/
  my_thread_global_end();

  DBUG_PRINT("info", ("file1 (%d) closed", file1.file));

  DBUG_PRINT("info", ("Program end"));

  DBUG_RETURN(0);
}
