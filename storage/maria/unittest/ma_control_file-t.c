/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Unit test of the control file module of the Aria engine WL#3234 */

/*
  Note that it is not possible to test the durability of the write (can't
  pull the plug programmatically :)
*/

#include <my_global.h>
#include <my_sys.h>
#include <tap.h>

#ifndef WITH_ARIA_STORAGE_ENGINE
/*
  If Aria is not compiled in, normally we don't come to building this test.
*/
#error "Aria engine is not compiled in, test cannot be built"
#endif

#include "maria.h"
#include "../../../storage/maria/maria_def.h"
#include <my_getopt.h>

#define EXTRACT_DEFINITIONS
#include "../ma_control_file.c"
#undef EXTRACT_DEFINITIONS

char file_name[FN_REFLEN];

/* The values we'll set and expect the control file module to return */
LSN    expect_checkpoint_lsn;
uint32 expect_logno;
TrID   expect_max_trid;
uint8  expect_recovery_failures;

static int delete_file(myf my_flags);
/*
  Those are test-specific wrappers around the module's API functions: after
  calling the module's API functions they perform checks on the result.
*/
static int close_file(void); /* wraps ma_control_file_end */
/* wraps ma_control_file_open_or_create */
static int open_file(void);
/* wraps ma_control_file_write_and_force */
static int write_file(LSN checkpoint_lsn, uint32 logno, TrID trid,
                      uint8 rec_failures);

/* Tests */
static int test_one_log_and_recovery_failures(void);
static int test_five_logs_and_max_trid(void);
static int test_3_checkpoints_and_2_logs(void);
static int test_binary_content(void);
static int test_start_stop(void);
static int test_2_open_and_2_close(void);
static int test_bad_magic_string(void);
static int test_bad_checksum(void);
static int test_bad_hchecksum(void);
static int test_future_size(void);
static int test_bad_blocksize(void);
static int test_bad_size(void);

/* Utility */
static int verify_module_values_match_expected(void);
static int verify_module_values_are_impossible(void);
static void usage(void);
static void get_options(int argc, char *argv[]);

/*
  If "expr" is FALSE, this macro will make the function print a diagnostic
  message and immediately return 1.
  This is inspired from assert() but does not crash the binary (sometimes we
  may want to see how other tests go even if one fails).
  RET_ERR means "return error".
*/

#define RET_ERR_UNLESS(expr) \
  {if (!(expr)) {diag("line %d: failure: '%s'", __LINE__, #expr); assert(0);return 1;}}


/* Used to ignore error messages from ma_control_file_open() */

static int my_ignore_message(uint error __attribute__((unused)),
                             const char *str __attribute__((unused)),
                             myf MyFlags __attribute__((unused)))
{
  DBUG_ENTER("my_message_no_curses");
  DBUG_PRINT("enter",("message: %s",str));
  DBUG_RETURN(0);
}

int (*default_error_handler_hook)(uint my_err, const char *str,
                                  myf MyFlags) = 0;


/* like ma_control_file_open(), but without error messages */

static CONTROL_FILE_ERROR local_ma_control_file_open(void)
{
  CONTROL_FILE_ERROR error;
  error_handler_hook= my_ignore_message;
  error= ma_control_file_open(TRUE, TRUE);
  error_handler_hook= default_error_handler_hook;
  return error;
}



int main(int argc,char *argv[])
{
  MY_INIT(argv[0]);
  my_init();

  maria_data_root= (char *)".";
  default_error_handler_hook= error_handler_hook;

  plan(12);

  diag("Unit tests for control file");

  get_options(argc,argv);

  diag("Deleting control file at startup, if there is an old one");
  RET_ERR_UNLESS(0 == delete_file(0)); /* if fails, can't continue */

  diag("Tests of normal conditions");
  ok(0 == test_one_log_and_recovery_failures(),
     "test of creating one log and recording recovery failures");
  ok(0 == test_five_logs_and_max_trid(),
     "test of creating five logs and many transactions");
  ok(0 == test_3_checkpoints_and_2_logs(),
     "test of creating three checkpoints and two logs");
  ok(0 == test_binary_content(), "test of the binary content of the file");
  ok(0 == test_start_stop(), "test of multiple starts and stops");
  diag("Tests of abnormal conditions");
  ok(0 == test_2_open_and_2_close(),
     "test of two open and two close (strange call sequence)");
  ok(0 == test_bad_magic_string(), "test of bad magic string");
  ok(0 == test_bad_checksum(), "test of bad checksum");
  ok(0 == test_bad_hchecksum(), "test of bad hchecksum");
  ok(0 == test_future_size(), "test of ability to handlr future versions");
  ok(0 == test_bad_blocksize(), "test of bad blocksize");
  ok(0 == test_bad_size(), "test of too small/big file");

  return exit_status();
}


static int delete_file(myf my_flags)
{
  RET_ERR_UNLESS(fn_format(file_name, CONTROL_FILE_BASE_NAME,
                           maria_data_root, "", MYF(MY_WME)) != NullS);
  /*
    Maybe file does not exist, ignore error.
    The error will however be printed on stderr.
  */
  my_delete(file_name, my_flags);
  expect_checkpoint_lsn= LSN_IMPOSSIBLE;
  expect_logno= FILENO_IMPOSSIBLE;
  expect_max_trid= expect_recovery_failures= 0;

  return 0;
}

/*
  Verifies that global values last_checkpoint_lsn, last_logno,
  max_trid_in_control_file (belonging to the module) match what we expect.
*/
static int verify_module_values_match_expected(void)
{
  RET_ERR_UNLESS(last_logno == expect_logno);
  RET_ERR_UNLESS(last_checkpoint_lsn == expect_checkpoint_lsn);
  RET_ERR_UNLESS(max_trid_in_control_file == expect_max_trid);
  RET_ERR_UNLESS(recovery_failures == expect_recovery_failures);
  return 0;
}


/*
  Verifies that global values last_checkpoint_lsn and last_logno (belonging
  to the module) are impossible (this is used when the file has been closed).
*/
static int verify_module_values_are_impossible(void)
{
  RET_ERR_UNLESS(last_logno == FILENO_IMPOSSIBLE);
  RET_ERR_UNLESS(last_checkpoint_lsn == LSN_IMPOSSIBLE);
  RET_ERR_UNLESS(max_trid_in_control_file == 0);
  return 0;
}


static int close_file(void)
{
  /* Simulate shutdown */
  ma_control_file_end();
  /* Verify amnesia */
  RET_ERR_UNLESS(verify_module_values_are_impossible() == 0);
  return 0;
}

static int open_file(void)
{
  RET_ERR_UNLESS(local_ma_control_file_open() == CONTROL_FILE_OK);
  /* Check that the module reports expected information */
  RET_ERR_UNLESS(verify_module_values_match_expected() == 0);
  return 0;
}

static int write_file(LSN checkpoint_lsn, uint32 logno, TrID trid,
                      uint8 rec_failures)
{
  RET_ERR_UNLESS(ma_control_file_write_and_force(checkpoint_lsn, logno, trid,
                                                 rec_failures)
                 == 0);
  /* Check that the module reports expected information */
  RET_ERR_UNLESS(verify_module_values_match_expected() == 0);
  return 0;
}

static int test_one_log_and_recovery_failures(void)
{
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  expect_logno= 123;
  RET_ERR_UNLESS(write_file(last_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            recovery_failures) == 0);
  expect_recovery_failures= 158;
  RET_ERR_UNLESS(write_file(last_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            expect_recovery_failures) == 0);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_five_logs_and_max_trid(void)
{
  uint i;

  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  expect_logno= 100;
  expect_max_trid= ULL(14111978111);
  for (i= 0; i<5; i++)
  {
    expect_logno*= 3;
    RET_ERR_UNLESS(write_file(last_checkpoint_lsn, expect_logno,
                              expect_max_trid,
                              recovery_failures) == 0);
  }
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_3_checkpoints_and_2_logs(void)
{
  /*
    Simulate one checkpoint, one log creation, two checkpoints, one
    log creation.
  */
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  expect_checkpoint_lsn= MAKE_LSN(5, 10000);
  RET_ERR_UNLESS(write_file(expect_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            recovery_failures) == 0);

  expect_logno= 17;
  RET_ERR_UNLESS(write_file(expect_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            recovery_failures) == 0);

  expect_checkpoint_lsn= MAKE_LSN(17, 20000);
  RET_ERR_UNLESS(write_file(expect_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            recovery_failures) == 0);

  expect_checkpoint_lsn= MAKE_LSN(17, 45000);
  RET_ERR_UNLESS(write_file(expect_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            recovery_failures) == 0);

  expect_logno= 19;
  RET_ERR_UNLESS(write_file(expect_checkpoint_lsn, expect_logno,
                            max_trid_in_control_file,
                            recovery_failures) == 0);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_binary_content(void)
{
  uint i;
  int fd;

  /*
    TEST4: actually check by ourselves the content of the file.
    Note that constants (offsets) are hard-coded here, precisely to prevent
    someone from changing them in the control file module and breaking
    backward-compatibility.
    TODO: when we reach the format-freeze state, we may even just do a
    comparison with a raw binary string, to not depend on any uint4korr
    future change/breakage.
  */

  uchar buffer[45];
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_read(fd, buffer, 45, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  i= uint3korr(buffer + 34 );
  RET_ERR_UNLESS(i == LSN_FILE_NO(last_checkpoint_lsn));
  i= uint4korr(buffer + 37);
  RET_ERR_UNLESS(i == LSN_OFFSET(last_checkpoint_lsn));
  i= uint4korr(buffer + 41);
  RET_ERR_UNLESS(i == last_logno);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_start_stop(void)
{
  /* TEST5: Simulate start/nothing/stop/start/nothing/stop/start */

  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_2_open_and_2_close(void)
{
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}


static int test_bad_magic_string(void)
{
  uchar buffer[4];
  int fd;

  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  /* Corrupt magic string */
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_pread(fd, buffer, 4, 0, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_pwrite(fd, (const uchar *)"papa", 4, 0,
                           MYF(MY_FNABP |  MY_WME)) == 0);

  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(local_ma_control_file_open() ==
             CONTROL_FILE_BAD_MAGIC_STRING);
  /* Restore magic string */
  RET_ERR_UNLESS(my_pwrite(fd, buffer, 4, 0, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_bad_checksum(void)
{
  uchar buffer[4];
  int fd;

  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  /* Corrupt checksum */
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_pread(fd, buffer, 1, 30, MYF(MY_FNABP |  MY_WME)) == 0);
  buffer[0]+= 3; /* mangle checksum */
  RET_ERR_UNLESS(my_pwrite(fd, buffer, 1, 30, MYF(MY_FNABP |  MY_WME)) == 0);
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(local_ma_control_file_open() ==
                 CONTROL_FILE_BAD_CHECKSUM);
  /* Restore checksum */
  buffer[0]-= 3;
  RET_ERR_UNLESS(my_pwrite(fd, buffer, 1, 30, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);

  return 0;
}


static int test_bad_blocksize(void)
{
  maria_block_size<<= 1;
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(local_ma_control_file_open() ==
                 CONTROL_FILE_WRONG_BLOCKSIZE);
  /* Restore blocksize */
  maria_block_size>>= 1;

  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}


static int test_future_size(void)
{
  /*
    Here we check ability to add fields only so we can use
    defined constants
  */
  uint32 sum;
  int fd;
  uchar buffer[CF_CREATE_TIME_TOTAL_SIZE + CF_CHANGEABLE_TOTAL_SIZE + 2];
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_read(fd, buffer,
                         CF_CREATE_TIME_TOTAL_SIZE + CF_CHANGEABLE_TOTAL_SIZE,
                         MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);
  /* "add" new field of 1 byte (value 1) to header and variable part */
  memmove(buffer + CF_CREATE_TIME_TOTAL_SIZE + 1,
          buffer + CF_CREATE_TIME_TOTAL_SIZE,
          CF_CHANGEABLE_TOTAL_SIZE);
  buffer[CF_CREATE_TIME_TOTAL_SIZE - CF_CHECKSUM_SIZE]= '\1';
  buffer[CF_CREATE_TIME_TOTAL_SIZE + CF_CHANGEABLE_TOTAL_SIZE + 1]= '\1';
  /* fix lengths */
  int2store(buffer + CF_CREATE_TIME_SIZE_OFFSET, CF_CREATE_TIME_TOTAL_SIZE + 1);
  int2store(buffer + CF_CHANGEABLE_SIZE_OFFSET, CF_CHANGEABLE_TOTAL_SIZE + 1);
  /* recalculete checksums */
  sum= (uint32) my_checksum(0, buffer, CF_CREATE_TIME_TOTAL_SIZE -
                            CF_CHECKSUM_SIZE + 1);
  int4store(buffer + CF_CREATE_TIME_TOTAL_SIZE - CF_CHECKSUM_SIZE + 1, sum);
  sum= (uint32) my_checksum(0, buffer +  CF_CREATE_TIME_TOTAL_SIZE + 1 +
                            CF_CHECKSUM_SIZE,
                            CF_CHANGEABLE_TOTAL_SIZE - CF_CHECKSUM_SIZE + 1);
  int4store(buffer + CF_CREATE_TIME_TOTAL_SIZE + 1, sum);
  /* write new file and check it */
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_pwrite(fd, buffer,
                           CF_CREATE_TIME_TOTAL_SIZE +
                           CF_CHANGEABLE_TOTAL_SIZE + 2,
                           0, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  return(0);
}

static int test_bad_hchecksum(void)
{
  uchar buffer[4];
  int fd;

  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  /* Corrupt checksum */
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_pread(fd, buffer, 1, 26, MYF(MY_FNABP |  MY_WME)) == 0);
  buffer[0]+= 3; /* mangle checksum */
  RET_ERR_UNLESS(my_pwrite(fd, buffer, 1, 26, MYF(MY_FNABP |  MY_WME)) == 0);
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(local_ma_control_file_open() ==
                 CONTROL_FILE_BAD_HEAD_CHECKSUM);
  /* Restore checksum */
  buffer[0]-= 3;
  RET_ERR_UNLESS(my_pwrite(fd, buffer, 1, 26, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);

  return 0;
}


static int test_bad_size(void)
{
  uchar buffer[]=
    "123456789012345678901234567890123456789012345678901234567890123456";
  int fd, i;

  /* A too short file */
  RET_ERR_UNLESS(delete_file(MYF(MY_WME)) == 0);
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR | O_CREAT,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_write(fd, buffer, 10, MYF(MY_FNABP |  MY_WME)) == 0);
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(local_ma_control_file_open() ==
                 CONTROL_FILE_TOO_SMALL);
  for (i= 0; i < 8; i++)
  {
    RET_ERR_UNLESS(my_write(fd, buffer, 66, MYF(MY_FNABP |  MY_WME)) == 0);
  }
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(local_ma_control_file_open() ==
                 CONTROL_FILE_TOO_BIG);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);

  /* Leave a correct control file */
  RET_ERR_UNLESS(delete_file(MYF(MY_WME)) == 0);
  RET_ERR_UNLESS(open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  return 0;
}


static struct my_option my_long_options[] =
{
#ifndef DBUG_OFF
  {"debug", '#', "Debug log.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"help", '?', "Display help and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Print version number and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void version(void)
{
  printf("ma_control_file_test: unit test for the control file "
         "module of the Aria storage engine. Ver 1.0 \n");
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'V':
    version();
    exit(0);
  case '#':
    DBUG_PUSH (argument);
    break;
  case '?':
    version();
    usage();
    exit(0);
  }
  return 0;
}


/* Read options */

static void get_options(int argc, char *argv[])
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options,
                               get_one_option)))
    exit(ho_error);

  return;
} /* get options */


static void usage(void)
{
  printf("Usage: %s [options]\n\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
