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

#define PCACHE_SIZE (1024*1024*10)
#define PCACHE_PAGE TRANSLOG_PAGE_SIZE
#define LOG_FILE_SIZE (1024L*1024L*1024L + 1024L*1024L*512)
#define LOG_FLAGS 0

static char *first_translog_file= (char*)"maria_log.00000001";
static char *file1_name= (char*)"page_cache_test_file_1";
static PAGECACHE_FILE file1;

int main(int argc __attribute__((unused)), char *argv[])
{
  uint pagen;
  uchar long_tr_id[6];
  PAGECACHE pagecache;
  LSN lsn;
  MY_STAT st, *stat;
  LEX_STRING parts[TRANSLOG_INTERNAL_PARTS + 1];

  MY_INIT(argv[0]);

  plan(1);

  bzero(&pagecache, sizeof(pagecache));
  maria_data_root= ".";
  if (maria_log_remove())
    exit(1);
  /* be sure that we have no logs in the directory*/
  if (my_stat(CONTROL_FILE_BASE_NAME, &st,  MYF(0)))
    my_delete(CONTROL_FILE_BASE_NAME, MYF(0));
  if (my_stat(first_translog_file, &st,  MYF(0)))
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

  if (ma_control_file_create_or_open(TRUE))
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
  if (translog_init(".", LOG_FILE_SIZE, 50112, 0, &pagecache, LOG_FLAGS))
  {
    fprintf(stderr, "Can't init loghandler (%d)\n", errno);
    translog_destroy();
    exit(1);
  }
  example_loghandler_init();
  /* Suppressing of automatic record writing */
  dummy_transaction_object.first_undo_lsn|= TRANSACTION_LOGGED_LONG_ID;

  if ((stat= my_stat(first_translog_file, &st,  MYF(0))) == 0)
  {
    fprintf(stderr, "There is no %s (%d)\n", first_translog_file, errno);
    exit(1);
  }
  if (st.st_size != TRANSLOG_PAGE_SIZE)
  {
    fprintf(stderr,
            "incorrect initial size of %s: %ld instead of %ld\n",
            first_translog_file, (long)st.st_size, (long)TRANSLOG_PAGE_SIZE);
    exit(1);
  }
  int4store(long_tr_id, 0);
  parts[TRANSLOG_INTERNAL_PARTS + 0].str= (char*)long_tr_id;
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
  if (chmod(file1_name, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
  {
    fprintf(stderr, "Got error during file1 chmod() (errno: %d)\n",
	    errno);
    exit(1);
  }

  {
    uchar page[PCACHE_PAGE];

    bzero(page, PCACHE_PAGE);
#define PAGE_LSN_OFFSET 0
    lsn_store(page + PAGE_LSN_OFFSET, lsn);
    pagecache_write(&pagecache, &file1, 0, 3, (char*)page,
                    PAGECACHE_LSN_PAGE,
                    PAGECACHE_LOCK_LEFT_UNLOCKED,
                    PAGECACHE_PIN_LEFT_UNPINNED,
                    PAGECACHE_WRITE_DELAY,
                    0, LSN_IMPOSSIBLE);
    flush_pagecache_blocks(&pagecache, &file1, FLUSH_FORCE_WRITE);
  }
  if ((stat= my_stat(first_translog_file, &st,  MYF(0))) == 0)
  {
    fprintf(stderr, "can't stat %s (%d)\n", first_translog_file, errno);
    exit(1);
  }
  if (st.st_size != TRANSLOG_PAGE_SIZE * 2)
  {
    fprintf(stderr,
            "incorrect initial size of %s: %ld instead of %ld\n",
            first_translog_file,
            (long)st.st_size, (long)(TRANSLOG_PAGE_SIZE * 2));
    ok(0, "log triggered");
    exit(1);
  }
  ok(1, "log triggered");

  translog_destroy();
  end_pagecache(&pagecache, 1);
  ma_control_file_end();
  my_delete(CONTROL_FILE_BASE_NAME, MYF(0));
  my_delete(first_translog_file, MYF(0));
  my_delete(file1_name, MYF(0));

  exit(0);
}
