/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "sort.h"

#if defined(HAVE_CILK)
#include <cilk/cilk.h>
#define cilk_worker_count (__cilkrts_get_nworkers())
#else
#define cilk_spawn
#define cilk_sync
#define cilk_for for
#define cilk_worker_count 1
#endif

static int
merge_c(void *vdest, void *va, int an, void *vb, int bn, int width,
        void *extra, int (*cmp)(void *, const void *, const void *))
{
    char *dest = vdest, *a = va, *b = vb;
    while (an > 0 && bn > 0) {
        if (cmp(extra, a, b) < 0) {
            memcpy(dest, a, width);
            dest += width; a += width; an--;
        } else {
            memcpy(dest, b, width);
            dest += width; b += width; bn--;
        }
    }
    if (an > 0) {
        memcpy(dest, a, an * width);
    }
    if (bn > 0) {
        memcpy(dest, b, bn * width);
    }
    return 0;
}

static int
binsearch(void *key, void *va, int n, int abefore, int width,
          void *extra, int (*cmp)(void *, const void *, const void *))
{
    if (n == 0) {
        return abefore;
    }
    char *a = va;
    int mid = n / 2;
    void *akey = &a[mid * width];
    int c = cmp(extra, key, akey);
    if (c < 0) {
        if (n == 1) {
            return abefore;
        } else {
            return binsearch(key, a, mid, abefore, width, extra, cmp);
        }
    } else if (c > 0) {
        if (n == 1) {
            return abefore + 1;
        } else {
            return binsearch(key, akey, n - mid, abefore + mid, width,
                             extra, cmp);
        }
    } else {
        // this won't happen because msns are unique, but is here for
        // completeness
        return abefore + mid;
    }
}

static int
merge(void *vdest, void *va, int an, void *vb, int bn, int width,
      void *extra, int (*cmp)(void *, const void *, const void *))
{
    if (an + bn < 10000) {
        return merge_c(vdest, va, an, vb, bn, width, extra, cmp);
    }

    char *dest = vdest, *a = va, *b = vb;
    if (an < bn) {
        char *tmp1 = a; a = b; b = tmp1;
        int tmp2 = an; an = bn; bn = tmp2;
    }
    int a2 = an / 2;
    void *akey = &a[a2 * width];
    int b2 = binsearch(akey, b, bn, 0, width, extra, cmp);
    int ra, rb;
    ra = cilk_spawn merge(dest, a, a2, b, b2, width, extra, cmp);
    rb = merge(&dest[(a2 + b2) * width], akey, an - a2, &b[b2 * width], bn - b2,
               width, extra, cmp);
    cilk_sync;
    if (ra != 0) return ra;
    return rb;
}

static inline void
swap(void *va, void *vb, int width)
{
    u_int64_t *ia = va, *ib = vb, it;
    while ((unsigned int) width >= sizeof(u_int64_t)) {
        it = *ia;
        *ia = *ib;
        *ib = it;
        width -= sizeof(u_int64_t);
        ia++;
        ib++;
    }
    if (width == 0) { return; }
    unsigned char *ca = (void *) ia, *cb = (void *) ib, ct;
    while (width > 0) {
        ct = *ca;
        *ca = *cb;
        *cb = ct;
        width--;
        ca++;
        cb++;
    }
}

static int
quicksort_r(void *va, int n, int width,
            void *extra, int (*cmp)(void *, const void *, const void *))
{
    if (n <= 1) { return 0; }
    unsigned char *a = va;
    unsigned char *pivot = &a[(n - 1) * width];
    unsigned char *mid = &a[(n / 2) * width];
    // The pivot is the last position in the array, but is the median of
    // three elements (first, middle, last).
    if (cmp(extra, a, pivot) > 0) {
        swap(a, pivot, width);
    }
    if (cmp(extra, pivot, mid) > 0) {
        swap(pivot, mid, width);
        if (cmp(extra, a, pivot) > 0) {
            swap(a, pivot, width);
        }
    }
    unsigned char *lp = a, *rp = &a[(n - 2) * width];
    while (lp < rp) {
        // In the case where we have a lot of duplicate elements, this is
        // kind of horrible (it's O(n^2)).  It could be fixed by
        // partitioning into less, equal, and greater, but since the only
        // place we're using it right now has no duplicates (the MSNs are
        // guaranteed unique), it's fine to do it this way, and probably
        // better because it's simpler.
        while (cmp(extra, lp, pivot) < 0) {
            lp += width;
        }
        while (cmp(extra, pivot, rp) <= 0) {
            rp -= width;
        }
        if (lp < rp) {
            swap(lp, rp, width);
            lp += width;
            rp -= width;
        }
    }
    if (lp == rp && cmp(extra, lp, pivot) < 0) {
        // A weird case where lp and rp are both pointing to the rightmost
        // element less than the pivot, we want lp to point to the first
        // element greater than or equal to the pivot.
        lp += width;
    }
    // Swap the pivot back into place.
    swap(pivot, lp, width);
    int r = quicksort_r(a, (lp - a) / width, width, extra, cmp);
    if (r != 0) { return r; }
    // The pivot is in this spot and we don't need to sort it, so move
    // over one space before calling quicksort_r again.
    lp += width;
    r = quicksort_r(lp, n - (lp - a) / width, width, extra, cmp);
    return r;
}

int
mergesort_r(void *va, int n, int width,
            void *extra, int (*cmp)(void *, const void *, const void *))
{
    if (n <= 1) { return 0; }
    if (n < 10000) {
        return quicksort_r(va, n, width, extra, cmp);
    }
    unsigned char *a = va;
    int mid = n / 2;
    int r1 = cilk_spawn mergesort_r(a, mid, width, extra, cmp);
    int r2 = mergesort_r(&a[mid * width], n - mid, width, extra, cmp);
    cilk_sync;
    if (r1 != 0) return r1;
    if (r2 != 0) return r2;

    void *tmp = toku_xmalloc(n * width);
    int r = merge(tmp, a, mid, &a[mid * width], n - mid, width, extra, cmp);
    if (r == 0) {
        memcpy(a, tmp, n * width);
    }
    toku_free(tmp);
    return r;
}
