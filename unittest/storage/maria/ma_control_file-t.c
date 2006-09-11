/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Unit test of the control file module of the Maria engine WL#3234 */

/*
  Note that it is not possible to test the durability of the write (can't
  pull the plug programmatically :)
*/

#include <my_global.h>
#include <my_sys.h>
#include <tap.h>

#ifndef WITH_MARIA_STORAGE_ENGINE
/*
  If Maria is not compiled in, normally we don't come to building this test.
*/
#error "Maria engine is not compiled in, test cannot be built"
#endif

#include "maria.h"
#include "../../../storage/maria/ma_control_file.h"
#include <my_getopt.h>

char file_name[FN_REFLEN];

/* The values we'll set and expect the control file module to return */
LSN expect_checkpoint_lsn;
uint32 expect_logno;

static int delete_file();
/*
  Those are test-specific wrappers around the module's API functions: after
  calling the module's API functions they perform checks on the result.
*/
static int close_file(); /* wraps ma_control_file_end */
static int create_or_open_file(); /* wraps ma_control_file_open_or_create */
static int write_file(); /* wraps ma_control_file_write_and_force */

/* Tests */
static int test_one_log();
static int test_five_logs();
static int test_3_checkpoints_and_2_logs();
static int test_binary_content();
static int test_start_stop();
static int test_2_open_and_2_close();
static int test_bad_magic_string();
static int test_bad_checksum();
static int test_bad_size();

/* Utility */
static int verify_module_values_match_expected();
static int verify_module_values_are_impossible();
static void usage();
static void get_options(int argc, char *argv[]);

/*
  If "expr" is FALSE, this macro will make the function print a diagnostic
  message and immediately return 1.
  This is inspired from assert() but does not crash the binary (sometimes we
  may want to see how other tests go even if one fails).
  RET_ERR means "return error".
*/

#define RET_ERR_UNLESS(expr) \
  {if (!(expr)) {diag("line %d: failure: '%s'", __LINE__, #expr); return 1;}}


int main(int argc,char *argv[])
{
  MY_INIT(argv[0]);

  plan(9);

  diag("Unit tests for control file");

  get_options(argc,argv);

  diag("Deleting control file at startup, if there is an old one");
  RET_ERR_UNLESS(0 == delete_file()); /* if fails, can't continue */

  diag("Tests of normal conditions");
  ok(0 == test_one_log(), "test of creating one log");
  ok(0 == test_five_logs(), "test of creating five logs");
  ok(0 == test_3_checkpoints_and_2_logs(),
     "test of creating three checkpoints and two logs");
  ok(0 == test_binary_content(), "test of the binary content of the file");
  ok(0 == test_start_stop(), "test of multiple starts and stops");
  diag("Tests of abnormal conditions");
  ok(0 == test_2_open_and_2_close(),
     "test of two open and two close (strange call sequence)");
  ok(0 == test_bad_magic_string(), "test of bad magic string");
  ok(0 == test_bad_checksum(), "test of bad checksum");
  ok(0 == test_bad_size(), "test of too small/big file");

  return exit_status();
}


static int delete_file()
{
  RET_ERR_UNLESS(fn_format(file_name, CONTROL_FILE_BASE_NAME,
                           maria_data_root, "", MYF(MY_WME)) != NullS);
  /*
    Maybe file does not exist, ignore error.
    The error will however be printed on stderr.
  */
  my_delete(file_name, MYF(MY_WME));
  expect_checkpoint_lsn= CONTROL_FILE_IMPOSSIBLE_LSN;
  expect_logno= CONTROL_FILE_IMPOSSIBLE_FILENO;

  return 0;
}

/*
  Verifies that global values last_checkpoint_lsn and last_logno (belonging
  to the module) match what we expect.
*/
static int verify_module_values_match_expected()
{
  RET_ERR_UNLESS(last_logno == expect_logno);
  RET_ERR_UNLESS(last_checkpoint_lsn.file_no ==
                 expect_checkpoint_lsn.file_no);
  RET_ERR_UNLESS(last_checkpoint_lsn.rec_offset ==
               expect_checkpoint_lsn.rec_offset);
  return 0;
}


/*
  Verifies that global values last_checkpoint_lsn and last_logno (belonging
  to the module) are impossible (this is used when the file has been closed).
*/
static int verify_module_values_are_impossible()
{
  RET_ERR_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  RET_ERR_UNLESS(last_checkpoint_lsn.file_no ==
                 CONTROL_FILE_IMPOSSIBLE_FILENO);
  RET_ERR_UNLESS(last_checkpoint_lsn.rec_offset ==
                 CONTROL_FILE_IMPOSSIBLE_LOG_OFFSET);
  return 0;
}


static int close_file()
{
  /* Simulate shutdown */
  ma_control_file_end();
  /* Verify amnesia */
  RET_ERR_UNLESS(verify_module_values_are_impossible() == 0);
  return 0;
}

static int create_or_open_file()
{
  RET_ERR_UNLESS(ma_control_file_create_or_open() == CONTROL_FILE_OK);
  /* Check that the module reports expected information */
  RET_ERR_UNLESS(verify_module_values_match_expected() == 0);
  return 0;
}

static int write_file(const LSN *checkpoint_lsn,
                                        uint32 logno,
                                        uint objs_to_write)
{
  RET_ERR_UNLESS(ma_control_file_write_and_force(checkpoint_lsn, logno,
                                             objs_to_write) == 0);
  /* Check that the module reports expected information */
  RET_ERR_UNLESS(verify_module_values_match_expected() == 0);
  return 0;
}

static int test_one_log()
{
  uint objs_to_write;

  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LOGNO;
  expect_logno= 123;
  RET_ERR_UNLESS(write_file(NULL, expect_logno,
                                          objs_to_write) == 0);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_five_logs()
{
  uint objs_to_write;
  uint i;

  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LOGNO;
  expect_logno= 100;
  for (i= 0; i<5; i++)
  {
    expect_logno*= 3;
    RET_ERR_UNLESS(write_file(NULL, expect_logno,
                                            objs_to_write) == 0);
  }
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_3_checkpoints_and_2_logs()
{
  uint objs_to_write;
  /*
    Simulate one checkpoint, one log creation, two checkpoints, one
    log creation.
  */
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LSN;
  expect_checkpoint_lsn= (LSN){5, 10000};
  RET_ERR_UNLESS(write_file(&expect_checkpoint_lsn,
                                          expect_logno, objs_to_write) == 0);

  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LOGNO;
  expect_logno= 17;
  RET_ERR_UNLESS(write_file(&expect_checkpoint_lsn,
                                          expect_logno, objs_to_write) == 0);

  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LSN;
  expect_checkpoint_lsn= (LSN){17, 20000};
  RET_ERR_UNLESS(write_file(&expect_checkpoint_lsn,
                                          expect_logno, objs_to_write) == 0);

  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LSN;
  expect_checkpoint_lsn= (LSN){17, 45000};
  RET_ERR_UNLESS(write_file(&expect_checkpoint_lsn,
                                          expect_logno, objs_to_write) == 0);

  objs_to_write= CONTROL_FILE_UPDATE_ONLY_LOGNO;
  expect_logno= 19;
  RET_ERR_UNLESS(write_file(&expect_checkpoint_lsn,
                                          expect_logno, objs_to_write) == 0);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_binary_content()
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

  char buffer[17];
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_read(fd, buffer, 17, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  i= uint4korr(buffer+5);
  RET_ERR_UNLESS(i == last_checkpoint_lsn.file_no);
  i= uint4korr(buffer+9);
  RET_ERR_UNLESS(i == last_checkpoint_lsn.rec_offset);
  i= uint4korr(buffer+13);
  RET_ERR_UNLESS(i == last_logno);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_start_stop()
{
  /* TEST5: Simulate start/nothing/stop/start/nothing/stop/start */

  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_2_open_and_2_close()
{
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}


static int test_bad_magic_string()
{
  char buffer[4];
  int fd;

  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  /* Corrupt magic string */
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_pread(fd, buffer, 4, 0, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_pwrite(fd, "papa", 4, 0, MYF(MY_FNABP |  MY_WME)) == 0);

  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(ma_control_file_create_or_open() ==
             CONTROL_FILE_BAD_MAGIC_STRING);
  /* Restore magic string */
  RET_ERR_UNLESS(my_pwrite(fd, buffer, 4, 0, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);
  return 0;
}

static int test_bad_checksum()
{
  char buffer[4];
  int fd;

  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
  RET_ERR_UNLESS(close_file() == 0);

  /* Corrupt checksum */
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_pread(fd, buffer, 1, 4, MYF(MY_FNABP |  MY_WME)) == 0);
  buffer[0]+= 3; /* mangle checksum */
  RET_ERR_UNLESS(my_pwrite(fd, buffer+1, 1, 4, MYF(MY_FNABP |  MY_WME)) == 0);
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(ma_control_file_create_or_open() ==
                 CONTROL_FILE_BAD_CHECKSUM);
  /* Restore checksum */
  buffer[0]-= 3;
  RET_ERR_UNLESS(my_pwrite(fd, buffer+1, 1, 4, MYF(MY_FNABP |  MY_WME)) == 0);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);

  return 0;
}


static int test_bad_size()
{
  char buffer[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  int fd;

  /* A too short file */
  RET_ERR_UNLESS(delete_file() == 0);
  RET_ERR_UNLESS((fd= my_open(file_name,
                          O_BINARY | O_RDWR | O_CREAT,
                          MYF(MY_WME))) >= 0);
  RET_ERR_UNLESS(my_write(fd, buffer, 10, MYF(MY_FNABP |  MY_WME)) == 0);
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(ma_control_file_create_or_open() == CONTROL_FILE_TOO_SMALL);
  RET_ERR_UNLESS(my_write(fd, buffer, 30, MYF(MY_FNABP |  MY_WME)) == 0);
  /* Check that control file module sees the problem */
  RET_ERR_UNLESS(ma_control_file_create_or_open() == CONTROL_FILE_TOO_BIG);
  RET_ERR_UNLESS(my_close(fd, MYF(MY_WME)) == 0);

  /* Leave a correct control file */
  RET_ERR_UNLESS(delete_file() == 0);
  RET_ERR_UNLESS(create_or_open_file() == CONTROL_FILE_OK);
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


static void version()
{
  printf("ma_control_file_test: unit test for the control file "
         "module of the Maria storage engine. Ver 1.0 \n");
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
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


static void usage()
{
  printf("Usage: %s [options]\n\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
