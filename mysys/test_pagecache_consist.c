#include "mysys_priv.h"
#include "../include/my_pthread.h"
#include "../include/pagecache.h"
#include <string.h>
#include "my_dir.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "../unittest/mytap/tap.h"
#include <my_pthread.h>

/*#define PAGE_SIZE 65536*/
#define PCACHE_SIZE (PAGE_SIZE*1024*20)

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif

static char *file1_name= (char*)"page_cache_test_file_1";
static PAGECACHE_FILE file1;
static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count;
static PAGECACHE pagecache;

#ifdef TEST_HIGH_CONCURENCY
static uint number_of_readers= 10;
static uint number_of_writers= 20;
static uint number_of_tests= 30000;
static uint record_length_limit= PAGE_SIZE/200;
static uint number_of_pages= 20;
static uint flush_divider= 1000;
#else /*TEST_HIGH_CONCURENCY*/
#ifdef TEST_READERS
static uint number_of_readers= 10;
static uint number_of_writers= 1;
static uint number_of_tests= 30000;
static uint record_length_limit= PAGE_SIZE/200;
static uint number_of_pages= 20;
static uint flush_divider= 1000;
#else /*TEST_READERS*/
#ifdef TEST_WRITERS
static uint number_of_readers= 0;
static uint number_of_writers= 10;
static uint number_of_tests= 30000;
static uint record_length_limit= PAGE_SIZE/200;
static uint number_of_pages= 20;
static uint flush_divider= 1000;
#else /*TEST_WRITERS*/
static uint number_of_readers= 10;
static uint number_of_writers= 10;
static uint number_of_tests= 50000;
static uint record_length_limit= PAGE_SIZE/200;
static uint number_of_pages= 20000;
static uint flush_divider= 1000;
#endif /*TEST_WRITERS*/
#endif /*TEST_READERS*/
#endif /*TEST_HIGH_CONCURENCY*/


/* check page consistemcy */
uint check_page(uchar *buff, ulong offset, int page_locked, int page_no,
                int tag)
{
  uint end= sizeof(uint);
  uint num= *((uint *)buff);
  uint i;
  DBUG_ENTER("check_page");

  for (i= 0; i < num; i++)
  {
    uint len= *((uint *)(buff + end));
    uint j;
    end+= sizeof(uint)+ sizeof(uint);
    if (len + end > PAGE_SIZE)
    {
      diag("incorrect field header #%u by offset %lu\n", i, offset + end + j);
      goto err;
    }
    for(j= 0; j < len; j++)
    {
      if (buff[end + j] != (uchar)((i+1) % 256))
      {
        diag("incorrect %lu byte\n", offset + end + j);
        goto err;
      }
    }
    end+= len;
  }
  for(i= end; i < PAGE_SIZE; i++)
  {
    if (buff[i] != 0)
    {
      int h;
      DBUG_PRINT("err",
                 ("byte %lu (%lu + %u), page %u (%s, end: %u, recs: %u, tag: %d) should be 0\n",
                  offset + i, offset, i, page_no,
                  (page_locked ? "locked" : "unlocked"),
                  end, num, tag));
      diag("byte %lu (%lu + %u), page %u (%s, end: %u, recs: %u, tag: %d) should be 0\n",
           offset + i, offset, i, page_no,
           (page_locked ? "locked" : "unlocked"),
           end, num, tag);
      h= my_open("wrong_page", O_CREAT | O_TRUNC | O_RDWR, MYF(0));
      my_pwrite(h, buff, PAGE_SIZE, 0, MYF(0));
      my_close(h, MYF(0));
      goto err;
    }
  }
  DBUG_RETURN(end);
err:
  DBUG_PRINT("err", ("try to flush"));
  if (page_locked)
  {
    pagecache_delete_page(&pagecache, &file1, page_no,
                          PAGECACHE_LOCK_LEFT_WRITELOCKED, 1);
  }
  else
  {
    flush_pagecache_blocks(&pagecache, &file1, FLUSH_RELEASE);
  }
  exit(1);
}

void put_rec(uchar *buff, uint end, uint len, uint tag)
{
  uint i;
  uint num= *((uint *)buff);
  if (!len)
    len= 1;
  if (end + sizeof(uint)*2 + len > PAGE_SIZE)
    return;
  *((uint *)(buff + end))= len;
  end+=  sizeof(uint);
  *((uint *)(buff + end))= tag;
  end+=  sizeof(uint);
  num++;
  *((uint *)buff)= num;
  *((uint*)(buff + end))= len;
  for (i= end; i < (len + end); i++)
  {
    buff[i]= (uchar) num % 256;
  }
}

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


void reader(int num)
{
  unsigned char *buffr= malloc(PAGE_SIZE);
  uint i;

  for (i= 0; i < number_of_tests; i++)
  {
    uint page= rand()/(RAND_MAX/number_of_pages);
    pagecache_read(&pagecache, &file1, page, 3, (char*)buffr,
                   PAGECACHE_PLAIN_PAGE,
                   PAGECACHE_LOCK_LEFT_UNLOCKED,
                   0);
    check_page(buffr, page * PAGE_SIZE, 0, page, -num);
    if (i % 500 == 0)
      printf("reader%d: %d\n", num, i);

  }
  printf("reader%d: done\n", num);
  free(buffr);
}


void writer(int num)
{
  unsigned char *buffr= malloc(PAGE_SIZE);
  uint i;

  for (i= 0; i < number_of_tests; i++)
  {
    uint end;
    uint page= rand()/(RAND_MAX/number_of_pages);
    pagecache_read(&pagecache, &file1, page, 3, (char*)buffr,
                   PAGECACHE_PLAIN_PAGE,
                   PAGECACHE_LOCK_WRITE,
                   0);
    end= check_page(buffr, page * PAGE_SIZE, 1, page, num);
    put_rec(buffr, end, rand()/(RAND_MAX/record_length_limit), num);
    pagecache_write(&pagecache, &file1, page, 3, (char*)buffr,
                    PAGECACHE_PLAIN_PAGE,
                    PAGECACHE_LOCK_WRITE_UNLOCK,
                    PAGECACHE_UNPIN,
                    PAGECACHE_WRITE_DELAY,
                    0);

    if (i % flush_divider == 0)
      flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
    if (i % 500 == 0)
      printf("writer%d: %d\n", num, i);
  }
  printf("writer%d: done\n", num);
  free(buffr);
}


static void *test_thread_reader(void *arg)
{
  int param=*((int*) arg);

  my_thread_init();
  DBUG_ENTER("test_reader");
  DBUG_PRINT("enter", ("param: %d", param));

  reader(param);

  DBUG_PRINT("info", ("Thread %s ended\n", my_thread_name()));
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((gptr) arg);
  my_thread_end();
  DBUG_RETURN(0);
}

static void *test_thread_writer(void *arg)
{
  int param=*((int*) arg);

  my_thread_init();
  DBUG_ENTER("test_writer");
  DBUG_PRINT("enter", ("param: %d", param));

  writer(param);

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
  default_dbug_option= "d:t:i:O,\\test_pagecache_consist.trace";
#else
  default_dbug_option= "d:t:i:o,/tmp/test_pagecache_consist.trace";
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
    fprintf(stderr, "COND_thread_count: %d from pthread_cond_init (errno: %d)\n",
	    error, errno);
    exit(1);
  }
  if ((error= pthread_mutex_init(&LOCK_thread_count, MY_MUTEX_INIT_FAST)))
  {
    fprintf(stderr, "LOCK_thread_count: %d from pthread_cond_init (errno: %d)\n",
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
  {
    unsigned char *buffr= malloc(PAGE_SIZE);
    uint i;
    memset(buffr, '\0', PAGE_SIZE);
    for (i= 0; i < number_of_pages; i++)
    {
      pagecache_write(&pagecache, &file1, i, 3, (char*)buffr,
                      PAGECACHE_PLAIN_PAGE,
                      PAGECACHE_LOCK_LEFT_UNLOCKED,
                      PAGECACHE_PIN_LEFT_UNPINNED,
                      PAGECACHE_WRITE_DELAY,
                      0);
    }
    flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
    free(buffr);
  }
  if ((error= pthread_mutex_lock(&LOCK_thread_count)))
  {
    fprintf(stderr,"LOCK_thread_count: %d from pthread_mutex_lock (errno: %d)\n",
            error,errno);
    exit(1);
  }
  while (number_of_readers != 0 || number_of_writers != 0)
  {
    if (number_of_readers != 0)
    {
      param=(int*) malloc(sizeof(int));
      *param= number_of_readers;
      if ((error= pthread_create(&tid, &thr_attr, test_thread_reader,
                                 (void*) param)))
      {
        fprintf(stderr,"Got error: %d from pthread_create (errno: %d)\n",
                error,errno);
        exit(1);
      }
      thread_count++;
      number_of_readers--;
    }
    if (number_of_writers != 0)
    {
      param=(int*) malloc(sizeof(int));
      *param= number_of_writers;
      if ((error= pthread_create(&tid, &thr_attr, test_thread_writer,
                                 (void*) param)))
      {
        fprintf(stderr,"Got error: %d from pthread_create (errno: %d)\n",
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
  if ((error= pthread_mutex_lock(&LOCK_thread_count)))
    fprintf(stderr,"LOCK_thread_count: %d from pthread_mutex_lock\n",error);
  while (thread_count)
  {
    if ((error= pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"COND_thread_count: %d from pthread_cond_wait\n",error);
  }
  if ((error= pthread_mutex_unlock(&LOCK_thread_count)))
    fprintf(stderr,"LOCK_thread_count: %d from pthread_mutex_unlock\n",error);
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
