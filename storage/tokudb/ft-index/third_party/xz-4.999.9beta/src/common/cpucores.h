/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       cpucores.h
/// \brief      Get the number of online CPU cores
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CPUCORES_H
#define CPUCORES_H

#if defined(HAVE_CPUCORES_SYSCONF)
#	include <unistd.h>

#elif defined(HAVE_CPUCORES_SYSCTL)
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#	ifdef HAVE_SYS_SYSCTL_H
#		include <sys/sysctl.h>
#	endif
#endif


static inline uint32_t
cpucores(void)
{
	uint32_t ret = 0;

#if defined(HAVE_CPUCORES_SYSCONF)
	const long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (cpus > 0)
		ret = (uint32_t)(cpus);

#elif defined(HAVE_CPUCORES_SYSCTL)
	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	if (!sysctl(name, &cpus, &cpus_size, NULL, NULL)
			&& cpus_size == sizeof(cpus) && cpus > 0)
		ret = (uint32_t)(cpus);
#endif

	return ret;
}

#endif
