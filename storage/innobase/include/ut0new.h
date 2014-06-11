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

#include <limits> /* std::numeric_limits */

#include <stddef.h>
#include <stdlib.h> /* malloc() */
#include <string.h> /* strlen(), strrchr(), strncmp() */

#include "my_global.h" /* needed for headers from mysql/psi/ */
#include "mysql/psi/mysql_memory.h" /* PSI_MEMORY_CALL() */
#include "mysql/psi/psi_memory.h" /* PSI_memory_key, PSI_memory_info */

#include "univ.i"

#include "os0thread.h" /* os_thread_sleep() */
#include "ut0mem.h" /* OUT_OF_MEMORY_MSG */

#ifdef UNIV_PFS_MEMORY

/** Keys for registering allocations with performance schema.
Keep this list alphabetically sorted. */
extern PSI_memory_key	mem_key_btr0btr;
extern PSI_memory_key	mem_key_dict0dict;
extern PSI_memory_key	mem_key_dict0stats;
extern PSI_memory_key	mem_key_dict_stats_index_map_t;
extern PSI_memory_key	mem_key_dict_stats_n_diff_on_level;
extern PSI_memory_key	mem_key_fil0fil;
extern PSI_memory_key	mem_key_other;
extern PSI_memory_key	mem_key_std;
extern PSI_memory_key	mem_key_sync0debug;
extern PSI_memory_key	mem_key_sync_debug_latches;
extern PSI_memory_key	mem_key_trx0trx;
extern PSI_memory_key	mem_key_trx_sys_t_rw_trx_ids;

extern const size_t	pfs_info_size;
extern PSI_memory_info	pfs_info[];

#ifdef ut0new_cc

/* Below are the declarations of mem_key_*, pfs_info_size and pfs_info[]
which only need to go in ut0new.cc, but we have put them here to make
editing easier - avoid having to edit the above in ut0new.h and the below
in ut0new.cc */

/** Keys for registering allocations with performance schema.
Keep this list alphabetically sorted. */
PSI_memory_key	mem_key_btr0btr;
PSI_memory_key	mem_key_dict0dict;
PSI_memory_key	mem_key_dict0stats;
PSI_memory_key	mem_key_dict_stats_index_map_t;
PSI_memory_key	mem_key_dict_stats_n_diff_on_level;
PSI_memory_key	mem_key_fil0fil;
PSI_memory_key	mem_key_other;
PSI_memory_key	mem_key_std;
PSI_memory_key	mem_key_sync0debug;
PSI_memory_key	mem_key_sync_debug_latches;
PSI_memory_key	mem_key_trx0trx;
PSI_memory_key	mem_key_trx_sys_t_rw_trx_ids;

const size_t	pfs_info_size = 12;

/** Auxiliary array of performance schema 'PSI_memory_info'.
Each allocation appears in
performance_schema.memory_summary_global_by_event_name (and alike) in the form
of e.g. 'memory/innodb/NAME' where the last component NAME is picked from
the list below:
1. If key is specified, then the respective name is used
2. Without a specified key, allocations from inside std::* containers use
   mem_key_std
3. Without a specified key, allocations from outside std::* pick up the key
   based on the file name, and if file name is not found in the list below
   then mem_key_other is used. */
PSI_memory_info	pfs_info[pfs_info_size] = {
	{&mem_key_btr0btr, "btr0btr", 0},
	{&mem_key_dict0dict, "dict0dict", 0},
	{&mem_key_dict0stats, "dict0stats", 0},
	{&mem_key_dict_stats_index_map_t, "dict_stats_index_map_t", 0},
	{&mem_key_dict_stats_n_diff_on_level, "dict_stats_n_diff_on_level", 0},
	{&mem_key_fil0fil, "fil0fil", 0},
	{&mem_key_other, "other", 0},
	{&mem_key_std, "std", 0},
	{&mem_key_sync0debug, "sync0debug", 0},
	{&mem_key_sync_debug_latches, "sync_debug_latches", 0},
	{&mem_key_trx0trx, "trx0trx", 0},
	{&mem_key_trx_sys_t_rw_trx_ids, "trx_sys_t::rw_trx_ids", 0},
};

#endif /* ut0new_cc */

#endif /* UNIV_PFS_MEMORY */

/** Setup the internal objects needed for UT_NEW() to operate.
This must be called before the first call to UT_NEW(). */
inline
void
ut_new_boot()
{
#ifdef UNIV_PFS_MEMORY
	PSI_MEMORY_CALL(register_memory)("innodb", pfs_info, pfs_info_size);
#endif /* UNIV_PFS_MEMORY */
}

/** A structure that holds the necessary data for performance schema
accounting. An object of this type is put in front of each allocated block
of memory. This is because the data is needed even when freeing the memory. */
struct ut_new_pfx_t {
	/** Performance schema key. Assigned to a name at startup via
	PSI_MEMORY_CALL(register_memory)() and later used for accounting
	allocations and deallocations with
	PSI_MEMORY_CALL(memory_alloc)(key, size) and
	PSI_MEMORY_CALL(memory_free)(key, size). */
	PSI_memory_key	m_key;

	/** Size of the allocated block in bytes, including this prepended
	aux structure. For example if InnoDB code requests to allocate
	100 bytes, and sizeof(ut_new_pfx_t) is 16, then 116 bytes are
	allocated in total and m_size will be 116. */
	size_t		m_size;
};

/** Allocator class for allocating memory from inside std::* containers. */
template <class T>
class ut_allocator {
public:
	typedef T*		pointer;
	typedef const T*	const_pointer;
	typedef T&		reference;
	typedef const T&	const_reference;
	typedef T		value_type;
	typedef size_t		size_type;
	typedef ptrdiff_t	difference_type;

	/** Default constructor. */
	explicit
	ut_allocator(
		PSI_memory_key	key = PSI_NOT_INSTRUMENTED)
		:
		m_key(key)
	{
	}

	/** Constructor from allocator of another type. */
	template <class U>
	ut_allocator(
		const ut_allocator<U>&	other)
	{
		m_key = other.get_mem_key(NULL, 0, NULL);
	}

	/** Assignment operator, not used, thus disabled. */
	template <class U>
	ut_allocator&
	operator=(
		const ut_allocator<U>&);

	/** Return the maximum number of objects that can be allocated by
	this allocator. */
	size_type
	max_size() const
	{
		return(std::numeric_limits<size_type>::max() / sizeof(T));
	}

	/** Allocate a chunk of memory that can hold 'n_elements' objects of
	type 'T' and trace the allocation.
	If the allocation fails this method may throw an exception. This
	is mandated by the standard and if it returns NULL instead, then
	STL containers that use it (e.g. std::vector) may get confused.
	After successfull allocation the returned pointer must be passed
	to ut_allocator::deallocate() when no longer needed.
	@param[in]	n_elements	number of elements
	@param[in]	hint		pointer to a nearby memory location,
	unused by this implementation
	@param[in]	file		file name of the caller
	@param[in]	line		line number in the file of the caller
	@param[in]	func		function name of the caller
	@return pointer to the allocated memory */
	pointer
	allocate(
		size_type	n_elements,
		const_pointer	hint = NULL,
		const char*	file = NULL,
		int		line = 0,
		const char*	func = NULL)
	{
		if (n_elements == 0) {
			return(NULL);
		}

		if (n_elements > max_size()) {
			throw(std::bad_alloc());
		}

		void*			ptr;
		static const size_t	max_retries = 60;
		size_t			retries = 0;
		const size_t		total_bytes
			= sizeof(ut_new_pfx_t) + n_elements * sizeof(T);

		do {
			ptr = malloc(total_bytes);

			if (ptr == NULL) {
				os_thread_sleep(1000000 /* 1 second */);
			}

			retries++;
		} while (ptr == NULL && retries < max_retries);

		if (ptr == NULL) {
			ib::fatal()
				<< "Cannot allocate " << total_bytes
				<< " bytes of memory after " << max_retries
				<< " retries over " << max_retries
				<< " seconds. OS error: " << strerror(errno)
				<< " (" << errno << "). " << OUT_OF_MEMORY_MSG;
			/* not reached */
			throw std::bad_alloc();
		}

		ut_new_pfx_t*	pfx = static_cast<ut_new_pfx_t*>(ptr);

		allocate_trace(total_bytes, file, line, func, pfx);

		return(reinterpret_cast<pointer>(pfx + 1));
	}

	/** Free a memory allocated by allocate() and trace the deallocation.
	@param[in,out]	ptr	pointer to memory to free
	@param[in]	file	file name of the caller
	@param[in]	line	line number within the file of the caller
	@param[in]	func	function name of the caller */
	void
	deallocate(
		void*		ptr,
		size_type	n_elements = 0,
		const char*	file = NULL,
		int		line = 0,
		const char*	func = NULL)
	{
		if (ptr == NULL) {
			return;
		}

		ut_new_pfx_t*	pfx = static_cast<ut_new_pfx_t*>(ptr) - 1;

		deallocate_trace(pfx, file, line, func);

		free(pfx);
	}

	/** Create an object of type 'T' using the value 'val' over the
	memory pointed by 'p'. */
	void
	construct(
		pointer		p,
		const T&	val)
	{
		new(p) T(val);
	}

	/** Destroy an object pointed by 'p'. */
	void
	destroy(
		pointer	p)
	{
		p->~T();
	}

	/** Return the address of an object. */
	pointer
	address(
		reference	x) const
	{
		return(&x);
	}

	/** Return the address of a const object. */
	const_pointer
	address(
		const_reference	x) const
	{
		return(&x);
	}

	template <class U>
	struct rebind {
		typedef ut_allocator<U>	other;
	};

	/* The following are custom methods, not required by the standard. */

	/** Allocate, trace the allocation and construct 'n_elements' objects
	of type 'T'. If the allocation fails or if some of the constructors
	throws an exception, then this method will return NULL. It does not
	throw exceptions. After successfull completion the returned pointer
	must be passed to delete_array() when no longer needed.
	@param[in]	n_elements	number of elements to allocate
	@param[in]	file		file name of the caller
	@param[in]	line		line number within the file of the caller
	@param[in]	func		function name of the caller
	@return pointer to the first allocated object or NULL */
	pointer
	new_array(
		size_type	n_elements,
		const char*	file,
		int		line,
		const char*	func)
	{
		T*	p;

		try {
			p = allocate(n_elements, NULL, file, line, func);
		} catch (...) {
			return(NULL);
		}

		ut_ad(p != NULL);

		T*	first = p;

		try {
			for (size_type i = 0; i < n_elements; i++) {
				new(p) T;
				++p;
			}
		} catch (...) {
			for (--p; p != first; --p) {
				p->~T();
			}
			first->~T();

			deallocate(first, 0, file, line, func);

			return(NULL);
		}

		return(first);
	}

	/** Destroy, deallocate and trace the deallocation of an array created
	by new_array().
	@param[in,out]	ptr		pointer to the first object in the array
	@param[in]	file		file name of the caller
	@param[in]	line		line number within the file of the caller
	@param[in]	func		function name of the caller */
	void
	delete_array(
		T*		ptr,
		const char*	file,
		int		line,
		const char*	func)
	{
		if (ptr == NULL) {
			return;
		}

		const size_type	n_elements = n_elements_allocated(ptr);

		T*		p = ptr + n_elements - 1;

		for (size_type i = 0; i < n_elements; i++) {
			p->~T();
			--p;
		}

		deallocate(ptr, 0, file, line, func);
	}

	/** Return the performance schema key to use for tracing allocations. */
	PSI_memory_key
	get_mem_key(
		const char*	file,
		int		line,
		const char*	func) const
	{
		if (m_key != PSI_NOT_INSTRUMENTED) {
			return(m_key);
		}

		if (file == NULL) {
			return(mem_key_std);
		}

		/* Assuming 'file' contains something like the following,
		extract the file name without the extenstion out of it by
		setting 'beg' and 'len'.
		...mysql-trunk/storage/innobase/dict/dict0dict.cc:302
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

		for (size_t i = 0; i < pfs_info_size; i++) {
			if (strncmp(pfs_info[i].m_name, beg, len) == 0) {
				return(*pfs_info[i].m_key);
			}
		}

		return(mem_key_other);
	}

private:
	/** Retrieve the size of a memory block allocated by new_array().
	@param[in]	ptr	pointer returned by new_array().
	@return size of memory block */
	size_type
	n_elements_allocated(
		const_pointer	ptr)
	{
		const ut_new_pfx_t*	pfx;

		pfx = reinterpret_cast<const ut_new_pfx_t*>(ptr) - 1;

		const size_type	user_bytes = pfx->m_size - sizeof(ut_new_pfx_t);

		ut_ad(user_bytes % sizeof(T) == 0);

		return(user_bytes / sizeof(T));
	}

	/** Trace a memory allocation.
	After the accounting, the data needed for tracing the deallocation
	later is written into 'pfx'.
	@param[in]	size	number of bytes that were allocated
	@param[in]	file	file name of the caller
	@param[in]	line	line number within the file of the caller
	@param[in]	func	function name of the caller
	@param[out]	pfx	placeholder to store the info which will be
	needed when freeing the memory */
	void
	allocate_trace(
		size_t		size,
		const char*	file,
		int		line,
		const char*	func,
		ut_new_pfx_t*	pfx)
	{
		const PSI_memory_key	key = get_mem_key(file, line, func);

		pfx->m_key = PSI_MEMORY_CALL(memory_alloc)(key, size);
		pfx->m_size = size;
	}

	/** Trace a memory deallocation.
	@param[in]	pfx	info for the deallocation */
	void
	deallocate_trace(
		const ut_new_pfx_t*	pfx,
		const char*		file,
		int			line,
		const char*		func)
	{
		PSI_MEMORY_CALL(memory_free)(pfx->m_key, pfx->m_size);
	}

	PSI_memory_key	m_key;
};

/** Compare two allocators of the same type.
As long as the type of A1 and A2 is the same, a memory allocated by A1
could be freed by A2 even if the pfs mem key is different. */
template <typename T>
inline
bool
operator==(
	const ut_allocator<T>&	lhs,
	const ut_allocator<T>&	rhs)
{
	return(true);
}

/** Compare two allocators of the same type. */
template <typename T>
inline
bool
operator!=(
	const ut_allocator<T>&	lhs,
	const ut_allocator<T>&	rhs)
{
	return(!(lhs == rhs));
}

#ifdef UNIV_PFS_MEMORY

/** A human readable string representing the current function. */
#ifdef _MSC_VER
#define IB_FUNC	__FUNCSIG__
#else
#define IB_FUNC	__PRETTY_FUNCTION__
#endif

/** Allocate, trace the allocation and construct an object.
Use this macro instead of 'new' within InnoDB.
For example: instead of
	Foo*	f = new Foo(args);
use:
	Foo*	f = UT_NEW(Foo(args), mem_key_btr0btr);
Upon failure to allocate the memory, this macro may return NULL. It
will not throw exceptions. After successfull allocation the returned
pointer must be passed to UT_DELETE() when no longer needed.
@param[in]	expr	any expression that could follow "new"
@param[in]	key	performance schema memory tracing key
@return pointer to the created object or NULL */
#define UT_NEW(expr, key) \
	({ \
		char*	_p = ut_allocator<char>(key).allocate( \
			sizeof expr, NULL, __FILE__, __LINE__, IB_FUNC); \
	 /* ut_allocator::construct() can't be used for classes with disabled
	 copy constructors, thus we use placement new here directly. */ \
	 	_p != NULL ? (new(_p) expr) : NULL; \
	 })

/** Allocate, trace the allocation and construct an object.
Use this macro instead of 'new' within InnoDB and instead of UT_NEW()
when creating a dedicated memory key is not feasible.
For example: instead of
	Foo*	f = new Foo(args);
use:
	Foo*	f = UT_NEW_NOKEY(Foo(args));
Upon failure to allocate the memory, this macro may return NULL. It
will not throw exceptions. After successfull allocation the returned
pointer must be passed to UT_DELETE() when no longer needed.
@param[in]	expr	any expression that could follow "new"
@return pointer to the created object or NULL */
#define UT_NEW_NOKEY(expr)	UT_NEW(expr, PSI_NOT_INSTRUMENTED)

/** Destroy, deallocate and trace the deallocation of an object created by
UT_NEW() or UT_NEW_NOKEY().
We can't instantiate ut_allocator without having the type of the object, thus
we redirect this to a templated function. */
#define UT_DELETE(ptr)		ut_delete(ptr, __FILE__, __LINE__, IB_FUNC)

/** Destroy and account object created by UT_NEW() or UT_NEW_NOKEY().
@param[in,out]	ptr		pointer to the first object in the array
@param[in]	file		file name of the caller
@param[in]	line		line number within the file of the caller
@param[in]	func		function name of the caller */
template <typename T>
inline
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

	ut_allocator<T>	allocator;

	allocator.destroy(ptr);
	allocator.deallocate(ptr, 0, file, line, func);
}

/** Allocate and account 'n_elements' objects of type 'type'.
Use this macro to allocate memory within InnoDB instead of 'new[]'.
The returned pointer must be passed to UT_DELETE_ARRAY().
@param[in]	type		type of objects being created
@param[in]	n_elements	number of objects to create
@param[in]	key		performance schema memory tracing key
@return pointer to the first allocated object or NULL */
#define UT_NEW_ARRAY(type, n_elements, key) \
	ut_allocator<type>(key).new_array(n_elements, __FILE__, __LINE__, IB_FUNC)

/** Allocate and account 'n_elements' objects of type 'type'.
Use this macro to allocate memory within InnoDB instead of 'new[]' and
instead of UT_NEW_ARRAY() when it is not feasible to create a dedicated key.
@param[in]	type		type of objects being created
@param[in]	n_elements	number of objects to create
@return pointer to the first allocated object or NULL */
#define UT_NEW_ARRAY_NOKEY(type, n_elements) \
	UT_NEW_ARRAY(type, n_elements, PSI_NOT_INSTRUMENTED)

/** Destroy, deallocate and trace the deallocation of an array created by
UT_NEW_ARRAY() or UT_NEW_ARRAY_NOKEY().
We can't instantiate ut_allocator without having the type of the object, thus
we redirect this to a templated function. */
#define UT_DELETE_ARRAY(ptr) \
	ut_delete_array(ptr, __FILE__, __LINE__, IB_FUNC)

/** Destroy and account objects created by UT_NEW_ARRAY() or
UT_NEW_ARRAY_NOKEY().
@param[in,out]	ptr		pointer to the first object in the array
@param[in]	file		file name of the caller
@param[in]	line		line number within the file of the caller
@param[in]	func		function name of the caller */
template <typename T>
inline
void
ut_delete_array(
	T*		ptr,
	const char*	file,
	int		line,
	const char*	func)
{
	ut_allocator<T>().delete_array(ptr, file, line, func);
}

#else /* UNIV_PFS_MEMORY */

/* Fallbacks when memory tracing is disabled at compile time. */

#define UT_NEW(expr, key)		new(std::nothrow) expr
#define UT_NEW_NOKEY(expr)		new(std::nothrow) expr
#define UT_DELETE(ptr)			delete ptr

#define UT_NEW_ARRAY(type, n_elements, key) \
	new(std::nothrow) type[n_elements]

#define UT_NEW_ARRAY_NOKEY(type, n_elements) \
	new(std::nothrow) type[n_elements]

#define UT_DELETE_ARRAY(ptr)		delete[] ptr

#endif /* UNIV_PFS_MEMORY */

#endif /* ut0new_h */
