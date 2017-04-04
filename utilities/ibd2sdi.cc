/*****************************************************************************

Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <my_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifndef _WIN32
# include <unistd.h>
#endif
#include <m_string.h>
#include <my_getopt.h>
#include <welcome_copyright_notice.h>
#include <iostream>
#include <map>

#include "btr0cur.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "lob0lob.h"
#include "mach0data.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_macros.h"
#include "page0page.h"
#include "page0size.h"
#include "page0types.h"
#include "print_version.h"
#include "typelib.h"
#include "univ.i"
#include "ut0byte.h"
#include "ut0crc32.h"
#include "zlib.h"

typedef enum {
	SUCCESS = 0,
	FALIURE = 1,
	NO_RECORDS =2
} err_t;

/** Length of ID field in record of SDI Index. */
static const uint32_t	REC_DATA_ID_LEN = 8;

/** Length of TYPE field in record of SDI Index. */
static const uint32_t	REC_DATA_TYPE_LEN = 4;

/** SDI Index record Origin. */
static const uint32_t	REC_ORIGIN = 0;

/** Length of SDI Index record header. */
static const uint32_t	REC_MIN_HEADER_SIZE = REC_N_NEW_EXTRA_BYTES;

/** Stored at rec origin minus 3rd byte. Only 3bits of 3rd byte are used for
rec type. */
static const uint32_t	REC_OFF_TYPE = 3;

/** Stored at rec_origin minus 2nd byte and length 2 bytes. */
static const uint32_t	REC_OFF_NEXT = 2;

/** Offset of ID field in record (0). */
static const uint32_t	REC_OFF_DATA_ID = REC_ORIGIN;
//ut_ad(REC_OFF_DATA_ID == 0);

/** Offset of TYPE field in record (8). */
static const uint32_t	REC_OFF_DATA_TYPE =
	REC_OFF_DATA_ID + REC_DATA_ID_LEN;
//ut_ad(REC_OFF_DATA_TYPE == 8);

/** Offset of 6-byte trx id (12). */
static const uint32_t	REC_OFF_DATA_TRX_ID =
	REC_OFF_DATA_TYPE + REC_DATA_TYPE_LEN;
//ut_ad(REC_OFF_DATA_TRX_ID == 12);

/** 7-byte roll-ptr (18). */
static const uint32_t	REC_OFF_DATA_ROLL_PTR =
	REC_OFF_DATA_TRX_ID + DATA_TRX_ID_LEN;
//ut_ad(REC_OFF_DATA_ROLL_PTR == 18);

/** Variable length Data (25). */
static const uint32_t	REC_OFF_DATA_VARCHAR =
	REC_OFF_DATA_ROLL_PTR + DATA_ROLL_PTR_LEN;
//ut_ad(REC_OFF_DATA_VARCHAR == 25);

/** Record size in page. This will be used determine the maximum number
of records on a page. */
static const uint32_t	SDI_REC_SIZE =
	1 /* rec_len */
	+ REC_MIN_HEADER_SIZE /* rec_header */
	+ REC_DATA_ID_LEN /* id field len */
	+ REC_DATA_TYPE_LEN /* type field size */
	+ DATA_ROLL_PTR_LEN /* roll ptr len */
	+ DATA_TRX_ID_LEN /* TRX_ID len */;

/** If page 0 is corrupted, the maximum number of pages to scan to
determine page size. */
static const page_no_t	MAX_PAGES_TO_SCAN = 60;

/** Indicates error. */
static const uint64_t	IB_ERROR = SIZE_T_MAX;
static const uint32_t	IB_ERROR_32 = (~((uint32) 0));

/** SDI BLOB not expected before the following page number.
0 (tablespace header), 1 (tabespace bitmap), 2 (ibuf bitmap)
3 (SDI Index root page of copy 0), 4 (SDI Index root page of
copy 1). */
static const uint64_t	SDI_BLOB_ALLOWED = 5;

/** Replaces declaration in srv0srv.c */
ulong			srv_page_size;
ulong			srv_page_size_shift;
page_size_t		univ_page_size(0, 0, false);

/** Global options structure. Option values passed at command line are
stored in this structure */
struct sdi_options {
	uint32_t	copy_num;
	uint64_t	sdi_rec_id;
	uint64_t	sdi_rec_type;
	bool		skip_data;
	bool		is_read_from_copy;
	bool		is_sdi_id;
	bool		is_sdi_type;
	bool		is_sdi_rec;
	bool		no_checksum;
	bool		is_dump_file;
	const char*	dbug_setting;
	char*		dump_filename;
	ulong		strict_check;
};
struct sdi_options	opts;

/** Possible values for "--strict-check" for strictly verify checksum */
static const char *checksum_algorithms[] = {
	"crc32",
	"innodb",
	"none",
	NullS
};

/** Used to define an enumerate type of the "checksum algorithm". */
static TYPELIB checksum_algorithms_typelib = {
	array_elements(checksum_algorithms)-1,"",
	checksum_algorithms, NULL
};

/* Command line argument for ibd2sdi tool. */
static struct my_option ibd2sdi_options[] = {
  {"help", 'h', "Display this help and exit.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'v', "Display version information and exit.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log. See " REFMAN "dbug-package.html",
    &opts.dbug_setting, &opts.dbug_setting, 0, GET_STR, OPT_ARG,
    0, 0, 0, 0, 0, 0},
#endif /* !DBUG_OFF */
  {"dump-file", 'd', "Dump the tablespace SDI into the file passed by user."
    " Without the filename, it will default to stdout",
    &opts.dump_filename, &opts.dump_filename, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"read", 'r', "Read from this Copy of SDI in tablespace.",
    &opts.copy_num, &opts.copy_num, 0, GET_UINT, REQUIRED_ARG,
    0, 0, 1, 0, 1, 0},
  {"skip-data", 's', "Skip retrieving data from SDI records. Retrieve only"
    " id and type.", &opts.skip_data, &opts.skip_data, 0, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"id", 'i', "Retrieve the SDI record matching the id passed by user.",
    &opts.sdi_rec_id, &opts.sdi_rec_id, 0, GET_ULL, REQUIRED_ARG,
    0, 0, ULLONG_MAX, 0, 1, 0},
  {"type", 't', "Retrieve the SDI records matching the type passed by user.",
    &opts.sdi_rec_type, &opts.sdi_rec_type, 0, GET_ULL, REQUIRED_ARG,
    0, 0, ULLONG_MAX, 0, 1, 0},
  {"strict-check", 'c', "Specify the strict checksum algorithm by the user."
    " Allowed values are innodb, crc32, none." ,
    &opts.strict_check, &opts.strict_check, &checksum_algorithms_typelib,
    GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-check", 'n', "Ignore the checksum verification.",
    &opts.no_checksum, &opts.no_checksum, 0, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0},

  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

/** Report a failed assertion.
@param[in]	expr	the failed assertion if not NULL
@param[in]	file	source file containing the assertion
@param[in]	line	line number of the assertion */
void
ut_dbg_assertion_failed(
	const char*	expr,
	const char*	file,
	ulint		line)
{
	fprintf(stderr,
		"ibd2sdi: Assertion failure in file %s line " ULINTPF "\n",
		file, line);

	if (expr != NULL) {
		fprintf(stderr, "ibd2sdi: Failing assertion: %s\n", expr);
	}

	fflush(stderr);
	fflush(stdout);
	abort();
}

/** Create a file in a system's temporary directory.
@param[in]	temp_file_buf	Buffer to hold the temporary file
				name generated
@param[in]	prefix_pattern	the temp file name is prefixed with
				this string
@return File pointer of the created file */
static
FILE*
create_tmp_file(
	char*		temp_file_buf,
	const char*	prefix_pattern)
{
	FILE*	file = NULL;
	File	fd = create_temp_file(
		temp_file_buf, NullS, prefix_pattern,
		O_CREAT | O_RDWR , MYF(0));

	if (fd >= 0) {
		file = my_fdopen(fd, temp_file_buf, O_RDWR, MYF(0));
	}

	DBUG_EXECUTE_IF("ib_tmp_file_fail", file = NULL;);

	if (file == NULL) {
		ib::error() << "Unable to create temporary file; errno: " <<
			errno;

		if (fd >= 0) {
			my_close(fd, MYF(0));
		}
	}

	return(file);
}

/** Print the ibd2sdi tool usage. */
static
void
usage()
{
#ifdef DBUG_OFF
	print_version();
#else
	print_version_debug();
#endif /* DBUG_OFF */
	puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2015"));
	printf("Usage: %s [-v] [-c <strict-check>] [-d <dump file name>] [-n]"
		" filename1 [filenames]\n", my_progname);
	printf("See " REFMAN "ibd2sdi.html for usage hints.\n");
	my_print_help(ibd2sdi_options);
	my_print_variables(ibd2sdi_options);
}

/** Parse the options passed to tool. */
extern "C"
bool
ibd2sdi_get_one_option(
	int			optid,
	const struct my_option	*opt MY_ATTRIBUTE((unused)),
	char			*argument MY_ATTRIBUTE((unused)))
{
	switch (optid) {
#ifndef DBUG_OFF
	case '#':
		opts.dbug_setting = argument
			? argument
			: IF_WIN("d:O,ibd2sdi.trace",
				 "d:o,/tmp/ibd2sdi.trace");
		DBUG_PUSH(opts.dbug_setting);
	break;
#endif /* !DBUG_OFF */
	case 'v':
#ifdef DBUG_OFF
		print_version();
#else
		print_version_debug();
#endif /* DBUG */
		exit(EXIT_SUCCESS);
		break;
	case 'c':
		switch (opts.strict_check) {

		case 0:
			srv_checksum_algorithm =
				SRV_CHECKSUM_ALGORITHM_STRICT_CRC32;
			break;
		case 1:
			srv_checksum_algorithm =
				SRV_CHECKSUM_ALGORITHM_STRICT_INNODB;
			break;

		case 2:
			srv_checksum_algorithm =
				SRV_CHECKSUM_ALGORITHM_STRICT_NONE;
			break;
		default:
			return(true);
		}
		break;
	case 'd':
		opts.is_dump_file = true;
		break;
	case 'r':
		opts.is_read_from_copy = true;
		break;
	case 'i':
		opts.is_sdi_id = true;
		break;
	case 't':
		opts.is_sdi_type = true;
		break;
	case 'n':
		opts.no_checksum = true;
		break;
	case 'h':
		usage();
		exit(EXIT_SUCCESS);
		break;
	}

	return(false);
}

/** Retrieve the options passed to the tool. */
static
bool
get_options(
	int	*argc,
	char	***argv)
{
	if (handle_options(argc, argv, ibd2sdi_options,
			   ibd2sdi_get_one_option)) {
		exit(true);
	}

	/* The next arg must be the filename */
	if (!*argc) {
		usage();
		return(true);
	}

	return(false);
}

/** Error logging classes. */
namespace ib {
	info::~info()
	{
		std::cerr << "[INFO] ibd2sdi: " << m_oss.str() << "."
			<< std::endl;
	}

	warn::~warn()
	{
		std::cerr << "[WARNING] ibd2sdi: " <<  m_oss.str() << "."
			<< std::endl;
	}

	error::~error()
	{
		std::cerr << "[ERROR] ibd2sdi: " << m_oss.str() << "."
			<< std::endl;
	}

	fatal::~fatal()
	{
		std::cerr << "[FATAL] ibd2sdi: " << m_oss.str() << "."
			<< std::endl;
		ut_error;
	}

	/* TODO: Improve Object creation & destruction on DBUG_OFF */
	class dbug : public logger {
	public:
		~dbug(){
			DBUG_PRINT("ibd2sdi", ("%s", m_oss.str().c_str()));
		}
	};
}

/** Check if page is corrupted or not.
@param[in]	buf		page frame
@param[in]	page_size	page size
@retval		true		if page is corrupted
@retval		false		if page is not corrupted */
static
bool
is_page_corrupted(
	const byte*		buf,
	const page_size_t&	page_size)
{
	BlockReporter reporter(false, buf, page_size, opts.strict_check);

	return(reporter.is_corrupted());
}

/** Seek the file pointer to page offset in the file.
@param[in]	file_in		File pointer
@param[in]	page_size	page size
@param[in]	page_num	page number
@retval		true		success
@retval		false		error */
static
bool
seek_to_page(
	File			file_in,
	const page_size_t&	page_size,
	page_no_t		page_num)
{
	my_off_t	offset = page_num * page_size.physical();

	DBUG_EXECUTE_IF("ib_seek_error", offset = -1;);
	if (my_seek(file_in, offset, MY_SEEK_SET, MYF(0))
	    == MY_FILEPOS_ERROR) {
		ib::error() << "Error: Unable to seek to necessary offset for"
		       " file with descriptor " << file_in << " and error msg"
		       " is: " << strerror(errno);
		return(false);
	}

	return(true);
}

/** Read physical page size from file into buffer.
@param[in]	page_num	page number
@param[in]	page_size	page size of the tablespace
@param[in]	buf_len		length of buffer
@param[in,out]	buf		read the file in buffer
@param[in,out]	file_in		file pointer created for the tablespace
@return number of bytes read or IB_ERROR on error */
static
size_t
read_page(
	page_no_t		page_num,
	const page_size_t&	page_size,
	uint32_t		buf_len,
	byte*			buf,
	File			file_in)
{
	if (!seek_to_page(file_in, page_size, page_num)) {
		return(IB_ERROR);
	}

	size_t	physical_page_size
		= static_cast<size_t>(page_size.physical());

	ut_ad(physical_page_size >= UNIV_ZIP_SIZE_MIN);
	ut_ad(buf_len >= physical_page_size);

	size_t	n_bytes_read = my_read(
		file_in, buf, physical_page_size, MYF(0));
	ut_ad(n_bytes_read == physical_page_size || n_bytes_read == IB_ERROR);

	return(n_bytes_read);
}

/** Datafile information. */
typedef struct ib_file {
	/** 0 in fil_per_table tablespaces, else the first page number in
	subsequent data file in multi-file tablespace. */
	page_no_t	first_page_num;
	/** Total number of pages in a data file. */
	page_no_t	tot_num_of_pages;
	/** File handle of the data file. */
	File		file_handle;
} ib_file_t;

/** Class to hold information about a single InnoDB tablespace. */
class ib_tablespace {
public:
	/** Constructor from space_id & page_size.
	@param[in]	space_id	tablespace id
	@param[in]	page_size	tablespace page size */
	ib_tablespace(
		space_id_t	space_id,
		const page_size_t&	page_size) :
		m_space_id(space_id),
		m_page_size(page_size),
		m_file_vec(),
		m_page_num_recs(0),
		m_max_recs_per_page(page_size.logical() / SDI_REC_SIZE),
		m_sdi_copy_0(0),
		m_sdi_copy_1(0),
		m_tot_pages(0)
	{
	}

	/** Destructor. Closes open file handles of Datafiles. */
	~ib_tablespace()
	{
		std::vector<ib_file>::const_iterator it;
		for (it = m_file_vec.begin(); it != m_file_vec.end(); ++it) {
			File	file_handle = it->file_handle;
			if (file_handle != -1) {
				my_close(file_handle, MYF(0));
			}
		}
	}

	/** Copy Constructor.
	@param[in]	copy	another object of ib_tablespace */
	ib_tablespace(const ib_tablespace& copy) :
		m_space_id(copy.m_space_id),
		m_page_size(copy.m_page_size),
		m_file_vec(copy.m_file_vec),
		m_page_num_recs(copy.m_page_num_recs),
		m_max_recs_per_page(copy.m_max_recs_per_page),
		m_sdi_copy_0(copy.m_sdi_copy_0),
		m_sdi_copy_1(copy.m_sdi_copy_1),
		m_tot_pages(copy.m_tot_pages)
	{
	}

	/** Add Datafile to vector of datafiles. Also
	resize of vector of pages.
	@param[in]	data_file	Datafile */
	inline
	void
	add_data_file(
		const ib_file_t&	data_file)
	{
		m_file_vec.push_back(data_file);

		m_page_num_recs.resize(
			m_page_num_recs.size()
			+ static_cast<size_t>(data_file.tot_num_of_pages),
			0);
		m_tot_pages += data_file.tot_num_of_pages;
	}

	/** Add the SDI root page numbers to tablespace.
	@param[in]	copy_0	root page number of SDI copy 0
	@param[in]	copy_1	root page number of SDI copy 1 */
	inline
	void
	add_sdi(
		page_no_t	copy_0,
		page_no_t	copy_1)
	{
		m_sdi_copy_0 = copy_0;
		m_sdi_copy_1 = copy_1;
	}

	/** Get the SDI root page number of a copy.
	@param[in]	copy_num	SDI copy number
	@return SDI root page number of a copy */
	inline
	page_no_t
	get_sdi_copy(uint32_t copy_num) const
	{
		switch(copy_num) {
		case 0:
			return(m_sdi_copy_0);
		case 1:
			return(m_sdi_copy_1);
		default:
			ut_error;
		}
	}

	/** Return space id of the tablespace.
	@return	space id of the tablespace */
	inline
	space_id_t
	get_space_id() const
	{
		return(m_space_id);
	}

	/** Return the page size of the tablespace.
	@return the page size of the tablespace. */
	inline
	const page_size_t&
	get_page_size() const
	{
		return(m_page_size);
	}

	/** Return the number of data files of tablespace.
	@return the number of data files of tablespace */
	inline
	uint64_t
	get_file_count() const
	{
		return(m_file_vec.size());
	}

	/** Return the first page number of nth Datafile.
	@param[in]	df_num	Datafile number
	@return the first page number of nth Datafile */
	inline
	ib_file_t
	get_nth_data_file(
		uint64_t	df_num) const
	{
		ut_ad(df_num < m_file_vec.size());
		return(m_file_vec[static_cast<unsigned>(df_num)]);
	}

	/** Increment the number of records on a page. This counter is
	used to detect page corruption. Every page has limit on maximum
	number of records stored.
	@param[in]	page_num	Page on which record is found
	@return true if the number of records exceed the max limit(i.e
	corruption detected), else return false. */
	bool
	inc_num_of_recs_on_page(page_no_t page_num)
	{
		ut_ad(page_num < m_page_num_recs.size());
		++m_page_num_recs[page_num];

		if (m_page_num_recs[page_num]
		    > get_max_recs_per_page()) {
			ib::error() << "Record Corruption detected. Too many"
				" records or infinite loop detected. Aborting";
			ib::error() << "The current iteration num is "
				<< m_page_num_recs[page_num]
				<< ". Maximum number of records expected on"
				<< " the page " << page_num << " is "
				<< get_max_recs_per_page();
			return(true);
		}
		return(false);
	}

	/** Get the maximum number of records possible on a page.
	@return the maximum possible number of records on a page */
	inline
	uint64_t
	get_max_recs_per_page() const
	{
		return(m_max_recs_per_page);
	}

	/** Get the number of records on a page.
	@param[in]	page_num	Page number
	@return the number of records on page */
	inline
	uint64_t
	get_cur_num_recs_on_page(page_no_t page_num) const
	{
		ut_ad(page_num < m_page_num_recs.size());
		return(m_page_num_recs[page_num]);
	}

	/** Return the file handle for which the page belongs. This
	is applicable for multi-file tablespaces (like ibdata*)
	@param[in]	page_num		Page number
	@param[in,out]	offset_in_datafile	offset of page
						in data file
	@return the File handle if found, else -1. */
	inline
	File
	get_file_handle_for_page(
		page_no_t	page_num,
		page_no_t*	offset_in_datafile) const
	{
		/* Now find which datafile of the tablespace to use for reading
		the page. */

		File		file_in = -1;
		*offset_in_datafile = FIL_NULL;

		for (uint32_t i = 0; i < get_file_count(); i++) {

			ib_file_t file = get_nth_data_file(i);
			if (page_num < file.first_page_num
			    + file.tot_num_of_pages) {

				file_in = file.file_handle;
				*offset_in_datafile = page_num
					- file.first_page_num;
				break;
			}
		}
		return(file_in);
	}

	/** Check if SDI exists in a tablespace. If SDI exists, retrieve
	SDI root page numbers.
	@param[in,out]	copy_0	SDI root page number of copy 0
	@param[in,out]	copy_1	SDI root page number of copy 1
	@return false on success, true on failure */
	inline
	bool
	check_sdi(
		page_no_t&	copy_0,
		page_no_t&	copy_1);

	/** Return the total number of pages of the tablespaces.
	This includes pages of all datafiles (ibdata*)
	@return total number of pages of tablespace */
	inline
	page_no_t
	get_tot_pages() const
	{
		return(m_tot_pages);
	}

private:
	/** Return the SDI Root page number stored in a page.
	@param[in]	byte		Page read from buffer
	@param[in]	copy_num	SDI copy num
	@return SDI root page number */
	inline
	page_no_t
	get_sdi_root_page_num(
		byte*		buf,
		uint16_t	copy_num);

	/** Determine the better root page number based on the page type
	FIL_PAGE_SDI
	@param[in]	buf_len			buffer length
	@param[in]	buf			buffer containing the page
	@param[in]	first_copy_num		root page number of first copy
	@param[in]	second_copy_num		root page number of second copy
	@return best copy number if determined, else IB_ERROR on failure */
	inline
	page_no_t
	determine_good_root_page_num(
		uint32_t	buf_len,
		byte*		buf,
		page_no_t	first_copy_num,
		page_no_t	second_copy_num);

	/** Space id of tablespace. */
	space_id_t		m_space_id;

	/** Page size of tablespace. */
	page_size_t		m_page_size;

	/** Vector of datafiles of a tablespace. */
	std::vector<ib_file_t>	m_file_vec;

	/** Vector of pages. For each page, the number of records
	found on each page of tablespace is stored in vector. */
	std::vector<uint64_t>	m_page_num_recs;

	/** Maximum number of records possible on a page. */
	uint64_t		m_max_recs_per_page;

	/** Root page number of SDI copy 0. */
	page_no_t		m_sdi_copy_0;

	/** Root page number of SDI copy 1. */
	page_no_t		m_sdi_copy_1;

	/** Total number of pages of all data files. */
	page_no_t		m_tot_pages;
};

/** Read page from file into memory. If page is compressed SDI page,
decompress and store the uncompressed copy in the buffer.
@param[in]	ts		tablespace structure
@param[in]	page_num	page number
@param[in]	buf_len		buf_len
@param[in,out]	buf		the page read will stored in this buffer
@return number of bytes read from file on success, else return IB_ERROR on
failure */
static
size_t
fetch_page(
	ib_tablespace*	ts,
	page_no_t	page_num,
	uint32_t	buf_len,
	byte*		buf)
{
	DBUG_ENTER("fetch_page");
	ib::dbug() << "Read page number: " << page_num;

	const page_size_t&	page_size = ts->get_page_size();

	/* Get last data file first page_num & total_pages in the
	datafile to check the bounds. */

	ut_ad(ts->get_file_count() > 0);

	DBUG_EXECUTE_IF("ib_invalid_page",
			page_num = ts->get_tot_pages(););

	if (page_num >= ts->get_tot_pages()) {
		ib::error() << "Read requested on invalid page number "
			<< page_num << ". The maximum valid page number"
			<< " in the tablespace is "
			<< ts->get_tot_pages() - 1;
		DBUG_RETURN(IB_ERROR);
	}

	/* Now find which datafile of the tablespace to use for reading
	the page. */

	page_no_t	offset_in_datafile;
	File		file_in = ts->get_file_handle_for_page(
		page_num, &offset_in_datafile);

	ut_ad(file_in != -1);
	ut_ad(buf_len >= page_size.physical());

	size_t	n_bytes = IB_ERROR;
	memset(buf, 0, page_size.physical());

	n_bytes = read_page(offset_in_datafile, page_size, buf_len,
			    buf, file_in);

	if (n_bytes == IB_ERROR) {
		DBUG_RETURN(IB_ERROR);
	}

	if (!opts.no_checksum) {
		bool	corrupt_status = is_page_corrupted(buf, page_size);

		if (corrupt_status) {
			page_id_t page_id(ts->get_space_id(), page_num);

			ib::error() << "Page " << page_id << " is corrupted."
				" Checksum verification failed";
			DBUG_RETURN(IB_ERROR);
		}
	}

	if (page_size.is_compressed()
	    && fil_page_get_type(buf) == FIL_PAGE_SDI) {

		byte*	uncomp_buf = static_cast<byte*>(
			ut_malloc_nokey(2 * page_size.logical()));

		byte*	uncomp_page = static_cast<byte*>(
			ut_align(uncomp_buf, page_size.logical()));

		memset(uncomp_page, 0, page_size.logical());
		page_zip_des_t	page_zip;

		page_zip_des_init(&page_zip);
		page_zip.data = buf;
		page_zip.ssize = page_size_to_ssize(
			page_size.physical());

		DBUG_EXECUTE_IF(
			"ib_decompress_fail",
			mach_write_to_2(buf + PAGE_HEADER + PAGE_N_HEAP,
					10000););

		page_id_t page_id(ts->get_space_id(), page_num);

		if (!page_zip_decompress_low(
			&page_zip, uncomp_page, true)) {
			/* To indicate error */
			n_bytes = IB_ERROR;
			ib::error() << "Decompression failed for"
				" compressed page " << page_id;
		} else {
			ib::dbug() << "Decompression Success for"
				" compressed page " << page_id;

			/* Page is decompressed. */
			memset(buf, 0, UNIV_PAGE_SIZE_MAX);
			ut_ad(buf_len >= page_size.logical());
			memcpy(buf, uncomp_page, page_size.logical());
		}

		ut_free(uncomp_buf);
	}

	DBUG_RETURN(n_bytes);
}

class tablespace_creator {
public:
	/** Constructor
	@param[in]	num_files	number of ibd files
	@param[in]	ibd_files	array of ibd file names
	@param[in]	map		tablespaces map */
	tablespace_creator(
		uint32_t		num_files,
		char**			ibd_files) :
		m_num_files(num_files),
		m_ibd_files(ibd_files),
		m_tablespace(NULL)
	{
	}

	/* Destructor */
	~tablespace_creator() {
		delete m_tablespace;
	}

	/** Create tablespace from the ibd files passed.
	@return false on success, true on failure */
	bool create();

	/** Return the tablespace object created from files.
	@return tablespace object */
	inline
	ib_tablespace*
	get_tablespace()
	{
		return(m_tablespace);
	}

private:
	/** Determine page_size by reading MAX_PAGES_TO_SCAN pages (or actual
	number of pages if less) and verifying the checksums.
	@param[in]	file_in		File pointer
	@param[in,out]	page_size	determined page size
	@return true if valid page_size is determined, else false */
	bool
	determine_page_size(
		File		file_in,
		page_size_t&	page_size);

	/** Determine the minimum corruption ratio and update page_size.
	@param[in]	file_in		file pointer
	@param[in]	num_pages	Total number pages to verify checksum
	@param[in]	in_page_size	the current page_size used for checksum
	@param[in,out]	final_page_size	updated to in_page_size if the current
					corruption ratio is less than minimum
					corruption ratio
	@param[in,out] min_corr_ratio	updated if the current page_size
					corruption ratio is less than
					current value.*/
	void
	determine_min_corruption_ratio(
		File			file_in,
		page_no_t		num_pages,
		const page_size_t&	in_page_size,
		page_size_t&		final_page_size,
		double*			min_corr_ratio);

	/** Verify checksum of the pages and return corruption ratio
	@param[in]	file_in		file pointer
	@param[in]	page_size	page size
	@param[in]	num_pages	Total number pages to verify checksum */
	double
	get_page_size_corruption_count(
		File			file_in,
		const page_size_t&	page_size,
		page_no_t		num_pages);

	/** Find the page_size from the map with the least corruption count.
	@param[in,out]	page_size	page size structure to hold determined
					page size */
	void
	find_best_page_size(
		page_size_t&	page_size);

	/** Get the page size of the tablespace from the tablespace header.
	If tablespace header is corrupted, we will determine the page size by
	reading some other pages and calculating checksums.
	@param[in]	buf		buffer which contains first page of tablespace
	@param[in]	file_in		File handle
	@param[in,out]	page_size	page_size that is determined
	@return true if page_size is determined, else false */
	bool
	get_page_size(
		const byte*	buf,
		File		file_in,
		page_size_t&	page_size);

	/** The total number of files passed to the tool. */
	uint32_t		m_num_files;
	/** The files passed to the tool. */
	char**			m_ibd_files;
	/** Tablespace object created from the list of file passed to
	the tool. */
	ib_tablespace*		m_tablespace;
};

/** Create tablespace from the ibd files passed.
@return false on success, true on failure */
bool
tablespace_creator::create()
{
	byte	buf[UNIV_ZIP_SIZE_MIN];
	memset(buf, 0, UNIV_ZIP_SIZE_MIN);

	DBUG_ENTER("tablespace_creator::create()");

	for (uint32_t i = 0; i < m_num_files; i++) {
		const char*	filename = m_ibd_files[i];
		MY_STAT		stat_info;

		/* stat the file to get size and page count. */
		if (my_stat(filename, &stat_info, MYF(0)) == NULL) {
			ib::error() << "Unable to get file stats "
				<< filename;
			ib::error() << "File doesn't exist";
			DBUG_RETURN(true);
		}

		uint64_t	size = stat_info.st_size;
		DBUG_EXECUTE_IF("ib_file_open_error", filename="junk_file";);
		File		file_in = my_open(filename, O_RDONLY, MYF(0));
		/* If file_in is NULL, terminate as some error encountered. */
		if (file_in == -1) {
			ib::error() << "Unable to open file " << filename;
			DBUG_RETURN(true);
		}

		/* Read minimum page_size. */
		size_t	bytes = my_read(file_in, buf, UNIV_ZIP_SIZE_MIN,
					MYF(0));

		if (bytes != UNIV_ZIP_SIZE_MIN) {
			ib::error() << " Unable to read the page header"
				<< " of " <<  UNIV_ZIP_SIZE_MIN << " bytes";
			if (bytes !=IB_ERROR) {
				ib::error() << " Bytes read was " << bytes;
			}
			my_close(file_in, MYF(0));
			DBUG_RETURN(true);
		}

		const space_id_t	space_id = mach_read_from_4(
			buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		const page_no_t		first_page_num = mach_read_from_4(
			buf + FIL_PAGE_OFFSET);

		ib::dbug() << "The space id of the file " << filename
			<< " is " << space_id;

		if (i == 0) {
			/* First data file of system tablespace
			or single table tablespace */

			page_size_t	page_size(univ_page_size);

			bool	success = get_page_size(buf, file_in, page_size);

			if (!success) {
				DBUG_RETURN(true);
			}

			ut_ad(first_page_num == 0);

			page_no_t	pages = static_cast<page_no_t>(
				size / page_size.physical());

			DBUG_EXECUTE_IF("ib_partial_page", size++;);

			if (pages * page_size.physical() != size) {
				ib::warn() << "There is a partial page at the"
					<< " end, of size "
					<< size - (pages * page_size.physical())
					<< ". This partial page is ignored";
			}

			ib::dbug() << "Total Number of pages in the file: "
				<<  pages;

			ib_file_t ibd_file;
			ibd_file.first_page_num = first_page_num;
			ibd_file.file_handle = file_in;
			ibd_file.tot_num_of_pages = pages;

			m_tablespace = new ib_tablespace(
				space_id, page_size);
			m_tablespace->add_data_file(ibd_file);

			page_no_t	copy_0;
			page_no_t	copy_1;
			if (m_tablespace->check_sdi(copy_0, copy_1)) {
				ib::error() << "SDI doesn't exist for"
					" this tablespace or the SDI"
					" root page numbers couldn't"
					" be determined";
				DBUG_RETURN(true);
			} else {
				m_tablespace->add_sdi(copy_0, copy_1);
			}

		} else {
			/* We found next file of system tablespace. */
			ut_ad(m_tablespace != NULL);

			if (space_id != m_tablespace->get_space_id()) {
				ib::error() << "Multiple tablespaces passed."
					<< " Please specify only one"
					<< " tablespace";
				my_close(file_in, MYF(0));
				DBUG_RETURN(true);
			}

			/* This datafile should belong system tablespace
			only. */
			ut_ad(space_id == 0);

			const page_size_t&	page_size =
				m_tablespace->get_page_size();
			uint64_t	phys_page_size = page_size.physical();
			bool		all_zero_page = false;

			byte	full_page[UNIV_PAGE_SIZE_MAX];
			memset(full_page, 0, UNIV_ZIP_SIZE_MAX);

			read_page(0, page_size, UNIV_PAGE_SIZE_MAX,
				  full_page, file_in);

			if (buf_page_is_zeroes(buf, page_size)) {
				all_zero_page = true;
			}

			/* Calculate the last page number of the
			last data_file */
			uint64_t	tot_data_files =
				m_tablespace->get_file_count();

			ib_file_t	file =
				m_tablespace->get_nth_data_file(
					tot_data_files - 1);

			page_no_t	last_file_first_page_num =
				file.first_page_num;

			page_no_t	last_file_tot_pages =
				file.tot_num_of_pages;

			page_no_t	last_page_num_of_last_data_file =
				last_file_first_page_num
				+ last_file_tot_pages
				- 1;

			if (!all_zero_page && (first_page_num !=
				last_page_num_of_last_data_file + 1)) {
				ib::error() << "The first page num "
					<< first_page_num << " of this"
					" datafile " << filename
					<< " is not equal to last"
					<< " page num "
					<< last_page_num_of_last_data_file
					<< " + 1 of previous data file."
					<< " Skipping this tablespace";
				my_close(file_in, MYF(0));
				DBUG_RETURN(true);
			}

			ib_file_t ibd_file;
			ibd_file.first_page_num = first_page_num;
			ibd_file.file_handle = file_in;
			ibd_file.tot_num_of_pages = static_cast<page_no_t>(
				size / phys_page_size);

			/* Add the datafile to the file vector
			of the tablespace */
			m_tablespace->add_data_file(ibd_file);
		}
	}

	DBUG_RETURN(false);
}

/** Get the page size of the tablespace from the tablespace header.
If tablespace header is corrupted, we will determine the page size by
reading some other pages and calculating checksums.
@param[in]	buf		buffer which contains first page of tablespace
@param[in]	file_in		File handle
@param[in,out]	page_size	page_size that is determined
@return true if page_size is determined, else false */
bool
tablespace_creator::get_page_size(
	const byte*	buf,
	File		file_in,
	page_size_t&	page_size)
{
	const ulint	flags = fsp_header_get_flags(buf);
	bool		is_valid_flags = fsp_flags_is_valid(flags);

	if (is_valid_flags) {
		const ulint	ssize = FSP_FLAGS_GET_PAGE_SSIZE(flags);

		if (ssize == 0) {
			srv_page_size = UNIV_PAGE_SIZE_ORIG;
		} else {
			srv_page_size = ((UNIV_ZIP_SIZE_MIN >> 1) << ssize);
		}
		srv_page_size_shift = page_size_validate(srv_page_size);
	}

	if (!is_valid_flags || srv_page_size_shift == 0) {

		page_size_t	min_valid_size(UNIV_ZIP_SIZE_MIN,
		UNIV_PAGE_SIZE_MIN, true);

		page_size_t	max_valid_size(UNIV_PAGE_SIZE_MAX,
		UNIV_PAGE_SIZE_MAX, false);

		/* Page corruption detected. Page size cannot be zero.
		We will read multiple pages to determine the page_size */
		ib::error() << "Page 0 corruption detected. Page size is"
			" either zero or out of bound";
		ib::error() << "Minimum valid page size is " << min_valid_size;
		ib::error() << "Maximum valid page size is " << max_valid_size;
		ib::error() << "Reading multiple pages to determine the"
			" page_size";

		bool	success = determine_page_size(file_in, page_size);

		if (!success) {
			return(false);
		}

		srv_page_size = static_cast<ulong>(page_size.logical());
		srv_page_size_shift = page_size_validate(srv_page_size);

		ut_ad(srv_page_size_shift != 0);

		univ_page_size.copy_from(
			page_size_t(srv_page_size, srv_page_size, false));

		return(true);
	}

	ut_ad(srv_page_size_shift != 0);

	univ_page_size.copy_from(
		page_size_t(srv_page_size, srv_page_size, false));

	page_size.copy_from(page_size_t(flags));
	return(true);
}

/** Determine the minimum corruption ratio and update page_size.
@param[in]	file_in		file pointer
@param[in]	num_pages	Total number pages to verify checksum
@param[in]	in_page_size	the current page_size used for checksum
@param[in,out]	final_page_size	updated to in_page_size if the current
				corruption ratio is less than minimum
				corruption ratio
@param[in,out] min_corr_ratio	updated if the current page_size
				corruption ratio is less than
				current value.*/
void
tablespace_creator::determine_min_corruption_ratio(
	File			file_in,
	page_no_t		num_pages,
	const page_size_t&	in_page_size,
	page_size_t&		final_page_size,
	double*			min_corr_ratio)
{
	double	corruption_ratio
		= get_page_size_corruption_count(
			file_in, in_page_size,
			num_pages);

	if (corruption_ratio < *min_corr_ratio) {
		*min_corr_ratio = corruption_ratio;
		final_page_size.copy_from(in_page_size);
	}
}

/** Determine page_size by reading MAX_PAGES_TO_SCAN pages (or actual
number of pages if less) and verifying the checksums.
@param[in]	file_in		File pointer
@param[in,out]	page_size	determined page size
@return true if valid page_size is determined, else false */
bool
tablespace_creator::determine_page_size(
	File		file_in,
	page_size_t&	page_size)
{
	uint64_t	size;
	page_size_t	final_page_size(0, 0, false);

	my_seek(file_in, 0, MY_SEEK_END, MYF(0));
	size = my_tell(file_in, MYF(0));
	my_seek(file_in, 0, MY_SEEK_SET, MYF(0));

	double		min_corruption_ratio = 1.0;

	for (uint32_t	logical_page_size = UNIV_PAGE_SIZE_MIN;
	     logical_page_size <= UNIV_PAGE_SIZE_MAX;
	     logical_page_size <<= 1) {

		srv_page_size = logical_page_size;

		for (uint32_t phys_page_size = UNIV_ZIP_SIZE_MIN;
		     phys_page_size <= logical_page_size;
		     phys_page_size <<= 1) {

			page_no_t	num_pages = size / phys_page_size;

			if (num_pages > MAX_PAGES_TO_SCAN) {
				num_pages = MAX_PAGES_TO_SCAN;
			}

			/* When physical == logical page size, we can have both
			compressed and uncompressed page sizes. For example,
			16k compressed and 16k uncompressed are both valid
			page sizes. */
			if (phys_page_size == logical_page_size) {
				const page_size_t	uncomp_page_size(
					phys_page_size, logical_page_size,
					false);

				determine_min_corruption_ratio(
					file_in, num_pages, uncomp_page_size,
					final_page_size,
					&min_corruption_ratio);
			}

			/* 32k and 64k are not valid compressed page size. */
			if (logical_page_size <= UNIV_ZIP_SIZE_MAX) {
				const page_size_t	comp_page_size(
					phys_page_size, logical_page_size,
					true);

				determine_min_corruption_ratio(
					file_in, num_pages, comp_page_size,
					final_page_size,
					&min_corruption_ratio);
			}
		}
	}

	if (min_corruption_ratio == 1.0) {
		ib::error() << "Page size couldn't be determined";
		return(false);
	} else {
		ib::info() << "Page size determined is : " << final_page_size;
		page_size.copy_from(final_page_size);
		return(true);
	}
}

/** Verify checksum of the page and return corruption ratio
@param[in]	file_in		file pointer
@param[in]	page_size	page size
@param[in]	num_pages	Total number pages to verify checksum. */
double
tablespace_creator::get_page_size_corruption_count(
	File			file_in,
	const page_size_t&	page_size,
	page_no_t		num_pages)
{
	uint32_t	corruption_count = 0;
	/* We will exclude all-zero pages from total number of pages
	when calculating corruption ratio. */
	page_no_t	all_zero_page_count = 0;
	byte		buf[UNIV_PAGE_SIZE_MAX];
	memset(buf, 0, UNIV_ZIP_SIZE_MAX);

	for (page_no_t page_num = 0; page_num < num_pages; page_num++) {

		read_page(page_num, page_size, UNIV_PAGE_SIZE_MAX, buf,
			  file_in);

		if (buf_page_is_zeroes(buf, page_size)) {
			++all_zero_page_count;
			continue;
		}

		bool	corrupt = is_page_corrupted(buf, page_size);

		if (corrupt) {
			++corruption_count;
		}
	}

	double	corruption_ratio =
		corruption_count * 1.0 / (num_pages - all_zero_page_count);

	return(corruption_ratio);
}

/** Check if SDI exists in a tablespace. If SDI exists, retrieve
SDI root page numbers.
@param[in,out]	copy_0	SDI root page number of copy 0
@param[in,out]	copy_1	SDI root page number of copy 1
@return false on success, true on failure and copy_0 & copy1
are filled with IB_ERROR on failure. */
inline
bool
ib_tablespace::check_sdi(
	page_no_t&	copy_0,
	page_no_t&	copy_1)
{
	DBUG_ENTER("ib_tablespace::check_sdi");

	byte	buf[UNIV_PAGE_SIZE_MAX];

	/* Read page 0 from file */
	if (fetch_page(this, 0, UNIV_PAGE_SIZE_MAX, buf) == IB_ERROR) {
		DBUG_RETURN(true);
	}

	ulint	space_flags = fsp_header_get_flags(buf);

	ib::dbug() << "flags are " << space_flags;

	bool	has_sdi;

	/* 1. Check if SDI flag is set or not */
	if (FSP_FLAGS_HAS_SDI(space_flags)) {
		ib::dbug() << "Tablespace has SDI space flag set. Lets"
			" read page 1 & 2 to confirm";
		has_sdi = true;
	} else {
		ib::dbug() << "Tablespace do not have SDI"
			" space flag set. Lets read page 1 & 2 to"
			" confirm";
		has_sdi = false;
	}

	/* 2. Seek to page 1 & 2, and read FIL_SDI_ROOT_PAGE_NUM */

	if(fetch_page(this, 1, UNIV_PAGE_SIZE_MAX, buf) == IB_ERROR) {
		ib::error() << "Couldn't read page 1";
		DBUG_RETURN(true);
	} else {
		ib::dbug() << "Read page number: 1";
	}

	page_no_t	copy_0_from_page_1 = get_sdi_root_page_num(
		buf, 0);
	page_no_t	copy_1_from_page_1 = get_sdi_root_page_num(
		buf, 1);

	ib::dbug() << "SDI copy 0 root page num from page 1 is "
		<< copy_0_from_page_1;

	ib::dbug() << "SDI copy 1 root page num from page 1 is "
		<< copy_1_from_page_1;

	if (fetch_page(this, 2, UNIV_PAGE_SIZE_MAX, buf) == IB_ERROR) {
		ib::error() << "Couldn't read page 2";
		DBUG_RETURN(true);
	} else {
		ib::dbug() << "Read page number: 2";
	}

	page_no_t	copy_0_from_page_2 =
		get_sdi_root_page_num(buf, 0);
	page_no_t	copy_1_from_page_2 =
		get_sdi_root_page_num(buf, 1);

	ib::dbug() << "copy 0 root page num from page 2 is "
		<< copy_0_from_page_2;
	ib::dbug() << "copy 1 root page num from page 2 is "
		<< copy_1_from_page_2;

	copy_0 = determine_good_root_page_num(
		UNIV_PAGE_SIZE_MAX, buf, copy_0_from_page_1,
		copy_0_from_page_2);

	if (copy_0 == IB_ERROR_32) {
		ib::error() << "Couldn't determine the best root"
			" page numbers";
		DBUG_RETURN(true);
	}

	copy_1 = determine_good_root_page_num(
		UNIV_PAGE_SIZE_MAX, buf, copy_1_from_page_1,
		copy_1_from_page_2);

	if (copy_1 == IB_ERROR_32) {
		ib::error() << "Couldn't determine the best root"
			" page numbers";
		DBUG_RETURN(true);
	}

	/* If tablespace flags suggest that we don't have SDI but
	reading from Page 1 & 2 suggest that we have SDI index
	pages, we warn and process the SDI pages. */
	if (!has_sdi) {
		ib::warn() << "Tablespace flags suggest SDI INDEX"
			" didn't exist but found SDI root page"
			" numbers in page 1 & page 2";
	}

	DBUG_RETURN(false);
}
/** Determine the better root page number based on the page type
FIL_PAGE_SDI
@param[in]	buf_len			buffer length
@param[in]	buf			buffer containing the page
@param[in]	first_copy_num		root page number of first copy
@param[in]	second_copy_num		root page number of second copy
@return best copy number if determined, else IB_ERROR on failure */
inline
page_no_t
ib_tablespace::determine_good_root_page_num(
	uint32_t	buf_len,
	byte*		buf,
	page_no_t	first_copy_num,
	page_no_t	second_copy_num)
{
	if (fetch_page(this, first_copy_num, buf_len, buf) == IB_ERROR) {
		ib::error() << "Unable to read page " << first_copy_num;
		return(IB_ERROR_32);
	}

	ulint	page_type_from_1 = fil_page_get_type(buf);

	if (fetch_page(this, second_copy_num, buf_len, buf) == IB_ERROR) {
		ib::error() << "Unable to read page " << second_copy_num;
		return(IB_ERROR_32);
	}

	ulint		page_type_from_2 = fil_page_get_type(buf);
	page_no_t	best_copy_num = IB_ERROR_32;

	DBUG_EXECUTE_IF("ib_no_sdi",
			page_type_from_1 = page_type_from_2 = 0;);

	if (page_type_from_1 == page_type_from_2
	    && page_type_from_1 == FIL_PAGE_SDI) {
		/* We can use any copy */
		best_copy_num = first_copy_num;
		return(best_copy_num);
	}

	if (first_copy_num == second_copy_num
	    && page_type_from_1 == page_type_from_2
	    && page_type_from_1 != FIL_PAGE_SDI) {

		ib::error() << "Root page numbers and page types are"
			" equal but they are of type: " << page_type_from_1
			<< " expected page type is " <<  FIL_PAGE_SDI;
			return(IB_ERROR_32);
	}

	ib::error() << "From Page 1: root page number: "
		<< first_copy_num
		<< ". From Page 2: root page number: "
		<< second_copy_num << " are mismatching";

	ib::error() << "Verifying page types to select the"
		" better root page number";

	if (page_type_from_1 == FIL_PAGE_SDI
	    && page_type_from_2 != FIL_PAGE_SDI) {

		best_copy_num = first_copy_num;

	} else if (page_type_from_2 == FIL_PAGE_SDI
		&& page_type_from_1 != FIL_PAGE_SDI) {

		best_copy_num = second_copy_num;
	}

	if (best_copy_num != IB_ERROR_32) {
		ib::info() << "Selected page no: " << best_copy_num;
	}

	return(best_copy_num);
}

/** Return the SDI Root page number stored in a page.
@param[in]	buf		Page of tablespace
@param[in]	copy_num	SDI copy num
@return SDI root page number */
inline
page_no_t
ib_tablespace::get_sdi_root_page_num(
	byte*		buf,
	uint16_t	copy_num)
{
	ut_ad(copy_num < MAX_SDI_COPIES);
	ut_ad(buf != NULL);
	return(mach_read_from_4(buf + FIL_SDI_ROOT_PAGE_NUM + copy_num * 4));
}

/** Class to dump SDI */
class ibd2sdi {
public:
	/** Constructor
	@param[in]	num_files	number of ibd files
	@param[in]	ibd_files	array of ibd files
	@param[in,out]	out_stream	Stream to dump SDI
	@param[in]	copy_num	SDI copy number: 0, 1 or
					UINT32_MAX to dump from both copies
	@param[in]	skip_data	if true, dump only SDI id & type */
	ibd2sdi(
		uint32_t	num_files,
		char**		ibd_files,
		FILE*		out_stream,
		uint32_t	copy_num,
		bool		skip_data) :
		m_num_files(num_files),
		m_ibd_files(ibd_files),
		m_out_stream(out_stream),
		m_copy_num(copy_num),
		m_skip_data(skip_data),
		m_is_specific_rec(false),
		m_specific_id(UINT64_MAX),
		m_specific_type(UINT64_MAX),
		m_tablespace_creator(NULL)
	{
	}

	/** Destructor. Close all open files. */
	~ibd2sdi() {
		delete m_tablespace_creator;
	}

	/** Process the files passed in constructor. We do this as
	method instead of constructor because we might fail when processing
	ibd files passed.
	@return false on success, true on failure */
	bool
	process_files();

	/** Dump SDI in a tablespace
	@return false on success, true on failure */
	bool
	dump();

	/** Dump SDI of a given SDI id & type
	@param[in]	sdi_id		SDI id
	@param[in]	sdi_type	SDI type
	@return false on success, true on failure */
	bool
	dump_one_sdi(
		uint64_t	sdi_id,
		uint64_t	sdi_type);

	/** Dump all SDI records matching with SDI id
	@param[in]	sdi_id		SDI id
	@return false on success, true on failure */
	bool
	dump_matching_ids(
		uint64_t	sdi_id);

	/** Dump all SDI records matching with SDI type
	@param[in]	sdi_type	SDI type
	@return false on success, true on failure */
	bool
	dump_matching_types(
		uint64_t	sdi_type);

private:
	/** Process SDI from a tablespace
	@param[in]	ts	tablespace structure
	@return false on success, true on failure */
	bool
	process_sdi_from_copy(
		ib_tablespace*	ts);

	/** Iterate over record from a single SDI copy. There is no comparision
	involved with the records in other copy
	@param[in]	ts		tablespace structure
	@param[in]	root_page_num	SDI root page number
	@param[in]	out_stream	file stream to dump SDI
	@return false on success, true on failure */
	bool
	dump_all_recs_in_leaf_level(
		ib_tablespace*	ts,
		page_no_t	root_page_num,
		FILE*		out_stream);

	/** Reach to page level zero and then iterate over the records
	in both SDI copies and compare them
	@param[in]	ts			tablespace structure
	@param[in]	root_page_num_copy_0	root page of SDI copy 0
	@param[in]	root_page_num_copy_1	root page of SDI copy 1
	@return false on success, true on failure */
	bool
	dump_all_recs_in_leaf_level_compare(
		ib_tablespace*	ts,
		page_no_t	root_page_num_copy_0,
		page_no_t	root_page_num_copy_1);

	/** Read page from file into buffer passed and return the page level.
	@param[in]	ts		tablespace structure
	@param[in]	buf_len		buffer length
	@param[in,out]	buf		page read will stored in this buffer
	@param[in]	page_num	page number to read
	@return level of the page read, UINT64_MAX on error */
	uint64_t
	read_page_and_return_level(
		ib_tablespace*	ts,
		uint32_t	buf_len,
		byte*		buf,
		page_no_t	page_num);

	/** Read the uncompressed blob stored in off-pages to the buffer.
	@param[in]	ts			tablespace structure
	@param[in]	first_blob_page_num	first blob page number
						of the chain
	@param[in]	total_off_page_length	total length of blob stored
						in record
	@param[in,out]	dest_buf		blob will be copied to this
						buffer
	@return 0 if blob is not read, else the total length of blob read from
	off-pages */
	uint64_t
	copy_uncompressed_blob(
		ib_tablespace*	ts,
		page_no_t	first_blob_page_num,
		uint64_t	total_off_page_length,
		byte*		dest_buf);

	/** Read the compressed blob stored in off-pages to the buffer.
	@param[in]	ts			tablespace structure
	@param[in]	first_blob_page_num	first blob page number of the
						chain
	@param[in]	total_off_page_length	total Length of blob stored
						in record
	@param[in,out]	dest_buf		blob will be copied to this
						buffer
	@return 0 if blob is not read, else the total length of blob read from
	off-pages */
	uint64_t
	copy_compressed_blob(
		ib_tablespace*	ts,
		page_no_t	first_blob_page_num,
		uint64_t	total_off_page_length,
		byte*		dest_buf);

	/** Reach to B-Tree level zero and read the first (lefmost) page into
	the buffer
	@param[in]	ts		tablespace structure
	@param[in]	buf_len		buffer length
	@param[in,out]	buf		the page read at level zero will stored
					in this buffer
	@param[in]	root_page_num	the root page number of the B-Tree
	@retval		SUCCESS		success
	@retval		FAILURE		failure
	@retval		NO_RECORSDS	zero records found on root page */
	err_t
	reach_to_leftmost_leaf_level(
		ib_tablespace*	ts,
		uint32_t	buf_len,
		byte*		buf,
		page_no_t	root_page_num);

	/** Extract SDI record fields
	@param[in]	ts		tablespace structure
	@param[in]	rec		pointer to record
	@param[in,out]	sdi_id		sdi id
	@param[in,out]	sdi_type	sdi type
	@param[in,out]	sdi_data	sdi blob
	@param[in,out]	sdi_data_len	length of sdi blob */
	void
	parse_fields_in_rec(
		ib_tablespace*	ts,
		byte*		rec,
		uint64_t*	sdi_id,
		uint64_t*	sdi_type,
		byte**		sdi_data,
		uint64_t*	sdi_data_len);

	/** Return the record type
	@param[in]	rec	sdi record in a page
	@return	the type of record */
	byte
	get_rec_type(
		byte*	rec);

	/** Return the location of the next record
	@param[in]	ts		tablespace structure
	@param[in]	current_rec	current sdi record in a page
	@param[in]	buf_len		buffer length
	@param[in]	buf		page containing the current rec
	@param[out]	corrupt		true if corruption detected, else false
	@return location of the next record, else NULL if there are no
	user records */
	byte*
	get_next_rec(
		ib_tablespace*	ts,
		byte*		current_rec,
		uint32_t	buf_len,
		byte*		buf,
		bool*		corrupt);

	/** Write the extracted SDI record fields to outfile
	if passed, else to stdout
	@param[in]	sdi_id		sdi id
	@param[in]	sdi_type	sdi type
	@param[in]	sdi_data	sdi data
	@param[in]	sdi_data_len	sdi data len
	@param[in]	out_stream	file stream to dump SDI */
	void
	dump_sdi_rec(
		uint64_t	sdi_id,
		uint64_t	sdi_type,
		byte*		sdi_data,
		uint64_t	sdi_data_len,
		FILE*		out_stream);

	/** Iterate over the records in both SDI copies and compare them
	@param[in]	ts		tablespace structure
	@param[in]	buf_copy_0_len	length of buffer containing copy 0
	@param[in]	buf_copy_0	buffer containing the left-most page
					number of copy 0
	@param[in]	buf_copy_0_len	length of buffer containing copy 0
	@param[in]	buf_copy_1	buffer containing the left-most page
					number of copy 1
	@return false on success, true on corruption */
	bool
	dump_recs_on_page_compare(
		ib_tablespace*	ts,
		uint32_t	buf_copy_0_len,
		byte*		buf_copy_0,
		uint32_t	buf_copy_1_len,
		byte*		buf_copy_1);

	/** Return pointer to the first user record in a page
	@param[in]	buf_len	buffer length
	@param[in]	buf	Memory containing entire page
	@return pointer to first user record in page */
	byte*
	get_first_user_rec(
		uint32_t	buf_len,
		byte*		buf);

	/** Compare two SDI records, if they are equal store the fields
	@param[in]	ts			tablespace structure
	@param[in]	rec_0			sdi rec from copy 0
	@param[in]	rec_1			sdi rec from copy 1
	@param[in,out]	sdi_id			sdi id
	@param[in,out]	sdi_type		sdi type
	@param[in,out]	sdi_data		sdi data
	@param[in,out]	sdi_data_len		sdi data len
	@return true if sdi recs are equal, else false */
	bool
	compare_sdi_recs(
		ib_tablespace*	ts,
		byte*		rec_0,
		byte*		rec_1,
		uint64_t*	sdi_id,
		uint64_t*	sdi_type,
		byte**		sdi_data,
		uint64_t*	sdi_data_len);

	/** Check the SDI record with user passed SDI record id & type
	and dump only if id & type matches
	@param[in]	sdi_id		sdi id
	@param[in]	sdi_type	sdi type
	@param[in]	sdi_data	sdi data
	@param[in]	sdi_data_len	sdi data len
	@param[in]	out_stream	file stream to dump SDI
	@return true if the record matched with the user passed SDI record */
	bool
	check_and_dump_record(
		uint64_t	sdi_id,
		uint64_t	sdi_type,
		byte*		sdi_data,
		uint64_t	sdi_data_len,
		FILE*		out_stream);

	/** Dump SDI copy of a tablespace to temporary file.
	@param[in]	ts			tablespace structure
	@param[in]	root_page_num		root page number of SDI copy
	@param[in]	copy_num		SDI copy number */
	void
	dump_sdi_to_err_file(
		ib_tablespace*	ts,
		page_no_t	root_page_num,
		uint32_t	copy_num);

	/** Number of ibd files. */
	uint32_t			m_num_files;
	/** Array of ibd files passed by user. */
	char**				m_ibd_files;
	/** Output stream to dump the parsed SDI. */
	FILE*				m_out_stream;
	/** SDI copy number (0 or 1). UINT32_MAX to read
	from both copies. */
	uint32_t			m_copy_num;
	/** True if we just want to dump SDI keys to output
	stream without data. */
	bool				m_skip_data;
	/** True if we are looking of specific SDI record. */
	bool				m_is_specific_rec;
	/** If we are looking specific SDI record, match this SDI id. */
	uint64_t			m_specific_id;
	/** If we are looking specific SDI record, match this SDI type. */
	uint64_t			m_specific_type;
	/** tablespace creator class. */
	tablespace_creator*		m_tablespace_creator;
};

/** Process SDI from a tablespace
@param[in]	ts	tablespace structure
@return false on success, true on failure */
bool
ibd2sdi::process_sdi_from_copy(
	ib_tablespace*	ts)
{
	switch (m_copy_num) {
	case UINT32_MAX:
		return(dump_all_recs_in_leaf_level_compare(
			ts, ts->get_sdi_copy(0), ts->get_sdi_copy(1)));
	case 0:
		return(dump_all_recs_in_leaf_level(
			ts,ts->get_sdi_copy(0), m_out_stream));
	case 1:
		return(dump_all_recs_in_leaf_level(
			ts, ts->get_sdi_copy(1), m_out_stream));
	default:
		return(true);
	}
}

/** Iterate over record from a single SDI copy. There is no comparision
involved with the records in other copy
@param[in]	ts		tablespace structure
@param[in]	root_page_num	SDI root page number
@param[in]	out_stream	file stream to dump SDI
@return false on success, true on failure */
bool
ibd2sdi::dump_all_recs_in_leaf_level(
	ib_tablespace*	ts,
	page_no_t	root_page_num,
	FILE*		out_stream)
{
	DBUG_ENTER("dump_all_recs_in_leaf_level");

	byte	buf[UNIV_PAGE_SIZE_MAX];

	switch (reach_to_leftmost_leaf_level(
		ts, UNIV_PAGE_SIZE_MAX, buf, root_page_num)) {
	case SUCCESS:
		break;
	case FALIURE:
		DBUG_RETURN(true);
	case NO_RECORDS:
		ib::info() << "SDI is empty";
		DBUG_RETURN(false);
	default:
		ut_ad(0);
	}

	bool		explicit_sdi_rec_found = false;
	byte*		current_rec = get_first_user_rec(UNIV_PAGE_SIZE_MAX,
							 buf);
	uint64_t	sdi_id;
	uint64_t	sdi_type;
	byte*		sdi_data = NULL;
	uint64_t	sdi_data_len = 0;
	bool		corrupt = false;

	while (current_rec != NULL
	       && get_rec_type(current_rec) != REC_STATUS_SUPREMUM
	       && !explicit_sdi_rec_found
	       && !corrupt) {

		parse_fields_in_rec(ts, current_rec,
				    &sdi_id, &sdi_type, &sdi_data,
				    &sdi_data_len);

		explicit_sdi_rec_found = check_and_dump_record(
			sdi_id, sdi_type, sdi_data,
			sdi_data_len, out_stream);

		free(sdi_data);
		sdi_data = NULL;

		if (explicit_sdi_rec_found) {
			break;
		}

		current_rec = get_next_rec(ts, current_rec,
					   UNIV_PAGE_SIZE_MAX, buf, &corrupt);
	}

	DBUG_RETURN(corrupt);
}

/** Dump SDI copy of a tablespace to temporary file.
@param[in]	ts			tablespace structure
@param[in]	root_page_num		root page number of SDI copy
@param[in]	copy_num		SDI copy number */
void
ibd2sdi::dump_sdi_to_err_file(
	ib_tablespace*	ts,
	page_no_t	root_page_num,
	uint32_t	copy_num)
{
	/* Buffer to hold temporary file name. */
	char	temp_filename_buf[FN_REFLEN];
	char	pattern[100];

	my_snprintf(pattern, 100, "%s_%d_%s_%d_", "ib_sdi",
		    ts->get_space_id(), "copy", copy_num);

	FILE*	dump_file = create_tmp_file(temp_filename_buf, pattern);

	if (dump_file == NULL) {
		ib::error() << "Unable to create temporary"
			<< " file to dump SDI copy:"
			<< strerror(errno);
		return;
	} else {
		ib::error() << "Dumping SDI Copy: " << copy_num
			<< " into file: " << temp_filename_buf;
	}

	dump_all_recs_in_leaf_level(ts, root_page_num, dump_file);
	my_fclose(dump_file, MYF(0));
}

/** Reach to page level zero and then iterate over the records in both SDI
copies and compare them
@param[in]	ts			tablespace structure
@param[in]	root_page_num_copy_0	root page of SDI copy 0
@param[in]	root_page_num_copy_1	root page of SDI copy 1
@return false on success, true on failure */
bool
ibd2sdi::dump_all_recs_in_leaf_level_compare(
	ib_tablespace*	ts,
	page_no_t	root_page_num_copy_0,
	page_no_t	root_page_num_copy_1)
{
	DBUG_ENTER("dump_all_recs_in_leaf_level_compare");

	byte	buf0[UNIV_PAGE_SIZE_MAX];
	byte	buf1[UNIV_PAGE_SIZE_MAX];

	err_t	err_0 = reach_to_leftmost_leaf_level(
		ts, UNIV_PAGE_SIZE_MAX, buf0, root_page_num_copy_0);

	err_t	err_1 = reach_to_leftmost_leaf_level(
		ts, UNIV_PAGE_SIZE_MAX, buf1, root_page_num_copy_1);

	if (err_0 == NO_RECORDS && err_1 == NO_RECORDS) {
		/* No records from both copies. */
		ib::info() << "SDI from both copies is empty";
		DBUG_RETURN(false);
	}

	if (err_0 == SUCCESS && err_1 == SUCCESS) {
		bool	ret_comp = dump_recs_on_page_compare(
			ts, UNIV_PAGE_SIZE_MAX, buf0,
			UNIV_PAGE_SIZE_MAX, buf1);
		if (ret_comp) {
			for (uint32_t i = 0; i < MAX_SDI_COPIES; i++) {
				dump_sdi_to_err_file(
					ts,
					i == 0 ? root_page_num_copy_0
					: root_page_num_copy_1, i);
			}
			ib::error() << "Please compare the above files to find"
				<< " the difference between two SDI copies";
			DBUG_RETURN(true);
		} else {
			DBUG_RETURN(false);
		}
	}

	if (err_0 == SUCCESS) {
		/* Print records from copy 0 but there is problem with
		copy 1. */
		if (err_1 == NO_RECORDS) {
			ib::error() << "No records from copy 1 but there"
			" are records from copy 0";
		} else if (err_1 ==  FALIURE) {
			ib::error() << "Error while reaching to leaf level"
				" of copy 1 but there are records from"
				" copy 0";
		}
		dump_sdi_to_err_file(ts, root_page_num_copy_0, 0);
		DBUG_RETURN(true);
	}

	if (err_1 == SUCCESS) {
		/* Print records from copy 0 but there is problem with
		copy 1. */
		if (err_0 == NO_RECORDS) {
			ib::error() << "No records from copy 0 but there"
			" are records from copy 1";
		} else if (err_0 == FALIURE) {
			ib::error() << "Error while reaching to leaf level"
				" of copy 0 but there are records from"
				" copy 1";
		}
		dump_sdi_to_err_file(ts, root_page_num_copy_1, 1);
		DBUG_RETURN(true);
	}
	DBUG_RETURN(true);
}

/** Read page from file into buffer passed and return the page level.
@param[in]	ts		tablespace structure
@param[in]	buf_len		buffer length
@param[in,out]	buf		page read will stored in this buffer
@param[in]	page_num	page number to read
@return level of the page read, UINT64_MAX on error */
uint64_t
ibd2sdi::read_page_and_return_level(
	ib_tablespace*	ts,
	uint32_t	buf_len,
	byte*		buf,
	page_no_t	page_num)
{
	/* 1. Read page */
	DBUG_ENTER("read_page_and_return_level");
	if(fetch_page(ts, page_num, buf_len, buf) == IB_ERROR) {
		ib::error() << "Couldn't read page " << page_num;
		DBUG_RETURN(UINT64_MAX);
	} else {
		ib::dbug() << "Read page number: " << page_num;
	}

	ulint	page_type = fil_page_get_type(buf);

	if (page_type != FIL_PAGE_SDI) {
		ib::error() << "Unexpected page type: " << page_type
			<< ". Expected page type:" << FIL_PAGE_SDI;
		DBUG_RETURN(UINT64_MAX);
	}

	/* 2. Get page level */
	ulint	page_level = mach_read_from_2(
		buf + FIL_PAGE_DATA + PAGE_LEVEL);

	ib::dbug() << "page level is " << page_level;
	DBUG_RETURN(page_level);
}

/** Read the uncompressed blob stored in off-pages to the buffer.
@param[in]	ts			tablespace structure
@param[in]	first_blob_page_num	first blob page number of the chain
@param[in]	total_off_page_length	total length of blob stored in record
@param[in,out]	dest_buf		blob will be copied to this buffer
@return 0 if blob is not read, else the total length of blob read from
off-pages */
uint64_t
ibd2sdi::copy_uncompressed_blob(
	ib_tablespace*	ts,
	page_no_t	first_blob_page_num,
	uint64_t	total_off_page_length,
	byte*		dest_buf)
{
	DBUG_ENTER("copy_uncompressed_blob");

	byte		page_buf[UNIV_PAGE_SIZE_MAX];
	uint64_t	calc_length = 0;
	uint64_t	part_len;
	page_no_t	next_page_num = first_blob_page_num;
	bool		error = false;

	do {
		ib::dbug() << "Reading blob from page no " << next_page_num;

		if (fetch_page(ts, next_page_num, UNIV_PAGE_SIZE_MAX, page_buf)
		    == IB_ERROR) {
			ib::error() <<  "Reading blob page " << next_page_num
				<< " failed";
			error = true;
			break;
		}

		if (fil_page_get_type(page_buf) != FIL_PAGE_SDI_BLOB) {
			ib::error() << "Unexpected BLOB page type "
				<< fil_page_get_type(page_buf) << " found."
				" Expected BLOB page type is "
				<< FIL_PAGE_SDI_BLOB;
			error = true;
			break;
		}

		part_len = mach_read_from_4(page_buf + FIL_PAGE_DATA
					    + lob::LOB_HDR_PART_LEN);

		memcpy(dest_buf + calc_length,
		       page_buf + FIL_PAGE_DATA + lob::LOB_HDR_SIZE,
		       static_cast<size_t>(part_len));

		calc_length += part_len;

		next_page_num = mach_read_from_4(
			page_buf + FIL_PAGE_DATA
			+ lob::LOB_HDR_NEXT_PAGE_NO);

		if (next_page_num <= SDI_BLOB_ALLOWED) {
			ib::error() << "Unexpected next BLOB page number. "
				" The first blob page number cannot start "
				" before page number " << SDI_BLOB_ALLOWED;
			error = true;
			break;
		}

	} while (next_page_num != FIL_NULL);


	if (!error) {
		ut_ad(calc_length == total_off_page_length);
	}
	DBUG_RETURN(calc_length);
}

/** Read the compressed blob stored in off-pages to the buffer.
@param[in]	ts			tablespace structure
@param[in]	first_blob_page_num	first blob page number of the chain
@param[in]	total_off_page_length	total Length of blob stored in record
@param[in,out]	dest_buf		blob will be copied to this buffer
@return 0 if blob is not read, else the total length of blob read from
off-pages */
uint64_t
ibd2sdi::copy_compressed_blob(
	ib_tablespace*	ts,
	page_no_t	first_blob_page_num,
	uint64_t	total_off_page_length,
	byte*		dest_buf)
{
	DBUG_ENTER("copy_compressed_blob");

	byte		page_buf[UNIV_PAGE_SIZE_MAX];
	uint64_t	calc_length = 0;
	uint64_t	part_len;
	page_no_t	page_num = first_blob_page_num;
	z_stream	d_stream;
	int		err;
	bool		error = false;
	page_size_t	page_size = ts->get_page_size();

	d_stream.next_out = dest_buf;
	d_stream.avail_out = static_cast<uInt>(total_off_page_length);
	d_stream.next_in = Z_NULL;
	d_stream.avail_in = 0;

	/* Zlib inflate needs 32KB for the default window size, plus
	a few KB for small objects */
	mem_heap_t*	heap = mem_heap_create(40000);
	page_zip_set_alloc(&d_stream, heap);

	ut_ad(page_size.is_compressed());

	err = inflateInit(&d_stream);
	ut_a(err == Z_OK);

	for (;;) {

		if (fetch_page(ts, page_num, UNIV_PAGE_SIZE_MAX, page_buf)
		    == IB_ERROR) {
			ib::error() << "Reading compressed blob page "
				<< page_num << " failed";
			break;
		}

		if (fil_page_get_type(page_buf) != FIL_PAGE_SDI_ZBLOB) {
			ib::error() << "Unexpected BLOB page type "
				<< fil_page_get_type(page_buf) << " found."
				" Expected BLOB page type is "
				<< FIL_PAGE_SDI_ZBLOB;
			error = true;
			break;
		}

		part_len = mach_read_from_4(page_buf + FIL_PAGE_DATA
					    + lob::LOB_HDR_PART_LEN);

		page_no_t	next_page_num = mach_read_from_4(
			page_buf + FIL_PAGE_NEXT);
		space_id_t	space_id = mach_read_from_4(
			page_buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		d_stream.next_in = page_buf + FIL_PAGE_DATA;
		d_stream.avail_in = static_cast<uInt>(page_size.physical()
						      - FIL_PAGE_DATA);
		calc_length += part_len;
		err = inflate(&d_stream, Z_NO_FLUSH);
		switch (err) {
		case Z_OK:
			if (!d_stream.avail_out) {
				goto func_exit;
			}
			break;
		case Z_STREAM_END:
			if (next_page_num == FIL_NULL) {
				goto func_exit;
			}
			/* fall through */
		default:
inflate_error:
		{
			page_id_t page_id(space_id, page_num);
			ib::error() << "Inflate() of compressed BLOB page "
				 << page_id << " returned " << err
				 << " (" << d_stream.msg << ")";
		}
		case Z_BUF_ERROR:
			goto func_exit;
		}

		if (next_page_num == FIL_NULL
		    || next_page_num <= SDI_BLOB_ALLOWED) {
			if (!d_stream.avail_in) {
				page_id_t page_id(space_id, page_num);
				ib::error() << "Unexpected end of compressed"
					<< " BLOB page " << page_id;
			} else {
				err = inflate(&d_stream, Z_FINISH);
				switch (err) {
				case Z_STREAM_END:
				case Z_BUF_ERROR:
					break;
				default:
					goto inflate_error;
				}
			}
		}

		page_num = next_page_num;

	}

func_exit:
	inflateEnd(&d_stream);
	mem_heap_free(heap);
	if (!error) {
		ut_ad(d_stream.total_out == total_off_page_length);
	}
	DBUG_RETURN(d_stream.total_out);
}

/** Reach to B-Tree level zero and read the first (leftmost) page into
the buffer
@param[in]	ts		tablespace structure
@param[in]	buf_len		buffer length
@param[in,out]	buf		the page read at level zero will stored in
				this buffer
@param[in]	root_page_num	the root page number of the B-Tree
@retval		SUCCESS		success
@retval		FAILURE		failure
@retval		NO_RECORSDS	zero records found on root page */
err_t
ibd2sdi::reach_to_leftmost_leaf_level(
	ib_tablespace*	ts,
	uint32_t	buf_len,
	byte*		buf,
	page_no_t	root_page_num)
{
	DBUG_ENTER("reach_to_leftmost_leaf_level");

	/* 1. Read root page */
	uint64_t	page_level = read_page_and_return_level(
		ts, buf_len, buf, root_page_num);

	ib::dbug() << "Root page level is " << page_level;

	if (page_level == UINT64_MAX) {
		ib::error() << "Couldn't reach upto level zero";
		DBUG_RETURN(FALIURE);
	}

	/* 2. Get Number of records in root page */

	ulint	num_of_recs = mach_read_from_2(
		buf + FIL_PAGE_DATA + PAGE_N_RECS);

	if (num_of_recs == 0) {
		ib::dbug() << "Number of records is zero. Nothing to process";
		DBUG_RETURN(NO_RECORDS);
	}

	if (page_level == 0) {
		DBUG_RETURN(SUCCESS);
	}

	page_no_t	cur_page_num = root_page_num;
	byte		rec_type_byte;
	byte		rec_type;
	/* 3. Reach to Level zero */
	while (page_level != 0 && page_level != UINT64_MAX) {

		/* Find infimum record, from infimum, we can find first
		record */

		rec_type_byte = *(buf + PAGE_NEW_INFIMUM - REC_OFF_TYPE);

		/* Retrieve the 3bits to determine the rec type */
		rec_type = rec_type_byte & 0x7;

		if (rec_type != REC_STATUS_INFIMUM) {
			ib::error() << "INFIMUM not found on index page "
				<< cur_page_num;
			break;
		} else {
			ib::dbug() << "INFIMUM found";
		}

		ulint	next_rec_off_t	= mach_read_from_2(
			buf + PAGE_NEW_INFIMUM - REC_OFF_NEXT);

		ib::dbug() << "Next record offset is " << next_rec_off_t;

		page_no_t	child_page_num = mach_read_from_4(
			buf + PAGE_NEW_INFIMUM + next_rec_off_t
			+ REC_DATA_ID_LEN + REC_DATA_TYPE_LEN);

		ib::dbug() << "Next leftmost child page number is "
			<< child_page_num;

		if (child_page_num < SDI_BLOB_ALLOWED) {
			ib::error() << "Invalid child page number: "
				<< child_page_num << " found";
			DBUG_RETURN(FALIURE);
		}

		uint64_t	curr_page_level = page_level;

		page_level = read_page_and_return_level(
			ts, buf_len, buf, child_page_num);

		if (page_level != curr_page_level -1) {
			break;
		}
	}

	if (page_level != 0) {
		ib::error() << "Leftmost leaf level page not found"
			<< " or invalid";
		DBUG_RETURN(FALIURE);
	} else {
		ib::dbug() << "Reached leaf level";
		DBUG_RETURN(SUCCESS);
	}
}

/** Extract SDI record fields
@param[in]	ts		tablespace structure
@param[in]	rec		pointer to record
@param[in,out]	sdi_id		sdi id
@param[in,out]	sdi_type	sdi type
@param[in,out]	sdi_data	sdi blob
@param[in,out]	sdi_data_len	length of sdi blob */
void
ibd2sdi::parse_fields_in_rec(
	ib_tablespace*	ts,
	byte*		rec,
	uint64_t*	sdi_id,
	uint64_t*	sdi_type,
	byte**		sdi_data,
	uint64_t*	sdi_data_len)
{
	DBUG_ENTER("parse_fields_in_rec");

	const page_size_t&	page_size = ts->get_page_size();

	*sdi_id = mach_read_from_8(rec + REC_OFF_DATA_ID);
	*sdi_type = mach_read_from_4(rec + REC_OFF_DATA_TYPE);

	if (m_skip_data) {
		DBUG_VOID_RETURN;
	}

	byte	rec_data_len_partial = *(rec - REC_MIN_HEADER_SIZE - 1);

	uint64_t	rec_data_length;
	bool		is_rec_data_external = false;
	uint32_t	rec_data_in_page_len = 0;

	if (rec_data_len_partial & 0x80) {
		/* Rec length is store in two bytes. Read next
		byte and calculate the total length. */

		rec_data_in_page_len = (rec_data_len_partial & 0x3f) << 8;
		if (rec_data_len_partial & 0x40) {
			is_rec_data_external = true;
			/* Rec is stored externally with 768 byte prefix
			inline */
			rec_data_length = mach_read_from_8(
				rec + REC_OFF_DATA_VARCHAR
				+ rec_data_in_page_len + lob::BTR_EXTERN_LEN);

			rec_data_length += rec_data_in_page_len;
		} else {
			rec_data_length = *(rec - REC_MIN_HEADER_SIZE - 2);
			rec_data_length += rec_data_in_page_len;
		}
	} else {
		/* Rec length is <= 127. Read the length from
		one byte only. */
		rec_data_length = rec_data_len_partial;
	}

	byte*	str = static_cast<byte*>(
		calloc(static_cast<size_t>(rec_data_length + 1),
		       sizeof(char)));

	byte*	rec_data_origin = rec + REC_OFF_DATA_VARCHAR;

	if (is_rec_data_external) {
		/* TODO: This 768 check should be removed when this tool
		supports only REC_FORMAT_DYNAMIC */
		ut_ad(rec_data_in_page_len == 0
		      || rec_data_in_page_len == REC_ANTELOPE_MAX_INDEX_COL_LEN);

		if (rec_data_in_page_len != 0) {
			memcpy(str, rec_data_origin, rec_data_in_page_len);
		}

		/* Copy from off-page blob-pages */
		page_no_t	first_blob_page_num = mach_read_from_4(
			rec + REC_OFF_DATA_VARCHAR
			+ rec_data_in_page_len
			+ lob::BTR_EXTERN_PAGE_NO);

		uint64_t	blob_len_retrieved;
		if (page_size.is_compressed()) {
			blob_len_retrieved = copy_compressed_blob(
				ts,
				first_blob_page_num,
				rec_data_length - rec_data_in_page_len,
				str + rec_data_in_page_len);
		} else {
			blob_len_retrieved = copy_uncompressed_blob(
				ts,
				first_blob_page_num,
				rec_data_length - rec_data_in_page_len,
				str + rec_data_in_page_len);
		}
		*sdi_data_len = rec_data_in_page_len + blob_len_retrieved;
	} else {
		memcpy(str, rec_data_origin, static_cast<size_t>(rec_data_length));
		*sdi_data_len = rec_data_length;
	}

	*sdi_data_len = rec_data_length;
	*sdi_data = str;

	DBUG_VOID_RETURN;
}

/** Return the record type
@param[in]	rec	sdi record in a page
@return	the type of record */
byte
ibd2sdi::get_rec_type(
	byte*	rec)
{
	byte	rec_type_byte = *(rec - REC_OFF_TYPE);

	/* Retrieve the 3bits to determine the rec type */
	return(rec_type_byte & 0x7);
}

/** Return the location of the next record
@param[in]	ts		tablespace structure
@param[in]	current_rec	current sdi record in a page
@param[in]	buf_len		buffer length
@param[in]	buf		page containing the current rec
@param[out]	corrupt		true if corruption detected, else false
@return location of the next record, else NULL if there are no
user records */
byte*
ibd2sdi::get_next_rec(
	ib_tablespace*	ts,
	byte*		current_rec,
	uint32_t	buf_len,
	byte*		buf,
	bool*		corrupt)
{
	DBUG_ENTER("get_next_rec");

	byte*		next_rec;
	page_no_t	page_num = mach_read_from_4(buf + FIL_PAGE_OFFSET);
	ulint		next_rec_off_t = mach_read_from_2(
		current_rec - REC_OFF_NEXT);

	if (next_rec_off_t == 0) {
		ib::error() << "Record Corruption detected. Aborting";
		*corrupt = true;
		DBUG_RETURN(NULL);
	}

	if ((next_rec_off_t >> 15) == 1) {
		/* offset is negative */
		next_rec_off_t = 0x10000 - next_rec_off_t;
		next_rec = current_rec - next_rec_off_t;
	} else {
		next_rec = current_rec + next_rec_off_t;
	}

	if (get_rec_type(next_rec) == REC_STATUS_SUPREMUM) {
		/* Reached last record on current page.
		Fetch record from next_page */

		if (memcmp(next_rec, "supremum", strlen("supremum")) != 0) {

			ib::warn() << "supremum record payload on page "
				<< page_num
				<<  " is corrupted";
		}

		ulint	supremum_next_rec_off =
			mach_read_from_2(next_rec - REC_OFF_NEXT);

		if (supremum_next_rec_off != 0) {

			ib::warn() << "Unexpected next-rec offset "
				<< supremum_next_rec_off
				<< " of supremum record on page "
				<< page_num;
		}

		page_no_t next_page_num = mach_read_from_4(buf + FIL_PAGE_NEXT);

		if (next_page_num == FIL_NULL) {
			*corrupt = false;
			DBUG_RETURN(NULL);
		}

		if(fetch_page(ts, next_page_num, buf_len, buf) == IB_ERROR) {
			ib::error() <<  "Couldn't read next page "
				<< next_page_num;
			*corrupt = true;
			DBUG_RETURN(NULL);
		} else {
			ib::dbug() << "Read page number: " << next_page_num;
		}

		ulint	page_type = fil_page_get_type(buf);

		if (page_type != FIL_PAGE_SDI) {
			ib::error() << "Unexpected page type: " << page_type
				<< ". Expected page type: " << FIL_PAGE_SDI;
			*corrupt = true;
			DBUG_RETURN(NULL);
		}

		if (ts->inc_num_of_recs_on_page(next_page_num)) {
			*corrupt = true;
			DBUG_RETURN(NULL);
		}
		next_rec = get_first_user_rec(buf_len, buf);
	} else {
		if (ts->inc_num_of_recs_on_page(page_num)) {
			*corrupt = true;
			DBUG_RETURN(NULL);
		}
	}

	*corrupt = false;

	DBUG_RETURN(next_rec);
}

/** Write the extracted SDI record fields to outfile
if passed, else to stdout.
@param[in]	sdi_id		sdi id
@param[in]	sdi_type	sdi type
@param[in]	sdi_data	sdi data
@param[in]	sdi_data_len	sdi data len
@param[in]	out_stream	file stream to dump SDI */
void
ibd2sdi::dump_sdi_rec(
	uint64_t	sdi_id,
	uint64_t	sdi_type,
	byte*		sdi_data,
	uint64_t	sdi_data_len,
	FILE*		out_stream)
{
	fprintf(out_stream, "[\n");
	fprintf(out_stream, " [\"ibd2sdi\", {\"id\":" UINT64PF ", \"type\":"
		UINT64PF "}],\n", sdi_id, sdi_type);
	if (!m_skip_data) {
		ut_ad(sdi_data != NULL);
		fprintf(out_stream, " [");
		fwrite(sdi_data, 1, static_cast<size_t>(sdi_data_len),
		       out_stream);
		fprintf(out_stream, "]\n");
	}

	fprintf(out_stream, "],\n");
	fflush(out_stream);
}

/** Iterate over the records in both SDI copies and compare them
@param[in]	ts		tablespace structure
@param[in]	buf_copy_0_len	length of buffer containing copy 0
@param[in]	buf_copy_0	buffer containing the left-most page number of
				copy 0
@param[in]	buf_copy_1_len	length of buffer containing copy 1
@param[in]	buf_copy_1	buffer containing the left-most page number of
				copy 1
@return false on success, true on corruption */
bool
ibd2sdi::dump_recs_on_page_compare(
	ib_tablespace*	ts,
	uint32_t	buf_copy_0_len,
	byte*		buf_copy_0,
	uint32_t	buf_copy_1_len,
	byte*		buf_copy_1)
{
	DBUG_ENTER("dump_recs_on_page_compare");

	byte*		rec_0 = get_first_user_rec(buf_copy_0_len, buf_copy_0);
	byte*		rec_1 = get_first_user_rec(buf_copy_1_len, buf_copy_1);
	bool		explicit_sdi_rec_found = false;
	uint64_t	sdi_id;
	uint64_t	sdi_type;
	byte*		sdi_data = NULL;
	uint64_t	sdi_data_len;
	bool		corrupt_0 = false;
	bool		corrupt_1 = false;

	while (rec_0 != NULL
	       && rec_1 != NULL
	       && !explicit_sdi_rec_found
	       && !corrupt_0
	       && !corrupt_1) {

		int recs_equal = compare_sdi_recs(
			ts, rec_0, rec_1,
			&sdi_id, &sdi_type, &sdi_data,
			&sdi_data_len);

		if (!recs_equal) {
			corrupt_0 = true;
			corrupt_1 = true;
			break;
		}

		explicit_sdi_rec_found = check_and_dump_record(
			sdi_id, sdi_type, sdi_data,
			sdi_data_len, m_out_stream);

		free(sdi_data);
		sdi_data = NULL;

		if (explicit_sdi_rec_found) {
			break;
		}

		rec_0 = get_next_rec(ts, rec_0, buf_copy_0_len, buf_copy_0,
				     &corrupt_0);
		rec_1 = get_next_rec(ts, rec_1, buf_copy_1_len, buf_copy_1,
				     &corrupt_1);
	}

	if (!corrupt_0 && !corrupt_1) {
		DBUG_RETURN(false);
	} else {
		ib::error() << "Corruption detected when comparing records";
		DBUG_RETURN(true);
	}
}

/** Return pointer to the first user record in a page
@param[in]	buf_len	buffer length
@param[in]	buf	Memory containing entire page
@return pointer to first user record in page or NULL
if corruption detected. */
byte*
ibd2sdi::get_first_user_rec(
	uint32_t	buf_len,
	byte*		buf)
{
	DBUG_ENTER("get_first_user_rec");
	ulint	next_rec_off_t = mach_read_from_2(buf + PAGE_NEW_INFIMUM
						  - REC_OFF_NEXT);

	/* First user record shouldn't be supremum */
	ut_ad(PAGE_NEW_INFIMUM + next_rec_off_t != PAGE_NEW_SUPREMUM);

	if (next_rec_off_t > buf_len) {
		ut_ad(0);
		return(NULL);
	}

	if (memcmp(buf + PAGE_NEW_INFIMUM, "infimum",
		   strlen("infimum")) != 0) {

		ib::warn() << "Infimum payload on page "
			<< mach_read_from_4(buf + FIL_PAGE_OFFSET)
			<< " is corrupted";
	}

	ib::dbug() << "Next record offset is " << next_rec_off_t;

	byte*	current_rec = buf + PAGE_NEW_INFIMUM + next_rec_off_t;

	DBUG_RETURN(current_rec);
}

/** Compare two SDI records, if they are equal store the fields
@param[in]	ts			tablespace structure
@param[in]	rec_0			sdi rec from copy 0
@param[in]	rec_1			sdi rec from copy 1
@param[in,out]	sdi_id			sdi id
@param[in,out]	sdi_type		sdi type
@param[in,out]	sdi_data		sdi data
@param[in,out]	sdi_data_len		sdi data len
@return true if sdi recs are equal, else false */
bool
ibd2sdi::compare_sdi_recs(
	ib_tablespace*	ts,
	byte*		rec_0,
	byte*		rec_1,
	uint64_t*	sdi_id,
	uint64_t*	sdi_type,
	byte**		sdi_data,
	uint64_t*	sdi_data_len)
{
	DBUG_ENTER("compare_sdi_recs");
	uint64_t	sdi_id_0;
	uint64_t	sdi_id_1;
	uint64_t	sdi_type_0;
	uint64_t	sdi_type_1;
	byte*		sdi_data_0 = NULL;
	byte*		sdi_data_1 = NULL;
	uint64_t	sdi_data_len_0 = 0;
	uint64_t	sdi_data_len_1 = 0;

	parse_fields_in_rec(ts, rec_0, &sdi_id_0, &sdi_type_0, &sdi_data_0,
			    &sdi_data_len_0);

	parse_fields_in_rec(ts, rec_1, &sdi_id_1, &sdi_type_1, &sdi_data_1,
			    &sdi_data_len_1);

	if ((sdi_id_0 == sdi_id_1)
	    && (sdi_type_0 == sdi_type_1)
	    && (sdi_data_len_0 == sdi_data_len_1)
	    && (m_skip_data
	        || memcmp(sdi_data_0, sdi_data_1,
			  static_cast<size_t>(sdi_data_len_0)) == 0)) {
		*sdi_id = sdi_id_0;
		*sdi_type = sdi_type_0;
		*sdi_data_len = sdi_data_len_0;
		*sdi_data = sdi_data_0;

		free(sdi_data_1);
		DBUG_RETURN(true);
	} else {
		/* Recs are not equal */
		free(sdi_data_0);
		free(sdi_data_1);
		DBUG_RETURN(false);
	}
}

/** Check the SDI record with user passed SDI record id & type
and dump only if id & type matches
@param[in]	sdi_id		sdi id
@param[in]	sdi_type	sdi type
@param[in]	sdi_data	sdi data
@param[in]	sdi_data_len	sdi data len
@param[in]	out_stream	file stream to dump SDI
@return true if the record matched with the user passed SDI record */
bool
ibd2sdi::check_and_dump_record(
	uint64_t	sdi_id,
	uint64_t	sdi_type,
	byte*		sdi_data,
	uint64_t	sdi_data_len,
	FILE*		out_stream)
{
	bool	explicit_sdi_rec_found = false;

	if (m_is_specific_rec) {
		if (m_specific_id == sdi_id
		    && m_specific_type == sdi_type) {
			explicit_sdi_rec_found = true;
			dump_sdi_rec(
				sdi_id, sdi_type, sdi_data,
				sdi_data_len, out_stream);
		} else if (sdi_id > m_specific_id
			   && sdi_type > m_specific_type) {
			/* This is set to make search for specific SDI
			record end faster. */
			explicit_sdi_rec_found = true;
		}
	} else {
		if(m_specific_id == sdi_id
		   || m_specific_type == sdi_type
		   || (m_specific_id == UINT64_MAX
		       && m_specific_type == UINT64_MAX)) {
			dump_sdi_rec(sdi_id, sdi_type, sdi_data,
				     sdi_data_len, out_stream);
		}
	}
	return(explicit_sdi_rec_found);
}

/** Process the files passed in constructor. We do this as
method instead of constructor because we might fail when processing
ibd files passed.
@return false on success, true on failure */
bool
ibd2sdi::process_files()
{
	m_tablespace_creator = new tablespace_creator(m_num_files, m_ibd_files);
	return(m_tablespace_creator->create());
}

/** Dump SDI in a tablespace
@return false on success, true on failure */
bool
ibd2sdi::dump()
{
	bool	ret = true;
	ut_a(m_tablespace_creator != 0);
	ib_tablespace* ts = m_tablespace_creator->get_tablespace();
	ret = process_sdi_from_copy(ts);
	return(ret);
}

/** Dump SDI of a given SDI id & type
@param[in]	sdi_id		SDI id
@param[in]	sdi_type	SDI type
@return false on success, true on failure */
bool
ibd2sdi::dump_one_sdi(
	uint64_t	sdi_id,
	uint64_t	sdi_type)
{
	m_specific_id = sdi_id;
	m_specific_type = sdi_type;
	m_is_specific_rec = true;

	bool	ret = dump();

	m_specific_id = UINT64_MAX;
	m_specific_id = UINT64_MAX;
	m_is_specific_rec = false;

	return(ret);
}

/** Dump all SDI records matching with SDI id
@param[in]	sdi_id		SDI id
@return false on success, true on failure */
bool
ibd2sdi::dump_matching_ids(
	uint64_t	sdi_id)
{
	m_specific_id = sdi_id;
	bool	ret = dump();
	m_specific_id = UINT64_MAX;

	return(ret);
}

/** Dump all SDI records matching with SDI type
@param[in]	sdi_type	SDI type
@return false on success, true on failure */
bool
ibd2sdi::dump_matching_types(
	uint64_t	sdi_type)
{
	m_specific_type = sdi_type;
	bool	ret = dump();
	m_specific_type = UINT64_MAX;

	return(ret);
}

int
main(
	int	argc,
	char	**argv)
{
	/* Buffer to hold temporary file name. */
	char		tmp_filename_buf[FN_REFLEN];
	FILE*		dump_file;
	bool		ret = true;

	ut_crc32_init();
	MY_INIT(argv[0]);
	DBUG_ENTER("main");
	DBUG_PROCESS(argv[0]);

	if (get_options(&argc,&argv)) {
		DBUG_RETURN(1);
	}

	if (opts.no_checksum
	    && srv_checksum_algorithm != SRV_CHECKSUM_ALGORITHM_INNODB)
	{
		ib::error() << "Invalid combination of options. Cannot use"
			       " --no-check & --strict-check together";
		DBUG_RETURN(1);
	}

	if (opts.is_sdi_id && opts.is_sdi_type) {
		opts.is_sdi_rec = true;
	}

	if (opts.is_dump_file) {
		dump_file = create_tmp_file(tmp_filename_buf, "ib_sdi");

		if (dump_file == NULL) {
			ib::error() << "Invalid Dumpfile passed";
			DBUG_RETURN(1);
		}
	} else {
		dump_file = stdout;
	}

	ibd2sdi	sdi(
		argc, argv, dump_file,
		opts.is_read_from_copy ? opts.copy_num : UINT32_MAX,
		opts.skip_data);

	if (sdi.process_files()) {
		DBUG_RETURN(1);
	}

	if (opts.is_sdi_rec) {
		ret = sdi.dump_one_sdi(opts.sdi_rec_id, opts.sdi_rec_type);
	} else if (opts.is_sdi_id) {
		ret = sdi.dump_matching_ids(opts.sdi_rec_id);
	} else if (opts.is_sdi_type) {
		ret = sdi.dump_matching_types(opts.sdi_rec_type);
	} else {
		ret = sdi.dump();
	}

	if (!ret && opts.is_dump_file) {
		/* Rename file can fail if the source and destination
		are across parititions. */
		if (my_rename(
			tmp_filename_buf, opts.dump_filename,
			MYF(0)) == -1) {

			if (my_copy(
				tmp_filename_buf,
				opts.dump_filename, MYF(0)) != 0) {

				ib::error() << "Copy failed: from: "
					<< tmp_filename_buf
					<< " to: "
					<< opts.dump_filename
					<< " because of system error: "
					<< strerror(errno);

				ib::error() << "Please check contents of"
					<< " temporary file "
					<< tmp_filename_buf
					<< " and delete it manually";
				DBUG_RETURN(1);
			} else {
				if (my_delete(tmp_filename_buf, MYF(0)) != 0) {
					ib::warn() << "Removal of temporary"
						<< " file "
						<< tmp_filename_buf
						<< " failed because of system"
						<< " error: "
						<< strerror(errno);
				}
			}
		}
	} else if (opts.is_dump_file) {
		if (my_delete(tmp_filename_buf, MYF(0)) != 0) {
			ib::warn() << "Removal of temporary"
				<< " file " << tmp_filename_buf
				<< " failed because of system error: "
				<< strerror(errno);
		}
	}

	DBUG_RETURN(ret ? 1 : 0);
}
