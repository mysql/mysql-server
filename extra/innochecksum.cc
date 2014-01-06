/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  InnoDB offline file checksum utility.  85% of the code in this utility
  is included from the InnoDB codebase.

  The final 15% was originally written by Mark Smith of Danga
  Interactive, Inc. <junior@danga.com>

  Published with a permission.
*/

#include <my_config.h>
#include <my_global.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __WIN__
# include <unistd.h>
#endif
#include <my_getopt.h>
#include <m_string.h>
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/* Only parts of these files are included from the InnoDB codebase.
The parts not included are excluded by #ifndef UNIV_INNOCHECKSUM. */

#include "univ.i"                /*  include all of this */

#include "buf0checksum.h"        /* buf_calc_page_*() */
#include "fil0fil.h"             /* FIL_* */
#include "fsp0fsp.h"             /* fsp_flags_get_page_size() &
                                    fsp_flags_get_zip_size() */
#include "mach0data.h"           /* mach_read_from_4() */
#include "ut0crc32.h"            /* ut_crc32_init() */

#ifdef UNIV_NONINL
# include "fsp0fsp.ic"
# include "mach0data.ic"
# include "ut0rnd.ic"
#endif

/* Global variables */
static my_bool verbose;
static my_bool debug;
static my_bool just_count;
static ullint start_page;
static ullint end_page;
static ullint do_page;
static my_bool use_end_page;
static my_bool do_one_page;
ulong srv_page_size;              /* replaces declaration in srv0srv.c */
static ulong physical_page_size;  /* Page size in bytes on disk. */
static ulong logical_page_size;   /* Page size when uncompressed. */

/* Get the page size of the filespace from the filespace header. */
static
my_bool
get_page_size(
/*==========*/
  FILE*  f,                     /*!< in: file pointer, must be open
                                         and set to start of file */
  byte* buf,                    /*!< in: buffer used to read the page */
  ulong* logical_page_size,     /*!< out: Logical/Uncompressed page size */
  ulong* physical_page_size)    /*!< out: Physical/Commpressed page size */
{
  ulong flags;

  ulong bytes= ulong(fread(buf, 1, UNIV_PAGE_SIZE_MIN, f));

  if (ferror(f))
  {
    perror("Error reading file header");
    return FALSE;
  }

  if (bytes != UNIV_PAGE_SIZE_MIN)
  {
    fprintf(stderr, "Error; Was not able to read the minimum page size ");
    fprintf(stderr, "of %d bytes.  Bytes read was %lu\n", UNIV_PAGE_SIZE_MIN, bytes);
    return FALSE;
  }

  rewind(f);

  flags = mach_read_from_4(buf + FIL_PAGE_DATA + FSP_SPACE_FLAGS);

  /* srv_page_size is used by InnoDB code as UNIV_PAGE_SIZE */
  srv_page_size = *logical_page_size = fsp_flags_get_page_size(flags);

  /* fsp_flags_get_zip_size() will return zero if not compressed. */
  *physical_page_size = fsp_flags_get_zip_size(flags);
  if (*physical_page_size == 0)
    *physical_page_size= *logical_page_size;

  return TRUE;
}

#ifdef __WIN__
/***********************************************//*
 @param		[in] error	error no. from the getLastError().

 @retval error message corresponding to error no.
*/
static
char*
win32_error_message(
	int	error)
{
	static char err_msg[1024] = {'\0'};
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)err_msg, sizeof(err_msg), NULL );

	return (err_msg);
}
#endif /* __WIN__ */

/***********************************************//*
 @param [in] name	name of file.
 @retval file pointer; file pointer is NULL when error occured.
*/

FILE*
open_file(
	const char*	name)
{
	int	fd;		/* file descriptor. */
	FILE*	fil_in;
#ifdef __WIN__
	HANDLE		hFile;		/* handle to open file. */
	DWORD		access;		/* define access control */
	int		flags;		/* define the mode for file
					descriptor */

	access = GENERIC_READ;
	flags = _O_RDONLY | _O_BINARY;
	hFile = CreateFile(
			(LPCTSTR) name, access, 0L, NULL,
			OPEN_EXISTING, NULL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		/* print the error message. */
		fprintf(stderr, "Filename::%s %s\n",
			win32_error_message(GetLastError()));

			return (NULL);
		}

	/* get the file descriptor. */
	fd= _open_osfhandle((intptr_t)hFile, flags);
#else /* __WIN__ */

	int	create_flag;
	create_flag = O_RDONLY;

	fd = open(name, create_flag);
	if ( -1 == fd) {
		perror("open");
		return (NULL);
	}

#endif /* __WIN__ */

	fil_in = fdopen(fd, "rb");

	return (fil_in);
}

/* command line argument to do page checks (that's it) */
/* another argument to specify page ranges... seek to right spot and go from there */

static struct my_option innochecksum_options[] =
{
  {"help", '?', "Displays this help and exits.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"info", 'I', "Synonym for --help.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Displays version information and exits.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Verbose (prints progress every 5 seconds).",
    &verbose, &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", 'd', "Debug mode (prints checksums for each page, implies verbose).",
    &debug, &debug, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 'c', "Print the count of pages in the file.",
    &just_count, &just_count, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"start_page", 's', "Start on this page number (0 based).",
    &start_page, &start_page, 0, GET_ULL, REQUIRED_ARG,
    0, 0, ULONGLONG_MAX, 0, 1, 0},
  {"end_page", 'e', "End at this page number (0 based).",
    &end_page, &end_page, 0, GET_ULL, REQUIRED_ARG,
    0, 0, ULONGLONG_MAX, 0, 1, 0},
  {"page", 'p', "Check only this page (0 based).",
    &do_page, &do_page, 0, GET_ULL, REQUIRED_ARG,
    0, 0, ULONGLONG_MAX, 0, 1, 0},

  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void print_version(void)
{
  printf("%s Ver %s, for %s (%s)\n",
         my_progname, INNODB_VERSION_STR,
         SYSTEM_TYPE, MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("InnoDB offline file checksum utility.\n");
  printf("Usage: %s [-c] [-s <start page>] [-e <end page>] [-p <page>] [-v] [-d] <filename>\n", my_progname);
  my_print_help(innochecksum_options);
  my_print_variables(innochecksum_options);
}

extern "C" my_bool
innochecksum_get_one_option(
/*========================*/
  int optid,
  const struct my_option *opt __attribute__((unused)),
  char *argument __attribute__((unused)))
{
  switch (optid) {
  case 'd':
    verbose=1;	/* debug implies verbose... */
    break;
  case 'e':
    use_end_page= 1;
    break;
  case 'p':
    end_page= start_page= do_page;
    use_end_page= 1;
    do_one_page= 1;
    break;
  case 'V':
    print_version();
    exit(0);
    break;
  case 'I':
  case '?':
    usage();
    exit(0);
    break;
  }
  return 0;
}

static int get_options(
/*===================*/
  int *argc,
  char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, innochecksum_options, innochecksum_get_one_option)))
    exit(ho_error);

  /* The next arg must be the filename */
  if (!*argc)
  {
    usage();
    return 1;
  }
  return 0;
} /* get_options */


int main(int argc, char **argv)
{
  FILE* f;                       /* our input file */
  char* filename;                /* our input filename. */
  unsigned char buf[UNIV_PAGE_SIZE_MAX]; /* Buffer to store pages read */
  ulong bytes;                   /* bytes read count */
  ulint ct;                      /* current page number (0 based) */
  time_t now;                    /* current time */
  time_t lastt;                  /* last time */
  ulint oldcsum, oldcsumfield, csum, csumfield, crc32, logseq, logseqfield;

                                 /* ulints for checksum storage */
  /* stat, to get file size. */
#ifdef __WIN__
  struct _stat64 st;
#else
  struct stat st;
#endif
  unsigned long long int size;   /* size of file (has to be 64 bits) */
  ulint pages;                   /* number of pages in file */
  off_t offset= 0;

  printf("InnoDB offline file checksum utility.\n");

  ut_crc32_init();

  MY_INIT(argv[0]);

  if (get_options(&argc,&argv))
    exit(1);

  if (verbose)
    my_print_variables(innochecksum_options);

  /* The file name is not optional */
  filename = *argv;
  if (*filename == '\0')
  {
    fprintf(stderr, "Error; File name missing\n");
    return 1;
  }

  /* stat the file to get size and page count */
#ifdef __WIN__
  if (_stat64(filename, &st))
#else
  if (stat(filename, &st))
#endif
  {
    fprintf(stderr, "Error; %s cannot be found\n", filename);
    return 1;
  }
  size= st.st_size;

  /* Open the file for reading */
  f= open_file(filename);
  if (f == NULL) {
    return 1;
  }

  if (!get_page_size(f, buf, &logical_page_size, &physical_page_size))
  {
    return 1;
  }

  /* This tool currently does not support Compressed tables */
  if (logical_page_size != physical_page_size)
  {
    fprintf(stderr, "Error; This file contains compressed pages\n");
    return 1;
  }

  pages= (ulint) (size / physical_page_size);

  if (just_count)
  {
    if (verbose)
      printf("Number of pages: ");
    printf("%lu\n", pages);
    return 0;
  }
  else if (verbose)
  {
    printf("file %s = %llu bytes (%lu pages)...\n", filename, size, pages);
    if (do_one_page)
      printf("InnoChecksum; checking page %llu\n", do_page);
    else
      printf("InnoChecksum; checking pages in range %llu to %llu\n", start_page, use_end_page ? end_page : (pages - 1));
  }

  /* seek to the necessary position */
  if (start_page)
  {

    offset= (off_t)start_page * (off_t)physical_page_size;

#ifdef __WIN__
	if (_fseeki64(f, offset, SEEK_SET)) {
#else
	if (fseeko(f, offset, SEEK_SET)) {
#endif /* __WIN__ */
	perror("Error; Unable to seek to necessary offset");
	return 1;
    }
  }

  /* main checksumming loop */
  ct= start_page;
  lastt= 0;
  while (!feof(f))
  {
    bytes= ulong(fread(buf, 1, physical_page_size, f));
    if (!bytes && feof(f))
      return 0;

    if (ferror(f))
    {
      fprintf(stderr, "Error reading %lu bytes", physical_page_size);
      perror(" ");
      return 1;
    }
    if (bytes != physical_page_size)
    {
      fprintf(stderr, "Error; bytes read (%lu) doesn't match page size (%lu)\n", bytes, physical_page_size);
      return 1;
    }

    /* check the "stored log sequence numbers" */
    logseq= mach_read_from_4(buf + FIL_PAGE_LSN + 4);
    logseqfield= mach_read_from_4(buf + logical_page_size - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);
    if (debug)
      printf("page %lu: log sequence number: first = %lu; second = %lu\n", ct, logseq, logseqfield);
    if (logseq != logseqfield)
    {
      fprintf(stderr, "Fail; page %lu invalid (fails log sequence number check)\n", ct);
      return 1;
    }

    /* check old method of checksumming */
    oldcsum= buf_calc_page_old_checksum(buf);
    oldcsumfield= mach_read_from_4(buf + logical_page_size - FIL_PAGE_END_LSN_OLD_CHKSUM);
    if (debug)
      printf("page %lu: old style: calculated = %lu; recorded = %lu\n", ct, oldcsum, oldcsumfield);
    if (oldcsumfield != mach_read_from_4(buf + FIL_PAGE_LSN) && oldcsumfield != oldcsum)
    {
      fprintf(stderr, "Fail;  page %lu invalid (fails old style checksum)\n", ct);
      return 1;
    }

    /* now check the new method */
    csum= buf_calc_page_new_checksum(buf);
    crc32= buf_calc_page_crc32(buf);
    csumfield= mach_read_from_4(buf + FIL_PAGE_SPACE_OR_CHKSUM);
    if (debug)
      printf("page %lu: new style: calculated = %lu; crc32 = %lu; recorded = %lu\n",
          ct, csum, crc32, csumfield);
    if (csumfield != 0 && crc32 != csumfield && csum != csumfield)
    {
      fprintf(stderr, "Fail; page %lu invalid (fails innodb and crc32 checksum)\n", ct);
      return 1;
    }

    /* end if this was the last page we were supposed to check */
    if (use_end_page && (ct >= end_page))
      return 0;

    /* do counter increase and progress printing */
    ct++;
    if (verbose)
    {
      if (ct % 64 == 0)
      {
        now= time(0);
        if (!lastt) lastt= now;
        if (now - lastt >= 1)
        {
          printf("page %lu okay: %.3f%% done\n", (ct - 1), (float) ct / pages * 100);
          lastt= now;
        }
      }
    }
  }
  return 0;
}

