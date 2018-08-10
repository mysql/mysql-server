/*
 *  Pool allocator for low memory targets.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "duktape.h"
#include "duk_alloc_pool.h"

/* Define to enable some debug printfs. */
/* #define DUK_ALLOC_POOL_DEBUG */

#if defined(DUK_ALLOC_POOL_ROMPTR_COMPRESSION)
#if 0  /* This extern declaration is provided by duktape.h, array provided by duktape.c. */
extern const void * const duk_rom_compressed_pointers[];
#endif
const void *duk_alloc_pool_romptr_low = NULL;
const void *duk_alloc_pool_romptr_high = NULL;
static void duk__alloc_pool_romptr_init(void);
#endif

#if defined(DUK_USE_HEAPPTR16)
void *duk_alloc_pool_ptrcomp_base = NULL;
#endif

#if defined(DUK_ALLOC_POOL_DEBUG)
static void duk__alloc_pool_dprintf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif

/*
 *  Pool initialization
 */

void *duk_alloc_pool_init(char *buffer,
                          size_t size,
                          const duk_pool_config *configs,
                          duk_pool_state *states,
                          int num_pools,
                          duk_pool_global *global) {
	double t_min, t_max, t_curr, x;
	int step, i, j, n;
	size_t total;
	char *p;

	/* XXX: check that 'size' is not too large when using pointer
	 * compression.
	 */

	/* To optimize pool counts first come up with a 't' which still allows
	 * total pool size to fit within user provided region.  After that
	 * sprinkle any remaining bytes to the counts.  Binary search with a
	 * fixed step count; last round uses 't_min' as 't_curr' to ensure it
	 * succeeds.
	 */

	t_min = 0.0;  /* Unless config is insane, this should always be "good". */
	t_max = 1e6;

	for (step = 0; ; step++) {
		if (step >= 100) {
			/* Force "known good", rerun config, and break out.
			 * Deals with rounding corner cases where t_curr is
			 * persistently "bad" even though t_min is a valid
			 * solution.
			 */
			t_curr = t_min;
		} else {
			t_curr = (t_min + t_max) / 2.0;
		}

		for (i = 0, total = 0; i < num_pools; i++) {
			states[i].size = configs[i].size;

			/* Target bytes = A*t + B ==> target count = (A*t + B) / block_size.
			 * Rely on A and B being small enough so that 'x' won't wrap.
			 */
			x = ((double) configs[i].a * t_curr + (double) configs[i].b) / (double) configs[i].size;

			states[i].count = (unsigned int) x;
			total += (size_t) states[i].size * (size_t) states[i].count;
			if (total > size) {
				goto bad;
			}
		}

		/* t_curr is good. */
#if defined(DUK_ALLOC_POOL_DEBUG)
		duk__alloc_pool_dprintf("duk_alloc_pool_init: step=%d, t=[%lf %lf %lf] -> total %ld/%ld (good)\n",
		                        step, t_min, t_curr, t_max, (long) total, (long) size);
#endif
		if (step >= 100) {
			/* Keep state[] initialization state.  The state was
			 * created using the highest 't_min'.
			 */
			break;
		}
		t_min = t_curr;
		continue;

	 bad:
		/* t_curr is bad. */
#if defined(DUK_ALLOC_POOL_DEBUG)
		duk__alloc_pool_dprintf("duk_alloc_pool_init: step=%d, t=[%lf %lf %lf] -> total %ld/%ld (bad)\n",
		                        step, t_min, t_curr, t_max, (long) total, (long) size);
#endif

		if (step >= 1000) {
			/* Cannot find any good solution; shouldn't happen
			 * unless config is bad or 'size' is so small that
			 * even a baseline allocation won't fit.
			 */
			return NULL;
		}
		t_max = t_curr;
		/* continue */
	}

	/* The base configuration is now good; sprinkle any leftovers to
	 * pools in descending order.  Note that for good t_curr, 'total'
	 * indicates allocated bytes so far and 'size - total' indicates
	 * leftovers.
	 */
	for (i = num_pools - 1; i >= 0; i--) {
		while (size - total >= states[i].size) {
			/* Ignore potential wrapping of states[i].count as the count
			 * is 32 bits and shouldn't wrap in practice.
			 */
			states[i].count++;
			total += states[i].size;
#if defined(DUK_ALLOC_POOL_DEBUG)
			duk__alloc_pool_dprintf("duk_alloc_pool_init: sprinkle %ld bytes (%ld left after)\n",
			                        (long) states[i].size, (long) (size - total));
#endif
		}
	}

	/* Pool counts are final.  Allocate the user supplied region based
	 * on the final counts, initialize free lists for each block size,
	 * and otherwise finalize 'state' for use.
	 */
	p = buffer;
	global->states = states;
	global->num_pools = num_pools;

#if defined(DUK_USE_HEAPPTR16)
	/* Register global base value for pointer compression, assumes
	 * a single active pool  -4 allows a single subtract to be used and
	 * still ensures no non-NULL pointer encodes to zero.
	 */
	duk_alloc_pool_ptrcomp_base = (void *) (p - 4);
#endif

	for (i = 0; i < num_pools; i++) {
		n = states[i].count;
		if (n > 0) {
			states[i].first = (duk_pool_free *) p;
			for (j = 0; j < n; j++) {
				char *p_next = p + states[i].size;
				((duk_pool_free *) p)->next =
					(j == n - 1) ? (duk_pool_free *) NULL : (duk_pool_free *) p_next;
				p = p_next;
			}
		} else {
			states[i].first = (duk_pool_free *) NULL;
		}
		states[i].alloc_end = p;  /* All members of 'state' now initialized. */

#if defined(DUK_ALLOC_POOL_DEBUG)
		duk__alloc_pool_dprintf("duk_alloc_pool_init: block size %5ld, count %5ld, %8ld total bytes, "
		                        "end %p\n",
		                        (long) states[i].size, (long) states[i].count,
		                        (long) states[i].size * (long) states[i].count,
		                        (void *) states[i].alloc_end);
#endif
	}

#if defined(DUK_ALLOC_POOL_ROMPTR_COMPRESSION)
	/* ROM pointer compression precomputation.  Assumes a single active
	 * pool.
	 */
	duk__alloc_pool_romptr_init();
#endif

	/* Use 'global' as udata. */
	return (void *) global;
}

/*
 *  Allocation providers
 */

void *duk_alloc_pool(void *udata, duk_size_t size) {
	duk_pool_global *g = (duk_pool_global *) udata;
	int i, n;

#if defined(DUK_ALLOC_POOL_DEBUG)
	duk__alloc_pool_dprintf("duk_alloc_pool: %p %ld\n", udata, (long) size);
#endif

	if (size == 0) {
		return NULL;
	}

	for (i = 0, n = g->num_pools; i < n; i++) {
		duk_pool_state *st = g->states + i;

		if (size <= st->size && st->first != NULL) {
			duk_pool_free *res = st->first;
			st->first = res->next;
			return (void *) res;
		}

		/* Allocation doesn't fit or no free entries, try to borrow
		 * from the next block size.  There's no support for preventing
		 * a borrow at present.
		 */
	}

	return NULL;
}

void *duk_realloc_pool(void *udata, void *ptr, duk_size_t size) {
	duk_pool_global *g = (duk_pool_global *) udata;
	int i, j, n;

#if defined(DUK_ALLOC_POOL_DEBUG)
	duk__alloc_pool_dprintf("duk_realloc_pool: %p %p %ld\n", udata, ptr, (long) size);
#endif

	if (ptr == NULL) {
		return duk_alloc_pool(udata, size);
	}
	if (size == 0) {
		duk_free_pool(udata, ptr);
		return NULL;
	}

	/* Non-NULL pointers are necessarily from the pool so we should
	 * always be able to find the allocation.
	 */

	for (i = 0, n = g->num_pools; i < n; i++) {
		duk_pool_state *st = g->states + i;
		char *new_ptr;

		/* Because 'ptr' is assumed to be in the pool and pools are
		 * allocated in sequence, it suffices to check for end pointer
		 * only.
		 */
		if ((char *) ptr >= st->alloc_end) {
			continue;
		}

		if (size <= st->size) {
			/* Allocation still fits existing allocation.  Check if
			 * we can shrink the allocation to a smaller block size
			 * (smallest possible).
			 */
			for (j = 0; j < i; j++) {
				duk_pool_state *st2 = g->states + j;

				if (size <= st2->size && st2->first != NULL) {
#if defined(DUK_ALLOC_POOL_DEBUG)
					duk__alloc_pool_dprintf("duk_realloc_pool: shrink, block size %ld -> %ld\n",
					                        (long) st->size, (long) st2->size);
#endif
					new_ptr = (char *) st2->first;
					st2->first = ((duk_pool_free *) new_ptr)->next;
					memcpy((void *) new_ptr, (const void *) ptr, (size_t) size);
					((duk_pool_free *) ptr)->next = st->first;
					st->first = (duk_pool_free *) ptr;
					return (void *) new_ptr;
				}
			}

			/* Failed to shrink; return existing pointer. */
			return ptr;
		}

		/* Find first free larger block. */
		for (j = i + 1; j < n; j++) {
			duk_pool_state *st2 = g->states + j;

			if (size <= st2->size && st2->first != NULL) {
				new_ptr = (char *) st2->first;
				st2->first = ((duk_pool_free *) new_ptr)->next;
				memcpy((void *) new_ptr, (const void *) ptr, (size_t) st->size);
				((duk_pool_free *) ptr)->next = st->first;
				st->first = (duk_pool_free *) ptr;
				return (void *) new_ptr;
			}
		}

		/* Failed to resize. */
		return NULL;
	}

	/* We should never be here because 'ptr' should be a valid pool
	 * entry and thus always found above.
	 */
	return NULL;
}

void duk_free_pool(void *udata, void *ptr) {
	duk_pool_global *g = (duk_pool_global *) udata;
	int i, n;

#if defined(DUK_ALLOC_POOL_DEBUG)
	duk__alloc_pool_dprintf("duk_free_pool: %p %p\n", udata, ptr);
#endif

	if (ptr == NULL) {
		return;
	}

	for (i = 0, n = g->num_pools; i < n; i++) {
		duk_pool_state *st = g->states + i;

		/* Enough to check end address only. */
		if ((char *) ptr >= st->alloc_end) {
			continue;
		}

		((duk_pool_free *) ptr)->next = st->first;
		st->first = (duk_pool_free *) ptr;
		return;
	}

	/* We should never be here because 'ptr' should be a valid pool
	 * entry and thus always found above.
	 */
}

/*
 *  Pointer compression
 */

#if defined(DUK_ALLOC_POOL_ROMPTR_COMPRESSION)
static void duk__alloc_pool_romptr_init(void) {
	/* Scan ROM pointer range for faster detection of "is 'p' a ROM pointer"
	 * later on.
	 */
	const void * const * ptrs = (const void * const *) duk_rom_compressed_pointers;
	duk_alloc_pool_romptr_low = duk_alloc_pool_romptr_high = (const void *) *ptrs;
	while (*ptrs) {
		if (*ptrs > duk_alloc_pool_romptr_high) {
			duk_alloc_pool_romptr_high = (const void *) *ptrs;
		}
		if (*ptrs < duk_alloc_pool_romptr_low) {
			duk_alloc_pool_romptr_low = (const void *) *ptrs;
		}
		ptrs++;
	}
}
#endif

/* Encode/decode functions are defined in the header to allow inlining. */

#if defined(DUK_ALLOC_POOL_ROMPTR_COMPRESSION)
duk_uint16_t duk_alloc_pool_enc16_rom(void *ptr) {
	/* The if-condition should be the fastest possible check
	 * for "is 'ptr' in ROM?".  If pointer is in ROM, we'd like
	 * to compress it quickly.  Here we just scan a ~1K array
	 * which is very bad for performance.
	 */
	const void * const * ptrs = duk_rom_compressed_pointers;
	while (*ptrs) {
		if (*ptrs == ptr) {
			return DUK_ALLOC_POOL_ROMPTR_FIRST + (duk_uint16_t) (ptrs - duk_rom_compressed_pointers);
		}
		ptrs++;
	}

	/* We should really never be here: Duktape should only be
	 * compressing pointers which are in the ROM compressed
	 * pointers list, which are known at 'make dist' time.
	 * We go on, causing a pointer compression error.
	 */
	return 0;
}
#endif
