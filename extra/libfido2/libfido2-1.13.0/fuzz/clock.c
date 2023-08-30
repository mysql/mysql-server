/*
 * Copyright (c) 2021 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <time.h>

#include "mutator_aux.h"

/*
 * A pseudo-random monotonic clock with a probabilistic discontinuity to
 * the end of time (as measured by struct timespec).
 */

extern int prng_up;
extern int __wrap_clock_gettime(clockid_t, struct timespec *);
extern int __real_clock_gettime(clockid_t, struct timespec *);
extern int __wrap_usleep(unsigned int);
static TLS struct timespec fuzz_clock;

static void
tick(unsigned int usec)
{
	long long drift;

	/*
	 * Simulate a jump to the end of time with 0.125% probability.
	 * This condition should be gracefully handled by callers of
	 * clock_gettime().
	 */
	if (uniform_random(800) < 1) {
		fuzz_clock.tv_sec = LLONG_MAX;
		fuzz_clock.tv_nsec = LONG_MAX;
		return;
	}

	drift = usec * 1000LL + (long long)uniform_random(10000000); /* 10ms */
	if (LLONG_MAX - drift < (long long)fuzz_clock.tv_nsec) {
		fuzz_clock_reset(); /* Not much we can do here. */
	} else if (drift + (long long)fuzz_clock.tv_nsec < 1000000000) {
		fuzz_clock.tv_nsec += (long)(drift);
	} else {
		fuzz_clock.tv_sec  += (long)(drift / 1000000000);
		fuzz_clock.tv_nsec += (long)(drift % 1000000000);
	}
}

int
__wrap_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	if (!prng_up || clk_id != CLOCK_MONOTONIC)
		return __real_clock_gettime(clk_id, tp);
	if (uniform_random(400) < 1)
		return -1;

	tick(0);
	*tp = fuzz_clock;

	return 0;
}

int
__wrap_usleep(unsigned int usec)
{
	if (uniform_random(400) < 1)
		return -1;

	tick(usec);

	return 0;
}

void
fuzz_clock_reset(void)
{
	memset(&fuzz_clock, 0, sizeof(fuzz_clock));
}
