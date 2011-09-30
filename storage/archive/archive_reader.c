/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "azlib.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <m_ctype.h>
#include <m_string.h>
#include <my_getopt.h>
#include <mysql_version.h>

#define BUFFER_LEN 1024
#define ARCHIVE_ROW_HEADER_SIZE 4

#define SHOW_VERSION "0.1"

static void get_options(int *argc,char * * *argv);
static void print_version(void);
static void usage(void);
static const char *opt_tmpdir;
static const char *new_auto_increment;
unsigned long long new_auto_increment_value;
static const char *load_default_groups[]= { "archive_reader", 0 };
static char **default_argv;
int opt_check, opt_force, opt_quiet, opt_backup= 0, opt_extract_frm;
int opt_autoincrement;

int main(int argc, char *argv[])
{
  unsigned int ret;
  azio_stream reader_handle;

  MY_INIT(argv[0]);
  get_options(&argc, &argv);

  if (argc < 1)
  {
    printf("No file specified. \n");
    return 0;
  }

  if (!(ret= azopen(&reader_handle, argv[0], O_RDONLY|O_BINARY)))
  {
    printf("Could not open Archive file\n");
    return 0;
  }

  if (opt_autoincrement)
  {
    azio_stream writer_handle;

    if (new_auto_increment_value)
    {
      if (reader_handle.auto_increment >= new_auto_increment_value)
      {
        printf("Value is lower then current value\n");
        goto end;
      }
    }
    else
    {
      new_auto_increment_value= reader_handle.auto_increment + 1;
    }

    if (!(ret= azopen(&writer_handle, argv[0], O_CREAT|O_RDWR|O_BINARY)))
    {
      printf("Could not open file for update: %s\n", argv[0]);
      goto end;
    }

    writer_handle.auto_increment= new_auto_increment_value;

    azclose(&writer_handle);
    azflush(&reader_handle, Z_SYNC_FLUSH);
  }

  printf("Version %u\n", reader_handle.version);
  if (reader_handle.version > 2)
  {
    printf("\tMinor version %u\n", reader_handle.minor_version);
    printf("\tStart position %llu\n", (unsigned long long)reader_handle.start);
    printf("\tBlock size %u\n", reader_handle.block_size);
    printf("\tRows %llu\n", reader_handle.rows);
    printf("\tAutoincrement %llu\n", reader_handle.auto_increment);
    printf("\tCheck Point %llu\n", reader_handle.check_point);
    printf("\tForced Flushes %llu\n", reader_handle.forced_flushes);
    printf("\tLongest Row %u\n", reader_handle.longest_row);
    printf("\tShortest Row %u\n", reader_handle.shortest_row);
    printf("\tState %s\n", ( reader_handle.dirty ? "dirty" : "clean"));
    printf("\tFRM stored at %u\n", reader_handle.frm_start_pos);
    printf("\tComment stored at %u\n", reader_handle.comment_start_pos);
    printf("\tData starts at %u\n", (unsigned int)reader_handle.start);
    if (reader_handle.frm_start_pos)
      printf("\tFRM length %u\n", reader_handle.frm_length);
    if (reader_handle.comment_start_pos)
    {
      char *comment =
        (char *) malloc(sizeof(char) * reader_handle.comment_length);
      azread_comment(&reader_handle, comment);
      printf("\tComment length %u\n\t\t%.*s\n", reader_handle.comment_length, 
             reader_handle.comment_length, comment);
      free(comment);
    }
  }
  else
  {
    goto end;
  }

  printf("\n");

  if (opt_check)
  {
    uchar size_buffer[ARCHIVE_ROW_HEADER_SIZE];
    int error;
    unsigned int x;
    unsigned int read;
    unsigned int row_len;
    unsigned long long row_count= 0;
    char buffer;

    while ((read= azread(&reader_handle, (uchar *)size_buffer, 
                        ARCHIVE_ROW_HEADER_SIZE, &error)))
    {
      if (error == Z_STREAM_ERROR ||  (read && read < ARCHIVE_ROW_HEADER_SIZE))
      {
        printf("Table is damaged\n");
        goto end;
      }

      /* If we read nothing we are at the end of the file */
      if (read == 0 || read != ARCHIVE_ROW_HEADER_SIZE)
        break;

      row_len=  uint4korr(size_buffer);
      row_count++;

      if (row_len > reader_handle.longest_row)
      {
        printf("Table is damaged, row %llu is invalid\n", 
               row_count);
        goto end;
      }


      for (read= x= 0; x < row_len ; x++) 
      {
        read+= (unsigned int)azread(&reader_handle, &buffer, sizeof(char), &error); 
        if (!read)
          break;
      }


      if (row_len != read)
      {
        printf("Row length did not match row (at %llu). %u != %u \n", 
               row_count, row_len, read);
        goto end;
      }
    }

    if (0)
    {
      printf("Table is damaged\n");
      goto end;
    }
    else
    {
      printf("Found %llu rows\n", row_count);
    }
  }

  if (opt_backup)
  {
    uchar size_buffer[ARCHIVE_ROW_HEADER_SIZE];
    int error;
    unsigned int read;
    unsigned int row_len;
    unsigned long long row_count= 0;
    char *buffer;

    azio_stream writer_handle;

    buffer= (char *)malloc(reader_handle.longest_row);
    if (buffer == NULL)
    {
      printf("Could not allocate memory for row %llu\n", row_count);
      goto end;
    }


    if (!(ret= azopen(&writer_handle, argv[1], O_CREAT|O_RDWR|O_BINARY)))
    {
      printf("Could not open file for backup: %s\n", argv[1]);
      goto end;
    }

    writer_handle.auto_increment= reader_handle.auto_increment;
    if (reader_handle.frm_length)
    {
      char *ptr;
      ptr= (char *)my_malloc(sizeof(char) * reader_handle.frm_length, MYF(0));
      azread_frm(&reader_handle, ptr);
      azwrite_frm(&writer_handle, ptr, reader_handle.frm_length);
      my_free(ptr);
    }

    if (reader_handle.comment_length)
    {
      char *ptr;
      ptr= (char *)my_malloc(sizeof(char) * reader_handle.comment_length, MYF(0));
      azread_comment(&reader_handle, ptr);
      azwrite_comment(&writer_handle, ptr, reader_handle.comment_length);
      my_free(ptr);
    }

    while ((read= azread(&reader_handle, (uchar *)size_buffer, 
                        ARCHIVE_ROW_HEADER_SIZE, &error)))
    {
      if (error == Z_STREAM_ERROR ||  (read && read < ARCHIVE_ROW_HEADER_SIZE))
      {
        printf("Table is damaged\n");
        goto end;
      }

      /* If we read nothing we are at the end of the file */
      if (read == 0 || read != ARCHIVE_ROW_HEADER_SIZE)
        break;

      row_len=  uint4korr(size_buffer);

      row_count++;

      memcpy(buffer, size_buffer, ARCHIVE_ROW_HEADER_SIZE);

      read= (unsigned int)azread(&reader_handle, buffer + ARCHIVE_ROW_HEADER_SIZE, 
                                 row_len, &error); 

      DBUG_ASSERT(read == row_len);

      azwrite(&writer_handle, buffer, row_len + ARCHIVE_ROW_HEADER_SIZE);


      if (row_len != read)
      {
        printf("Row length did not match row (at %llu). %u != %u \n", 
               row_count, row_len, read);
        goto end;
      }

      if (reader_handle.rows == writer_handle.rows)
        break;
    }

    free(buffer);

    azclose(&writer_handle);
  }

  if (opt_extract_frm)
  {
    File frm_file;
    char *ptr;
    frm_file= my_open(argv[1], O_CREAT|O_RDWR|O_BINARY, MYF(0));
    ptr= (char *)my_malloc(sizeof(char) * reader_handle.frm_length, MYF(0));
    azread_frm(&reader_handle, ptr);
    my_write(frm_file, (uchar*) ptr, reader_handle.frm_length, MYF(0));
    my_close(frm_file, MYF(0));
    my_free(ptr);
  }

end:
  printf("\n");
  azclose(&reader_handle);

  return 0;
}

static my_bool
get_one_option(int optid,
	       const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 'b':
    opt_backup= 1;
    break;
  case 'c':
    opt_check= 1;
    break;
  case 'e':
    opt_extract_frm= 1;
    break;
  case 'f':
    opt_force= 1;
    printf("Not implemented yet\n");
    break;
  case 'q':
    opt_quiet= 1;
    printf("Not implemented yet\n");
    break;
  case 'V':
    print_version();
    exit(0);
  case 't':
    printf("Not implemented yet\n");
    break;
  case 'A':
    opt_autoincrement= 1;
    if (argument)
      new_auto_increment_value= strtoull(argument, NULL, 0);
    else
      new_auto_increment_value= 0;
    break;
  case '?':
    usage();
    exit(0);
  case '#':
    if (argument == disabled_my_option)
    {
      DBUG_POP();
    }
    else
    {
      DBUG_PUSH(argument ? argument : "d:t:o,/tmp/archive_reader.trace");
    }
    break;
  }
  return 0;
}

static struct my_option my_long_options[] =
{
  {"backup", 'b',
   "Make a backup of an archive table.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"check", 'c', "Check table for errors.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#',
   "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"extract-frm", 'e',
   "Extract the frm file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f',
   "Restart with -r if there are any errors in the table.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?',
   "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q', "Faster repair by not modifying the data file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"repair", 'r', "Repair a damaged Archive version 3 or above file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"set-auto-increment", 'A',
   "Force auto_increment to start at this or higher value. If no value is given, then sets the next auto_increment value to the highest used value for the auto key + 1.",
   &new_auto_increment, &new_auto_increment,
   0, GET_ULL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's',
   "Only print errors. One can use two -s to make archive_reader very silent.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files.",
   &opt_tmpdir,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V',
   "Print version and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage(void)
{
  print_version();
  puts("Copyright 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc.");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Read and modify Archive files directly\n");
  printf("Usage: %s [OPTIONS] file_to_be_looked_at [file_for_backup]\n", my_progname);
  print_defaults("my", load_default_groups);
  my_print_help(my_long_options);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, SHOW_VERSION,
         MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
}

static void get_options(int *argc, char ***argv)
{
  if (load_defaults("my", load_default_groups, argc, argv))
    exit(1);
  default_argv= *argv;

  handle_options(argc, argv, my_long_options, get_one_option);

  if (*argc == 0)
  {
    usage();
    exit(-1);
  }

  return;
} /* get options */
