/* File: $Id$
   Author: John Wu <John.Wu at acm.org>
      Lawrence Berkeley National Laboratory
   Copyright (c) 2006-2016 the Regents of the University of California
*/
#ifndef IBIS_CAPI_H
#define IBIS_CAPI_H
/**
@file This header file defines a C API for accessing the querying
functionality of FastBit IBIS implementation.  It deals with data
tables as directories and queries as pointers to the struct
FastBitQuery.

@note For functions that return integer error code, 0 always indicate
success, a negative number indicate error, a positive number may also be
returned to carry results, such as in fastbit_get_result_size.

@note For functions that returns pointers, a nil pointer is returned in
case of error.

An example of using these functions is in <A
HREF="http://crd.lbl.gov/~kewu/fastbit/doc/html/tcapi_8c.html">examples/tcapi.c</A>.
*/
#if _MSC_VER >= 1000
#pragma once
#endif /* _MSC_VER >= 1000 */

#if !defined(WITHOUT_FASTBIT_CONFIG_H) && !defined(__MINGW32__) && !defined(_MSC_VER)
#include "fastbit-config.h"
#endif
#if defined(HAVE_SYS_TYPES_H) || defined(__PGI)
#  include <sys/types.h>	/* required by PGI compilers (bug 775993) */
#endif
#if defined(HAVE_INTTYPES_H) || defined(__sun) || defined(__MINGW32__) || defined(__FreeBSD__)
#  include <inttypes.h>
#elif defined(HAVE_STDINT_H) || defined(__unix__) || defined(__APPLE__)
#  include <stdint.h>
#elif defined(_WIN32) && defined(_MSC_VER)
/* MS windows has its own exact-width types, use them */
#  ifndef int16_t
#    define int16_t __int16
#  endif
#  ifndef uint16_t
#    define uint16_t unsigned __int16
#  endif
#  ifndef int32_t
#    define int32_t __int32
#  endif
#  ifndef uint32_t
#    define uint32_t unsigned __int32
#  endif
#  ifndef int64_t
#    define int64_t __int64
#  endif
#  ifndef uint64_t
#    define uint64_t unsigned __int64
#  endif
#else
#error Do NOT know how to find the exact width data types!
#endif

#if defined(_WIN32) && (defined(_MSC_VER) || defined(__MINGW32__))
#  if defined(_USRDLL) || defined(CXX_USE_DLL)
#    if defined(DLL_EXPORT)
#      define FASTBIT_DLLSPEC __declspec(dllexport)
#    else
#      define FASTBIT_DLLSPEC __declspec(dllimport)
#    endif
#  else
#    define FASTBIT_DLLSPEC
#  endif
#else
#  define FASTBIT_DLLSPEC
#endif

#include <stdio.h>	/* FILE* */

/**
@defgroup FastBitCAPI FastBit C API.
@{
*/
#ifdef __cplusplus
extern "C" {
#endif
    /** @brief Build indexes for all columns in the named directory. */
    FASTBIT_DLLSPEC int
    fastbit_build_indexes(const char *indexLocation,
			  const char *indexOptions);
    /** @brief Purge all index files. */
    FASTBIT_DLLSPEC int fastbit_purge_indexes(const char *indexLocation);
    /** @brief Build an index for the named attribute. */
    FASTBIT_DLLSPEC int
    fastbit_build_index(const char *indexLocation, const char* cname,
			const char *indexOptions);
    /** @brief Purge the index of the named attribute. */
    FASTBIT_DLLSPEC int
    fastbit_purge_index(const char *indexLocation, const char* cname);
    FASTBIT_DLLSPEC int fastbit_reorder_partition(const char *dir);

    /** @brief The opaque object to hold a FastBit query. */
    struct FastBitQuery;
    /** @brief A handle to be used by C clients. */
    typedef struct FastBitQuery* FastBitQueryHandle;

    /** @brief Build a new FastBit query. */
    FASTBIT_DLLSPEC FastBitQueryHandle
    fastbit_build_query(const char *selectClause, const char *indexLocation,
			const char *queryConditions);
    /** @brief Free all resource associated with the handle.
	@note The handle becomes invalid. */
    FASTBIT_DLLSPEC int fastbit_destroy_query(FastBitQueryHandle query);

    /** Count the number of columns selected in the select clause of the
	query. */
    FASTBIT_DLLSPEC int fastbit_get_result_columns(FastBitQueryHandle query);
    /** Retrieve the number of hits in the query.  It is also the number of
        rows in the result set.  The arrays returned by
        fastbit_get_qualified_floats shall have this many elements. */
    FASTBIT_DLLSPEC int fastbit_get_result_rows(FastBitQueryHandle query);
    /** Retrieve the row number of those satisfying the query conditions. */
    FASTBIT_DLLSPEC int fastbit_get_result_row_ids
    (FastBitQueryHandle, uint32_t *);

    /** @brief Return the string form of the select clause. */
    FASTBIT_DLLSPEC const char*
    fastbit_get_select_clause(FastBitQueryHandle query);
    /** @brief Return the table name. */
    FASTBIT_DLLSPEC const char*
    fastbit_get_from_clause(FastBitQueryHandle query);
    /** @brief Return the where clause of the query. */
    FASTBIT_DLLSPEC const char*
    fastbit_get_where_clause(FastBitQueryHandle query);

    /**
       @defgroup FastBitRawResult Column-access functiones.
       This set of functions provide column-wise accesses to query results.
       @{
       Return a pointer to an array holding the values of attribute @c att
       that qualifies the specified selection conditions.
       @note The caller can NOT free the memory pointed by the pointer
       returned.  Must call fastbit_destroy_query to free the memory after
       use.  This applies to all other versions of
       fastbit_get_qualified_ttt. */
    FASTBIT_DLLSPEC const float*
    fastbit_get_qualified_floats(FastBitQueryHandle query,
				 const char* cname);
    FASTBIT_DLLSPEC const double*
    fastbit_get_qualified_doubles(FastBitQueryHandle query,
				  const char* cname);
    FASTBIT_DLLSPEC const signed char*
    fastbit_get_qualified_bytes(FastBitQueryHandle query,
				const char* cname);
    FASTBIT_DLLSPEC const int16_t*
    fastbit_get_qualified_shorts(FastBitQueryHandle query,
				 const char* cname);
    FASTBIT_DLLSPEC const int32_t*
    fastbit_get_qualified_ints(FastBitQueryHandle query,
			       const char* cname);
    FASTBIT_DLLSPEC const int64_t*
    fastbit_get_qualified_longs(FastBitQueryHandle query,
				const char* cname);
    FASTBIT_DLLSPEC const unsigned char*
    fastbit_get_qualified_ubytes(FastBitQueryHandle query,
				 const char* cname);
    FASTBIT_DLLSPEC const uint16_t*
    fastbit_get_qualified_ushorts(FastBitQueryHandle query,
				  const char* cname);
    FASTBIT_DLLSPEC const uint32_t*
    fastbit_get_qualified_uints(FastBitQueryHandle query,
				const char* cname);
    FASTBIT_DLLSPEC const uint64_t*
    fastbit_get_qualified_ulongs(FastBitQueryHandle query,
				 const char* cname);
    FASTBIT_DLLSPEC const char**
    fastbit_get_qualified_strings(FastBitQueryHandle query,
				  const char* cname);
    /** @} */

    /** @defgroup FastBitResultSet Row-wise access functiones.
	This set of functions provide row-wise accesses to query results.
	@{
	@brief The opaque object used to hold a result set. */
    struct FastBitResultSet;
    /** @brief A handle to identify a set of query results. */
    typedef struct FastBitResultSet* FastBitResultSetHandle;

    /** @brief Build a new result set from a query object. */
    FASTBIT_DLLSPEC FastBitResultSetHandle
    fastbit_build_result_set(FastBitQueryHandle query);
    /** @brief Destroy a result set. */
    FASTBIT_DLLSPEC int
    fastbit_destroy_result_set(FastBitResultSetHandle rset);

    /** @brief Returns 0 if there are more results, otherwise returns -1. */
    FASTBIT_DLLSPEC int fastbit_result_set_next(FastBitResultSetHandle);
    /** @brief Returns 0 if there are more bundles, otherwise returns -1. */
    FASTBIT_DLLSPEC int fastbit_result_set_next_bundle(FastBitResultSetHandle);
    /** @brief Get the value of the named column as an integer. */
    FASTBIT_DLLSPEC int
    fastbit_result_set_get_int(FastBitResultSetHandle rset, const char *cname);
    /** @brief Get the value of the named column as an unsigned integer. */
    FASTBIT_DLLSPEC unsigned
    fastbit_result_set_get_unsigned(FastBitResultSetHandle rset,
				    const char *cname);
    /** @brief Get the value of the named column as a long. */
    FASTBIT_DLLSPEC int64_t
    fastbit_result_set_get_long(FastBitResultSetHandle rset,
				const char *cname);
    /** @brief Get the value of the named column as a single-precision
	floating-point number. */
    FASTBIT_DLLSPEC float
    fastbit_result_set_get_float(FastBitResultSetHandle rset,
				 const char *cname);
    /** @brief Get the value of the named column as a double-precision
	floating-point number. */
    FASTBIT_DLLSPEC double
    fastbit_result_set_get_double(FastBitResultSetHandle rset,
				  const char *cname);
    /** @brief Get the value of the named column as a string. */
    FASTBIT_DLLSPEC const char*
    fastbit_result_set_get_string(FastBitResultSetHandle rset,
				  const char *cname);
    /** Get the value of the named column as an integer.
	The argument @c index is the position (starting with 0) of the
	attribute in the select clause.  This should be faster than the one
	with @c cname as argument since it avoids name look up. */
    FASTBIT_DLLSPEC int32_t
    fastbit_result_set_getInt(FastBitResultSetHandle rset,
			      unsigned position);
    /** @brief Get the value of the named column as an unsigned integer. */
    FASTBIT_DLLSPEC uint32_t
    fastbit_result_set_getUnsigned(FastBitResultSetHandle rset,
				   unsigned position);
    /** @brief Get the value of the named column as a long. */
    FASTBIT_DLLSPEC int64_t
    fastbit_result_set_getLong(FastBitResultSetHandle rset,
				unsigned position);
    /** @brief Get the value of the named column as a single-precision
	floating-point number. */
    FASTBIT_DLLSPEC float
    fastbit_result_set_getFloat(FastBitResultSetHandle rset,
				unsigned position);
    /** @brief Get the value of the named column as a double-precision
	floating-point number. */
    FASTBIT_DLLSPEC double
    fastbit_result_set_getDouble(FastBitResultSetHandle rset,
				 unsigned position);
    /** @brief Get the value of the named column as a string. */
    FASTBIT_DLLSPEC const char*
    fastbit_result_set_getString(FastBitResultSetHandle rset,
				 unsigned position);
    /** @} */

    /** @brief Flush the in-memory data to the named directory. */
    FASTBIT_DLLSPEC int fastbit_flush_buffer(const char *dir);
    /** @brief Add @c nelem values of the specified column (@c colname) to
	the in-memory buffer. */
    FASTBIT_DLLSPEC int
    fastbit_add_values(const char *colname, const char *coltype,
		       void *vals, uint32_t nelem, uint32_t start);
    /** @brief Return the number of rows in the data partition. */
    FASTBIT_DLLSPEC int fastbit_rows_in_partition(const char *dir);
    /** @brief Return the number of columns in the data partition. */
    FASTBIT_DLLSPEC int fastbit_columns_in_partition(const char *dir);

    /** @brief Initialization function. */
    FASTBIT_DLLSPEC void fastbit_init(const char *rcfile);
    /** @brief Clean up resources hold by FastBit file manager. */
    FASTBIT_DLLSPEC void fastbit_cleanup(void);
    /** @brief Change the verboseness of FastBit functions. */
    FASTBIT_DLLSPEC int  fastbit_set_verbose_level(int v);
    /** @brief Return the current verboseness level. */
    FASTBIT_DLLSPEC int  fastbit_get_verbose_level(void);
    /** @brief Change the name of the log file. */
    FASTBIT_DLLSPEC int  fastbit_set_logfile(const char* filename);
    /** @brief Find out the name of the current log file. */
    FASTBIT_DLLSPEC const char* fastbit_get_logfile();
    /** @brief Return the file pointer to the log file. */
    FASTBIT_DLLSPEC FILE* fastbit_get_logfilepointer();
    FASTBIT_DLLSPEC double fastbit_read_clock();

    /** @brief Return the software version as a string. */
    FASTBIT_DLLSPEC const char* fastbit_get_version_string();
    /** @brief Return the software version as an integer. */
    FASTBIT_DLLSPEC int fastbit_get_version_number();
#ifdef __cplusplus
}
#endif
/** @} */
#endif /* ifndef IBIS_CAPI_H */
