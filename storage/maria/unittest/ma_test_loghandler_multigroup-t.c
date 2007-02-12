#include "../maria_def.h"
#include <stdio.h>
#include <errno.h>
#include <tap.h>

#ifndef DBUG_OFF
static const char *default_dbug_option;
#endif

#define PCACHE_SIZE (1024*1024*10)

#define LONG_BUFFER_SIZE ((1024L*1024L*1024L) + (1024L*1024L*512))

#define MIN_REC_LENGTH (1024L*1024L + 1024L*512L + 1)

#define SHOW_DIVIDER 2

#define LOG_FILE_SIZE (1024L*1024L*1024L + 1024L*1024L*512)
#define ITERATIONS 2
/*#define ITERATIONS 63 */

/*
#define LOG_FILE_SIZE 1024L*1024L*3L
#define ITERATIONS 1600
*/
/*
#define LOG_FILE_SIZE 1024L*1024L*100L
#define ITERATIONS 65000
*/


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
  uchar buff[4];
  DBUG_ENTER("check_content");
  for (i= 0; i < length; i++)
  {
    if (i % 4 == 0)
      int4store(buff, (i >> 2));
    if (ptr[i] != buff[i % 4])
    {
      fprintf(stderr, "Byte # %lu is %x instead of %x",
              i, (uint) ptr[i], (uint) buff[i % 4]);
      DBUG_DUMP("mem", ptr +(ulong) (i > 16 ? i - 16 : 0),
                (i > 16 ? 16 : i) + (i + 16 < length ? 16 : length - i));
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
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
  DBUG_ENTER("read_and_check_content");
  DBUG_ASSERT(rec->record_length < LONG_BUFFER_SIZE + 7 * 2 + 2);
  if ((len= translog_read_record(rec->lsn, 0, rec->record_length,
                                 buffer, NULL)) != rec->record_length)
  {
    fprintf(stderr, "Requested %lu byte, read %lu\n",
            (ulong) rec->record_length, (ulong) len);
    res= 1;
  }
  res|= check_content(buffer + skip, rec->record_length - skip);
  DBUG_RETURN(res);
}


static uint32 get_len()
{
  uint32 rec_len;
  do
  {
    rec_len= random() /
      (RAND_MAX / (LONG_BUFFER_SIZE - MIN_REC_LENGTH - 1)) + MIN_REC_LENGTH;
  } while (rec_len >= LONG_BUFFER_SIZE);
  return rec_len;
}

int main(int argc, char *argv[])
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
  uchar *long_buffer= malloc(LONG_BUFFER_SIZE + 7 * 2 + 2);
  PAGECACHE pagecache;
  LSN lsn, lsn_base, first_lsn, lsn_ptr;
  TRANSLOG_HEADER_BUFFER rec;
  struct st_translog_scanner_data scanner;
  int rc;

  MY_INIT(argv[0]);

  bzero(&pagecache, sizeof(pagecache));
  maria_data_root= ".";

  {
    uchar buff[4];
    for (i= 0; i < (LONG_BUFFER_SIZE + 7 * 2 + 2); i++)
    {
      if (i % 4 == 0)
        int4store(buff, (i >> 2));
      long_buffer[i]= buff[i % 4];
    }
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

  if (ma_control_file_create_or_open())
  {
    fprintf(stderr, "Can't init control file (%d)\n", errno);
    exit(1);
  }
  if ((pagen= init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                             TRANSLOG_PAGE_SIZE)) == 0)
  {
    fprintf(stderr, "Got error: init_pagecache() (errno: %d)\n", errno);
    exit(1);
  }
  if (translog_init(".", LOG_FILE_SIZE, 50112, 0, &pagecache, 0))
  {
    fprintf(stderr, "Can't init loghandler (%d)\n", errno);
    translog_destroy();
    exit(1);
  }

  srandom(122334817L);

  long_tr_id[5]= 0xff;

  int4store(long_tr_id, 0);
  if (translog_write_record(&lsn,
                            LOGREC_LONG_TRANSACTION_ID,
                            0, NULL, 6, long_tr_id, 0))
  {
    fprintf(stderr, "Can't write record #%lu\n", (ulong) 0);
    translog_destroy();
    exit(1);
  }
  lsn_base= first_lsn= lsn;

  for (i= 1; i < ITERATIONS; i++)
  {
    if (i % SHOW_DIVIDER == 0)
      printf("write %d\n", i);
    if (i % 2)
    {
      lsn7store(lsn_buff, lsn_base);
      if (translog_write_record(&lsn,
                                LOGREC_CLR_END,
                                (i % 0xFFFF), NULL, 7, lsn_buff, 0))
      {
        fprintf(stderr, "1 Can't write reference before record #%lu\n",
                (ulong) i);
        translog_destroy();
        exit(1);
      }
      lsn7store(lsn_buff, lsn_base);
      rec_len= get_len();
      if (translog_write_record(&lsn,
                                LOGREC_UNDO_KEY_INSERT,
                                (i % 0xFFFF),
                                NULL, 7, lsn_buff, rec_len, long_buffer, 0))
      {
        fprintf(stderr, "1 Can't write var reference before record #%lu\n",
                (ulong) i);
        translog_destroy();
        exit(1);
      }
    }
    else
    {
      lsn7store(lsn_buff, lsn_base);
      lsn7store(lsn_buff + 7, first_lsn);
      if (translog_write_record(&lsn,
                                LOGREC_UNDO_ROW_DELETE,
                                (i % 0xFFFF), NULL, 23, lsn_buff, 0))
      {
        fprintf(stderr, "0 Can't write reference before record #%lu\n",
                (ulong) i);
        translog_destroy();
        exit(1);
      }
      lsn7store(lsn_buff, lsn_base);
      lsn7store(lsn_buff + 7, first_lsn);
      rec_len= get_len();
      if (translog_write_record(&lsn,
                                LOGREC_UNDO_KEY_DELETE,
                                (i % 0xFFFF),
                                NULL, 14, lsn_buff, rec_len, long_buffer, 0))
      {
        fprintf(stderr, "0 Can't write var reference before record #%lu\n",
                (ulong) i);
        translog_destroy();
        exit(1);
      }
    }
    int4store(long_tr_id, i);
    if (translog_write_record(&lsn,
                              LOGREC_LONG_TRANSACTION_ID,
                              (i % 0xFFFF), NULL, 6, long_tr_id, 0))
    {
      fprintf(stderr, "Can't write record #%lu\n", (ulong) i);
      translog_destroy();
      exit(1);
    }

    lsn_base= lsn;

    rec_len= get_len();
    if (translog_write_record(&lsn,
                              LOGREC_REDO_INSERT_ROW_HEAD,
                              (i % 0xFFFF), NULL, rec_len, long_buffer, 0))
    {
      fprintf(stderr, "Can't write variable record #%lu\n", (ulong) i);
      translog_destroy();
      exit(1);
    }
  }

  translog_destroy();
  end_pagecache(&pagecache, 1);
  ma_control_file_end();

  if (ma_control_file_create_or_open())
  {
    fprintf(stderr, "pass2: Can't init control file (%d)\n", errno);
    exit(1);
  }
  if ((pagen= init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                             TRANSLOG_PAGE_SIZE)) == 0)
  {
    fprintf(stderr, "pass2: Got error: init_pagecache() (errno: %d)\n", errno);
    exit(1);
  }
  if (translog_init(".", LOG_FILE_SIZE, 50112, 0, &pagecache, 0))
  {
    fprintf(stderr, "pass2: Can't init loghandler (%d)\n", errno);
    translog_destroy();
    exit(1);
  }

  srandom(122334817L);

  rc= 1;

  {
    translog_size_t len= translog_read_record_header(first_lsn, &rec);
    if (len == 0)
    {
      fprintf(stderr, "translog_read_record_header failed (%d)\n", errno);
      translog_free_record_header(&rec);
      goto err;
    }
    if (rec.type !=LOGREC_LONG_TRANSACTION_ID || rec.short_trid != 0 ||
        rec.record_length != 6 || uint4korr(rec.header) != 0 ||
        (uint)rec.header[4] != 0 || rec.header[5] != 0xFF ||
        first_lsn != rec.lsn)
    {
      fprintf(stderr, "Incorrect LOGREC_LONG_TRANSACTION_ID data read(0)\n"
              "type %u, strid %u, len %u, i: %u, 4: %u 5: %u, "
              "lsn(0x%lu,0x%lx)\n",
              (uint) rec.type, (uint) rec.short_trid, (uint) rec.record_length,
              (uint)uint4korr(rec.header), (uint) rec.header[4],
              (uint) rec.header[5],
              (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
      translog_free_record_header(&rec);
      goto err;
    }
    translog_free_record_header(&rec);
    lsn= first_lsn;
    lsn_ptr= first_lsn;
    for (i= 1;; i++)
    {
      if (i % SHOW_DIVIDER == 0)
        printf("read %d\n", i);
      len= translog_read_next_record_header(lsn_ptr, &rec, 1, &scanner);
      if (len == 0)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header failed (%d)\n",
                i, errno);
        translog_free_record_header(&rec);
        goto err;
      }
      if (rec.lsn == CONTROL_FILE_IMPOSSIBLE_LSN)
      {
        if (i != ITERATIONS)
        {
          fprintf(stderr, "EOL met at iteration %u instead of %u\n",
                  i, ITERATIONS);
          translog_free_record_header(&rec);
          goto err;
        }
        break;
      }
      /* use scanner after its initialization */
      lsn_ptr= 0;

      if (i % 2)
      {
        LSN ref;
        ref= lsn7korr(rec.header);
        if (rec.type != LOGREC_CLR_END || rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != 7 || ref != lsn)
        {
          fprintf(stderr, "Incorrect LOGREC_CLR_END data read(%d)"
                  "type %u, strid %u, len %u, ref(%lu,0x%lx), lsn(%lu,0x%lx)\n",
                  i, (uint) rec.type, (uint) rec.short_trid,
                  (uint) rec.record_length,
                  (ulong) LSN_FILE_NO(ref), (ulong) LSN_OFFSET(ref),
                  (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
          translog_free_record_header(&rec);
          goto err;
        }
      }
      else
      {
        LSN ref1, ref2;
        ref1= lsn7korr(rec.header);
        ref2= lsn7korr(rec.header + 7);
        if (rec.type !=LOGREC_UNDO_ROW_DELETE ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != 23 ||
            ref1 != lsn ||
            ref2 != first_lsn ||
            rec.header[22] != 0x55 || rec.header[21] != 0xAA ||
            rec.header[20] != 0x55 || rec.header[19] != 0xAA ||
            rec.header[18] != 0x55 || rec.header[17] != 0xAA ||
            rec.header[16] != 0x55 || rec.header[15] != 0xAA ||
            rec.header[14] != 0x55)
        {
          fprintf(stderr, "Incorrect LOGREC_UNDO_ROW_DELETE data read(%d)"
                  "type %u, strid %u, len %u, ref1(%lu,0x%lx), "
                  "ref2(%lu,0x%lx) %x%x%x%x%x%x%x%x%x "
                  "lsn(%lu,0x%lx)\n",
                  i, (uint) rec.type, (uint) rec.short_trid,
                  (uint) rec.record_length,
                  (ulong) LSN_FILE_NO(ref1), (ulong) LSN_OFFSET(ref1),
                  (ulong) LSN_FILE_NO(ref2), (ulong) LSN_OFFSET(ref2),
                  (uint) rec.header[14], (uint) rec.header[15],
                  (uint) rec.header[16], (uint) rec.header[17],
                  (uint) rec.header[18], (uint) rec.header[19],
                  (uint) rec.header[20], (uint) rec.header[21],
                  (uint) rec.header[22],
                  (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
          translog_free_record_header(&rec);
          goto err;
        }
      }
      translog_free_record_header(&rec);

      len= translog_read_next_record_header(lsn_ptr, &rec, 1, &scanner);
      if (len == 0)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header (var) "
                "failed (%d)\n", i, errno);
        goto err;
      }
      if (rec.lsn == CONTROL_FILE_IMPOSSIBLE_LSN)
      {
        fprintf(stderr, "EOL met at the middle of iteration (first var) %u "
                "instead of beginning of %u\n", i, ITERATIONS);
        goto err;
      }
      if (i % 2)
      {
        LSN ref;
        ref= lsn7korr(rec.header);
        rec_len= get_len();
        if (rec.type !=LOGREC_UNDO_KEY_INSERT ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != rec_len + 7 ||
            len != 12 || ref != lsn ||
            check_content(rec.header + 7, len - 7))
        {
          fprintf(stderr, "Incorrect LOGREC_UNDO_KEY_INSERT data read(%d)"
                  "type %u (%d), strid %u (%d), len %lu, %lu + 7 (%d), "
                  "hdr len: %u (%d), "
                  "ref(%lu,0x%lx), lsn(%lu,0x%lx) (%d), content: %d\n",
                  i, (uint) rec.type,
                  rec.type !=LOGREC_UNDO_KEY_INSERT,
                  (uint) rec.short_trid,
                  rec.short_trid != (i % 0xFFFF),
                  (ulong) rec.record_length, (ulong) rec_len,
                  rec.record_length != rec_len + 7,
                  (uint) len,
                  len != 12,
                  (ulong) LSN_FILE_NO(ref), (ulong) LSN_OFFSET(ref),
                  (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn),
                  (ref != lsn),
                  check_content(rec.header + 7, len - 7));
          translog_free_record_header(&rec);
          goto err;
        }
        if (read_and_check_content(&rec, long_buffer, 7))
        {
          fprintf(stderr,
                  "Incorrect LOGREC_UNDO_KEY_INSERT in whole rec read "
                  "lsn(%lu,0x%lx)\n",
                  (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
          translog_free_record_header(&rec);
          goto err;
        }
      }
      else
      {
        LSN ref1, ref2;
        ref1= lsn7korr(rec.header);
        ref2= lsn7korr(rec.header + 7);
        rec_len= get_len();
        if (rec.type !=LOGREC_UNDO_KEY_DELETE ||
            rec.short_trid != (i % 0xFFFF) ||
            rec.record_length != rec_len + 14 ||
            len != 19 ||
            ref1 != lsn ||
            ref2 != first_lsn ||
            check_content(rec.header + 14, len - 14))
        {
          fprintf(stderr, "Incorrect LOGREC_UNDO_KEY_DELETE data read(%d)"
                  "type %u, strid %u, len %lu != %lu + 7, hdr len: %u, "
                  "ref1(%lu,0x%lx), ref2(%lu,0x%lx), "
                  "lsn(%lu,0x%lx)\n",
                  i, (uint) rec.type, (uint) rec.short_trid,
                  (ulong) rec.record_length, (ulong) rec_len,
                  (uint) len,
                  (ulong) LSN_FILE_NO(ref1), (ulong) LSN_OFFSET(ref1),
                  (ulong) LSN_FILE_NO(ref2), (ulong) LSN_OFFSET(ref2),
                  (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
          translog_free_record_header(&rec);
          goto err;
        }
        if (read_and_check_content(&rec, long_buffer, 14))
        {
          fprintf(stderr,
                  "Incorrect LOGREC_UNDO_KEY_DELETE in whole rec read "
                  "lsn(%lu,0x%lx)\n",
                  (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
          translog_free_record_header(&rec);
          goto err;
        }
      }
      translog_free_record_header(&rec);

      len= translog_read_next_record_header(lsn_ptr, &rec, 1, &scanner);
      if (len == 0)
      {
        fprintf(stderr, "1-%d translog_read_next_record_header failed (%d)\n",
                i, errno);
        translog_free_record_header(&rec);
        goto err;
      }
      if (rec.lsn == CONTROL_FILE_IMPOSSIBLE_LSN)
      {
        fprintf(stderr, "EOL met at the middle of iteration %u "
                "instead of beginning of %u\n", i, ITERATIONS);
        translog_free_record_header(&rec);
        goto err;
      }
      if (rec.type !=LOGREC_LONG_TRANSACTION_ID ||
          rec.short_trid != (i % 0xFFFF) ||
          rec.record_length != 6 || uint4korr(rec.header) != i ||
          rec.header[4] != 0 || rec.header[5] != 0xFF)
      {
        fprintf(stderr, "Incorrect LOGREC_LONG_TRANSACTION_ID data read(%d)\n"
                "type %u, strid %u, len %u, i: %u, 4: %u 5: %u "
                "lsn(%lu,0x%lx)\n",
                i, (uint) rec.type, (uint) rec.short_trid,
                (uint) rec.record_length,
                (uint)uint4korr(rec.header), (uint) rec.header[4],
                (uint) rec.header[5],
                (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
        translog_free_record_header(&rec);
        goto err;
      }
      translog_free_record_header(&rec);

      lsn= rec.lsn;

      len= translog_read_next_record_header(lsn_ptr, &rec, 1, &scanner);
      rec_len= get_len();
      if (rec.type !=LOGREC_REDO_INSERT_ROW_HEAD ||
          rec.short_trid != (i % 0xFFFF) ||
          rec.record_length != rec_len ||
          len != 9 || check_content(rec.header, len))
      {
        fprintf(stderr, "Incorrect LOGREC_REDO_INSERT_ROW_HEAD data read(%d)"
                "type %u, strid %u, len %lu != %lu, hdr len: %u, "
                "lsn(%lu,0x%lx)\n",
                i, (uint) rec.type, (uint) rec.short_trid,
                (ulong) rec.record_length, (ulong) rec_len,
                (uint) len,
                (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
        translog_free_record_header(&rec);
        goto err;
      }
      if (read_and_check_content(&rec, long_buffer, 0))
      {
        fprintf(stderr,
                "Incorrect LOGREC_UNDO_KEY_DELETE in whole rec read "
                "lsn(%lu,0x%lx)\n",
                (ulong) LSN_FILE_NO(rec.lsn), (ulong) LSN_OFFSET(rec.lsn));
        translog_free_record_header(&rec);
        goto err;
      }
    }
  }

  rc= 0;
err:
  translog_destroy();
  end_pagecache(&pagecache, 1);
  ma_control_file_end();

  return (test(exit_status() || rc));
}
