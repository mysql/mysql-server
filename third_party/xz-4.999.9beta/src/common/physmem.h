/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       physmem.h
/// \brief      Get the amount of physical memory
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PHYSMEM_H
#define PHYSMEM_H

// Test for Windows first, because we want to use Windows-specific code
// on Cygwin, which also has memory information available via sysconf(), but
// on Cygwin 1.5 and older it gives wrong results (from our point of view).
#if defined(_WIN32) || defined(__CYGWIN__)
#	ifndef _WIN32_WINNT
#		define _WIN32_WINNT 0x0500
#	endif
#	include <windows.h>

#elif defined(HAVE_PHYSMEM_SYSCONF)
#	include <unistd.h>

#elif defined(HAVE_PHYSMEM_SYSCTL)
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#	ifdef HAVE_SYS_SYSCTL_H
#		include <sys/sysctl.h>
#	endif

#elif defined(HAVE_PHYSMEM_SYSINFO)
#	include <sys/sysinfo.h>

#elif defined(__DJGPP__)
#	include <dpmi.h>
#endif


/// \brief      Get the amount of physical memory in bytes
///
/// \return     Amount of physical memory in bytes. On error, zero is
///             returned.
static inline uint64_t
physmem(void)
{
	uint64_t ret = 0;

#if defined(_WIN32) || defined(__CYGWIN__)
	if ((GetVersion() & 0xFF) >= 5) {
		// Windows 2000 and later have GlobalMemoryStatusEx() which
		// supports reporting values greater than 4 GiB. To keep the
		// code working also on older Windows versions, use
		// GlobalMemoryStatusEx() conditionally.
		HMODULE kernel32 = GetModuleHandle("kernel32.dll");
		if (kernel32 != NULL) {
			BOOL (WINAPI *gmse)(LPMEMORYSTATUSEX) = GetProcAddress(
					kernel32, "GlobalMemoryStatusEx");
			if (gmse != NULL) {
				MEMORYSTATUSEX meminfo;
				meminfo.dwLength = sizeof(meminfo);
				if (gmse(&meminfo))
					ret = meminfo.ullTotalPhys;
			}
		}
	}

	if (ret == 0) {
		// GlobalMemoryStatus() is supported by Windows 95 and later,
		// so it is fine to link against it unconditionally. Note that
		// GlobalMemoryStatus() has no return value.
		MEMORYSTATUS meminfo;
		meminfo.dwLength = sizeof(meminfo);
		GlobalMemoryStatus(&meminfo);
		ret = meminfo.dwTotalPhys;
	}

#elif defined(HAVE_PHYSMEM_SYSCONF)
	const long pagesize = sysconf(_SC_PAGESIZE);
	const long pages = sysconf(_SC_PHYS_PAGES);
	if (pagesize != -1 || pages != -1)
		// According to docs, pagesize * pages can overflow.
		// Simple case is 32-bit box with 4 GiB or more RAM,
		// which may report exactly 4 GiB of RAM, and "long"
		// being 32-bit will overflow. Casting to uint64_t
		// hopefully avoids overflows in the near future.
		ret = (uint64_t)(pagesize) * (uint64_t)(pages);

#elif defined(HAVE_PHYSMEM_SYSCTL)
	int name[2] = {
		CTL_HW,
#ifdef HW_PHYSMEM64
		HW_PHYSMEM64
#else
		HW_PHYSMEM
#endif
	};
	union {
		uint32_t u32;
		uint64_t u64;
	} mem;
	size_t mem_ptr_size = sizeof(mem.u64);
	if (!sysctl(name, 2, &mem.u64, &mem_ptr_size, NULL, NULL)) {
		// IIRC, 64-bit "return value" is possible on some 64-bit
		// BSD systems even with HW_PHYSMEM (instead of HW_PHYSMEM64),
		// so support both.
		if (mem_ptr_size == sizeof(mem.u64))
			ret = mem.u64;
		else if (mem_ptr_size == sizeof(mem.u32))
			ret = mem.u32;
	}

#elif defined(HAVE_PHYSMEM_SYSINFO)
	struct sysinfo si;
	if (sysinfo(&si) == 0)
		ret = (uint64_t)(si.totalram) * si.mem_unit;

#elif defined(__DJGPP__)
	__dpmi_free_mem_info meminfo;
	if (__dpmi_get_free_memory_information(&meminfo) == 0
			&& meminfo.total_number_of_physical_pages
				!= (unsigned long)(-1))
		ret = (uint64_t)(meminfo.total_number_of_physical_pages)
				* 4096;
#endif

	return ret;
}

#endif
