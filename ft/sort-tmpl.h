/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef SORT_TMPL_H
#define SORT_TMPL_H

#if defined(HAVE_CILK)
#include <cilk/cilk.h>
#define cilk_worker_count (__cilkrts_get_nworkers())
#else
#define cilk_spawn
#define cilk_sync
#define cilk_for for
#define cilk_worker_count 1
#endif

#include <memory.h>
#include <string.h>

namespace toku {

    template<typename sortdata_t, typename sortextra_t, int (*cmp)(sortextra_t &, const sortdata_t &, const sortdata_t &)>
    struct sort {

        static const int single_threaded_threshold = 10000;

        /**
         * Effect: Sort n elements of type sortdata_t in the array a.
         *   Elements are compared by the template parameter cmp, using
         *   the context in extra.
         */
        static int
        mergesort_r(sortdata_t *a, const int n, sortextra_t &extra)
        {
            sortdata_t *as[2] = { a, nullptr };
            if (n >= single_threaded_threshold) {
                XMALLOC_N(n, as[1]);
            }
            int which = mergesort_internal(as, 0, n, extra);
            if (which == 1) {
                memcpy(a, as[1], n * (sizeof a[0]));
            }
            if (n >= single_threaded_threshold) {
                toku_free(as[1]);
            }
            return 0;
        }

    private:

        // Sorts the data in as[which].  Returns dest such that as[dest]
        // contains the sorted data (might be which or 1-which).
        static int
        mergesort_internal(sortdata_t *as[2], const int which, const int n, sortextra_t &extra)
        {
            if (n <= 1) { return which; }
            if (n < single_threaded_threshold) {
                quicksort_r(as[which], n, extra);
                return which;
            }
            const int mid = n / 2;
            sortdata_t *right_as[2] = { &(as[0])[mid], &(as[1])[mid] };
            const int r1 = cilk_spawn mergesort_internal(as, which, mid, extra);
            const int r2 = mergesort_internal(right_as, which, n - mid, extra);
            cilk_sync;
            if (r1 != r2) {
                // move everything to the same place (r2)
                memcpy(as[r2], as[r1], mid * (sizeof as[r2][0]));
            }
            // now as[r2] has both sorted arrays
            const int dest = 1 - r2;
            merge(&(as[dest])[0], &(as[1-dest])[0], mid, &(as[1-dest])[mid], n - mid, extra);
            return dest;
        }

        static void
        merge_c(sortdata_t *dest, const sortdata_t *a, const int an, const sortdata_t *b, const int bn, sortextra_t &extra)
        {
            int ai, bi, i;
            for (ai = 0, bi = 0, i = 0; ai < an && bi < bn; ++i) {
                if (cmp(extra, a[ai], b[bi]) < 0) {
                    dest[i] = a[ai];
                    ai++;
                } else {
                    dest[i] = b[bi];
                    bi++;
                }
            }
            if (ai < an) {
                memcpy(&dest[i], &a[ai], (an - ai) * (sizeof dest[0]));
            } else if (bi < bn) {
                memcpy(&dest[i], &b[bi], (bn - bi) * (sizeof dest[0]));
            }
        }

        static int
        binsearch(const sortdata_t &key, const sortdata_t *a, const int n, const int abefore, sortextra_t &extra)
        {
            if (n == 0) {
                return abefore;
            }
            const int mid = n / 2;
            const sortdata_t *akey = &a[mid];
            int c = cmp(extra, key, *akey);
            if (c < 0) {
                if (n == 1) {
                    return abefore;
                } else {
                    return binsearch(key, a, mid, abefore, extra);
                }
            } else if (c > 0) {
                if (n == 1) {
                    return abefore + 1;
                } else {
                    return binsearch(key, akey, n - mid, abefore + mid, extra);
                }
            } else {
                return abefore + mid;
            }
        }

        static void
        merge(sortdata_t *dest, const sortdata_t *a_, const int an_, const sortdata_t *b_, const int bn_, sortextra_t &extra)
        {
            if (an_ + bn_ < single_threaded_threshold) {
                merge_c(dest, a_, an_, b_, bn_, extra);
            } else {
                const bool swapargs = an_ < bn_;
                const sortdata_t *a = swapargs ? b_ : a_;
                const sortdata_t *b = swapargs ? a_ : b_;
                const int an = swapargs ? bn_ : an_;
                const int bn = swapargs ? an_ : bn_;

                const int a2 = an / 2;
                const sortdata_t *akey = &a[a2];
                const int b2 = binsearch(*akey, b, bn, 0, extra);
                cilk_spawn merge(dest, a, a2, b, b2, extra);
                merge(&dest[a2 + b2], akey, an - a2, &b[b2], bn - b2, extra);
                cilk_sync;
            }
        }

        static void
        quicksort_r(sortdata_t *a, const int n, sortextra_t &extra)
        {
            if (n > 1) {
                const int lo = 0;
                int pivot = n / 2;
                const int hi = n - 1;
                if (cmp(extra, a[lo], a[pivot]) > 0) {
                    const sortdata_t tmp = a[lo]; a[lo] = a[pivot]; a[pivot] = tmp;
                }
                if (cmp(extra, a[pivot], a[hi]) > 0) {
                    const sortdata_t tmp = a[pivot]; a[pivot] = a[hi]; a[hi] = tmp;
                    if (cmp(extra, a[lo], a[pivot]) > 0) {
                        const sortdata_t tmp2 = a[lo]; a[lo] = a[pivot]; a[pivot] = tmp2;
                    }
                }
                int li = lo + 1, ri = hi - 1;
                while (li <= ri) {
                    while (cmp(extra, a[li], a[pivot]) < 0) {
                        li++;
                    }
                    while (cmp(extra, a[pivot], a[ri]) < 0) {
                        ri--;
                    }
                    if (li < ri) {
                        sortdata_t tmp = a[li]; a[li] = a[ri]; a[ri] = tmp;
                        // fix up pivot if we moved it
                        if (pivot == li) { pivot = ri; }
                        else if (pivot == ri) { pivot = li; }
                        li++;
                        ri--;
                    } else if (li == ri) {
                        li++;
                        ri--;
                    }
                }

                quicksort_r(&a[lo], ri + 1, extra);
                quicksort_r(&a[li], hi - li + 1, extra);
            }
        }
    };

};

#endif /* SORT_TMPL_H */
