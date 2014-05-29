/*****************************************************************************

Copyright (c) 2014, 2014, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file ut/ut0new.cc
Instrumented memory allocator.

Created May 26, 2014 Vasil Dimov
*******************************************************/

#include <stdlib.h> /* malloc() */

#include "my_global.h" /* needed for headers from mysql/psi/ */
#include "mysql/psi/mysql_memory.h" /* PSI_MEMORY_CALL() */
#include "mysql/psi/psi_memory.h" /* PSI_memory_key, PSI_memory_info */

#include "univ.i"

#include "os0thread.h" /* os_thread_sleep() */
#include "ut0mem.h" /* OUT_OF_MEMORY_MSG */
#include "ut0new.h"
#include "ut0ut.h" /* UT_ARR_SIZE() */

#ifdef UNIV_PFS_MEMORY

/** Performance schema instrumentation names.
Each allocation appears in
performance_schema.memory_summary_global_by_event_name (and alike) in the form
of e.g. 'memory/innodb/dict0stats' where the last component is picked up from
the list below, based on the file name of the caller. */
static const char*	pfs_names[] = {
	/* Keep in alphabetical order to ease maintenance. */
	"btr0btr",
	"dict0dict",
	"dict0stats",
	"fil0fil",
	"trx0trx",
	/* Keep this as the last one, it is a fallback if none of the above
	matches. */
	"other",
};

/** Auxiliary array of performance schema keys. */
static PSI_memory_key	pfs_keys[UT_ARR_SIZE(pfs_names)];

/** Auxiliary array of performance schema 'PSI_memory_info'. */
static PSI_memory_info	pfs_info[UT_ARR_SIZE(pfs_names)];

#endif /* UNIV_PFS_MEMORY */

/** Setup the internal objects needed for UT_NEW() to operate.
This must be called before the first call to UT_NEW(). */
void
ut_new_boot()
{
#ifdef UNIV_PFS_MEMORY
	const size_t	n = UT_ARR_SIZE(pfs_names);

	for (size_t i = 0; i < n; i++) {
		pfs_info[i].m_key = &pfs_keys[i];
		pfs_info[i].m_name = pfs_names[i];
		pfs_info[i].m_flags = 0;
	}

	PSI_MEMORY_CALL(register_memory)("innodb", pfs_info, n);
#endif /* UNIV_PFS_MEMORY */
}

#ifdef UNIV_PFS_MEMORY

/** A structure that holds the necessary data for performance schema
accounting. An object of this type is put in front of each allocated block
of memory. This is because the data is needed even when freeing the memory. */
struct ut_new_pfx_t {
	PSI_memory_key	m_key;
	size_t		m_size;
};

/** Account a memory allocation.
After the accounting, the data needed for accounting the deallocation later
is written into 'pfx'.
@param[in]	size	number of bytes that were allocated
@param[in]	file	file name of the caller
@param[in]	line	line number within the file of the caller
@param[in]	func	function name of the caller
@param[out]	pfx	placeholder to store the info which will be needed
when freeing the memory */
static
void
ut_new_account(
	size_t		size,
	const char*	file,
	int		line,
	const char*	func,
	ut_new_pfx_t*	pfx)
{
	/* Assuming 'file' contains something like the following, extract
	the file name without the extenstion out of it by setting 'beg' and
	'len'.
	/bzrroot/server/mysql-trunk/storage/innobase/dict/dict0dict.cc:302
	                                                  ^-- beg, len=9
	*/

	const char*	beg = strrchr(file, OS_PATH_SEPARATOR);

	if (beg == NULL) {
		beg = file;
	} else {
		beg++;
	}

	size_t		len = strlen(beg);

	const char*	end = strrchr(beg, '.');

	if (end != NULL) {
		len = end - beg;
	}

	size_t	i;
	/* If none matches, then use the last one as a default, thus -1 below. */
	for (i = 0; i < UT_ARR_SIZE(pfs_names) - 1; i++) {
		if (strncmp(pfs_info[i].m_name, beg, len) == 0) {
			break;
		}
	}

	pfx->m_key = PSI_MEMORY_CALL(memory_alloc)(*pfs_info[i].m_key, size);
	pfx->m_size = size;
}

/** Allocate 'size' bytes of memory and account the allocation.
The returned pointer must be passed to ut_delete_low() when no longer needed.
@param[in]	size	number of bytes to allocate
@param[in]	file	file name of the caller
@param[in]	line	line number within the file of the caller
@param[in]	func	function name of the caller
@return pointer to the allocated memory */
void*
ut_new_low(
	size_t		size,
	const char*	file,
	int		line,
	const char*	func)
{
	void*			ptr;
	static const size_t	max_retries = 60;
	size_t			retries = 0;

	do {
		ptr = malloc(sizeof(ut_new_pfx_t) + size);

		if (ptr == NULL) {
			os_thread_sleep(1000000 /* 1 second */);
		}

		retries++;
	} while (ptr == NULL && retries < max_retries);

	if (ptr == NULL) {
		ib::fatal()
			<< "Cannot allocate " << size << " bytes of memory"
			<< " after " << max_retries << " retries over"
			<< " " << max_retries << " seconds."
			<< " OS error: " << strerror(errno) << " ("
			<< errno << "). " << OUT_OF_MEMORY_MSG;
		/* not reached */
		return(NULL);
	}

	ut_new_pfx_t*	pfx = static_cast<ut_new_pfx_t*>(ptr);

	ut_new_account(size, file, line, func, pfx);

	return(pfx + 1);
}

static
void
ut_delete_account(
	const ut_new_pfx_t*	pfx,
	const char*		file,
	int			line,
	const char*		func)
{
	PSI_MEMORY_CALL(memory_free)(pfx->m_key, pfx->m_size);
}

/** Free a memory allocated by ut_new_low() and account the deallocation.
The pointer must have been returned by ut_new_low().
@param[in,out]	ptr	pointer to memory to free
@param[in]	file	file name of the caller
@param[in]	line	line number within the file of the caller
@param[in]	func	function name of the caller */
void
ut_delete_low(
	void*		ptr,
	const char*	file,
	int		line,
	const char*	func)
{
	ut_new_pfx_t*	pfx = static_cast<ut_new_pfx_t*>(ptr) - 1;

	ut_delete_account(pfx, file, line, func);

	free(pfx);
}

/** Retrieve the size of a memory block allocated by ut_new_low().
@param[in]	ptr	pointer returned by ut_new_low().
@return size of memory block */
size_t
ut_new_size(
	const void*	ptr)
{
	const ut_new_pfx_t*	pfx = static_cast<const ut_new_pfx_t*>(ptr);
	--pfx;
	return(pfx->m_size);
}

#endif /* UNIV_PFS_MEMORY */
