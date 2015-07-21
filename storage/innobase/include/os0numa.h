/*****************************************************************************

Copyright (c) 2015, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/os0numa.h
NUMA API wrapper over various operating system specific APIs

Created Jul 16, 2015 Vasil Dimov
*******************************************************/

#ifndef os0numa_h
#define os0numa_h

#include "univ.i"

#ifdef HAVE_LIBNUMA
#include <numa.h>
#endif /* HAVE_LIBNUMA */

#ifdef HAVE_SCHED_GETCPU
#include <utmpx.h>
#endif /* HAVE_SCHED_GETCPU */

/** Check if NUMA is available. This function must be called before any
other os_numa_*() functions and it must return != -1, otherwise the behavior
of the rest of the functions is undefined.
@return != -1 if available. */
inline
int
os_numa_available()
{
#ifdef HAVE_LIBNUMA
	return(numa_available());
#else
	return(-1);
#endif /* HAVE_LIBNUMA */
}

/** Get the number of CPUs in the system, including disabled ones.
@return number of CPUs */
inline
int
os_numa_num_configured_cpus()
{
#ifdef HAVE_LIBNUMA
	return(numa_num_configured_cpus());
#else
	ut_error;
	return(-1);
#endif /* HAVE_LIBNUMA */
}

/** Get the NUMA node of a given CPU.
@param[in]	cpu	CPU whose NUMA node to return
@return NUMA node id */
inline
int
os_numa_node_of_cpu(
	int	cpu)
{
#ifdef HAVE_LIBNUMA
	return(numa_node_of_cpu(cpu));
#else
	ut_error;
	return(-1);
#endif /* HAVE_LIBNUMA */
}

/** Allocate a memory on a given NUMA node.
@param[in]	size	number of bytes to allocate
@param[in]	node	NUMA node on which to allocate the memory
@return pointer to allocated memory or NULL if allocation failed */
inline
void*
os_numa_alloc_onnode(
	size_t	size,
	int	node)
{
#ifdef HAVE_LIBNUMA
	return(numa_alloc_onnode(size, node));
#else
	ut_error;
	return(NULL);
#endif /* HAVE_LIBNUMA */
}

/** Free a memory allocated by os_numa_alloc_onnode().
@param[in]	ptr	pointer to memory to free
@param[in]	size	size of the memory */
inline
void
os_numa_free(
	void*	ptr,
	size_t	size)
{
#ifdef HAVE_LIBNUMA
	numa_free(ptr, size);
#else
	ut_error;
#endif /* HAVE_LIBNUMA */
}

#if defined(HAVE_SCHED_GETCPU) || defined(_WIN32)

#define HAVE_OS_GETCPU

/** Get the number of the CPU that executes the current thread now.
@return CPU number */
inline
int
os_getcpu()
{
#if defined(HAVE_SCHED_GETCPU)
	return(sched_getcpu());
#elif defined(_WIN32)
	PROCESSOR_NUMBER	p;

	GetCurrentProcessorNumberEx(&p);

	return(static_cast<int>(p.Group << 6 | p.Number));
#endif
}
#endif /* HAVE_SCHED_GETCPU || _WIN32 */

#endif /* os0numa_h */
