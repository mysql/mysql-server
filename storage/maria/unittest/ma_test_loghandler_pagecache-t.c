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
#define PCACHE_PAGE TRANSLOG_PAGE_SIZE
#define LOG_FILE_SIZE (1024L*1024L*1024L + 1024L*1024L*512)
#define LOG_FLAGS 0

static char *first_translog_file= (char*)"maria_log.00000001";
static char *file1_name= (char*)"page_cache_test_file_1";
static PAGECACHE_FILE file1;


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


int main(int argc __attribute__((unused)), char *argv[])
{
  uint pagen;
  uchar long_tr_id[6];
  PAGECACHE pagecache;
  LSN lsn;
  my_off_t file_size;
  LEX_CUSTRING parts[TRANSLOG_INTERNAL_PARTS + 1];

  MY_INIT(argv[0]);

  plan(1);

  bzero(&pagecache, sizeof(pagecache));
  maria_data_root= (char *)".";
  if (maria_log_remove())
    exit(1);
  /* be sure that we have no logs in the directory*/
  my_delete(CONTROL_FILE_BASE_NAME, MYF(0));
  my_delete(first_translog_file, MYF(0));

  bzero(long_tr_id, 6);
#ifndef DBUG_OFF
#if defined(__WIN__)
  default_dbug_option= "d:t:i:O,\\ma_test_loghandler_pagecache.trace";
#else
  default_dbug_option= "d:t:i:o,/tmp/ma_test_loghandler_pagecache.trace";
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
                             PCACHE_PAGE, 0)) == 0)
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

  if ((file1.file= my_open(first_translog_file, O_RDONLY,  MYF(MY_WME))) < 0)
  {
    fprintf(stderr, "There is no %s (%d)\n", first_translog_file, errno);
    exit(1);
  }
  file_size= my_seek(file1.file, 0, SEEK_END, MYF(MY_WME));
  if (file_size != TRANSLOG_PAGE_SIZE)
  {
    fprintf(stderr,
            "incorrect initial size of %s: %ld instead of %ld\n",
            first_translog_file, (long)file_size, (long)TRANSLOG_PAGE_SIZE);
    exit(1);
  }
  my_close(file1.file, MYF(MY_WME));
  int4store(long_tr_id, 0);
  parts[TRANSLOG_INTERNAL_PARTS + 0].str= long_tr_id;
  parts[TRANSLOG_INTERNAL_PARTS + 0].length= 6;
  dummy_transaction_object.first_undo_lsn= TRANSACTION_LOGGED_LONG_ID;
  if (translog_write_record(&lsn,
                            LOGREC_FIXED_RECORD_0LSN_EXAMPLE,
                            &dummy_transaction_object, NULL, 6,
                            TRANSLOG_INTERNAL_PARTS + 1,
                            parts, NULL, NULL))
  {
    fprintf(stderr, "Can't write record #%lu\n", (ulong) 0);
    translog_destroy();
    exit(1);
  }

  if ((file1.file= my_open(file1_name,
                           O_CREAT | O_TRUNC | O_RDWR, MYF(0))) == -1)
  {
    fprintf(stderr, "Got error during file1 creation from open() (errno: %d)\n",
	    errno);
    exit(1);
  }
  pagecache_file_init(file1, &dummy_callback, &dummy_callback,
                      &dummy_fail_callback, maria_flush_log_for_page, NULL);
  if (my_chmod(file1_name, S_IRWXU | S_IRWXG | S_IRWXO, MYF(MY_WME)))
    exit(1);

  {
    uchar page[PCACHE_PAGE];

    bzero(page, PCACHE_PAGE);
    lsn_store(page, lsn);
    pagecache_write(&pagecache, &file1, 0, 3, page,
                    PAGECACHE_LSN_PAGE,
                    PAGECACHE_LOCK_LEFT_UNLOCKED,
                    PAGECACHE_PIN_LEFT_UNPINNED,
                    PAGECACHE_WRITE_DELAY,
                    0, LSN_IMPOSSIBLE);
    flush_pagecache_blocks(&pagecache, &file1, FLUSH_RELEASE);
  }
  my_close(file1.file, MYF(MY_WME));
  if ((file1.file= my_open(first_translog_file, O_RDONLY, MYF(MY_WME))) < 0)
  {
    fprintf(stderr, "can't open %s (%d)\n", first_translog_file, errno);
    exit(1);
  }
  file_size= my_seek(file1.file, 0, SEEK_END, MYF(MY_WME));
  if (file_size != TRANSLOG_PAGE_SIZE * 2)
  {
    fprintf(stderr,
            "incorrect initial size of %s: %ld instead of %ld\n",
            first_translog_file,
            (long)file_size, (long)(TRANSLOG_PAGE_SIZE * 2));
    ok(0, "log triggered");
    exit(1);
  }
  my_close(file1.file, MYF(MY_WME));
  ok(1, "log triggered");

  translog_destroy();
  end_pagecache(&pagecache, 1);
  ma_control_file_end();
  my_delete(CONTROL_FILE_BASE_NAME, MYF(0));
  my_delete(first_translog_file, MYF(0));
  my_delete(file1_name, MYF(0));

  exit(0);
}
