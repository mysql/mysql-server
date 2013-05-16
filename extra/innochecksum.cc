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
#ifndef _WIN32
# include <unistd.h>
#endif
#include <my_getopt.h>
#include <m_string.h>
#include <welcome_copyright_notice.h>	/* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/* Only parts of these files are included from the InnoDB codebase.
The parts not included are excluded by #ifndef UNIV_INNOCHECKSUM. */

#include "univ.i"			/* include all of this */
#include "page0zip.h"			/* page_zip_calc_checksum() */
#include "page0page.h"			/* PAGE_* */
#include "trx0undo.h"			/* TRX_UNDO_* */
#include "fut0lst.h"			/* FLST_NODE_SIZE */
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
static bool			verbose;
static bool			just_count;
static ullint			start_page;
static ullint			end_page;
static ullint			do_page;
static bool			use_end_page;
static bool			do_one_page;
/* replaces declaration in srv0srv.c */
ulong				srv_page_size;
extern ulong			srv_checksum_algorithm;
/* Current page number (0 based). */
ullint				cur_page_num;
/* Skip the checksum verification. */
static bool			no_check;
/* Enabled for strict checksum verification. */
bool				strict_verify = 0;
/* Enabled for rewrite checksum. */
static bool			do_write;
/* Mismatches count allowed (0 by default). */
static ullint			allow_mismatches;
static bool			page_type_summary;
static bool			page_type_dump;
/* Store filename for page-type-dump option. */
char*				page_dump_filename = 0;
/* skip the checksum verification & rewrite if page is doublewrite buffer. */
static bool			skip_page = 0;
const char			*dbug_setting = "FALSE";

#ifndef _WIN32
/* advisory lock for non-window system. */
struct flock			lk;
#endif /* _WIN32 */

/* Strict check algorithm name. */
static ulong			strict_check;
/* Rewrite checksum algorithm name. */
static ulong			write_check;

/* Innodb page type. */
struct innodb_page_type {
	int n_undo_state_active;
	int n_undo_state_cached;
	int n_undo_state_to_free;
	int n_undo_state_to_purge;
	int n_undo_state_prepared;
	int n_undo_state_other;
	int n_undo_insert;
	int n_undo_update;
	int n_undo_other;
	int n_fil_page_index;
	int n_fil_page_undo_log;
	int n_fil_page_inode;
	int n_fil_page_ibuf_free_list;
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
 @param		[in] buf			buffer used to read the page.
 @param		[out] logical_page_size		Logical/Uncompressed page size.
 @param		[out] physical_page_size	Physical/Commpressed page size.
 @param		[out] compressed		Enable if tablespace is
						compressed.
*/
static
void
get_page_size(
	byte*		buf,
	ulong*		logical_page_size,
	ulong*		physical_page_size,
	bool*		compressed)
{
	ulong flags;

	flags = mach_read_from_4(buf + FIL_PAGE_DATA + FSP_SPACE_FLAGS);

	/* srv_page_size is used by InnoDB code as UNIV_PAGE_SIZE */
	srv_page_size = *logical_page_size = fsp_flags_get_page_size(flags);

	/* fsp_flags_get_zip_size() will return zero if not compressed. */
	*physical_page_size = fsp_flags_get_zip_size(flags);

	if (*physical_page_size == 0) {
		/* uncompressed page. */
		*physical_page_size= *logical_page_size;
		DBUG_ASSERT(*physical_page_size >= UNIV_PAGE_SIZE_MIN);
		*compressed = FALSE;
	} else {
		*compressed = TRUE;
	}

}

#ifdef _WIN32
/***********************************************//*
 @param		[in] error	error no. from the getLastError().

 @retval error message corresponding to error no.
*/
static
char*
error_message(
	int	error)
{
	static char err_msg[1024] = {'\0'};
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)err_msg, sizeof(err_msg), NULL );

	return (err_msg);
}
#endif /* _WIN32 */

/***********************************************//*
 @param>>_______[in] name>_____name of file.
 @retval file pointer; file pointer is NULL when error occured.
*/

FILE*
open_file(
	const char*	name)
{
	int	fd;		/* file descriptor. */
	FILE*	fil_in;
#ifdef _WIN32
	HANDLE		hFile;		/* handle to open file. */
	DWORD		access;		/* define access control */
	int		flags = 0;	/* define the mode for file
					descriptor */

	if (do_write) {
		access =  GENERIC_READ | GENERIC_WRITE;
		flags =  _O_RDWR | _O_BINARY;
	} else {
		access = GENERIC_READ;
		flags = _O_RDONLY | _O_BINARY;
	}
	/* CreateFile() also provide advisory lock with the usage of
	access and share mode of the file.*/
	hFile = CreateFile(
			(LPCTSTR) name, access, 0L, NULL,
			OPEN_EXISTING, NULL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		/* print the error message. */
		fprintf(stderr, "Filename::%s %s\n",
			error_message(GetLastError()));

			return (NULL);
		}

	/* get the file descriptor. */
	fd= _open_osfhandle((intptr_t)hFile, flags);
#else /* _WIN32 */

	int	create_flag;
	/* define the advisory lock and open file mode. */
	if (do_write) {
		create_flag = O_RDWR;
		lk.l_type = F_WRLCK;
	}
	else {
		create_flag = O_RDONLY;
		lk.l_type = F_RDLCK;
	}

	fd = open(name, create_flag);

	lk.l_whence = SEEK_SET;
	lk.l_start = lk.l_len = 0;

	if (fcntl(fd, F_SETLK, &lk) == -1) {
		fprintf(stderr, "Error: Unable to lock file::"
			" %s\n", name);
		perror("fcntl");
		return (NULL);
	}
#endif /* _WIN32 */

	if (do_write) {
		fil_in = fdopen(fd, "rb+");
	} else {
		fil_in = fdopen(fd, "rb");
	}

	return (fil_in);
}

/************************************************************//*
 Read the content of file

 @param  [in,out]	buf			read the file in buffer
 @param  [in]		partial_page_read	enable when to read the
						remaining buffer for first page.
 @param  [in]		physical_page_size	Physical/Commpressed page size.
 @param  [in,out]	fil_in			file pointer created for the
						tablespace.
 @retval no. of bytes read.
*/
ulong read_file(
	byte*	buf,
	bool	partial_page_read,
	ulong	physical_page_size,
	FILE*	fil_in)
{
	ulong bytes = 0;

	DBUG_ASSERT(physical_page_size >= UNIV_ZIP_SIZE_MIN);

	if (partial_page_read) {
		buf += UNIV_ZIP_SIZE_MIN;
		physical_page_size -= UNIV_ZIP_SIZE_MIN;
		bytes = UNIV_ZIP_SIZE_MIN;
	}

	return bytes + fread(buf, 1, physical_page_size, fil_in);
}

/*****************************************************************//*
 Check if page is corrupted or not.

 @param [in/out]	buf			page buffer read
 @param [in]		compressed		enable when tablespace is compressed.
 @param	[in]		logical_page_size	Logical/Uncompressed page size.
 @param	[in]		physical_page_size	Physical/Commpressed page size.

 @retval true if page is corrupted otherwise false.
*/
static
bool
is_page_corrupted(
	const	byte* buf,
	bool	compressed,
	ulong	logical_page_size,
	ulong	physical_page_size) {

	/* enable if page is corrupted. */
	 bool is_corrupted;
	/* use to store LSN values. */
	 ulint logseq;
	 ulint logseqfield;

	if (!compressed) {
		/* check the stored log sequence numbers
		 for uncompressed tablespace. */
		logseq = mach_read_from_4(buf + FIL_PAGE_LSN + 4);
		logseqfield = mach_read_from_4(
				buf + logical_page_size -
				FIL_PAGE_END_LSN_OLD_CHKSUM + 4);

		DBUG_PRINT("info", ("page::%llu;"
			   " log sequence number:first = %lu; "
			   "second = %lu",
			   cur_page_num, logseq, logseqfield));

		if (logseq != logseqfield) {
			DBUG_PRINT("info", ("Fail; page %llu invalid "
				   "(fails log sequence number check)",
				   cur_page_num));
		}

		is_corrupted = buf_page_is_corrupted(true, buf, 0,
						     cur_page_num,
						     strict_verify);
	} else {
		is_corrupted = buf_page_is_corrupted(true, buf,
						     physical_page_size,
						     cur_page_num,
						     strict_verify);
	}

	return (is_corrupted);
}

/********************************************//*
 Check if page is doublewrite buffer or not.
 @param [in] page	buffer page

 @retval true  if page is doublewrite buffer otherwise false.
*/
static
bool
is_page_doublewritebuffer(
	const byte*	page)
{
	if ((cur_page_num >= FSP_EXTENT_SIZE)
		&& (cur_page_num < FSP_EXTENT_SIZE * 3)) {
		/* page is doublewrite buffer. */
		return (true);
	}

	return (false);
}

/*******************************************************//*
Check if page is empty or not.
 @param		[in] page		page to checked for empty.
 @param		[in] len	size of page.

 @retval TRUE if page is empty.
 @retval FALSE if page is not empty.
*/
static
bool
is_page_empty(
	const byte*	page,
	size_t		len)
{
	while (len--) {
		if (*page++) {
			return (FALSE);
		}
        }
        return (TRUE);
}

/********************************************************************//**
Rewrite the checksum for the page.
@param	[in/out] page			page buffer
@param	[in] physical_page_size		page size in bytes on disk.
@param	[in] iscompressed		Is compressed/Uncompressed Page.

@retval TRUE  : do rewrite
@retval FALSE : skip the rewrite as checksum stored match with
		calculated or page is doublwrite buffer.
*/

bool
update_checksum(
	byte*	page,
	ulong	physical_page_size,
	bool	iscompressed)
{
	ib_uint32_t	checksum = 0;
	byte		stored1[4];	/* get FIL_PAGE_SPACE_OR_CHKSUM field checksum */
	byte		stored2[4];	/* get FIL_PAGE_END_LSN_OLD_CHKSUM field checksum */

	ut_ad(page);
	/* If page is doublewrite buffer, skip the rewrite of checksum. */
	if (skip_page) {
		return (FALSE);
	}

	memcpy(stored1, page + FIL_PAGE_SPACE_OR_CHKSUM, 4);
	memcpy(stored2, page + physical_page_size -
	       FIL_PAGE_END_LSN_OLD_CHKSUM, 4);

	/* Check if page is empty, exclude the checksum field */
	if (is_page_empty(page + 4, physical_page_size - 12)
	    && is_page_empty(page + physical_page_size - 4, 4)) {

		memset(page + FIL_PAGE_SPACE_OR_CHKSUM, 0, 4);
		memset(page + physical_page_size -
		       FIL_PAGE_END_LSN_OLD_CHKSUM, 0, 4);

		goto func_exit;
	}

	if (iscompressed) {
		/* page is compressed */
		checksum = page_zip_calc_checksum(page, physical_page_size,
						  static_cast<srv_checksum_algorithm_t>(write_check));

		mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
		DBUG_PRINT("info", ("page %llu: Updated checksum = %u;\n",
			   cur_page_num, checksum));
	} else {
		/* page is uncompressed. */

		/* Store the new formula checksum */
		switch ((srv_checksum_algorithm_t) write_check) {

		case SRV_CHECKSUM_ALGORITHM_CRC32:
		case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
			checksum = buf_calc_page_crc32(page);
			break;

		case SRV_CHECKSUM_ALGORITHM_INNODB:
		case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
			checksum = (ib_uint32_t)
					buf_calc_page_new_checksum(page);
			break;

		case SRV_CHECKSUM_ALGORITHM_NONE:
		case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
			checksum = BUF_NO_CHECKSUM_MAGIC;
			break;
		/* no default so the compiler will emit a warning if new
		enum is added and not handled here */
		}

		mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);

		DBUG_PRINT("info", ("page %llu: Updated checksum field1"
			   " = %u;", cur_page_num, checksum));

		if (write_check == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB
		    || write_check == SRV_CHECKSUM_ALGORITHM_INNODB) {
			checksum = (ib_uint32_t)
					buf_calc_page_old_checksum(page);
		}

		mach_write_to_4(page + physical_page_size -
				FIL_PAGE_END_LSN_OLD_CHKSUM,checksum);

		DBUG_PRINT("info", ("page %llu: Updated checksum field2"
			   " = %u;", cur_page_num, checksum));

	}

	func_exit:
	/* The following code is to check the stored checksum with the
	calculated checksum. If it matches, then return FALSE to skip
	the rewrite of checksum, otherwise return TRUE. */
	if (iscompressed) {
		if (!memcmp(stored1, page + FIL_PAGE_SPACE_OR_CHKSUM, 4)) {
			return (FALSE);
		}
		return (TRUE);
	}

	if (!memcmp(stored1, page + FIL_PAGE_SPACE_OR_CHKSUM, 4)
	    && !memcmp(stored2, page + physical_page_size -
		       FIL_PAGE_END_LSN_OLD_CHKSUM, 4)) {
		return (FALSE);

	}

	return (TRUE);
}

/*
Parse the page and collect/dump the information about page type
@param [in] page	buffer page
@param [in] file	file for diagnosis.
*/
void
parse_page(
	const byte*	page,
	FILE*		file)
{
	unsigned long long id;
	ulint undo_page_type;
	char str[20]={'\0'};

	/* Check whether page is doublewrite buffer. */
	if(skip_page) {
		strcpy(str, "Double_write_buffer");
	} else {
		strcpy(str, "-");
	}

	switch (mach_read_from_2(page + FIL_PAGE_TYPE)) {

	case FIL_PAGE_INDEX:
		page_type.n_fil_page_index++;
		id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tIndex page\t\t\t|"
				"\tindex id=%llu,",cur_page_num, (ullint)id);

			fprintf(file,
				" page level=%lu, No. of records=%lu,"
				" garbage=%lu, %s\n",
				page_header_get_field(page, PAGE_LEVEL),
				page_header_get_field(page, PAGE_N_RECS),
				page_header_get_field(page, PAGE_GARBAGE), str);
		}
		break;

	case FIL_PAGE_UNDO_LOG:
		page_type.n_fil_page_undo_log++;
		undo_page_type = mach_read_from_2(page +
				     TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE);
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tUndo log page\t\t\t|",
				cur_page_num);
		}
		if (undo_page_type == TRX_UNDO_INSERT) {
			page_type.n_undo_insert++;
			if (page_type_dump) {
				fprintf(file, "\t%s",
					"Insert Undo log page");
			}

		} else if (undo_page_type == TRX_UNDO_UPDATE) {
			page_type.n_undo_update++;
			if (page_type_dump) {
				fprintf(file, "\t%s",
					"Update undo log page");
			}
		}

		undo_page_type = mach_read_from_2(page + TRX_UNDO_SEG_HDR +
						  TRX_UNDO_STATE);
		switch (undo_page_type) {
			case TRX_UNDO_ACTIVE:
				page_type.n_undo_state_active++;
				if (page_type_dump) {
					fprintf(file, ", %s", "Undo log of "
						"an active transaction");
				}
				break;

			case TRX_UNDO_CACHED:
				page_type.n_undo_state_cached++;
				if (page_type_dump) {
					fprintf(file, ", %s", "Page is "
						"cached for quick reuse");
				}
				break;

			case TRX_UNDO_TO_FREE:
				page_type.n_undo_state_to_free++;
				if (page_type_dump) {
					fprintf(file, ", %s", "Insert undo "
						"segment that can be freed");
				}
				break;

			case TRX_UNDO_TO_PURGE:
				page_type.n_undo_state_to_purge++;
				if (page_type_dump) {
					fprintf(file, ", %s", "Will be "
						"freed in purge when all undo"
					"data in it is removed");
				}
				break;

			case TRX_UNDO_PREPARED:
				page_type.n_undo_state_prepared++;
				if (page_type_dump) {
					fprintf(file, ", %s", "Undo log of "
						"an prepared transaction");
				}
				break;

			default:
				page_type.n_undo_state_other++;
				break;
		}
		if(page_type_dump) {
			fprintf(file, ", %s\n", str);
		}
		break;

	case FIL_PAGE_INODE:
		page_type.n_fil_page_inode++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tInode page\t\t\t|"
				"\t%s\n",cur_page_num, str);
		}
		break;

	case FIL_PAGE_IBUF_FREE_LIST:
		page_type.n_fil_page_ibuf_free_list++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tInsert buffer free list"
				" page\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_ALLOCATED:
		page_type.n_fil_page_type_allocated++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tFreshly allocated "
				"page\t\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_IBUF_BITMAP:
		page_type.n_fil_page_ibuf_bitmap++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tInsert Buffer "
				"Bitmap\t\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_SYS:
		page_type.n_fil_page_type_sys++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tSystem page\t\t\t|"
				"\t%s\n",cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_TRX_SYS:
		page_type.n_fil_page_type_trx_sys++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tTransaction system "
				"page\t\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_FSP_HDR:
		page_type.n_fil_page_type_fsp_hdr++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tFile Space "
				"Header\t\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_XDES:
		page_type.n_fil_page_type_xdes++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tExtent descriptor "
				"page\t\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_BLOB:
		page_type.n_fil_page_type_blob++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tBLOB page\t\t\t|\t%s\n",
				cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_ZBLOB:
		page_type.n_fil_page_type_zblob++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tCompressed BLOB "
				"page\t\t|\t%s\n", cur_page_num, str);
		}
		break;

	case FIL_PAGE_TYPE_ZBLOB2:
		page_type.n_fil_page_type_zblob2++;
		if (page_type_dump) {
			fprintf(file, "#::%8llu\t\t|\t\tSubsequent Compressed "
				"BLOB page\t|\t%s\n", cur_page_num, str);
		}
			break;

	default:
		page_type.n_fil_page_type_other++;
		break;
	}
}

/*
 Print the page type count of a tablespace.
 @param [in] fil_out	stream where the output goes.
*/
void
print_summary(
	FILE*	fil_out)
{
	fprintf(fil_out, "\n================PAGE TYPE SUMMARY==============\n");
	fprintf(fil_out, "#PAGE_COUNT\tPAGE_TYPE");
	fprintf(fil_out, "\n===============================================\n");
	fprintf(fil_out, "%8d\tIndex page\n",
		page_type.n_fil_page_index);
	fprintf(fil_out, "%8d\tUndo log page\n",
		page_type.n_fil_page_undo_log);
	fprintf(fil_out, "%8d\tInode page\n",
		page_type.n_fil_page_inode);
	fprintf(fil_out, "%8d\tInsert buffer free list page\n",
		page_type.n_fil_page_ibuf_free_list);
	fprintf(fil_out, "%8d\tFreshly allocated page\n",
		page_type.n_fil_page_type_allocated);
	fprintf(fil_out, "%8d\tInsert buffer bitmap\n",
		page_type.n_fil_page_ibuf_bitmap);
	fprintf(fil_out, "%8d\tSystem page\n",
		page_type.n_fil_page_type_sys);
	fprintf(fil_out, "%8d\tTransaction system page\n",
		page_type.n_fil_page_type_trx_sys);
	fprintf(fil_out, "%8d\tFile Space Header\n",
		page_type.n_fil_page_type_fsp_hdr);
	fprintf(fil_out, "%8d\tExtent descriptor page\n",
		page_type.n_fil_page_type_xdes);
	fprintf(fil_out, "%8d\tBLOB page\n",
		page_type.n_fil_page_type_blob);
	fprintf(fil_out, "%8d\tCompressed BLOB page\n",
		page_type.n_fil_page_type_zblob);
	fprintf(fil_out, "%8d\tOther type of page",
		page_type.n_fil_page_type_other);
	fprintf(fil_out, "\n===============================================\n");
	fprintf(fil_out, "Additional information:\n");
	fprintf(fil_out, "Undo page type: %d insert, %d update, %d other\n",
		page_type.n_undo_insert,
		page_type.n_undo_update,
		page_type.n_undo_other);
	fprintf(fil_out, "Undo page state: %d active, %d cached, %d to_free, %d"
		" to_purge, %d prepared, %d other\n",
		page_type.n_undo_state_active,
		page_type.n_undo_state_cached,
		page_type.n_undo_state_to_free,
		page_type.n_undo_state_to_purge,
		page_type.n_undo_state_prepared,
		page_type.n_undo_state_other);
}

/* command line argument for innochecksum tool. */
static struct my_option innochecksum_options[] = {
  {"help", '?', "Displays this help and exits.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"info", 'I', "Synonym for --help.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Displays version information and exits.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Verbose (prints progress every 5 seconds).",
    &verbose, &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log. See " REFMAN "dbug-package.html",
    &dbug_setting, &dbug_setting, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 'c', "Print the count of pages in the file and exits.",
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
  {"strict-check", 'C', "Specify the strict checksum algorithm by the user.",
    &strict_check, &strict_check, &innochecksum_algorithms_typelib,
    GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-check", 'n', "Ignore the checksum verification.",
    &no_check, &no_check, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"allow-mismatches", 'a', "Maximum checksum mismatch allowed.",
    &allow_mismatches, &allow_mismatches, 0,
    GET_ULL, REQUIRED_ARG, 0, 0, ULONGLONG_MAX, 0, 1, 0},
  {"write", 'w', "Rewrite the checksum algorithm by the user.",
    &write_check, &write_check, &innochecksum_algorithms_typelib,
    GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"page-type-summary", 'S', "Display a count of each page type "
   "in a tablespace.", &page_type_summary, &page_type_summary, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"page-type-dump", 'D', "Dump the page type info for each page in a "
   "tablespace.", &page_dump_filename, &page_dump_filename, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

/* Print out the Innodb version and machine information. */
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
	printf("Usage: %s [-c] [-s <start page>] [-e <end page>] "
		"[-p <page>] [-v]  [-a <allow mismatches>] [-n] "
		"[-C <strict-check>] [-w <write>] [-S] [-D <page type dump>] "
		"[-d <dbug-package name>] <filename or [-]>\n", my_progname);
	printf("See " REFMAN "innochecksum.html for usage hints.\n");
	my_print_help(innochecksum_options);
	my_print_variables(innochecksum_options);
}

extern "C" my_bool
innochecksum_get_one_option(
	int			optid,
	const struct my_option	*opt __attribute__((unused)),
	char			*argument __attribute__((unused)))
{
	switch (optid) {
	case '#':
		dbug_setting = argument
			? argument
			: IF_WIN("d:O,innochecksum.trace",
				 "d:o,/tmp/innochecksum.trace");
		DBUG_PUSH(dbug_setting);
		break;
	case 'e':
		use_end_page = TRUE;
		break;
	case 'p':
		end_page = start_page = do_page;
		use_end_page = TRUE;
		do_one_page = TRUE;
		break;
	case 'V':
		print_version();
		exit(EXIT_SUCCESS);
		break;
	case 'C':
		strict_verify = TRUE;
		switch ((srv_checksum_algorithm_t) strict_check) {

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
		default:
			return(TRUE);
		}
		break;
	case 'n':
		no_check = TRUE;
		break;
	case 'a':
	case 'S':
		break;
	case 'w':
		do_write = TRUE;
		break;
	case 'D':
		page_type_dump = TRUE;
		break;
	case 'I':
	case '?':
		usage();
		exit(EXIT_SUCCESS);
		break;
	}

	return(FALSE);
}

static
int
get_options(
	int	*argc,
	char	***argv)
{
	if (handle_options(argc, argv, innochecksum_options,
		innochecksum_get_one_option))
		exit(TRUE);

	/* The next arg must be the filename */
	if (!*argc) {
		usage();
		return (TRUE);
	}

	return (FALSE);
}

int main(
	int	argc,
	char	**argv)
{
	/* our input file. */
	FILE*		fil_in = NULL;
	/* our input filename. */
	char*		filename;
	/* Buffer to store pages read. */
	byte* buf = (uchar*) malloc(
			sizeof(uchar) * (UNIV_PAGE_SIZE_MAX));

	/* bytes read count */
	ulong		bytes;
	/* current time */
	time_t		now;
	/* last time */
	time_t		lastt;
	/* for stat, if you couldn't guess */
	struct stat	st;
	/* Page size in bytes on disk. */
	static ulong	physical_page_size;
	/* Page size when uncompressed. */
	static ulong	logical_page_size;

	/* size of file (has to be 64 bits) */
	unsigned long long int	size		= 0;
	/* number of pages in file */
	ulint		pages;

	off_t		offset			= 0;
	/* enable if tablespace is compressed. */
	bool		compressed		= FALSE;
	/* count the no. of page corrupted. */
	ulint		mismatch_count		= 0;
	/* Variable to ack the page is corrupted or not. */
	bool		is_corrupted		= FALSE;

	bool		partial_page_read	= FALSE;
	/* Enabled when read from stdin is done. */
	bool		read_from_stdin		= FALSE;
	FILE*		fil_page_type		= NULL;
	fpos_t		pos;

	/* Use to check the space id of given file. If space_id is zero,
	then check whether page is doublewrite buffer.*/
	ulint		space_id = 0UL;
	/* enable when space_id of given file is zero. */
	bool		is_system_tablespace = FALSE;

	ut_crc32_init();
	MY_INIT(argv[0]);

	DBUG_ENTER("main");
	DBUG_PROCESS(argv[0]);
	DBUG_PRINT("info", ("InnoDB File Checksum Utility."));

	if (get_options(&argc,&argv)) {
		DBUG_RETURN(1);
	}

	if (strict_verify && no_check) {
		fprintf(stderr, "Error: --strict-check option cannot be used "
			"together with --no-check option.\n");
		DBUG_RETURN(1);
	}

	if (no_check && !do_write) {
		fprintf(stderr, "Error: --no-check must be associated with "
			"--write option.\n");
		DBUG_RETURN(1);
	}

	if (page_type_dump) {
#ifndef _WIN32
		fil_page_type = fopen(page_dump_filename, "wb");
#else
	HANDLE		hFile;		/* handle to open file. */
	int fd = 0;
	hFile = CreateFile((LPCTSTR) page_dump_filename,
			  GENERIC_READ | GENERIC_WRITE,
			  FILE_SHARE_READ | FILE_SHARE_DELETE,
			  NULL, CREATE_NEW, NULL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		/* print the error message. */
		fprintf(stderr, "Filename::%s %s\n",
			page_dump_filename,
			error_message(GetLastError()));

			DBUG_RETURN (1);
		}

	/* get the file descriptor. */
	fd= _open_osfhandle((intptr_t)hFile, _O_RDWR | _O_BINARY);
	fil_page_type = fdopen(fd, "wb");
#endif /* _WIN32 */
	}

	if (verbose) {
		my_print_variables_ex(innochecksum_options, stderr);
	}

	/* The file name is not optional. */
	for (int i = 0; i < argc; ++i) {
		/* Reset parameters for each file. */
		filename = argv[i];
		memset(&page_type, 0, sizeof(innodb_page_type));
		is_corrupted = FALSE;
		partial_page_read = 0;
		skip_page = FALSE;

		DBUG_PRINT("info", ("Filename = %s", filename));
		if (*filename == '-') {
			/* read from stdin. */
			fil_in = stdin;
			read_from_stdin = 1;
		}

		/* stat the file to get size and page count. */
		if (!read_from_stdin && stat(filename, &st)) {
			fprintf(stderr, "Error: %s cannot be found\n",
				filename);

			DBUG_RETURN(1);
		}

		if (!read_from_stdin) {
			size = st.st_size;
			fil_in = open_file(filename);
			/*If fil_in is NULL, terminate as some error encountered */
			if(fil_in == NULL) {
				DBUG_RETURN(1);
			}
			/* Save the current file pointer in pos variable.*/
			fgetpos(fil_in, &pos);
		}

		/* Testing for lock mechanism. The innochecksum
		acquire lock on given file. So other tools accessing the same
		file for processsing must fail. */
#ifdef _WIN32
		DBUG_EXECUTE_IF("innochecksum_cause_mysqld_crash",
			ut_ad(page_dump_filename);
			while((_access( page_dump_filename, 0)) == 0) {
				sleep(1);
			}
			DBUG_RETURN(0); );
#else
		DBUG_EXECUTE_IF("innochecksum_cause_mysqld_crash",
			ut_ad(page_dump_filename);
			struct stat status_buf;
			while(stat(page_dump_filename, &status_buf) == 0) {
				sleep(1);
			}
			DBUG_RETURN(0); );
#endif /* _WIN32 */

		/* Read the minimum page size. */
		bytes = fread(buf, 1, UNIV_ZIP_SIZE_MIN, fil_in);
		partial_page_read = 1;

		if (bytes != UNIV_ZIP_SIZE_MIN) {
			fprintf(stderr, "Error: Was not able to read the "
				"minimum page size ");
			fprintf(stderr, "of %d bytes.  Bytes read was %lu\n",
				UNIV_ZIP_SIZE_MIN, bytes);

			DBUG_RETURN(1);
		}

		/* enable variable is_system_tablespace when space_id of given
		file is zero. Use to skip the checksum verification and rewrite
		for doublewrite pages. */
		is_system_tablespace = (!memcmp(&space_id, buf +
					FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 4))
					? true : false;

		get_page_size(buf, &logical_page_size,
			      &physical_page_size, &compressed);

		pages = (ulint)(size / physical_page_size);

		if (just_count) {
			DBUG_PRINT("info", ("Number of pages:%lu", pages));
			if(read_from_stdin) {
				fprintf(stderr, "Number of pages:%lu\n", pages);
			} else {
				printf("Number of pages:%lu\n", pages);
			}
			continue;
		} else if (verbose && !read_from_stdin) {
			DBUG_PRINT("info", ("file %s = %llu bytes (%lu pages)",
				   filename, size, pages));
			if (do_one_page) {
				DBUG_PRINT("info", ("InnoChecksum: checking "
					   "page %llu", do_page));
			} else {
				DBUG_PRINT("info", ("InnoChecksum: checking "
					   "pages in range %llu to %llu",
					   start_page, use_end_page ?
					   end_page : (pages - 1)));
			}
		}

		/* seek to the necessary position */
		if (start_page) {
			if (!read_from_stdin) {
				/* If read is not from stdin, we can use
				fseeko() to position the file pointer to
				the desired page. */
				partial_page_read = 0;

				offset = (off_t)start_page * (off_t)physical_page_size;
#ifdef _WIN32
				if (_fseeki64(fil_in, offset, SEEK_SET)) {
#else
				if (fseeko(fil_in, offset, SEEK_SET)) {
#endif /* _WIN32 */
					perror("Error: Unable to seek to "
						"necessary offset");

					DBUG_RETURN(1);
				}
				/* Save the current file pointer in
				pos variable. */
				fgetpos(fil_in, &pos);
			} else {

				ulong count = 0;

				while (!feof(fil_in)) {
					if (start_page == count) {
						break;
					}
					/* We read a part of page to find the
					minimum page size. We cannot reset
					the file pointer to the beginning of
					the page if we are reading from stdin
					(fseeko() on stdin doesn't work). So
					read only the remaining part of page,
					if partial_page_read is enable. */
					bytes = read_file(buf,
							  partial_page_read,
							  physical_page_size,
							  fil_in);

					partial_page_read = 0;
					count++;

					if (!bytes || feof(fil_in)) {
						fprintf(stderr, "Error: Unable "
							"to seek to necessary "
							"offset");

						DBUG_RETURN(1);
					}
				}
			}
		}

		if (page_type_dump) {
			fprintf(fil_page_type,
				"\n\nFilename::%s\n", filename);
			fprintf(fil_page_type,
				"========================================"
				"======================================\n");
			fprintf(fil_page_type,
				"\tPAGE_NO\t\t|\t\tPAGE_TYPE\t\t"
				"\t|\tEXTRA INFO\n");
			fprintf(fil_page_type,
				"========================================"
				"======================================\n");
		}

		/* main checksumming loop */
		cur_page_num = start_page;
		lastt = 0;
		while (!feof(fil_in)) {

			bytes = read_file(buf, partial_page_read,
					  physical_page_size, fil_in);
			partial_page_read = 0;

			if (!bytes && feof(fil_in)) {
				break;
			}

			if (ferror(fil_in)) {
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

			if (is_system_tablespace) {
				/* enable when page is double write buffer.*/
				skip_page = is_page_doublewritebuffer(buf);
			} else {
				skip_page = FALSE;
			}

			/* If no-check is enabled, skip the
			checksum verification.*/
			if (!no_check) {
				/* Checksum verification */
				if (!skip_page)
				{
					is_corrupted = is_page_corrupted(buf,
									compressed,
									logical_page_size,
									physical_page_size);

					if (is_corrupted) {
						fprintf(stderr, "Fail: page "
							"%llu invalid\n",
							cur_page_num);

						mismatch_count++;

						if(mismatch_count > allow_mismatches) {
							fprintf(stderr,
								"Exceeded the "
								"maximum allowed "
								"checksum mismatch "
								"count::%llu\n",
								allow_mismatches);

							DBUG_RETURN(1);
						}
					}
				}
			}

			/* Rewrite checksum */
			if (do_write) {

				if (read_from_stdin) {
					update_checksum(
						buf,
						physical_page_size, compressed);
					fwrite(buf,
					       physical_page_size, 1, stdout);
				} else {
					if (update_checksum(
						buf, physical_page_size,
						compressed)) {
						/* Set the previous file pointer
						saved in pos to current file position. */
						fsetpos(fil_in, &pos);
						fwrite(buf, physical_page_size,
							1, fil_in);
					}
					/* Save the current file pointer in pos
					variable */
					fflush(fil_in);
					fgetpos(fil_in, &pos);
				}
			}

			/* end if this was the last page we were supposed to check */
			if (use_end_page && (cur_page_num >= end_page)) {
				break;
			}

			if (page_type_summary || page_type_dump) {
				parse_page(buf, fil_page_type);
			}

			/* do counter increase and progress printing */
			cur_page_num++;
			if (verbose && !read_from_stdin) {
				if ((cur_page_num % 64) == 0) {
					now = time(0);
					if (!lastt) {
						lastt= now;
					}
					if (now - lastt >= 1) {
						DBUG_PRINT("info",("page %llu "
							   "okay: %.3f%% done",
							   (cur_page_num - 1),
							   (float) cur_page_num / pages * 100));
						lastt = now;
					}
				}
			}
		}

		if (!read_from_stdin) {
			/* flcose() will flush the data and release the lock if
			any acquired. */
			fclose(fil_in);
		}

		/* Enabled for page type summary. */
		if (page_type_summary) {
			if(!read_from_stdin) {
				fprintf(stdout, "\nFile::%s",filename);
				print_summary(stdout);
			} else {
				print_summary(stderr);
			}
		}
	}

	DBUG_RETURN(0);
}
