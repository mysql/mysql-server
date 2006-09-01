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

/* Unit test of the control file module of the Maria engine */

/* TODO: make it fit the mytap framework */

/*
  Note that it is not possible to test the durability of the write (can't
  pull the plug programmatically :)
*/

#include "maria.h"
#include "ma_control_file.h"
#include <my_getopt.h>

char file_name[FN_REFLEN];
int fd= -1;

static void clean_files();
static void run_test_normal();
static void run_test_abnormal();
static void usage();
static void get_options(int argc, char *argv[]);

int main(int argc,char *argv[])
{
  MY_INIT(argv[0]);

  get_options(argc,argv);

  clean_files();
  run_test_normal();
  run_test_abnormal();

  exit(0); /* all ok, if some test failed, we will have aborted */
}

/*
  Abort unless given expression is non-zero.

  SYNOPSIS
    DIE_UNLESS(expr)

  DESCRIPTION
    We can't use any kind of system assert as we need to
    preserve tested invariants in release builds as well.

  NOTE
    This is infamous copy-paste from mysql_client_test.c;
    we should instead put it in some include in one single place.
*/

#define DIE_UNLESS(expr) \
        ((void) ((expr) ? 0 : (die(__FILE__, __LINE__, #expr), 0)))
#define DIE_IF(expr) \
        ((void) (!(expr) ? 0 : (die(__FILE__, __LINE__, #expr), 0)))
#define DIE(expr) \
        die(__FILE__, __LINE__, #expr)

void die(const char *file, int line, const char *expr)
{
  fprintf(stderr, "%s:%d: check failed: '%s'\n", file, line, expr);
  abort();
}


static void clean_files()
{
  DIE_IF(fn_format(file_name, "control", maria_data_root, "", MYF(MY_WME)) ==
         NullS);
  my_delete(file_name, MYF(0)); /* maybe file does not exist, ignore error */
}


static void run_test_normal()
{
  LSN checkpoint_lsn;
  uint32 logno;
  uint objs_to_write;
  uint i;
  char buffer[4];

  /* TEST0: Instance starts from scratch (control file does not exist) */
  DIE_UNLESS(ma_control_file_create_or_open() == 0);
  /* Check that the module reports no information */
  DIE_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.file_no == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == CONTROL_FILE_IMPOSSIBLE_LOG_OFFSET);

  /* TEST1: Simulate creation of one log */

  objs_to_write= CONTROL_FILE_WRITE_ONLY_LOGNO;
  logno= 123;
  DIE_UNLESS(ma_control_file_write_and_force(NULL, logno,
                                          objs_to_write) == 0);
  /* Check that last_logno was updated */
  DIE_UNLESS(last_logno == logno);
  /* Simulate shutdown */
  ma_control_file_end();
  /* Verify amnesia */
  DIE_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.file_no == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == CONTROL_FILE_IMPOSSIBLE_LOG_OFFSET);
  /* And restart */
  DIE_UNLESS(ma_control_file_create_or_open() == 0);
  DIE_UNLESS(last_logno == logno);

  /* TEST2: Simulate creation of 5 logs */

  objs_to_write= CONTROL_FILE_WRITE_ONLY_LOGNO;
  logno= 100;
  for (i= 0; i<5; i++)
  {
    logno*= 3;
    DIE_UNLESS(ma_control_file_write_and_force(NULL, logno,
                                            objs_to_write) == 0);
  }
  ma_control_file_end();
  DIE_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.file_no == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == CONTROL_FILE_IMPOSSIBLE_LOG_OFFSET);
  DIE_UNLESS(ma_control_file_create_or_open() == 0);
  DIE_UNLESS(last_logno == logno);

  /*
    TEST3: Simulate one checkpoint, one log creation, two checkpoints, one
    log creation.
  */

  objs_to_write= CONTROL_FILE_WRITE_ONLY_LSN;
  checkpoint_lsn= (LSN){5, 10000};
  logno= 10;
  DIE_UNLESS(ma_control_file_write_and_force(&checkpoint_lsn, logno,
                                          objs_to_write) == 0);
  /* check that last_logno was not updated */
  DIE_UNLESS(last_logno != logno);
  /* Check that last_checkpoint_lsn was updated */
  DIE_UNLESS(last_checkpoint_lsn.file_no == checkpoint_lsn.file_no);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == checkpoint_lsn.rec_offset);

  objs_to_write= CONTROL_FILE_WRITE_ONLY_LOGNO;
  checkpoint_lsn= (LSN){5, 20000};
  logno= 17;
  DIE_UNLESS(ma_control_file_write_and_force(&checkpoint_lsn, logno,
                                          objs_to_write) == 0);
  /* Check that checkpoint LSN was not updated */
  DIE_UNLESS(last_checkpoint_lsn.rec_offset != checkpoint_lsn.rec_offset);
  objs_to_write= CONTROL_FILE_WRITE_ONLY_LSN;
  checkpoint_lsn= (LSN){17, 20000};
  DIE_UNLESS(ma_control_file_write_and_force(&checkpoint_lsn, logno,
                                          objs_to_write) == 0);
  objs_to_write= CONTROL_FILE_WRITE_ONLY_LSN;
  checkpoint_lsn= (LSN){17, 45000};
  DIE_UNLESS(ma_control_file_write_and_force(&checkpoint_lsn, logno,
                                          objs_to_write) == 0);
  objs_to_write= CONTROL_FILE_WRITE_ONLY_LOGNO;
  logno= 19;
  DIE_UNLESS(ma_control_file_write_and_force(&checkpoint_lsn, logno,
                                          objs_to_write) == 0);

  ma_control_file_end();
  DIE_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.file_no == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == CONTROL_FILE_IMPOSSIBLE_LOG_OFFSET);
  DIE_UNLESS(ma_control_file_create_or_open() == 0);
  DIE_UNLESS(last_logno == logno);
  DIE_UNLESS(last_checkpoint_lsn.file_no == checkpoint_lsn.file_no);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == checkpoint_lsn.rec_offset);

  /*
    TEST4: actually check by ourselves the content of the file.
    Note that constants (offsets) are hard-coded here, precisely to prevent
    someone from changing them in the control file module and breaking
    backward-compatibility.
  */

  DIE_IF((fd= my_open(file_name,
                      O_BINARY | O_RDWR,
                      MYF(MY_WME))) < 0);
  DIE_IF(my_read(fd, buffer, 16, MYF(MY_FNABP |  MY_WME)) != 0);
  DIE_IF(my_close(fd, MYF(MY_WME)) != 0);  
  i= uint4korr(buffer+4);
  DIE_UNLESS(i == last_checkpoint_lsn.file_no);
  i= uint4korr(buffer+8);
  DIE_UNLESS(i == last_checkpoint_lsn.rec_offset);
  i= uint4korr(buffer+12);
  DIE_UNLESS(i == last_logno);


  /* TEST5: Simulate stop/start/nothing/stop/start */

  ma_control_file_end();
  DIE_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(ma_control_file_create_or_open() == 0);
  ma_control_file_end();
  DIE_UNLESS(last_logno == CONTROL_FILE_IMPOSSIBLE_FILENO);
  DIE_UNLESS(ma_control_file_create_or_open() == 0);
  DIE_UNLESS(last_logno == logno);
  DIE_UNLESS(last_checkpoint_lsn.file_no == checkpoint_lsn.file_no);
  DIE_UNLESS(last_checkpoint_lsn.rec_offset == checkpoint_lsn.rec_offset);

}

static void run_test_abnormal()
{
  /* Corrupt the control file */
  DIE_IF((fd= my_open(file_name,
                      O_BINARY | O_RDWR,
                      MYF(MY_WME))) < 0);
  DIE_IF(my_write(fd, "papa", 4, MYF(MY_FNABP |  MY_WME)) != 0);
  DIE_IF(my_close(fd, MYF(MY_WME)) != 0);

  /* Check that control file module sees the problem */
  DIE_IF(ma_control_file_create_or_open() == 0);
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

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  return;
} /* get options */


static void usage()
{
  printf("Usage: %s [options]\n\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
