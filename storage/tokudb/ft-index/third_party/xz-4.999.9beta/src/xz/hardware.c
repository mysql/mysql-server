/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       hardware.c
/// \brief      Detection of available hardware resources
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include "physmem.h"
#include "cpucores.h"


/// Maximum number of free *coder* threads. This can be set with
/// the --threads=NUM command line option.
static uint32_t threadlimit;

/// Memory usage limit
static uint64_t memlimit;


extern void
hardware_threadlimit_set(uint32_t new_threadlimit)
{
	if (new_threadlimit == 0) {
		// The default is the number of available CPU cores.
		threadlimit = cpucores();
		if (threadlimit == 0)
			threadlimit = 1;
	} else {
		threadlimit = new_threadlimit;
	}

	return;
}


extern uint32_t
hardware_threadlimit_get(void)
{
	return threadlimit;
}


extern void
hardware_memlimit_set(uint64_t new_memlimit)
{
	if (new_memlimit == 0) {
		// The default is 40 % of total installed physical RAM.
		hardware_memlimit_set_percentage(40);
	} else {
		memlimit = new_memlimit;
	}

	return;
}


extern void
hardware_memlimit_set_percentage(uint32_t percentage)
{
	assert(percentage > 0);
	assert(percentage <= 100);

	uint64_t mem = physmem();

	// If we cannot determine the amount of RAM, assume 32 MiB. Maybe
	// even that is too much on some systems. But on most systems it's
	// far too little, and can be annoying.
	if (mem == 0)
		mem = UINT64_C(32) * 1024 * 1024;

	memlimit = percentage * mem / 100;
	return;
}


extern uint64_t
hardware_memlimit_get(void)
{
	return memlimit;
}


extern void
hardware_init(void)
{
	hardware_memlimit_set(0);
	hardware_threadlimit_set(0);
	return;
}
