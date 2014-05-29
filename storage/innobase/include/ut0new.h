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
@file ut/ut0new.h
Instrumented memory allocator.

Created May 26, 2014 Vasil Dimov
*******************************************************/

#ifndef ut0new_h
#define ut0new_h

#include <stddef.h>

#include "univ.i"

/** Setup the internal objects needed for UT_NEW()/UT_NEW_ARRAY() to operate.
This must be called before the first call to UT_NEW()/UT_NEW_ARRAY(). */
void
ut_new_boot();

#ifdef UNIV_PFS_MEMORY

/** Allocate and account 'size' bytes of memory.
The returned pointer must be passed to ut_delete_low().
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
	const char*	func);

/** Free and account a memory allocated by ut_new_low().
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
	const char*	func);

/** Retrieve the size of a memory block allocated by ut_new_low().
@param[in]	ptr	pointer returned by ut_new_low().
@return size of memory block */
size_t
ut_new_size(
	const void*	ptr);

/** A human readable string representing the current function. */
#ifdef _MSC_VER
#define IB_FUNC	__FUNCSIG__
#else
#define IB_FUNC	__PRETTY_FUNCTION__
#endif

/** Allocate and account an object of the specified type.
Use this macro to allocate memory within InnoDB instead of 'new'.
The returned pointer must be passed to UT_DELETE().
@return pointer to the allocated object or NULL */
#define UT_NEW(expr)	\
	new(ut_new_low(sizeof(expr), __FILE__, __LINE__, IB_FUNC)) expr

/** Destroy and account object created by UT_NEW(). */
#define UT_DELETE(ptr)	\
	ut_delete(ptr, __FILE__, __LINE__, IB_FUNC)

/** Destroy and account object created by UT_NEW().
@param[in,out]	ptr		pointer to the first object in the array
@param[in]	file		file name of the caller
@param[in]	line		line number within the file of the caller
@param[in]	func		function name of the caller */
template <typename T>
void
ut_delete(
	T*		ptr,
	const char*	file,
	int		line,
	const char*	func)
{
	if (ptr == NULL) {
		return;
	}

	ptr->~T();

	ut_delete_low(ptr, file, line, func);
}

/** Allocate and account 'n_elements' objects of type 'type'.
Use this macro to allocate memory within InnoDB instead of 'new[]'.
The returned pointer must be passed to UT_DELETE_ARRAY().
@return pointer to the first allocated object or NULL */
#define UT_NEW_ARRAY(type, n_elements)	\
	ut_new_array<type>(n_elements, __FILE__, __LINE__, IB_FUNC)

/** Allocate and account 'n_elements' objects of type 'T'.
The returned pointer must be passed to ut_delete_array().
@param[in]	n_elements	number of elements to allocate
@param[in]	file		file name of the caller
@param[in]	line		line number within the file of the caller
@param[in]	func		function name of the caller
@return pointer to the first allocated object or NULL */
template <typename T>
T*
ut_new_array(
	size_t		n_elements,
	const char*	file,
	int		line,
	const char*	func)
{
	T*	p = static_cast<T*>(ut_new_low(sizeof(T) * n_elements,
					       file, line, func));

	if (p == NULL) {
		return(NULL);
	}

	T*	beg = p;

	try {
		for (size_t i = 0; i < n_elements; i++) {
			new(p) T;
			++p;
		}
	} catch (...) {
		for (--p; p != beg; --p) {
			p->~T();
		}
		beg->~T();

		ut_delete_low(beg, file, line, func);

		return(NULL);
	}

	return(beg);
}

/** Destroy and account objects created by UT_NEW_ARRAY(). */
#define UT_DELETE_ARRAY(ptr)	\
	ut_delete_array(ptr, __FILE__, __LINE__, IB_FUNC)

/** Destroy and account objects created by ut_new_array().
@param[in,out]	ptr		pointer to the first object in the array
@param[in]	file		file name of the caller
@param[in]	line		line number within the file of the caller
@param[in]	func		function name of the caller */
template <typename T>
void
ut_delete_array(
	T*		ptr,
	const char*	file,
	int		line,
	const char*	func)
{
	if (ptr == NULL) {
		return;
	}

	const size_t	size = ut_new_size(ptr);

	ut_ad(size % sizeof(T) == 0);

	const size_t	n_elements = size / sizeof(T);

	T*		p = ptr + n_elements - 1;

	for (size_t i = 0; i < n_elements; i++) {
		p->~T();
		--p;
	}

	ut_delete_low(ptr, file, line, func);
}

#else /* UNIV_PFS_MEMORY */

/** Allocate an object of the specified type.
Use this macro to allocate memory within InnoDB instead of 'new'.
The returned pointer must be passed to UT_DELETE().
@return pointer to the allocated object or NULL */
#define UT_NEW(expr)			new(std::nothrow) expr

/** Destroy and account object created by UT_NEW(). */
#define UT_DELETE(ptr)			delete ptr

/** Allocate 'n_elements' objects of type 'type'.
Use this macro to allocate memory within InnoDB instead of 'new[]'.
The returned pointer must be passed to UT_DELETE_ARRAY().
@return pointer to the first allocated object or NULL */
#define UT_NEW_ARRAY(type, n_elements)	new(std::nothrow) type[n_elements]

/** Destroy objects created by UT_NEW_ARRAY(). */
#define UT_DELETE_ARRAY(ptr)		delete[] ptr

#endif /* UNIV_PFS_MEMORY */

#endif /* ut0new_h */
