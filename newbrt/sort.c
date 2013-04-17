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
        int c = cmp(extra, a, b);
        if (c < 0) {
            memcpy(dest, a, width);
            dest+=width; a+=width; an--;
        } else {
            memcpy(dest, b, width);
            dest+=width; b+=width; bn--;
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
    void *akey = a + mid * width;
    int c = cmp(extra, key, akey);
    if (c == 0) {
        // this won't happen because msns are unique, but is here for completeness
        return abefore + mid;
    } else if (c < 0) {
        if (n == 1) {
            return abefore;
        } else {
            return binsearch(key, a, mid, abefore, width, extra, cmp);
        }
    } else {
        if (n == 1) {
            return abefore + 1;
        } else {
            return binsearch(key, a+mid*width, n-mid, abefore+mid, width, extra, cmp);
        }
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
    int a2 = an/2;
    void *akey = a + a2 * width;
    int b2 = binsearch(akey, b, bn, 0, width, extra, cmp);
    int ra, rb;
    ra = cilk_spawn merge(dest, a, a2, b, b2, width, extra, cmp);
    rb = merge(dest+(a2+b2)*width, a+a2*width, an-a2, b+b2*width, bn-b2, width, extra, cmp);
    cilk_sync;
    if (ra != 0) return ra;
    return rb;
}

int
mergesort_r(void *va, int n, int width,
            void *extra, int (*cmp)(void *, const void *, const void *))
{
    const BOOL use_cilk = (n > 10000);
    if (n <= 1) { return 0; }
    unsigned char *a = va;
    int mid = n/2;
    int r1, r2;
    if (use_cilk) {
        r1 = cilk_spawn mergesort_r(a, mid, width, extra, cmp);
    } else {
        r1 = mergesort_r(a, mid, width, extra, cmp);
    }
    r2 = mergesort_r(a+mid*width, n-mid, width, extra, cmp);
    cilk_sync;
    if (r1 != 0) return r1;
    if (r2 != 0) return r2;

    void *tmp = toku_xmalloc(n * width);
    int r;
    if (use_cilk) {
        r = merge(tmp, a, mid, a+mid*width, n-mid, width, extra, cmp);
    } else {
        r = merge_c(tmp, a, mid, a+mid*width, n-mid, width, extra, cmp);
    }
    if (r != 0) {
        toku_free(tmp);
        return r;
    }
    memcpy(a, tmp, n*width);
    toku_free(tmp);
    return 0;
}
