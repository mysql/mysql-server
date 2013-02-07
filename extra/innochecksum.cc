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
#include <welcome_copyright_notice.h>	/* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/* Only parts of these files are included from the InnoDB codebase.
The parts not included are excluded by #ifndef UNIV_INNOCHECKSUM. */

#include "univ.i"			/* include all of this */
#include "buf0buf.h"
#include "page0zip.h"
#include "page0page.h"
#include "fsp0types.h"
#include "trx0undo.h"
#include "fut0lst.h"
#include "buf0checksum.h"		/* buf_calc_page_*() */
#include "fil0fil.h"			/* FIL_* */
#include "fsp0fsp.h"			/* fsp_flags_get_page_size() &
					fsp_flags_get_zip_size() */
#include "mach0data.h"			/* mach_read_from_4() */
#include "ut0crc32.h"			/* ut_crc32_init() */


#ifdef UNIV_NONINL
# include "fsp0fsp.ic"
# include "mach0data.ic"
# include "ut0rnd.ic"
#endif

/* Global variables */
static my_bool	verbose;
my_bool		debug = 0;
static my_bool	just_count;
static ulong	start_page;
static ulong	end_page;
static ulong	do_page;
static my_bool	use_end_page;
static my_bool	do_one_page;
ulong		srv_page_size;		/* replaces declaration in srv0srv.c */
extern ulong	srv_checksum_algorithm;
ulint		ct;			/* Current page number (0 based). */
static my_bool	no_check;		/* Skip the checksum verification. */
my_bool		strict_verify = 0;	/* Enabled for strict checksum
					verification. */
static my_bool	do_write;		/* Enabled for rewrite checksum. */
static ulong	allow_mismatches;	/* Mismatches count allowed
					(0 by default). */
static my_bool	page_type_summary;
static my_bool	page_type_dump;
char*		dump_filename = 0;		/* Store filename for page-type-dump option. */
const char	*default_dbug_option = IF_WIN("d:O,\\innochecksum.trace","d:o,/tmp/innochecksum.trace");
/* strict check algorithm name. */
static srv_checksum_algorithm_t	strict_check;
/* Rewrite checksum algorithm name. */
static srv_checksum_algorithm_t	write_check;

/* Innodb page type */
struct innodb_page_type {
	int n_undo_state_active;
	int n_undo_state_cached;
	int n_undo_state_to_free;
	int n_undo_state_to_purge;
	int n_undo_state_prepared;
	int n_undo_state_other;
	int n_undo_insert, n_undo_update, n_undo_other;
	int n_fil_page_index;
	int n_fil_page_undo_log;
	int n_fil_page_inode;
	int n_fil_page_ibuf_free_list;
	int n_fil_page_allocated;
	int n_fil_page_ibuf_bitmap;
	int n_fil_page_type_sys;
	int n_fil_page_type_trx_sys;
	int n_fil_page_type_fsp_hdr;
	int n_fil_page_type_allocated;
	int n_fil_page_type_xdes;
	int n_fil_page_type_blob;
	int n_fil_page_type_zblob;
	int n_fil_page_type_other;
	int n_fil_page_type_zblob2;
} page_type;

/* Possible values for "--strict-check" for strictly verify checksum
and "--write" for rewrite checksum. */
static const char *innochecksum_algorithms[] = {
	"crc32",
	"crc32",
	"innodb",
	"innodb",
	"none",
	"none",
	NullS
};

/* Used to define an enumerate type of the "innochecksum algorithm". */
static TYPELIB innochecksum_algorithms_typelib = {
	array_elements(innochecksum_algorithms)-1,"",
	innochecksum_algorithms, NULL
};


/****************************************************************************//*
Get the page size of the filespace from the filespace header.
 @param		buf			[in] buffer used to read the page.
 @param		logical_page_size	[out] Logical/Uncompressed page size.
 @param		physical_page_size	[out] Physical/Commpressed page size.
 @param		compressed		[out] Enable if tablespace is compressed
*/

static
void
get_page_size(
	byte*		buf,
	ulong*		logical_page_size,
	ulong*		physical_page_size,
	my_bool*	compressed) {

	ulong flags;

	flags = mach_read_from_4(buf + FIL_PAGE_DATA + FSP_SPACE_FLAGS);

	/* srv_page_size is used by InnoDB code as UNIV_PAGE_SIZE */
	srv_page_size = *logical_page_size = fsp_flags_get_page_size(flags);

	/* fsp_flags_get_zip_size() will return zero if not compressed. */
	*physical_page_size = fsp_flags_get_zip_size(flags);
	if (*physical_page_size == 0) {
		*physical_page_size= *logical_page_size;
		*compressed = 0;
	}
	else
		*compressed = 1;

}

/*******************************************************//*
Check weather page is empty or not.
 @param		s	[in] page to checked for empty.
 @param		len	[in] size of page.

 @retval TRUE if page is empty.
 @retval FALSE if page is not empty.
*/
static
bool
is_page_empty(
	const byte*	page,
	size_t		len) {

	while (len--) {
		if (*page++)
			return(FALSE);
        }
        return(TRUE);
}

/********************************************************************//**
Rewrite the checksum for the page.
@param	page			[in/out]	page buffer
@param	physical_page_size	[in]		Page size in bytes on disk.
@param	iscompressed		[in]		Is compressed/Uncompressed Page.

@retval True means do rewrite
@retval FALSE means skip the rewrite as checksum stored is same as calculated.
*/
UNIV_INTERN
my_bool
update_checksum(
	byte*	page,
	ulong	physical_page_size,
	bool	iscompressed) {

	ib_uint32_t	checksum_field1 = 0;
	byte		stored1[4];
	byte		stored2[4];

	ut_ad(page);

	memcpy(stored1, page + FIL_PAGE_SPACE_OR_CHKSUM, 4);
	memcpy(stored2, page + physical_page_size -
		FIL_PAGE_END_LSN_OLD_CHKSUM, 4);

	/* Checked is page is empty ,exclude the checksum field */
	if (is_page_empty(page + 4, physical_page_size - 12) &&
		is_page_empty(page + physical_page_size - 4, 4)) {

		memset(page + FIL_PAGE_SPACE_OR_CHKSUM, 0, 4);
		memset(page + physical_page_size -
			FIL_PAGE_END_LSN_OLD_CHKSUM, 0, 4);

		goto func_exit;
	}

	if (iscompressed) {
		/* means page is compressed */
		checksum_field1 = page_zip_calc_checksum(
						page, physical_page_size,
						write_check);
		if (debug)
			DBUG_PRINT("info", ("page %lu: Updated checksum = %u;\n",
				ct, checksum_field1));
	} else {
		/*means page is  uncompressed. */

		ib_uint32_t	checksum_field2 = 0 ;
		/* Store the new formula checksum */
		switch (write_check) {

		case SRV_CHECKSUM_ALGORITHM_CRC32:
		case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
			checksum_field1 = buf_calc_page_crc32(page);
			checksum_field2 = checksum_field1;
			break;

		case SRV_CHECKSUM_ALGORITHM_INNODB:
		case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
			checksum_field1 = (ib_uint32_t)
					buf_calc_page_new_checksum(page);
			checksum_field2 = (ib_uint32_t)
					buf_calc_page_old_checksum(page);
			break;

		case SRV_CHECKSUM_ALGORITHM_NONE:
		case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
			checksum_field1 = BUF_NO_CHECKSUM_MAGIC;
			checksum_field2 = checksum_field1;
			break;
		/* no default so the compiler will emit a warning if new
		enum is added and not handled here */
		}

		if (debug)
		DBUG_PRINT("info", ("page %lu: Updated checksum field1 = %u;"
			"checksum field2 = %u;\n", ct,
			checksum_field1,checksum_field2));

		mach_write_to_4(page + physical_page_size -
			FIL_PAGE_END_LSN_OLD_CHKSUM,checksum_field2);

	}

	mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum_field1);

	func_exit:

	if (iscompressed) {
		if (!memcmp(stored1,page + FIL_PAGE_SPACE_OR_CHKSUM,4))
			return FALSE;
		return TRUE;
	}

	if (!memcmp(stored1,page + FIL_PAGE_SPACE_OR_CHKSUM,4) &&
	    !memcmp(stored2,page + physical_page_size -
		FIL_PAGE_END_LSN_OLD_CHKSUM,4))
		return FALSE;

	return TRUE;
}

/*
Parse the page and collect/dump the information about page type
@param page	[in]	buffer page
@param f	[in]	file for diagnosis.
*/
void
parse_page(
	const byte*	page,
	FILE*		f) {

	unsigned long long id;
	ulint x;

	switch (mach_read_from_2(page + FIL_PAGE_TYPE)){

	case FIL_PAGE_INDEX:
		page_type.n_fil_page_index++;
		id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
		if(page_type_dump) {
			fprintf(f,"#::%8lu\t\t|\t\tIndex page\t\t\t|\tindex id=%llu,",ct,(ullint)id);

			fprintf(f,
				" page level=%lu,No. of records=%lu,"
				" garbage=%lu\n",
				page_header_get_field(page, PAGE_LEVEL),
				page_header_get_field(page, PAGE_N_RECS),
			 	page_header_get_field(page, PAGE_GARBAGE));
		}
		break;

	case FIL_PAGE_UNDO_LOG:
		page_type.n_fil_page_undo_log++;
		x = mach_read_from_2(page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE);
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tUndo log page\t\t\t|",ct);
		if (x == TRX_UNDO_INSERT) {
			page_type.n_undo_insert++;
			if(page_type_dump)
				fprintf(f, " %-65s\n","Insert Undo log page");

		} else if (x == TRX_UNDO_UPDATE) {
			page_type.n_undo_update++;
			if(page_type_dump)
				fprintf(f, " %-65s\n","Update undo log page");
		}

		x = mach_read_from_2(page + TRX_UNDO_SEG_HDR + TRX_UNDO_STATE);
		switch (x) {
			case TRX_UNDO_ACTIVE:
				page_type.n_undo_state_active++;
				if(page_type_dump)
					fprintf(f, " %-65s\n","Undo log of an active transaction");
				break;
			case TRX_UNDO_CACHED:
				page_type.n_undo_state_cached++;
				if(page_type_dump)
					fprintf(f, " %-65s\n","Page is cached for quick reuse");
				break;
			case TRX_UNDO_TO_FREE:
				page_type.n_undo_state_to_free++;
				if(page_type_dump)
					fprintf(f, " %-65s\n","Insert undo segment that can be freed");
				break;
			case TRX_UNDO_TO_PURGE:
				page_type.n_undo_state_to_purge++;
				if(page_type_dump)
					fprintf(f, " %-65s\n","Will be freed in purge when all undo data in it is removed");
				break;
			case TRX_UNDO_PREPARED:
				page_type.n_undo_state_prepared++;
				if(page_type_dump)
					fprintf(f, " %-65s\n","Undo log of an prepared transaction");
				break;
			default:
				page_type.n_undo_state_other++;
				break;
		}
		break;

	case FIL_PAGE_INODE:
		page_type.n_fil_page_inode++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tInode Page\t\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_IBUF_FREE_LIST:
		page_type.n_fil_page_ibuf_free_list++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tInsert buffer free list page\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_ALLOCATED:
		page_type.n_fil_page_type_allocated++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tFreshly allocated page\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_IBUF_BITMAP:
		page_type.n_fil_page_ibuf_bitmap++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tInsert Buffer Bitmap\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_SYS:
		page_type.n_fil_page_type_sys++;
		if(page_type_dump)
			fprintf(f, "#::%lud\t\t|\t\tSystem page\t\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_TRX_SYS:
		page_type.n_fil_page_type_trx_sys++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tTransaction system page\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_FSP_HDR:
		page_type.n_fil_page_type_fsp_hdr++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tFile Space Header\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_XDES:
		page_type.n_fil_page_type_xdes++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tan extent descriptor page\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_BLOB:
		page_type.n_fil_page_type_blob++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tBLOB page\t\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_ZBLOB:
		page_type.n_fil_page_type_zblob++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tCompressed BLOB page\t\t|\t-\n",ct);
		break;

	case FIL_PAGE_TYPE_ZBLOB2:
		page_type.n_fil_page_type_zblob2++;
		if(page_type_dump)
			fprintf(f, "#::%8lu\t\t|\t\tCompressed BLOB page\t\t|\t-\n",ct);
			break;

	default:
		page_type.n_fil_page_type_other++;
		break;
	}
}

/*Display the page type count of a tablespace. */
void
print_summary() {

	printf("\n================PAGE TYPE SUMMARY=====================\n");
	printf("%d\tFIL_PAGE_INDEX\n", page_type.n_fil_page_index);
	printf("%d\tFIL_PAGE_UNDO_LOG\n", page_type.n_fil_page_undo_log);
	printf("%d\tFIL_PAGE_INODE\n", page_type.n_fil_page_inode);
	printf("%d\tFIL_PAGE_IBUF_FREE_LIST\n", page_type.n_fil_page_ibuf_free_list);
	printf("%d\tFIL_PAGE_TYPE_ALLOCATED\n", page_type.n_fil_page_type_allocated);
	printf("%d\tFIL_PAGE_IBUF_BITMAP\n", page_type.n_fil_page_ibuf_bitmap);
	printf("%d\tFIL_PAGE_TYPE_SYS\n", page_type.n_fil_page_type_sys);
	printf("%d\tFIL_PAGE_TYPE_TRX_SYS\n", page_type.n_fil_page_type_trx_sys);
	printf("%d\tFIL_PAGE_TYPE_FSP_HDR\n",page_type.n_fil_page_type_fsp_hdr);
	printf("%d\tFIL_PAGE_TYPE_XDES\n", page_type.n_fil_page_type_xdes);
	printf("%d\tFIL_PAGE_TYPE_BLOB\n", page_type.n_fil_page_type_blob);
	printf("%d\tFIL_PAGE_TYPE_ZBLOB\n", page_type.n_fil_page_type_zblob);
	printf("%d\tother\n", page_type.n_fil_page_type_other);
	printf("undo type: %d insert, %d update, %d other\n",
		page_type.n_undo_insert, page_type.n_undo_update,
		page_type.n_undo_other);
	printf("undo state: %d active, %d cached, %d to_free, %d"
		"to_purge, %d prepared, %d other\n",
		page_type.n_undo_state_active,
		page_type.n_undo_state_cached,
		page_type.n_undo_state_to_free,
		page_type.n_undo_state_to_purge,
		page_type.n_undo_state_prepared,
		page_type.n_undo_state_other);
}

/* command line argument to do page checks (that's it) */
/* another argument to specify page ranges... seek to right spot and go from there */
static struct my_option innochecksum_options[] = {
  {"help", '?', "Displays this help and exits.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"info", 'I', "Synonym for --help.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Displays version information and exits.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Verbose (prints progress every 5 seconds).",
    &verbose, &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", 'd', "Output debug log.", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 'c', "Print the count of pages in the file.",
    &just_count, &just_count, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"start_page", 's', "Start on this page number (0 based).",
    &start_page, &start_page, 0, GET_ULONG, REQUIRED_ARG,
    0, 0, (longlong) 2L*1024L*1024L*1024L, 0, 1, 0},
  {"end_page", 'e', "End at this page number (0 based).",
    &end_page, &end_page, 0, GET_ULONG, REQUIRED_ARG,
    0, 0, (longlong) 2L*1024L*1024L*1024L, 0, 1, 0},
  {"page", 'p', "Check only this page (0 based).",
    &do_page, &do_page, 0, GET_ULONG, REQUIRED_ARG,
    0, 0, (longlong) 2L*1024L*1024L*1024L, 0, 1, 0},
  {"strict-check", 'C', "Specify the strict checksum algorithm by the user.",
    &strict_check, &strict_check, &innochecksum_algorithms_typelib,
    GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-check", 'n', "Ignore the checksum verification.",
    &no_check, &no_check, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"allow-mismatches", 'a', "Maximum checksum mismatch allowed.",
    &allow_mismatches, &allow_mismatches, 0,
    GET_ULONG, REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1, 0},
  {"write", 'w', "Rewrite the checksum algorithm by the user.",
    &write_check, &write_check, &innochecksum_algorithms_typelib,
    GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"page-type-summary", 'S', "Display the all different page type sum count "
   "of a tablespace.", &page_type_summary, &page_type_summary, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"page-type-dump", 'D', "Dump the all different page type of a tablespace.",
   &dump_filename,&dump_filename, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

/* Print out the Innodb version and machine information. */
static
void
print_version(
void) {
	printf("%s Ver %s, for %s (%s)\n",
		my_progname, INNODB_VERSION_STR,
		SYSTEM_TYPE, MACHINE_TYPE);
}

static void usage(void) {
	print_version();
	puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
	printf("InnoDB offline file checksum utility.\n");
	printf("Usage: %s [-c] [-s <start page>] [-e <end page>] [-p <page>] [-v] [-d] <filename>\n", my_progname);
	my_print_help(innochecksum_options);
	my_print_variables(innochecksum_options);
}

extern "C" my_bool
innochecksum_get_one_option(
	int			optid,
	const struct my_option	*opt __attribute__((unused)),
	char			*argument __attribute__((unused))) {

	switch (optid) {
		case 'd':
			DBUG_PUSH(argument ? argument : default_dbug_option);
			debug = 1;
			break;
		case 'e':
			use_end_page = 1;
			break;
		case 'p':
			end_page = start_page = do_page;
			use_end_page = 1;
			do_one_page = 1;
			break;
		case 'V':
			print_version();
			exit(0);
			break;
		case 'C':
			strict_verify = 1;
			switch (strict_check) {

				case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
				case SRV_CHECKSUM_ALGORITHM_CRC32:
					srv_checksum_algorithm =
						SRV_CHECKSUM_ALGORITHM_STRICT_CRC32;
					break;

				case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
				case SRV_CHECKSUM_ALGORITHM_INNODB:
					srv_checksum_algorithm =
						SRV_CHECKSUM_ALGORITHM_STRICT_INNODB;
				break;

				case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
				case SRV_CHECKSUM_ALGORITHM_NONE:
					srv_checksum_algorithm =
						SRV_CHECKSUM_ALGORITHM_STRICT_NONE;
					break;
				default :
					return 1;
			}
			break;
		case 'n':
			no_check = 1;
			break;
		case 'a':
		case 'S':
			break;
		case 'w':
			do_write = 1;
			break;
		case 'D':
			page_type_dump = 1;
			break;
		case 'I':
		case '?':
			usage();
			exit(0);
			break;
	}

	return 0;
}

static
int
get_options(
	int	*argc,
	char	***argv) {

	int ho_error;

	if ((ho_error=handle_options(argc, argv, innochecksum_options,
		innochecksum_get_one_option)))
		exit(1);

	/* The next arg must be the filename */
	if (!*argc) {
		usage();
		return 1;
	}

	return 0;
}

int
main(
	int	argc,
	char	**argv) {

	FILE*		f;		/* our input file. */
	char*		filename;	/* our input filename. */

	/* Buffer to store pages read. */
	uchar		buf[UNIV_PAGE_SIZE_MAX];
	/* Buffer the minimum page size for first page. */
	uchar		min_page[UNIV_PAGE_SIZE_MIN];
	/* Buffer the remaining portion of first page. */
	uchar*		page;
	
	ulong		bytes;		/* bytes read count */
	time_t		now;		/* current time */
	time_t		lastt;		/* last time */
	struct stat	st;		/* for stat, if you couldn't guess */
	unsigned long long int	size = 0;/* size of file (has to be 64 bits) */
	ulint		pages;		/* number of pages in file */
	off_t		offset = 0;
	int		fd;
	my_bool		compressed = 0;
	ulint		mismatch_count =0;
	static ulong	physical_page_size;	/* Page size in bytes on disk. */
	static ulong	logical_page_size;	/* Page size when uncompressed. */
	bool		iscorrupted = 0;
	bool		flag = 0;
	ulong		min_bytes;
	bool		read_from_stdin = 0;
	FILE*		pagedump;
	struct flock	lk;			/* advisory lock. */
	fpos_t pos;

	ut_crc32_init();
	MY_INIT(argv[0]);

	DBUG_ENTER("main");
	DBUG_PROCESS(argv[0]);
	DBUG_PRINT("info",("InnoDB File Checksum Utility."));


	if (get_options(&argc,&argv))
		DBUG_RETURN(1);

	if (strict_verify && no_check) {
		fprintf(stderr, "Error: --strict-check option cannot be used "
			"together with --no-check option.");
		DBUG_RETURN(1);
	}

	if (no_check && !do_write) {
		fprintf(stderr, "Error: --no-check must be associated with "
			"--write option");
		DBUG_RETURN(1);
	}

	if (page_type_dump)
		pagedump = fopen(dump_filename,"wb");

	if (verbose)
		my_print_variables(innochecksum_options);

	/* The file name is not optional */
	for (int i=0; i < argc;++i) {
		/* Initialize the parameters */
		filename = argv[i];
		memset(&page_type, 0, sizeof(innodb_page_type));
		iscorrupted = 0;
		flag = 0;

		DBUG_PRINT("info", ("Filename = %s",filename));
		if (*filename == '\0') {
			fprintf(stderr, "Error: File name missing\n");

			DBUG_RETURN(1);
		}
		if (*filename == '-') {
			f = stdin;
			read_from_stdin = 1;
		}

		/* stat the file to get size and page count */
		if (!read_from_stdin && stat(filename, &st)) {
			fprintf(stderr, "Error: %s cannot be found\n",
				filename);

			DBUG_RETURN(1);
		}

		if (!read_from_stdin) {
			size = st.st_size;
			if (do_write)
				f = fopen(filename, "rb+");
			else
				f = fopen(filename, "rb");
			if (f == NULL) {
				fprintf(stderr, "Error: %s cannot be opened",
					filename);
				perror(" ");

				DBUG_RETURN(1);
			}
			fgetpos(f,&pos);
		}

		min_bytes = fread(min_page, 1, UNIV_PAGE_SIZE_MIN, f);
		flag = 1;
		if (min_bytes != UNIV_PAGE_SIZE_MIN) {
			fprintf(stderr, "Error: Was not able to read the "
				"minimum page size ");
			fprintf(stderr, "of %d bytes.  Bytes read was %lu\n",
				UNIV_PAGE_SIZE_MIN, min_bytes);

			DBUG_RETURN(1);
		}

		get_page_size(min_page, &logical_page_size,
			      &physical_page_size, &compressed);

		pages= (ulint) (size / physical_page_size);

		if (just_count && !read_from_stdin) {
			DBUG_PRINT("info",("Number of pages:%lu", pages));
			continue;
		} else if (verbose && !read_from_stdin) {
			DBUG_PRINT("info",("file %s = %llu bytes (%lu pages)",
				   filename, size, pages));
			if (do_one_page)
				DBUG_PRINT("info" ,("InnoChecksum; checking "
				"page %lu", do_page));
			else
				DBUG_PRINT("info",("InnoChecksum; checking "
					   "pages in range %lu to %lu",
					   start_page, use_end_page ?
					   end_page : (pages - 1)));
		}

		if(!read_from_stdin) {
			fd = fileno(f);
			if (!fd) {
				perror("Error: Unable to obtain file "
					"descriptor number");

				DBUG_RETURN(1);
			}

			if(do_write)
				lk.l_type = F_WRLCK;
			else
				lk.l_type = F_RDLCK;
			lk.l_whence = SEEK_SET;
			lk.l_start = lk.l_len = 0;
			if (fcntl(fd, F_SETLK, &lk) == -1) {
				perror("fcntl");

				DBUG_RETURN(1);
			}
		}

		/* seek to the necessary position */
		if (start_page) {
			if(!read_from_stdin) {
				flag = 0;

				offset = (off_t)start_page * (off_t)physical_page_size;
				if (fseeko(f, offset, SEEK_SET)) {
					perror("Error: Unable to seek to "
						"necessary offset");

					DBUG_RETURN(1);
				}
				fgetpos(f, &pos);
			} else {

				ulong count=0;

				while (!feof(f)) {
					if(start_page==count)
						break;
					if (flag) {
						flag=0;
						page = (unsigned char*)malloc(
							sizeof(unsigned char) * (physical_page_size - UNIV_PAGE_SIZE_MIN));
						bytes= fread(page, 1, physical_page_size - UNIV_PAGE_SIZE_MIN, f);
						memcpy(buf, min_page, UNIV_PAGE_SIZE_MIN);
						memcpy(buf + UNIV_PAGE_SIZE_MIN, page, physical_page_size - UNIV_PAGE_SIZE_MIN);
						 bytes +=min_bytes;
					} else
						bytes= fread(buf, 1, physical_page_size, f);
					count++;
					if (!bytes || feof(f)) {
						fprintf(stderr,"Error: Unable "
							"to seek to necessary offset");

						DBUG_RETURN(1);
					}
				}
			}
		}

		if(page_type_dump) {
			fprintf(pagedump,"\n\nFilename::%s\n",filename);
			fprintf(pagedump,"===================================="
				"==========================================\n");
			fprintf(pagedump, "\tPAGE_NO\t\t|\t\tPAGE_TYPE\t\t\t|\tEXTRA INFO\n");
			fprintf(pagedump,"===================================="
				"==========================================\n");

		}

		/* main checksumming loop */
		ct = start_page;
		lastt = 0;
		while (!feof(f)) {
			if (flag) {
				flag=0;
				page=(unsigned char*)malloc(sizeof(unsigned char) * (physical_page_size - UNIV_PAGE_SIZE_MIN));
				bytes= fread(page, 1, physical_page_size - UNIV_PAGE_SIZE_MIN, f);
				memcpy(buf, min_page, UNIV_PAGE_SIZE_MIN);
				memcpy(buf + UNIV_PAGE_SIZE_MIN, page, physical_page_size - UNIV_PAGE_SIZE_MIN);
				bytes +=min_bytes;
			}
			else
				bytes= fread(buf, 1, physical_page_size, f);

			if (!bytes && feof(f))
				break;

			if (ferror(f)) {
				fprintf(stderr, "Error reading %lu bytes",
					physical_page_size);
				perror(" ");

				DBUG_RETURN(1);
			}

			if (bytes != physical_page_size) {
				fprintf(stderr, "Error: bytes read (%lu) "
					"doesn't match page size (%lu)\n",
					bytes, physical_page_size);
				DBUG_RETURN(1);
			}

			if (!no_check) {
				/* Checksum verification */
				if (compressed)
					iscorrupted = buf_page_is_corrupted(true,buf,physical_page_size);
				else
					iscorrupted = buf_page_is_corrupted(true,buf,0);

				if (iscorrupted) {
					fprintf(stderr, "Fail: page %lu invalid"
						"\n", ct);
					mismatch_count++;
					if(mismatch_count>allow_mismatches) {
						fprintf(stderr, "Exceeded the "
							"maximum checksum "
							"mismatched\n");

							DBUG_RETURN(1);
						}
				}
			}

			/* Rewrite checksum */
			if (do_write) {

				if(read_from_stdin) {
					update_checksum(buf, physical_page_size, compressed);
					fwrite(buf, physical_page_size, 1, stdout);
				} else {
					if(update_checksum(buf, physical_page_size, compressed)) {
						fsetpos(f, &pos);
						fwrite(buf, physical_page_size, 1, f);
						fgetpos(f, &pos);
					}
				}
			}

			/* end if this was the last page we were supposed to check */
			if (use_end_page && (ct >= end_page))
				break;

			if(page_type_summary || page_type_dump)
				parse_page(buf,pagedump);

			/* do counter increase and progress printing */
			ct++;
			if (verbose) {
				if (ct % 64 == 0) {
					now= time(0);
					if (!lastt) lastt= now;
					if (now - lastt >= 1) {
						DBUG_PRINT("info",("page %lu "
							"okay: %.3f%% done",
							(ct - 1), (float) ct / pages * 100));
						lastt= now;
					}
				}
			}
		}

		if(!read_from_stdin) {
			/* Remove the lock. */
			lk.l_type = F_UNLCK;
			if (fcntl(fd, F_SETLK, &lk) == -1) {
				perror("fcntl");
				DBUG_RETURN(1);
			}
			fclose(f);
		}

		if (page_type_summary)
			print_summary();
	}

	DBUG_RETURN(0);
}
