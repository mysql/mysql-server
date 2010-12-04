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

#include "../maria_def.h"
#include <stdio.h>
#include <errno.h>
#include <tap.h>
#include "../trnman.h"

extern my_bool maria_log_remove();
extern void translog_example_table_init();

#ifndef DBUG_OFF
static const char *default_dbug_option;
#endif

#define PCACHE_SIZE (1024*1024*10)

#define LOG_FILE_SIZE (1024L*1024L*1024L + 1024L*1024L*512)
/*#define LOG_FLAGS TRANSLOG_SECTOR_PROTECTION | TRANSLOG_PAGE_CRC */
#define LOG_FLAGS 0
/*#define LONG_BUFFER_SIZE (1024L*1024L*1024L + 1024L*1024L*512)*/

#ifdef MULTIFLUSH_TEST

#define LONG_BUFFER_SIZE (16384L)
#define MIN_REC_LENGTH 10
#define SHOW_DIVIDER 20
#define ITERATIONS 10000
#define FLUSH_ITERATIONS 1000
#define WRITERS 2
#define FLUSHERS 10

#else

#define LONG_BUFFER_SIZE (512L*1024L*1024L)
#define MIN_REC_LENGTH 30
#define SHOW_DIVIDER 10
#define ITERATIONS 3
#define FLUSH_ITERATIONS 0
#define WRITERS 3
#define FLUSHERS 0

#endif

static uint number_of_writers= WRITERS;
static uint number_of_flushers= FLUSHERS;

static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count;

static ulong lens[WRITERS][ITERATIONS];
static LSN lsns1[WRITERS][ITERATIONS];
static LSN lsns2[WRITERS][ITERATIONS];
static uchar *long_buffer;


static LSN last_lsn; /* For test purposes the variable allow dirty read/write */

/*
  Get pseudo-random length of the field in
    limits [MIN_REC_LENGTH..LONG_BUFFER_SIZE]

  SYNOPSIS
    get_len()

  RETURN
    length - length >= 0 length <= LONG_BUFFER_SIZE
*/

static uint32 get_len()
{
  return MIN_REC_LENGTH +
    (uint32)(((ulonglong)rand())*
       (LONG_BUFFER_SIZE - MIN_REC_LENGTH - 1)/RAND_MAX);
}


/*
  Check that the buffer filled correctly

  SYNOPSIS
    check_content()
    ptr                  Pointer to the buffer
    length               length of the buffer

  RETURN
    0 - OK
    1 - Error
*/

static my_bool check_content(uchar *ptr, ulong length)
{
  ulong i;
  for (i= 0; i < length; i++)
  {
    if (((uchar)ptr[i]) != (i & 0xFF))
    {
      fprintf(stderr, "Byte # %lu is %x instead of %x",
              i, (uint) ptr[i], (uint) (i & 0xFF));
      return 1;
    }
  }
  return 0;
}


/*
  Read whole record content, and check content (put with offset)

  SYNOPSIS
    read_and_check_content()
    rec                  The record header buffer
    buffer               The buffer to read the record in
    skip                 Skip this number of bytes ot the record content

  RETURN
    0 - OK
    1 - Error
*/


static my_bool read_and_check_content(TRANSLOG_HEADER_BUFFER *rec,
                                      uchar *buffer, uint skip)
{
  int res= 0;
  translog_size_t len;

  if ((len= translog_read_record(rec->lsn, 0, rec->record_length,
                                 buffer, NULL)) != rec->record_length)
  {
    fprintf(stderr, "Requested %lu byte, read %lu\n",
            (ulong) rec->record_length, (ulong) len);
    res= 1;
  }
  res|= check_content(buffer + skip, rec->record_length - skip);
  return(res);
}

void writer(int num)
{
  LSN lsn;
  TRN trn;
  uchar long_tr_id[6];
  uint i;

  trn.short_id= num;
  trn.first_undo_lsn= TRANSACTION_LOGGED_LONG_ID;
  for (i= 0; i < ITERATIONS; i++)
  {
    uint len= get_len();
    LEX_CUSTRING parts[TRANSLOG_INTERNAL_PARTS + 1];
    lens[num][i]= len;

    int2store(long_tr_id, num);
    int4store(long_tr_id + 2, i);
    parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_tr_id;
    parts[TRANSLOG_INTERNAL_PARTS + 0].length= 6;
    if (translog_write_record(&lsn,
                              LOGREC_FIXED_RECORD_0LSN_EXAMPLE,
                              &trn, NULL, 6, TRANSLOG_INTERNAL_PARTS + 1,
                              parts, NULL, NULL))
    {
      fprintf(stderr, "Can't write LOGREC_FIXED_RECORD_0LSN_EXAMPLE record #%lu "
              "thread %i\n", (ulong) i, num);
      translog_destroy();
      pthread_mutex_lock(&LOCK_thread_count);
      ok(0, "write records");
      pthread_mutex_unlock(&LOCK_thread_count);
      return;
    }
    lsns1[num][i]= lsn;
    parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_buffer;
    parts[TRANSLOG_INTERNAL_PARTS + 0].length= len;
    if (translog_write_record(&lsn,
                              LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE,
                              &trn, NULL,
                              len, TRANSLOG_INTERNAL_PARTS + 1,
                              parts, NULL, NULL))
    {
      fprintf(stderr, "Can't write variable record #%lu\n", (ulong) i);
      translog_destroy();
      pthread_mutex_lock(&LOCK_thread_count);
      ok(0, "write records");
      pthread_mutex_unlock(&LOCK_thread_count);
      return;
    }
    lsns2[num][i]= lsn;
    last_lsn= lsn;
    pthread_mutex_lock(&LOCK_thread_count);
    ok(1, "write records");
    pthread_mutex_unlock(&LOCK_thread_count);
  }
  return;
}


static void *test_thread_writer(void *arg)
{
  int param= *((int*) arg);

  my_thread_init();

  writer(param);

  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  ok(1, "writer finished"); /* just to show progress */
  VOID(pthread_cond_signal(&COND_thread_count));        /* Tell main we are
                                                           ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((uchar*) arg);
  my_thread_end();
  return(0);
}


static void *test_thread_flusher(void *arg)
{
  int param= *((int*) arg);
  int i;

  my_thread_init();

  for(i= 0; i < FLUSH_ITERATIONS; i++)
  {
    translog_flush(last_lsn);
    pthread_mutex_lock(&LOCK_thread_count);
    ok(1, "-- flush %d", param);
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  ok(1, "flusher finished"); /* just to show progress */
  VOID(pthread_cond_signal(&COND_thread_count));        /* Tell main we are
                                                           ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((uchar*) arg);
  my_thread_end();
  return(0);
}


int main(int argc __attribute__((unused)),
         char **argv __attribute__ ((unused)))
{
  uint32 i;
  uint pagen;
  PAGECACHE pagecache;
  LSN first_lsn;
  TRANSLOG_HEADER_BUFFER rec;
  struct st_translog_scanner_data scanner;
  pthread_t tid;
  pthread_attr_t thr_attr;
  int *param, error;
  int rc;

  plan(WRITERS + FLUSHERS +
       ITERATIONS * WRITERS * 3 + FLUSH_ITERATIONS * FLUSHERS );

  bzero(&pagecache, sizeof(pagecache));
  maria_data_root= (char *)".";
  long_buffer= malloc(LONG_BUFFER_SIZE + 7 * 2 + 2);
  if (long_buffer == 0)
  {
    fprintf(stderr, "End of memory\n");
    exit(1);
  }
  for (i= 0; i < (LONG_BUFFER_SIZE + 7 * 2 + 2); i++)
    long_buffer[i]= (i & 0xFF);

  MY_INIT(argv[0]);
  if (maria_log_remove())
    exit(1);


#ifndef DBUG_OFF
#if defined(__WIN__)
  default_dbug_option= "d:t:i:O,\\ma_test_loghandler.trace";
#else
  default_dbug_option= "d:t:i:o,/tmp/ma_test_loghandler.trace";
#endif
  if (argc > 1)
  {
    DBUG_SET(default_dbug_option);
    DBUG_SET_INITIAL(default_dbug_option);
  }
#endif


  if ((error= pthread_cond_init(&COND_thread_count, NULL)))
  {
    fprintf(stderr, "COND_thread_count: %d from pthread_cond_init "
            "(errno: %d)\n", error, errno);
    exit(1);
  }
  if ((error= pthread_mutex_init(&LOCK_thread_count, MY_MUTEX_INIT_FAST)))
  {
    fprintf(stderr, "LOCK_thread_count: %d from pthread_cond_init "
            "(errno: %d)\n", error, errno);
    exit(1);
  }
  if ((error= pthread_attr_init(&thr_attr)))
  {
    fprintf(stderr, "Got error: %d from pthread_attr_init "
            "(errno: %d)\n", error, errno);
    exit(1);
  }
  if ((error= pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED)))
  {
    fprintf(stderr,
            "Got error: %d from pthread_attr_setdetachstate (errno: %d)\n",
            error, errno);
    exit(1);
  }

#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(2));
#endif

  my_thread_global_init();

  if (ma_control_file_open(TRUE, TRUE))
  {
    fprintf(stderr, "Can't init control file (%d)\n", errno);
    exit(1);
  }
  if ((pagen= init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                             TRANSLOG_PAGE_SIZE, 0)) == 0)
  {
    fprintf(stderr, "Got error: init_pagecache() (errno: %d)\n", errno);
    exit(1);
  }
  if (translog_init_with_table(".", LOG_FILE_SIZE, 50112, 0, &pagecache,
                               LOG_FLAGS, 0, &translog_example_table_init,
                               0))
  {
    fprintf(stderr, "Can't init loghandler (%d)\n", errno);
    exit(1);
  }
  /* Suppressing of automatic record writing */
  dummy_transaction_object.first_undo_lsn|= TRANSACTION_LOGGED_LONG_ID;

  srand(122334817L);
  {
    LEX_CUSTRING parts[TRANSLOG_INTERNAL_PARTS + 1];
    uchar long_tr_id[6]=
    {
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66
    };

    parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_tr_id;
    parts[TRANSLOG_INTERNAL_PARTS + 0].length= 6;
    dummy_transaction_object.first_undo_lsn= TRANSACTION_LOGGED_LONG_ID;
    if (translog_write_record(&first_lsn,
                              LOGREC_FIXED_RECORD_0LSN_EXAMPLE,
                              &dummy_transaction_object, NULL, 6,
                              TRANSLOG_INTERNAL_PARTS + 1,
                              parts, NULL, NULL))
    {
      fprintf(stderr, "Can't write the first record\n");
      translog_destroy();
      exit(1);
    }
  }


  pthread_mutex_lock(&LOCK_thread_count);
  while (number_of_writers != 0 || number_of_flushers != 0)
  {
    if (number_of_writers)
    {
      param= (int*) malloc(sizeof(int));
      *param= number_of_writers - 1;
      if ((error= pthread_create(&tid, &thr_attr, test_thread_writer,
                                 (void*) param)))
      {
        fprintf(stderr, "Got error: %d from pthread_create (errno: %d)\n",
                error, errno);
        exit(1);
      }
      thread_count++;
      number_of_writers--;
    }
    if (number_of_flushers)
    {
      param= (int*) malloc(sizeof(int));
      *param= number_of_flushers - 1;
      if ((error= pthread_create(&tid, &thr_attr, test_thread_flusher,
                                 (void*) param)))
      {
        fprintf(stderr, "Got error: %d from pthread_create (errno: %d)\n",
                error, errno);
        exit(1);
      }
      thread_count++;
      number_of_flushers--;
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);

  pthread_attr_destroy(&thr_attr);

  /* wait finishing */
  pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    if ((error= pthread_cond_wait(&COND_thread_count, &LOCK_thread_count)))
      fprintf(stderr, "COND_thread_count: %d from pthread_cond_wait\n", error);
  }
  pthread_mutex_unlock(&LOCK_thread_count);

  /* Find last LSN and flush up to it (all our log) */
  {
    LSN max= 0;
    for (i= 0; i < WRITERS; i++)
    {
      if (cmp_translog_addr(lsns2[i][ITERATIONS - 1], max) > 0)
        max= lsns2[i][ITERATIONS - 1];
    }
    translog_flush(max);
  }

  rc= 1;

  {
    uint indeces[WRITERS];
    uint index, stage;
    int len;
    bzero(indeces, sizeof(uint) * WRITERS);

    bzero(indeces, sizeof(indeces));

    if (translog_scanner_init(first_lsn, 1, &scanner, 0))
    {
      fprintf(stderr, "scanner init failed\n");
      goto err;
    }
    for (i= 0;; i++)
    {
      len= translog_read_next_record_header(&scanner, &rec);

      if (len == RECHEADER_READ_ERROR)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header failed (%d)\n",
                i, errno);
        translog_free_record_header(&rec);
        goto err;
      }
      if (len == RECHEADER_READ_EOF)
      {
        if (i != WRITERS * ITERATIONS * 2)
        {
          fprintf(stderr, "EOL met at iteration %u instead of %u\n",
                  i, ITERATIONS * WRITERS * 2);
          translog_free_record_header(&rec);
          goto err;
        }
        break;
      }
      index= indeces[rec.short_trid] / 2;
      stage= indeces[rec.short_trid] % 2;
      if (stage == 0)
      {
        if (rec.type !=LOGREC_FIXED_RECORD_0LSN_EXAMPLE ||
            rec.record_length != 6 ||
            uint2korr(rec.header) != rec.short_trid ||
            index != uint4korr(rec.header + 2) ||
            cmp_translog_addr(lsns1[rec.short_trid][index], rec.lsn) != 0)
        {
          fprintf(stderr, "Incorrect LOGREC_FIXED_RECORD_0LSN_EXAMPLE "
                  "data read(%d)\n"
                  "type %u, strid %u %u, len %u, i: %u %u, "
                  "lsn(%lu,0x%lx) (%lu,0x%lx)\n",
                  i, (uint) rec.type,
                  (uint) rec.short_trid, (uint) uint2korr(rec.header),
                  (uint) rec.record_length,
                  (uint) index, (uint) uint4korr(rec.header + 2),
                  LSN_IN_PARTS(rec.lsn),
                  LSN_IN_PARTS(lsns1[rec.short_trid][index]));
          translog_free_record_header(&rec);
          goto err;
        }
      }
      else
      {
        if (rec.type != LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE ||
            len != 9 ||
            rec.record_length != lens[rec.short_trid][index] ||
            cmp_translog_addr(lsns2[rec.short_trid][index], rec.lsn) != 0 ||
            check_content(rec.header, (uint)len))
        {
          fprintf(stderr,
                  "Incorrect LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE "
                  "data read(%d) "
                  "thread: %d, iteration %d, stage %d\n"
                  "type %u (%d), len %d, length %lu %lu (%d) "
                  "lsn(%lu,0x%lx) (%lu,0x%lx)\n",
                  i, (uint) rec.short_trid, index, stage,
                  (uint) rec.type, (rec.type !=
                                    LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE),
                  len,
                  (ulong) rec.record_length, lens[rec.short_trid][index],
                  (rec.record_length != lens[rec.short_trid][index]),
                  LSN_IN_PARTS(rec.lsn),
                  LSN_IN_PARTS(lsns2[rec.short_trid][index]));
          translog_free_record_header(&rec);
          goto err;
        }
        if (read_and_check_content(&rec, long_buffer, 0))
        {
          fprintf(stderr,
                  "Incorrect LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE "
                  "in whole rec read lsn(%lu,0x%lx)\n",
                  LSN_IN_PARTS(rec.lsn));
          translog_free_record_header(&rec);
          goto err;
        }
      }
      ok(1, "record read");
      translog_free_record_header(&rec);
      indeces[rec.short_trid]++;
    }
  }

  rc= 0;
err:
  if (rc)
    ok(0, "record read");
  translog_destroy();
  end_pagecache(&pagecache, 1);
  ma_control_file_end();
  if (maria_log_remove())
    exit(1);

  return(exit_status());
}
