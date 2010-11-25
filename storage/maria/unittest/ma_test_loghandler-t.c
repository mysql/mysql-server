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
extern void example_loghandler_init();

#ifndef DBUG_OFF
static const char *default_dbug_option;
#endif
static TRN *trn= &dummy_transaction_object;

#define PCACHE_SIZE (1024*1024*10)

#define LONG_BUFFER_SIZE (100 * 1024)

#ifdef LONG_LOG_TEST
#define LOG_FLAGS 0
#define LOG_FILE_SIZE (1024L*1024L*8)
#define ITERATIONS (1600*4)

#else
#undef SKIP_BIG_TESTS
#define SKIP_BIG_TESTS(X) /* no-op */
#define LOG_FLAGS (TRANSLOG_SECTOR_PROTECTION | TRANSLOG_PAGE_CRC)
#define LOG_FILE_SIZE (1024L*1024L*8L)
#define ITERATIONS 1600
#endif

/*
#define LOG_FLAGS 0
#define LOG_FILE_SIZE 1024L*1024L*1024L
#define ITERATIONS 181000
*/

/*
#define LOG_FLAGS 0
#define LOG_FILE_SIZE 1024L*1024L*3L
#define ITERATIONS 1600
*/

/*
#define LOG_FLAGS 0
#define LOG_FILE_SIZE 1024L*1024L*100L
#define ITERATIONS 65000
*/

/*
  Generate random value in the range (0,LONG_BUFFER_SIZE)
*/
static uint32 rand_buffer_size()
{
  return (uint32)((ulonglong)rand()*(LONG_BUFFER_SIZE + 1)/RAND_MAX);
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
  uchar buff[2];
  for (i= 0; i < length; i++)
  {
    if (i % 2 == 0)
      int2store(buff, i >> 1);
    if (ptr[i] != buff[i % 2])
    {
      fprintf(stderr, "Byte # %lu is %x instead of %x",
              i, (uint) ptr[i], (uint) buff[i % 2]);
      return 1;
    }
  }
  return 0;
}


/*
  Report OK for read operation

  SYNOPSIS
    read_ok()
    rec                  the record header
*/

void read_ok(TRANSLOG_HEADER_BUFFER *rec)
{
  ok(1, "read record type: %u  LSN: (%lu,0x%lx)",
     rec->type, LSN_IN_PARTS(rec->lsn));
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
  DBUG_ASSERT(rec->record_length < LONG_BUFFER_SIZE * 2 + 7 * 2 + 2);
  if (translog_read_record(rec->lsn, 0, rec->record_length, buffer, NULL) !=
      rec->record_length)
      return 1;
  return check_content(buffer + skip, rec->record_length - skip);
}


int main(int argc __attribute__((unused)), char *argv[])
{
  uint32 i;
  uint32 rec_len;
  uint pagen;
  uchar long_tr_id[6];
  uchar lsn_buff[23]=
  {
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
  };
  uchar long_buffer[LONG_BUFFER_SIZE * 2 + LSN_STORE_SIZE * 2 + 2];
  PAGECACHE pagecache;
  LSN lsn, lsn_base, first_lsn;
  TRANSLOG_HEADER_BUFFER rec;
  LEX_CUSTRING parts[TRANSLOG_INTERNAL_PARTS + 3];
  struct st_translog_scanner_data scanner;
  int rc;

  MY_INIT(argv[0]);

  if (my_set_max_open_files(100) < 100)
  {
    fprintf(stderr, "can't allocate 100 file descriptors\n");
    exit(1);
  }
  bzero(&pagecache, sizeof(pagecache));
  maria_data_root= (char *)".";
  if (maria_log_remove())
    exit(1);

  for (i= 0; i < (LONG_BUFFER_SIZE + LSN_STORE_SIZE * 2 + 2); i+= 2)
  {
    int2store(long_buffer + i, (i >> 1));
    /* long_buffer[i]= (i & 0xFF); */
  }

  bzero(long_tr_id, 6);
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
  trn->first_undo_lsn|= TRANSACTION_LOGGED_LONG_ID;

  plan(((ITERATIONS - 1) * 4 + 1)*2 + ITERATIONS - 1 + 1);

  SKIP_BIG_TESTS(((ITERATIONS - 1) * 4 + 1)*2 + ITERATIONS - 1 + 1)
  {

  srand(122334817L);

  long_tr_id[5]= 0xff;

  int4store(long_tr_id, 0);
  parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_tr_id;
  parts[TRANSLOG_INTERNAL_PARTS + 0].length= 6;
  trn->short_id= 0;
  trn->first_undo_lsn= TRANSACTION_LOGGED_LONG_ID;
  if (translog_write_record(&lsn,
                            LOGREC_FIXED_RECORD_0LSN_EXAMPLE,
                            trn, NULL, 6, TRANSLOG_INTERNAL_PARTS + 1,
                            parts, NULL, NULL))
  {
    fprintf(stderr, "Can't write record #%lu\n", (ulong) 0);
    translog_destroy();
    ok(0, "write LOGREC_FIXED_RECORD_0LSN_EXAMPLE");
    exit(1);
  }
  ok(1, "write LOGREC_FIXED_RECORD_0LSN_EXAMPLE");
  lsn_base= first_lsn= lsn;

  for (i= 1; i < ITERATIONS; i++)
  {
    trn->short_id= i % 0xFFFF;
    if (i % 2)
    {
      lsn_store(lsn_buff, lsn_base);
      parts[TRANSLOG_INTERNAL_PARTS + 0].str= lsn_buff;
      parts[TRANSLOG_INTERNAL_PARTS + 0].length= LSN_STORE_SIZE;
      /* check auto-count feature */
      parts[TRANSLOG_INTERNAL_PARTS + 1].str= NULL;
      parts[TRANSLOG_INTERNAL_PARTS + 1].length= 0;
      if (translog_write_record(&lsn, LOGREC_FIXED_RECORD_1LSN_EXAMPLE, trn,
                                NULL, LSN_STORE_SIZE, 0, parts, NULL, NULL))
      {
        fprintf(stderr, "1 Can't write reference defore record #%lu\n",
                (ulong) i);
        translog_destroy();
        ok(0, "write LOGREC_FIXED_RECORD_1LSN_EXAMPLE");
        exit(1);
      }
      ok(1, "write LOGREC_FIXED_RECORD_1LSN_EXAMPLE");
      lsn_store(lsn_buff, lsn_base);
      if ((rec_len= rand_buffer_size()) < 12)
        rec_len= 12;
      parts[TRANSLOG_INTERNAL_PARTS + 0].str= lsn_buff;
      parts[TRANSLOG_INTERNAL_PARTS + 0].length= LSN_STORE_SIZE;
      parts[TRANSLOG_INTERNAL_PARTS + 1].str= long_buffer;
      parts[TRANSLOG_INTERNAL_PARTS + 1].length= rec_len;
      /* check record length auto-counting */
      if (translog_write_record(&lsn,
                                LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE,
                                trn, NULL, 0, TRANSLOG_INTERNAL_PARTS + 2,
                                parts, NULL, NULL))
      {
        fprintf(stderr, "1 Can't write var reference defore record #%lu\n",
                (ulong) i);
        translog_destroy();
        ok(0, "write LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE");
        exit(1);
      }
      ok(1, "write LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE");
    }
    else
    {
      lsn_store(lsn_buff, lsn_base);
      lsn_store(lsn_buff + LSN_STORE_SIZE, first_lsn);
      parts[TRANSLOG_INTERNAL_PARTS + 0].str= lsn_buff;
      parts[TRANSLOG_INTERNAL_PARTS + 0].length= 23;
      if (translog_write_record(&lsn,
                                LOGREC_FIXED_RECORD_2LSN_EXAMPLE,
                                trn, NULL, 23, TRANSLOG_INTERNAL_PARTS + 1,
                                parts, NULL, NULL))
      {
        fprintf(stderr, "0 Can't write reference defore record #%lu\n",
                (ulong) i);
        translog_destroy();
        ok(0, "write LOGREC_FIXED_RECORD_2LSN_EXAMPLE");
        exit(1);
      }
      ok(1, "write LOGREC_FIXED_RECORD_2LSN_EXAMPLE");
      lsn_store(lsn_buff, lsn_base);
      lsn_store(lsn_buff + LSN_STORE_SIZE, first_lsn);
      if ((rec_len= rand_buffer_size()) < 19)
        rec_len= 19;
      parts[TRANSLOG_INTERNAL_PARTS + 0].str= lsn_buff;
      parts[TRANSLOG_INTERNAL_PARTS + 0].length= 14;
      parts[TRANSLOG_INTERNAL_PARTS + 1].str= long_buffer;
      parts[TRANSLOG_INTERNAL_PARTS + 1].length= rec_len;
      if (translog_write_record(&lsn,
                                LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE,
                                trn, NULL, 14 + rec_len,
                                TRANSLOG_INTERNAL_PARTS + 2, parts, NULL,
                                NULL))
      {
        fprintf(stderr, "0 Can't write var reference defore record #%lu\n",
                (ulong) i);
        translog_destroy();
        ok(0, "write LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE");
        exit(1);
      }
      ok(1, "write LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE");
    }
    int4store(long_tr_id, i);
    parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_tr_id;
    parts[TRANSLOG_INTERNAL_PARTS + 0].length= 6;
    if (translog_write_record(&lsn,
                              LOGREC_FIXED_RECORD_0LSN_EXAMPLE,
                              trn, NULL, 6,
                              TRANSLOG_INTERNAL_PARTS + 1,
                              parts, NULL, NULL))
    {
      fprintf(stderr, "Can't write record #%lu\n", (ulong) i);
      translog_destroy();
      ok(0, "write LOGREC_FIXED_RECORD_0LSN_EXAMPLE");
      exit(1);
    }
    ok(1, "write LOGREC_FIXED_RECORD_0LSN_EXAMPLE");

    lsn_base= lsn;

    if ((rec_len= rand_buffer_size()) < 9)
      rec_len= 9;
    parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_buffer;
    parts[TRANSLOG_INTERNAL_PARTS + 0].length= rec_len;
    if (translog_write_record(&lsn,
                              LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE,
                              trn, NULL, rec_len,
                              TRANSLOG_INTERNAL_PARTS + 1,
                              parts, NULL, NULL))
    {
      fprintf(stderr, "Can't write variable record #%lu\n", (ulong) i);
      translog_destroy();
      ok(0, "write LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE");
      exit(1);
    }
    ok(1, "write LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE");
    if (translog_flush(lsn))
    {
      fprintf(stderr, "Can't flush #%lu\n", (ulong) i);
      translog_destroy();
      ok(0, "flush");
      exit(1);
    }
    ok(1, "flush");
  }

  if (translog_flush(translog_get_horizon()))
  {
    fprintf(stderr, "Can't flush up to horizon\n");
    translog_destroy();
    ok(0, "flush");
    exit(1);
  }
  ok(1, "flush");

  srand(122334817L);

  rc= 1;

  {
    int len= translog_read_record_header(first_lsn, &rec);
    if (len == RECHEADER_READ_ERROR)
    {
      fprintf(stderr, "translog_read_record_header failed (%d)\n", errno);
      goto err;
    }
    if (rec.type !=LOGREC_FIXED_RECORD_0LSN_EXAMPLE || rec.short_trid != 0 ||
        rec.record_length != 6 || uint4korr(rec.header) != 0 ||
        ((uchar)rec.header[4]) != 0 || ((uchar)rec.header[5]) != 0xFF ||
        first_lsn != rec.lsn)
    {
      fprintf(stderr, "Incorrect LOGREC_FIXED_RECORD_0LSN_EXAMPLE "
              "data read(0)\n"
              "type %u, strid %u, len %u, i: %u, 4: %u 5: %u, "
              "lsn(%lu,0x%lx)\n",
              (uint) rec.type, (uint) rec.short_trid, (uint) rec.record_length,
              (uint) uint4korr(rec.header), (uint) rec.header[4],
              (uint) rec.header[5],
              LSN_IN_PARTS(rec.lsn));
      goto err;
    }
    read_ok(&rec);
    translog_free_record_header(&rec);
    lsn= first_lsn;
    if (translog_scanner_init(first_lsn, 1, &scanner, 0))
    {
      fprintf(stderr, "scanner init failed\n");
      goto err;
    }
    for (i= 1;; i++)
    {
      len= translog_read_next_record_header(&scanner, &rec);
      if (len == RECHEADER_READ_ERROR)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header failed (%d)\n",
                i, errno);
        goto err;
      }
      if (len == RECHEADER_READ_EOF)
      {
        if (i != ITERATIONS)
        {
          fprintf(stderr, "EOL met at iteration %u instead of %u\n",
                  i, ITERATIONS);
          goto err;
        }
        break;
      }
      if (i % 2)
      {
        LSN ref;
        ref= lsn_korr(rec.header);
        if (rec.type != LOGREC_FIXED_RECORD_1LSN_EXAMPLE ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != 7 || ref != lsn)
        {
          fprintf(stderr, "Incorrect LOGREC_FIXED_RECORD_1LSN_EXAMPLE "
                  "data read(%d) "
                  "type: %u  strid: %u  len: %u"
                  "ref: (%lu,0x%lx)  (%lu,0x%lx)  "
                  "lsn(%lu,0x%lx)\n",
                  i, (uint) rec.type, (uint) rec.short_trid,
                  (uint) rec.record_length,
                  LSN_IN_PARTS(ref), LSN_IN_PARTS(lsn),
                  LSN_IN_PARTS(rec.lsn));
          goto err;
        }
      }
      else
      {
        LSN ref1, ref2;
        ref1= lsn_korr(rec.header);
        ref2= lsn_korr(rec.header + LSN_STORE_SIZE);
        if (rec.type != LOGREC_FIXED_RECORD_2LSN_EXAMPLE ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != 23 ||
            ref1 != lsn ||
            ref2 != first_lsn ||
            ((uchar)rec.header[22]) != 0x55 ||
            ((uchar)rec.header[21]) != 0xAA ||
            ((uchar)rec.header[20]) != 0x55 ||
            ((uchar)rec.header[19]) != 0xAA ||
            ((uchar)rec.header[18]) != 0x55 ||
            ((uchar)rec.header[17]) != 0xAA ||
            ((uchar)rec.header[16]) != 0x55 ||
            ((uchar)rec.header[15]) != 0xAA ||
            ((uchar)rec.header[14]) != 0x55)
        {
          fprintf(stderr, "Incorrect LOGREC_FIXED_RECORD_2LSN_EXAMPLE "
                  "data read(%d) "
                  "type %u, strid %u, len %u, ref1(%lu,0x%lx), "
                  "ref2(%lu,0x%lx) %x%x%x%x%x%x%x%x%x "
                  "lsn(%lu,0x%lx)\n",
                  i, (uint) rec.type, (uint) rec.short_trid,
                  (uint) rec.record_length,
                  LSN_IN_PARTS(ref1), LSN_IN_PARTS(ref2),
                  (uint) rec.header[14], (uint) rec.header[15],
                  (uint) rec.header[16], (uint) rec.header[17],
                  (uint) rec.header[18], (uint) rec.header[19],
                  (uint) rec.header[20], (uint) rec.header[21],
                  (uint) rec.header[22],
                  LSN_IN_PARTS(rec.lsn));
          goto err;
        }
      }
      read_ok(&rec);
      translog_free_record_header(&rec);

      len= translog_read_next_record_header(&scanner, &rec);
      if (len == RECHEADER_READ_ERROR)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header (var) "
                "failed (%d)\n", i, errno);
        goto err;
      }
      if (len == RECHEADER_READ_EOF)
      {
        fprintf(stderr, "EOL met at the middle of iteration (first var) %u "
                "instead of beginning of %u\n", i, ITERATIONS);
        goto err;
      }
      if (i % 2)
      {
        LSN ref;
        ref= lsn_korr(rec.header);
        if ((rec_len= rand_buffer_size()) < 12)
          rec_len= 12;
        if (rec.type != LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != rec_len + LSN_STORE_SIZE ||
            len != 12 || ref != lsn ||
            check_content(rec.header + LSN_STORE_SIZE, len - LSN_STORE_SIZE))
        {
          fprintf(stderr, "Incorrect LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE "
                  "data read(%d)"
                  "type %u (%d), strid %u (%d), len %lu, %lu + 7 (%d), "
                  "hdr len: %u (%d), "
                  "ref(%lu,0x%lx), lsn(%lu,0x%lx) (%d), content: %d\n",
                  i, (uint) rec.type,
                  rec.type != LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE,
                  (uint) rec.short_trid,
                  rec.short_trid != (i % 0xFFFF),
                  (ulong) rec.record_length, (ulong) rec_len,
                  rec.record_length != rec_len + LSN_STORE_SIZE,
                  (uint) len,
                  len != 12,
                  LSN_IN_PARTS(ref), LSN_IN_PARTS(rec.lsn),
                  (len != 12 || ref != lsn),
                  check_content(rec.header + LSN_STORE_SIZE,
                                len - LSN_STORE_SIZE));
          goto err;
        }
        if (read_and_check_content(&rec, long_buffer, LSN_STORE_SIZE))
        {
          fprintf(stderr,
                  "Incorrect LOGREC_VARIABLE_RECORD_1LSN_EXAMPLE "
                  "in whole rec read lsn(%lu,0x%lx)\n",
                  LSN_IN_PARTS(rec.lsn));
          goto err;
        }
      }
      else
      {
        LSN ref1, ref2;
        ref1= lsn_korr(rec.header);
        ref2= lsn_korr(rec.header + LSN_STORE_SIZE);
        if ((rec_len= rand_buffer_size()) < 19)
          rec_len= 19;
        if (rec.type != LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != rec_len + LSN_STORE_SIZE * 2 ||
            len != 19 ||
            ref1 != lsn ||
            ref2 != first_lsn ||
            check_content(rec.header + LSN_STORE_SIZE * 2,
                          len - LSN_STORE_SIZE * 2))
        {
          fprintf(stderr, "Incorrect LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE "
                  "data read(%d) "
                  "type %u, strid %u, len %lu != %lu + 14, hdr len: %d, "
                  "ref1(%lu,0x%lx), ref2(%lu,0x%lx), "
                  "lsn(%lu,0x%lx)\n",
                  i, (uint) rec.type, (uint) rec.short_trid,
                  (ulong) rec.record_length, (ulong) rec_len,
                  len, LSN_IN_PARTS(ref1), LSN_IN_PARTS(ref2),
                  LSN_IN_PARTS(rec.lsn));
          goto err;
        }
        if (read_and_check_content(&rec, long_buffer, LSN_STORE_SIZE * 2))
        {
          fprintf(stderr,
                  "Incorrect LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE "
                  "in whole rec read lsn(%lu,0x%lx)\n",
                  LSN_IN_PARTS(rec.lsn));
          goto err;
        }
      }
      read_ok(&rec);
      translog_free_record_header(&rec);

      len= translog_read_next_record_header(&scanner, &rec);
      if (len == RECHEADER_READ_ERROR)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header failed (%d)\n",
                i, errno);
        goto err;
      }
      if (len == RECHEADER_READ_EOF)
      {
        fprintf(stderr, "EOL met at the middle of iteration %u "
                "instead of beginning of %u\n", i, ITERATIONS);
        goto err;
      }
      if (rec.type != LOGREC_FIXED_RECORD_0LSN_EXAMPLE ||
          rec.short_trid != (i % 0xFFFF) ||
          rec.record_length != 6 || uint4korr(rec.header) != i ||
          ((uchar)rec.header[4]) != 0 || ((uchar)rec.header[5]) != 0xFF)
      {
        fprintf(stderr, "Incorrect LOGREC_FIXED_RECORD_0LSN_EXAMPLE "
                "data read(%d)\n"
                "type %u, strid %u, len %u, i: %u, 4: %u 5: %u "
                "lsn(%lu,0x%lx)\n",
                i, (uint) rec.type, (uint) rec.short_trid,
                (uint) rec.record_length,
                (uint) uint4korr(rec.header), (uint) rec.header[4],
                (uint) rec.header[5],
                LSN_IN_PARTS(rec.lsn));
        goto err;
      }
      lsn= rec.lsn;
      read_ok(&rec);
      translog_free_record_header(&rec);

      len= translog_read_next_record_header(&scanner, &rec);
      if ((rec_len= rand_buffer_size()) < 9)
        rec_len= 9;
      if (rec.type != LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE ||
          rec.short_trid != (i % 0xFFFF) ||
          rec.record_length != rec_len ||
          len != 9 || check_content(rec.header, (uint)len))
      {
        fprintf(stderr, "Incorrect LOGREC_VARIABLE_RECORD_0LSN_EXAMPLE "
                "data read(%d) "
                "type %u, strid %u, len %lu != %lu, hdr len: %d, "
                "lsn(%lu,0x%lx)\n",
                i, (uint) rec.type, (uint) rec.short_trid,
                (ulong) rec.record_length, (ulong) rec_len,
                len, LSN_IN_PARTS(rec.lsn));
        goto err;
      }
      if (read_and_check_content(&rec, long_buffer, 0))
      {
        fprintf(stderr,
                "Incorrect LOGREC_VARIABLE_RECORD_2LSN_EXAMPLE "
                "in whole rec read lsn(%lu,0x%lx)\n",
                LSN_IN_PARTS(rec.lsn));
        goto err;
      }
      read_ok(&rec);
      translog_free_record_header(&rec);
    }
  }

  rc= 0;
err:
  if (rc)
    ok(0, "read record");
  } /* SKIP_BIG_TESTS */
  translog_destroy();
  end_pagecache(&pagecache, 1);
  ma_control_file_end();

  if (maria_log_remove())
    exit(1);

  return(test(exit_status()));
}
