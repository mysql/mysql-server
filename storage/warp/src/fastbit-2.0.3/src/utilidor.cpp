// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2008-2016 the Regents of the University of California
#include "utilidor.h"
#include <typeinfo>     // typeid
#include <iostream>     // std::cout, etc

#ifndef FASTBIT_QSORT_MIN
#define FASTBIT_QSORT_MIN 64
#endif
#ifndef FASTBIT_QSORT_MAX_DEPTH
#define FASTBIT_QSORT_MAX_DEPTH 20
#endif

namespace ibis {
    namespace util {
        template <typename T1, typename T2>
        void sortAll_quick(array_t<T1> &arr1, array_t<T2> &arr2);
        /// Shell sort.  Sort both arrays arr1 and arr2.
        template <typename T1, typename T2>
        void sortAll_shell(array_t<T1> &arr1, array_t<T2> &arr2);
        /// The parititioning function for ibis::util::sortAll.  Uses
        /// the standard two-way partitioning.
        template <typename T1, typename T2>
        uint32_t sortAll_split(array_t<T1> &arr1, array_t<T2> &arr2);

        /// Quicksort.  Sort the keys only.  Use the standard two-way
        /// partitioning.
        template <typename T1, typename T2>
        void sort_quick(array_t<T1> &keys, array_t<T2> &vals, uint32_t lvl);
        /// Quicksort.  Sort the keys only.  Use a nonstandard three-way
        /// partitioning.
        template <typename T1, typename T2>
        void sort_quick3(array_t<T1> &keys, array_t<T2> &vals);
        /// Insertion sort.  It has relatively straightforward memory
        /// access pattern and may be useful to sort a few numbers at the
        /// end of a recursive procedure.
        template <typename T1, typename T2>
        void sort_insertion(array_t<T1> &keys, array_t<T2> &vals);
        /// Shell sort.  It has relatively straightforward memory access
        /// pattern and may be useful to sort a few numbers at the end of a
        /// recursive sorting function.
        template <typename T1, typename T2>
        void sort_shell(array_t<T1> &keys, array_t<T2> &vals);
        /// Heapsort.  Sort the keys only.  Move the vals along with
        /// the keys.
        template <typename T1, typename T2>
        void sort_heap(array_t<T1> &keys, array_t<T2> &vals);
        /// Partition function for quicksort.  The return value p separates
        /// keys into two parts, keys[..:p-1] < keys[p:..].  A return value
        /// equal to the size of keys indicates all keys are sorted.
        template <typename T1, typename T2>
        uint32_t sort_partition(array_t<T1> &keys, array_t<T2> &vals);
        /// Three-way partitioning algorithm for quicksort.  Upon return
        /// from this function, keys satisfying the following order
        /// keys[0:starteq] < keys[starteq:stargt-1] < keys[startgt:..].
        /// The keys are ordered if starteq = startgt = keys.size().
        template <typename T1, typename T2>
        void sort_partition3(array_t<T1> &keys, array_t<T2> &vals,
                             uint32_t &starteq, uint32_t &startgt);

        /// Shell sorting procedure.  To clean up after the quick sort
        /// procedure.
        void sortStrings_shell(std::vector<std::string> &keys,
                               array_t<uint32_t> &vals,
                               uint32_t begin, uint32_t end);
        /// The partitioning procedure for quick sort.  It implements the
        /// standard two-way partitioning with the median-of-three pivot.
        uint32_t sortStrings_partition(std::vector<std::string> &keys,
                                       array_t<uint32_t> &vals,
                                       uint32_t begin, uint32_t end);

        /// Shell sorting procedure.  To clean up after the quick sort
        /// procedure.
        void sortStrings_shell(array_t<const char*> &keys,
                               array_t<uint32_t> &vals,
                               uint32_t begin, uint32_t end);
        /// The partitioning procedure for quick sort.  It implements the
        /// standard two-way partitioning with the median-of-three pivot.
        uint32_t sortStrings_partition(array_t<const char*> &keys,
                                       array_t<uint32_t> &vals,
                                       uint32_t begin, uint32_t end);

        /// LSD Radix sort.  Allocates buffers needed for copying data.
        /// @{
        template <typename T>
        void sort_radix(array_t<char> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<signed char> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<unsigned char> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<uint16_t> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<int16_t> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<uint32_t> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<int32_t> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<uint64_t> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<int64_t> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<float> &keys, array_t<T> &vals);
        template <typename T>
        void sort_radix(array_t<double> &keys, array_t<T> &vals);
        /// @}

        /// Gaps for Shell sort from
        /// http://en.wikipedia.org/wiki/Shell_sort
        /// by Ciura, 2001.
        const uint32_t shellgaps[8] = {1, 4, 10, 23, 57, 132, 301, 701};
    }
}

void ibis::util::sortRIDs(ibis::RIDSet &rids) {
    rids.nosharing();
    if (rids.size() > 20)
        ibis::util::sortRIDsq(rids, 0, rids.size());
    else if (rids.size() > 1)
        ibis::util::sortRIDsi(rids, 0, rids.size());
}

/// Sort RIDs in the range of [i, j).
void ibis::util::sortRIDsq(ibis::RIDSet &rids, uint32_t i, uint32_t j) {
    if (i >= j) return;
    std::less<ibis::rid_t> cmp;
    if (i+32 >= j) { // use buble sort
        sortRIDsi(rids, i, j);
    }
    else { // use quick sort
        ibis::rid_t tgt = rids[(i+j)/2];
        uint32_t i1 = i;
        uint32_t i2 = j-1;
        bool left = cmp(rids[i1], tgt);
        bool right = !cmp(rids[i2], tgt);
        while (i1 < i2) {
            if (left && right) {
                // both i1 and i2 are in the right position
                ++ i1; -- i2;
                left = cmp(rids[i1], tgt);
                right = !cmp(rids[i2], tgt);
            }
            else if (right) {
                // i2 is in the right position
                -- i2;
                right = !cmp(rids[i2], tgt);
            }
            else if (left) {
                // i1 is in the right position
                ++ i1;
                left = cmp(rids[i1], tgt);
            }
            else { // both in the wrong position, swap them
                ibis::rid_t tmp = rids[i1];
                rids[i1] = rids[i2];
                rids[i2] = tmp;
                ++ i1; -- i2;
                left = cmp(rids[i1], tgt);
                right = !cmp(rids[i2], tgt);
            }
        }
        i1 += (left); // if left is true, rids[i1] should be on the left side
        // everything below i1 is less than tgt
        if (i1 > i) {
            sortRIDsq(rids, i, i1);
            sortRIDsq(rids, i1, j);
        }
        else { // nothing has been swapped, i.e., tgt is the smallest
            while (i1 < j &&
                   0 == memcmp(&tgt, &(rids[i1]), sizeof(ibis::rid_t)))
                ++ i1;
            if (i1+i1 < i+j) {
                i2 = (i+j) / 2;
                ibis::rid_t tmp = rids[i2];
                rids[i2] = rids[i1];
                rids[i1] = tmp;
                ++ i1;
            }
            sortRIDsq(rids, i1, j);
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    int cnt = 0;
    ibis::util::logger lg(4);
    ibis::RIDSet::const_iterator it;
    lg() << "sortRIDsq(..., " << i << ", " << j << "):\n";
    for (it = rids.begin(); it != rids.end(); ++ it, ++ cnt)
        lg() << cnt << "\t" << *it << "\n";
#endif
} // ibis::util::sortRIDsq

/// Sort RIDs in the range of [i, j).
void ibis::util::sortRIDsi(ibis::RIDSet &rids, uint32_t i, uint32_t j) {
    uint32_t i1, i2, i3;
    ibis::rid_t tmp;
    for (i1 = i; i1 < j-1; ++i1) {
        i3 = i1 + 1;
        for (i2 = i3+1; i2 < j; ++i2) {
            if (rids[i3] > rids[i2])
                i3 = i2;
        }
        // place rids[i3] at the right position
        if (rids[i3] < rids[i1]) {
            tmp = rids[i1];
            rids[i1] = rids[i3];
            rids[i3] = tmp;
        }
        else { // rids[i1] is at the right position, rids[i3] should be
               // rids[i1+1]
            ++i1;
            if (rids[i3] < rids[i1]) {
                tmp = rids[i1];
                rids[i1] = rids[i3];
                rids[i3] = tmp;
            }
        }
    }
} // ibis::util::sortRIDsi

/// This implementation uses copy-and-swap algorithm.
template<class T>
void ibis::util::reorder(array_t<T> &arr, const array_t<uint32_t> &ind) {
    if (ind.size() <= arr.size()) {
        array_t<T> tmp(ind.size());
        for (uint32_t i = 0; i < ind.size(); ++ i)
            tmp[i] = arr[ind[i]];
        arr.swap(tmp);
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- util::reorder expects arr[" << arr.size()
            << "] and ind[" << ind.size() << "] to be the same size";
    }
} // ibis::util::reorder

/// Reorder string values.  This function keeps the actual strings in their
/// input positions by using the function swap.  This procedure should
/// avoid most of the memory allocations.
void ibis::util::reorder(std::vector<std::string> &arr,
                         const array_t<uint32_t> &ind) {
    if (ind.size() <= arr.size()) {
        std::vector<std::string> tmp(ind.size());
        for (uint32_t i = 0; i < ind.size(); ++ i)
            tmp[i].swap(arr[ind[i]]);
        arr.swap(tmp);
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- util::reorder expects arr[" << arr.size()
            << "] and ind[" << ind.size() << "] to be the same size";
    }
} // ibis::util::reorder

template<class T>
void ibis::util::reorder(array_t<T*> &arr, const array_t<uint32_t> &ind) {
    if (ind.size() < arr.size()) {
        array_t<T*> tmp(ind.size());
        for (uint32_t i = 0; i < ind.size(); ++ i)
            tmp[i] = arr[ind[i]];
        arr.swap(tmp);

        // free the pointers that have not been copied
        array_t<uint32_t> copied(arr.size(), 0);
        for (uint32_t i = 0; i < ind.size(); ++ i)
            copied[ind[i]] = 1;
        for (uint32_t i = 0; i < arr.size(); ++ i)
            if (copied[i] == 0)
                delete tmp[i];
    }
    else if (ind.size() == arr.size()) {
        array_t<T*> tmp(arr.size());
        for (uint32_t i = 0; i < ind.size(); ++ i)
            tmp[i] = arr[ind[i]];
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- util::reorder expects arr[" << arr.size()
            << "] and ind[" << ind.size() << "] to be the same size";
    }
} // ibis::util::reorder

template <typename T1, typename T2>
void ibis::util::sortAll(array_t<T1> &arr1, array_t<T2> &arr2) {
    arr1.nosharing();
    arr2.nosharing();
    if (arr1.size() >= FASTBIT_QSORT_MIN) {
        sortAll_quick(arr1, arr2);
    }
    else {
        sortAll_shell(arr1, arr2);
    }
} // ibis::util::sortAll

/// Quick sort.  Uses both arrays as keys and Moves all records of both arrays.
template <typename T1, typename T2>
void ibis::util::sortAll_quick(array_t<T1> &arr1, array_t<T2> &arr2) {
    const uint32_t nvals = (arr1.size() <= arr2.size() ?
                            arr1.size() : arr2.size());
    if (nvals >= FASTBIT_QSORT_MIN) {
        // split the arrays
        uint32_t split = sortAll_split(arr1, arr2);
        if (split < nvals) {
            if (split > 0) {
                array_t<T1> front1(arr1, 0, split);
                array_t<T2> front2(arr2, 0, split);
                sortAll_quick(front1, front2);
            }
            array_t<T1> back1(arr1, split, nvals);
            array_t<T2> back2(arr2, split, nvals);
            sortAll_quick(back1, back2);
        }
    }
    else {
        sortAll_shell(arr1, arr2);
    }

#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = 1; j < nvals; ++ j) {
        if (arr1[j-1] > arr1[j] ||
            (arr1[j-1] == arr1[j] && arr2[j-1] > arr2[j])) {
            sorted = false;
            break;
        }
    }

    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "util::sortAll_quick(arr1[" << arr1.size()
             << "], arr2[" << arr2.size() << "]) completed successfully";
    }
    else {
        lg() << "Warning -- util::sortAll_quick(arr1[" << arr1.size()
             << "], arr2[" << arr2.size() << "]) completed with errors";
        const uint32_t nprt = ((nvals >> ibis::gVerbose) > 0 ?
                               (1 << ibis::gVerbose) : nvals);
        for (unsigned j = 0; j < nprt; ++ j) {
            lg() << "\narr1[" << j << "]=" << arr1[j] << ", arr2[" << j
                 << "]=" << arr2[j];
            if (j > 0 && (arr1[j] < arr1[j-1] ||
                          (arr1[j] == arr1[j-1] && arr2[j] < arr2[j-1])))
                lg() << "\t*";
        }
        if (nprt < nvals)
            lg() << "\n... " << nvals-nprt << " ommitted\n";
    }
#endif
} // ibis::util::sortAll_quick

template <typename T1, typename T2>
void ibis::util::sortAll_shell(array_t<T1> &arr1, array_t<T2> &arr2) {
    const uint32_t nvals = (arr1.size() <= arr2.size() ?
                            arr1.size() : arr2.size());
    uint32_t gap = nvals / 2;
    while (gap >= shellgaps[7]) {
        for (uint32_t j = gap; j < nvals; ++j) {
            const T1 tmp1 = arr1[j];
            const T2 tmp2 = arr2[j];
            uint32_t i = j;
            while (i >= gap && (arr1[i-gap] > tmp1 ||
                                (arr1[i-gap] == tmp1 && arr2[i-gap] > tmp2))) {
                arr1[i] = arr1[i-gap];
                arr2[i] = arr2[i-gap];
                i -= gap;
            }
            arr1[i] = tmp1;
            arr2[i] = tmp2;
        }
        gap = (uint32_t) (gap / 2.25);
    }

    int ig = 7;
    while (ig > 0 && gap < shellgaps[ig]) -- ig;
    while (ig >= 0) {
        gap = shellgaps[ig];
        for (uint32_t j = gap; j < nvals; ++j) {
            const T1 tmp1 = arr1[j];
            const T2 tmp2 = arr2[j];
            uint32_t i = j;
            while (i >= gap && (arr1[i-gap] > tmp1 ||
                                (arr1[i-gap] == tmp1 && arr2[i-gap] > tmp2))) {
                arr1[i] = arr1[i-gap];
                arr2[i] = arr2[i-gap];
                i -= gap;
            }
            arr1[i] = tmp1;
            arr2[i] = tmp2;
        }
        -- ig;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = 1; j < nvals; ++ j) {
        if (arr1[j-1] > arr1[j] ||
            (arr1[j-1] == arr1[j] && arr2[j-1] > arr2[j])) {
            sorted = false;
            break;
        }
    }

    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "util::sortAll_shell(arr1[" << arr1.size()
             << "], arr2[" << arr2.size() << "]) completed successfully";
    }
    else {
        const uint32_t nprt = ((nvals >> ibis::gVerbose) > 0 ?
                               (1 << ibis::gVerbose) : nvals);
        lg() << "Warning -- util::sortAll_shell(arr1[" << arr1.size()
             << "], arr2[" << arr2.size() << "]) completed with errors";
        for (unsigned j = 0; j < nprt; ++ j) {
            lg() << "\narr1[" << j << "]=" << arr1[j] << ", arr2[" << j
                 << "]=" << arr2[j];
            if (j > 0 && (arr1[j] < arr1[j-1] ||
                          (arr1[j] == arr1[j-1] && arr2[j] < arr2[j-1])))
                lg() << "\t*";
        }
        if (nprt < nvals)
            lg() << "\n... " << nvals-nprt << " ommitted\n";
    }
#endif
} // ibis::util::sortAll_shell

template <typename T1, typename T2>
uint32_t ibis::util::sortAll_split(array_t<T1> &arr1, array_t<T2> &arr2) {
    const uint32_t nvals = (arr1.size() <= arr2.size() ?
                            arr1.size() : arr2.size());
    // first sort three values [0], [nvals/2], [nvals-1], with Shell sort
    if (arr1[0] > arr1[nvals/2] ||
        (arr1[0] == arr1[nvals/2] && arr2[0] > arr2[nvals/2])) {
        T1 tmp1 = arr1[0];
        arr1[0] = arr1[nvals/2];
        arr1[nvals/2] = tmp1;
        T2 tmp2 = arr2[0];
        arr2[0] = arr2[nvals/2];
        arr2[nvals/2] = tmp2;
    }
    if (arr1[nvals/2] > arr1[nvals-1] ||
        (arr1[nvals/2] == arr1[nvals-1] && arr2[nvals/2] > arr2[nvals-1])) {
        T1 tmp1 = arr1[nvals/2];
        arr1[nvals/2] = arr1[nvals-1];
        arr1[nvals-1] = tmp1;
        T2 tmp2 = arr2[nvals/2];
        arr2[nvals/2] = arr2[nvals-1];
        arr2[nvals-1] = tmp2;
        if (arr1[0] > arr1[nvals/2] ||
            (arr1[0] == arr1[nvals/2] && arr2[0] > arr2[nvals/2])) {
            tmp1 = arr1[0];
            arr1[0] = arr1[nvals/2];
            arr1[nvals/2] = tmp1;
            tmp2 = arr2[0];
            arr2[0] = arr2[nvals/2];
            arr2[nvals/2] = tmp2;
        }
    }

    // select the middle entry as the pivot
    const T1 pivot1 = arr1[nvals/2];
    const T2 pivot2 = arr2[nvals/2];
    uint32_t i0 = 0;
    uint32_t i1 = nvals;
    while (i0 < i1) {
        if (arr1[i1-1] > pivot1 ||
            (arr1[i1-1] == pivot1 && arr2[i1-1] >= pivot2)) {
            -- i1;
        }
        else if (arr1[i0] < pivot1 ||
                 (arr1[i0] == pivot1 && arr2[i0] < pivot2)) {
            ++ i0;
        }
        else {
            -- i1;
            T1 tmp1 = arr1[i0];
            arr1[i0] = arr1[i1];
            arr1[i1] = tmp1;
            T2 tmp2 = arr2[i0];
            arr2[i0] = arr2[i1];
            arr2[i1] = tmp2;
            ++ i0;
        }
    }
    if (i0 == 0) { // selected pivot happens to be the minimal value
        i1 = nvals;
        while (i0 < i1) {
            if (arr1[i1-1] > pivot1 ||
                (arr1[i1-1] == pivot1 && arr2[i1-1] > pivot2)) {
                -- i1;
            }
            else if (arr1[i0] < pivot1 ||
                     (arr1[i0] == pivot1 && arr2[i0] <= pivot2)) {
                ++ i0;
            }
            else {
                -- i1;
                T1 tmp1 = arr1[i0];
                arr1[i0] = arr1[i1];
                arr1[i1] = tmp1;
                T2 tmp2 = arr2[i0];
                arr2[i0] = arr2[i1];
                arr2[i1] = tmp2;
                ++ i0;
            }
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    uint32_t iprt = ((nvals >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nvals);
    ibis::util::logger lg(4);
    lg() << "util::sortAll_split(arr1[" << arr1.size()
         << "], arr2[" << arr2.size() << "]) completed with i0 = "
         << i0 << " and i1 = " << i1 << ", pivot = (";
    if (sizeof(T1) > 1)
        lg() << pivot1;
    else
        lg() << (int)pivot1;
    lg() << ", ";
    if (sizeof(T2) > 1)
        lg() << pivot2;
    else
        lg() << (int)pivot2;
    lg() << ")";
    lg() << "\npartition 1, # elements = " << i0 << ": ";
    uint32_t j1 = (iprt <= i0 ? iprt : i0);
    for (uint32_t j0 = 0; j0 < j1; ++ j0) {
        lg() << " (";
        if (sizeof(T1) > 1)
            lg() << arr1[j0];
        else
            lg() << (int) arr1[j0];
        lg() << ", ";
        if (sizeof(T2) > 1)
            lg() << arr2[j0];
        else
            lg() << (int)arr2[j0];
        lg() << ")";
    }
    if (j1 < i0)
        lg() << " ... (" << i0 - j1 << " ommitted)";
    lg() << "\npartition 2, # elements = "
         << nvals - i0 << ": ";
    j1 = (i0+iprt < nvals ? i0+iprt : nvals);
    for (uint32_t j0 = i0; j0 < j1; ++ j0) {
        lg() << " (";
        if (sizeof(T1) > 1)
            lg() << arr1[j0];
        else
            lg() << (int) arr1[j0];
        lg() << ", ";
        if (sizeof(T2) > 1)
            lg() << arr2[j0];
        else
            lg() << (int)arr2[j0];
        lg() << ")";
    }
    if (nvals > j1)
        lg() << " ... (" << nvals - j1 << " ommitted)";
#endif
    return i0;
} // ibis::util::sortAll_split

template <typename T1, typename T2>
void ibis::util::sortKeys(array_t<T1> &keys, array_t<T2> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    keys.nosharing();
    vals.nosharing();
    if (nelm > 8192) {
        try { // use radix sort only for large arrays
            sort_radix(keys, vals);
        }
        catch (...) {
            // the main reason that the radix sort might fails is out of memory,
            // since quick sort does not need any extra memory, give it a try
            sort_quick(keys, vals, 0);
        }
    }
    else {
        sort_quick(keys, vals, 0);
    }
} // ibis::util::sortKeys

/// Quick sort with introspection.  It will switch to heap sort after
/// FASTBIT_QSORT_MAX_DEPTH levels of recursion.  Performs recursive call
/// only on the smaller half, while iterate over the larger half.
template <typename T1, typename T2>
void ibis::util::sort_quick(array_t<T1> &keys, array_t<T2> &vals,
                            uint32_t lvl) {
    const uint32_t nelm =
        (keys.size() <= vals.size() ? keys.size() : vals.size());
    uint32_t back = nelm;
    uint32_t front = 0;
    while (back >= front + FASTBIT_QSORT_MIN) {
        // find the pivot element
        uint32_t pivot;
        if (front > 0 || back < nelm) {
            array_t<T1> ktmp(keys, front, back);
            array_t<T2> vtmp(vals, front, back);
            pivot = front + ibis::util::sort_partition(ktmp, vtmp);
        }
        else {
            pivot = ibis::util::sort_partition(keys, vals);
        }

        if (pivot >= back) {
            front = back;
        }
        else if (pivot-front <= back-pivot) { // the front part is smaller
            array_t<T1> kfront(keys, front, pivot);
            array_t<T2> vfront(vals, front, pivot);
            if (pivot-front >= FASTBIT_QSORT_MIN) {
                if (lvl <= FASTBIT_QSORT_MAX_DEPTH)
                    sort_quick(kfront, vfront, lvl+1);
                else // no more recursions
                    sort_heap(kfront, vfront);
            }
            else {
                sort_shell(kfront, vfront);
            }
            front = pivot;
        }
        else { // the back part is smaller
            array_t<T1> kback(keys, pivot, back);
            array_t<T2> vback(vals, pivot, back);
            if (back-pivot >= FASTBIT_QSORT_MIN) {
                if (lvl <= FASTBIT_QSORT_MAX_DEPTH)
                    sort_quick(kback, vback, lvl+1);
                else // no more recursions
                    sort_heap(kback, vback);
            }
            else {
                sort_shell(kback, vback);
            }
            back = pivot;
        }
    }
    if (back > front) { // sort the left over elements
        array_t<T1> kfront(keys, front, back);
        array_t<T2> vfront(vals, front, back);
        sort_shell(kfront, vfront);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "util::sort_quick(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed successfully";
    }
    else {
        const uint32_t nprt = ((nelm >> ibis::gVerbose) > 0 ?
                               (1 << ibis::gVerbose) : nelm);
        lg() << "Warning -- util::sort_quick(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed with errors";
        for (unsigned j = 0; j < nprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (nprt < nelm)
            lg() << "\n... " << nelm-nprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_quick

template <typename T1, typename T2>
void ibis::util::sort_quick3(array_t<T1> &keys, array_t<T2> &vals) {
    uint32_t j0, j1;
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    ibis::util::sort_partition3(keys, vals, j0, j1);
    if (0 < j0 && j0 < nelm) {
        array_t<T1> kfront(keys, 0, j0);
        array_t<T2> vfront(vals, 0, j0);
        if (j0 >= 32)
            sort_quick3(kfront, vfront);
        else
            sort_shell(kfront, vfront);
    }
    if (j0 < j1 && j1 < nelm) {
        array_t<T1> kback(keys, j1, nelm);
        array_t<T2> vback(vals, j1, nelm);
        if (nelm-j1 >= 32)
            sort_quick3(kback, vback);
        else
            sort_shell(kback, vback);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "util::sort_quick3(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed successfully";
    }
    else {
        lg() << "Warning -- util::sort_quick3(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed with errors";
        const uint32_t nprt = ((nelm >> ibis::gVerbose) > 0 ?
                               (1 << ibis::gVerbose) : nelm);
        for (unsigned j = 0; j < nprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (nprt < nelm)
            lg() << "\n... " << nelm-nprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_quick3

template <typename T1, typename T2>
void ibis::util::sort_heap(array_t<T1> &keys, array_t<T2> &vals) {
    uint32_t nelm = (keys.size() <= vals.size() ? keys.size() : vals.size());
    uint32_t parent = nelm / 2;
    uint32_t curr, child;
    T1 ktmp;
    T2 vtmp;
    while (true) {
        if (parent > 0) {
            // stage 1 -- form heap
            -- parent;
            ktmp = keys[parent];
            vtmp = vals[parent];
        }
        else {
            // stage 2 -- extract element from the top of heap
            -- nelm; // heap size, decrease by one
            if (nelm == 0) break;
            ktmp = keys[nelm];
            keys[nelm] = keys[0]; // top of the heap to position n
            vtmp = vals[nelm];
            vals[nelm] = vals[0];
        }

        // push-down procedure
        curr = parent;
        child = curr*2 + 1; // the left child
        while (child < nelm) {
            if (child+1 < nelm && keys[child+1] > keys[child])
                ++ child; // use the right child
            if (ktmp < keys[child]) {
                // the child has a larger value, need to decend further
                keys[curr] = keys[child];
                curr = child;
                child += child + 1;
            }
            else { // heap property is satisfied
                break;
            }
        } // while (child < n)
        // temporary values go into their final locations
        keys[curr] = ktmp;
        vals[curr] = vtmp;
    } // while (true)

#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    nelm = (keys.size() <= vals.size() ? keys.size() : vals.size());
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "util::sort_heap(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed successfully";
    }
    else {
        const uint32_t nprt = ((nelm >> ibis::gVerbose) > 0 ?
                               (1 << ibis::gVerbose) : nelm);
        lg() << "Warning -- util::sort_heap(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed with errors";
        for (unsigned j = 0; j < nprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (nprt < nelm)
            lg() << "\n... " << nelm-nprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_heap

template <typename T1, typename T2>
uint32_t ibis::util::sort_partition(array_t<T1> &keys, array_t<T2> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm < 7) {
        ibis::util::sort_shell(keys, vals);
        return nelm;
    }

    // pick the median of three values
    T1 pivot[3];
    pivot[0] = keys[0];
    pivot[1] = keys[nelm/2];
    pivot[2] = keys[nelm-1];
    if (pivot[0] > pivot[1]) {
        T1 ptmp = pivot[0];
        pivot[0] = pivot[1];
        pivot[1] = ptmp;
    }
    if (pivot[1] > pivot[2]) {
        pivot[1] = pivot[2];
        if (pivot[0] > pivot[1])
            pivot[1] = pivot[0];
    }
    pivot[0] = pivot[1]; // put the middle one in front for ease of reference

    uint32_t i0 = 0;
    uint32_t i1 = nelm;
    while (i0 < i1) {
        if (keys[i1-1] >= *pivot) {
            -- i1;
        }
        else if (keys[i0] < *pivot) {
            ++ i0;
        }
        else {
            // exchange i0, i1
            -- i1;
            T1 ktmp = keys[i0];
            keys[i0] = keys[i1];
            keys[i1] = ktmp;
            T2 vtmp = vals[i0];
            vals[i0] = vals[i1];
            vals[i1] = vtmp;
            ++ i0;
        }
    }
    if (i0 == 0) {
        // The median of three was the smallest value, switch to have the
        // left side <= pivot.
        i1 = nelm;
        while (i0 < i1) {
            if (keys[i1-1] > *pivot) {
                -- i1;
            }
            else if (keys[i0] <= *pivot) {
                ++ i0;
            }
            else {
                // exchange i0, i1
                -- i1;
                T1 ktmp = keys[i0];
                keys[i0] = keys[i1];
                keys[i1] = ktmp;
                T2 vtmp = vals[i0];
                vals[i0] = vals[i1];
                vals[i1] = vtmp;
                ++ i0;
            }
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    lg() << "util::sort_partition(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed with i0 = "
         << i0 << " and i1 = " << i1;
    lg() << "\npartition 1 (<  ";
    if (sizeof(T1) > 1)
        lg() << *pivot;
    else
        lg() << (int)*pivot;
    lg() << ") # elements = "
         << i0 << ": ";
    uint32_t j1 = (iprt <= i0 ? iprt : i0);
    if (sizeof(T1) > 1) {
        for (uint32_t j0 = 0; j0 < j1; ++ j0)
            lg() << keys[j0] << " ";
    }
    else {
        for (uint32_t j0 = 0; j0 < j1; ++ j0)
            lg() << (int) keys[j0] << " ";
    }
    if (j1 < i0)
        lg() << " ... (" << i0 - j1 << " ommitted)";
    lg() << "\npartition 2 (>= ";
    if (sizeof(T1) > 1)
        lg() << *pivot;
    else
        lg() << (int) *pivot;
    lg() << ") # elements = " << nelm - i0 << ": ";
    j1 = (i0+iprt < nelm ? i0+iprt : nelm);
    if (sizeof(T1) > 1) {
        for (uint32_t j0 = i0; j0 < j1; ++ j0)
            lg() << keys[j0] << " ";
    }
    else {
        for (uint32_t j0 = i0; j0 < j1; ++ j0)
            lg() << (int) keys[j0] << " ";
    }
    if (nelm > j1)
        lg() << " ... (" << nelm - j1 << " ommitted)";
#endif
    return i0;
} // ibis::util::sort_partition

template <typename T1, typename T2>
void ibis::util::sort_partition3(array_t<T1> &keys, array_t<T2> &vals,
                                 uint32_t &starteq, uint32_t &startgt) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm < 13) {
        ibis::util::sort_shell(keys, vals);
        starteq = keys.size();
        startgt = keys.size();
        return;
    }

    // pick the median of five values
    T1 pivot[5];
    pivot[0] = keys[0];
    pivot[1] = keys[nelm/4];
    pivot[2] = keys[nelm/2];
    pivot[3] = keys[3*nelm/4];
    pivot[4] = keys[nelm-1];
    for (uint32_t j = 3; j < 5; ++ j) {
        if (pivot[j] < pivot[j-3]) {
            T1 ptmp = pivot[j];
            uint32_t i = j;
            while (i >= 3 && pivot[i] < pivot[i-3]) {
                pivot[i] = pivot[i-3];
                i -= 3;
            }
            pivot[i] = ptmp;
            
        }
    }
    for (uint32_t j = 1; j < 5; ++ j) {
        if (pivot[j] < pivot[j-1]) {
            T1 ptmp = pivot[j];
            uint32_t i = j;
            while (i >= 1 && pivot[i] < pivot[i-1]) {
                pivot[i] = pivot[i-1];
                -- i;
            }
            pivot[i] = ptmp;
        }
    }
    pivot[0] = pivot[2]; // put the middle one in front for ease of reference

    uint32_t i0 = 0;
    uint32_t i1 = nelm;
    uint32_t j0 = 0;
    uint32_t j1 = nelm;
    while (i0 < i1-1) {
        if (keys[i1-1] > *pivot) {
            -- i1;
        }
        else if (keys[i0] < *pivot) {
            ++ i0;
        }
        else {
            // exchange i0, i1
            -- i1;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            std::cout << "DEBUG -- util::sort_partition3 swapping keys["
                      << i0 << "] (" << keys[i0] << " with keys[" << i1
                      << "] (" << keys[i1] << std::endl;
#endif
            T1 ktmp = keys[i0];
            keys[i0] = keys[i1];
            keys[i1] = ktmp;
            T2 vtmp = vals[i0];
            vals[i0] = vals[i1];
            vals[i1] = vtmp;
            if (keys[i0] == *pivot) { // exchange i0, j0
                ktmp = keys[i0];
                keys[i0] = keys[j0];
                keys[j0] = ktmp;
                vtmp = vals[i0];
                vals[i0] = vals[j0];
                vals[j0] = vtmp;
                ++ j0;
            }
            ++ i0;
            if (keys[i1] == *pivot) { // exahnge i1, j1
                -- j1;
                ktmp = keys[i1];
                keys[i1] = keys[j1];
                keys[j1] = ktmp;
                vtmp = vals[i1];
                vals[i1] = vals[j1];
                vals[j1] = vtmp;
            }
        }
    }
    if (i0 < i1) { // can only be i0 == i1-1
        i1 -= (keys[i0] >= *pivot);
        i0 += (keys[i0] <= *pivot);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    std::cout << "DEBUG -- util::sort_partition3 -- keys[" << i0
              << "] = " << keys[i0] << ", keys[" << i1 << "] = "
              << keys[i1] << ", pivot = " << *pivot << std::endl;
#endif
    for (uint32_t j = 0; j < j0; ++ j) {
        -- i1;
        T1 ktmp = keys[j];
        keys[j] = keys[i1];
        keys[i1] = ktmp;
        T2 vtmp = vals[j];
        vals[j] = vals[i1];
        vals[i1] = vtmp;
    }
    for (uint32_t j = j1; j < nelm; ++ j) {
        T1 ktmp = keys[i0];
        keys[i0] = keys[j];
        keys[j] = ktmp;
        T2 vtmp = vals[i0];
        vals[i0] = vals[j];
        vals[j] = vtmp;
        ++ i0;
    }
    starteq = i1;
    startgt = i0;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    uint32_t cnt1 = 0;
    uint32_t cnt2 = 0;
    uint32_t cnt3 = 0;
    for (uint32_t j = 0; j < i1; ++j)
        cnt1 += (keys[j] >= *pivot);
    for (uint32_t j = i1; j < i0; ++j)
        cnt2 += (keys[j] != *pivot);
    for (uint32_t j = i0; j < nelm; ++j)
        cnt3 += (keys[j] <= *pivot);

    ibis::util::logger lg(4);
    lg() << (cnt1+cnt2+cnt3>0 ? "Warning -- " : "")
         << "util::sort_partition3(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed with starteq = "
         << starteq << " and startgt = " << startgt;
    lg() << "\npartition 1 ( < ";
    if (sizeof(T1) > 1)
        lg() << *pivot;
    else
        lg() << (int)*pivot;
    lg() << ", cnt1 = " << cnt1 << ") # elements = " << starteq << ": ";
    j1 = (iprt <= starteq ? iprt : starteq);
    if (sizeof(T1) > 1) {
        for (j0 = 0; j0 < j1; ++ j0)
            lg() << keys[j0] << " ";
    }
    else {
        for (j0 = 0; j0 < j1; ++ j0)
            lg() << (int) keys[j0] << " ";
    }
    if (j1 < starteq)
        lg() << " ... (" << starteq - j1 << " ommitted)";
    lg() << "\npartition 2 ( = ";
    if (sizeof(T1) > 1)
        lg() << *pivot;
    else
        lg() << (int)*pivot;
    lg() << ", cnt2 = " << cnt2 << ") # elements = "
         << startgt - starteq << ": ";
    j1 = (starteq+iprt <= startgt ? starteq+iprt  : startgt);
    if (sizeof(T1) > 1) {
        for (j0 = starteq; j0 < j1; ++ j0)
            lg() << keys[j0] << " ";
    }
    else {
        for (j0 = starteq; j0 < j1; ++ j0)
            lg() << (int)keys[j0] << " ";
    }
    if (j1 < startgt)
        lg() << " ... (" << startgt - j1 << " ommitted)";
    lg() << "\npartition 3 ( > ";
    if (sizeof(T1) > 1)
        lg() << *pivot;
    else
        lg() << (int) *pivot;
    lg() << ", cnt3 = " << cnt3 << ") # elements = "
         << nelm - startgt << ": ";
    j1 = (startgt+iprt < nelm ? startgt+iprt : nelm);
    if (sizeof(T1) > 1) {
        for (j0 = startgt; j0 < j1; ++ j0)
            lg() << keys[j0] << " ";
    }
    else {
        for (j0 = startgt; j0 < j1; ++ j0)
            lg() << (int) keys[j0] << " ";
    }
    if (nelm > j1)
        lg() << " ... (" << nelm - j1 << " ommitted)";
#endif
} // ibis::util::sort_partition3

template <typename T1, typename T2>
void ibis::util::sort_shell(array_t<T1> &keys, array_t<T2> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    uint32_t gap = nelm / 2;
    while (gap >= shellgaps[7]) {
        for (uint32_t j = gap; j < nelm; ++ j) {
            const T1 ktmp = keys[j];
            const T2 vtmp = vals[j];
            uint32_t i = j;
            while (i >= gap && keys[i-gap] > ktmp) {
                keys[i] = keys[i-gap];
                vals[i] = vals[i-gap];
                i -= gap;
            }
            keys[i] = ktmp;
            vals[i] = vtmp;
        }
        gap = (uint32_t) (gap / 2.25);
    }

    int ig = 7;
    while (ig > 0 && gap < shellgaps[ig]) -- ig;
    while (ig >= 0) {
        gap = shellgaps[ig];
        for (uint32_t j = gap; j < nelm; ++ j) {
            const T1 ktmp = keys[j];
            const T2 vtmp = vals[j];
            uint32_t i = j;
            while (i >= gap && keys[i-gap] > ktmp) {
                keys[i] = keys[i-gap];
                vals[i] = vals[i-gap];
                i -= gap;
            }
            keys[i] = ktmp;
            vals[i] = vtmp;
        }
        -- ig;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "util::sort_shell(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed successfully";
    }
    else {
        lg() << "Warning -- util::sort_shell(keys[" << keys.size() << "], vals["
             << vals.size() << "]) completed with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_shell

template <typename T1, typename T2>
void ibis::util::sort_insertion(array_t<T1> &keys, array_t<T2> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    bool sorted = true;
    // first loop goes backward to find the smallest element
    for (uint32_t j = nelm-1; j > 0; -- j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            T1 ktmp = keys[j];
            keys[j] = keys[j-1];
            keys[j-1] = ktmp;
            T2 vtmp = vals[j];
            vals[j] = vals[j-1];
            vals[j-1] = vtmp;
        }
    }
    if (sorted) return;

    for (uint32_t i = 2; i < nelm; ++i) {
        T1 ktmp = keys[i];
        T2 vtmp = vals[i];
        uint32_t j = i;
        while (keys[j-1] > ktmp) {
            keys[j] = keys[j-1];
            vals[j] = vals[j-1];
            -- j;
        }
        keys[j] = ktmp;
        vals[j] = vtmp;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_insertion(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_insertion

/// It uses quick sort if there are more than FASTBIT_QSORT_MIN elements to
/// sort, otherwise, it uses shell sort.
///
/// @note This function operates completely in-memory; all arrays and
/// whatever auxiliary data must fit in memory.  Furthermore, FastBit does
/// not track the memory usage of std::vector nor std::string.  In case
/// this function runs out of memory, the two input arrays are left in
/// undefined states.  Normally, one should not have lost any data values,
/// but the values are in undetermined orders.  However, this is not
/// guaranteed to be the case.
void ibis::util::sortStrings(std::vector<std::string> &keys,
                             array_t<uint32_t> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    try {
        if (nelm >= FASTBIT_QSORT_MIN) {
            sortStrings(keys, vals, 0, nelm);
        }
        else if (nelm > 1) {
            sortStrings_shell(keys, vals, 0, nelm);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- util::sortStrings failed with exception "
            << e.what();
    }
    catch (const char *s) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- util::sortStrings failed with string exception "
            << s;
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- util::sortStrings failed with an expected exception";
    }
} // ibis::util::sortStrings

/// Quick-sort for strings with shell sort as clean-up procedure.
void ibis::util::sortStrings(std::vector<std::string> &keys,
                             array_t<uint32_t> &vals,
                             uint32_t begin, uint32_t end) {
    while (end >= begin+FASTBIT_QSORT_MIN) {
        uint32_t split = sortStrings_partition(keys, vals, begin, end);
        if (split < end) {
            if (split - begin <= end - split) {
                sortStrings(keys, vals, begin, split);
                begin = split;
            }
            else {
                sortStrings(keys, vals, split, end);
                end = split;
            }
        }
        else {
            begin = split;
        }
    }

    if (end > begin) { // clean up with shell sort
        sortStrings_shell(keys, vals, begin, end);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = begin+1; j < end; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = begin+(((end-begin) >> ibis::gVerbose) > 0 ?
                           (1 << ibis::gVerbose) : (end-begin));
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sortStrings(keys[" << keys.size()
         << "], vals[" << vals.size() << "], " << begin << ", " << end
         << ") completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = begin; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < end)
            lg() << "\n... " << end-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sortStrings

void ibis::util::sortStrings_shell(std::vector<std::string> &keys,
                                   ibis::array_t<uint32_t> &vals,
                                   uint32_t begin, uint32_t end) {
    const uint32_t nelm = end - begin;
    uint32_t gap = nelm / 2;
    while (gap >= shellgaps[7]) {
        for (uint32_t j = begin+gap; j < end; ++ j) {
            const uint32_t vtmp = vals[j];
            uint32_t i = j;
            while (i >= begin+gap && keys[i].compare(keys[i-gap]) < 0) {
                keys[i].swap(keys[i-gap]);
                vals[i] = vals[i-gap];
                i -= gap;
            }
            vals[i] = vtmp;
        }
        gap = (uint32_t) (gap / 2.25);
    }

    int ig = 7;
    while (ig > 0 && gap < shellgaps[ig]) -- ig;
    while (ig >= 0) {
        gap = shellgaps[ig];
        for (uint32_t j = begin+gap; j < end; ++ j) {
            const uint32_t vtmp = vals[j];
            uint32_t i = j;
            while (i >= begin+gap && keys[i].compare(keys[i-gap]) < 0) {
                keys[i].swap(keys[i-gap]);
                vals[i] = vals[i-gap];
                i -= gap;
            }
            vals[i] = vtmp;
        }
        -- ig;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = begin+1; j < end; ++ j) {
        if (keys[j-1].compare(keys[j]) > 0) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sortStrings_shell(keys[" << keys.size()
         << "], vals[" << vals.size() << "], " << begin << ", "
         << end << ") completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = begin; j < begin+iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sortStrings_shell

/// Median-of-3 partitioning algorithm.
#if defined(_MSC_VER) && defined(_WIN32) && _MSC_VER < 1400
#pragma optimize("g", off)
#endif
uint32_t
ibis::util::sortStrings_partition(std::vector<std::string> &keys,
                                  array_t<uint32_t> &vals,
                                  uint32_t begin, uint32_t end) {
    if (end < begin+7) {
        ibis::util::sortStrings_shell(keys, vals, begin, end);
        return end;
    }

    // sort three values at position 0, nelm/2, and nelm-1
    if (keys[begin].compare(keys[(begin+end)/2]) > 0) {
        keys[begin].swap(keys[(begin+end)/2]);
        uint32_t vtmp = vals[begin];
        vals[begin] = vals[(begin+end)/2];
        vals[(begin+end)/2] = vtmp;
    }
    if (keys[(begin+end)/2].compare(keys[end-1]) > 0) {
        keys[(begin+end)/2].swap(keys[end-1]);
        uint32_t vtmp = vals[(begin+end)/2];
        vals[(begin+end)/2] = vals[end-1];
        vals[end-1] = vtmp;
        if (keys[begin].compare(keys[(begin+end)/2]) > 0) {
            keys[begin].swap(keys[(begin+end)/2]);
            vtmp = vals[begin];
            vals[begin] = vals[(begin+end)/2];
            vals[(begin+end)/2] = vtmp;
        }
    }
    // pick the median of three values
    std::string pivot(keys[(begin+end)/2]);

    uint32_t i0 = begin;
    uint32_t i1 = end;
    while (i0 < i1) {
        if (pivot.compare(keys[i1-1]) <= 0) {
            -- i1;
        }
        else if (pivot.compare(keys[i0]) > 0) {
            ++ i0;
        }
        else {
            // exchange i0, i1
            -- i1;
            keys[i0].swap(keys[i1]);
            uint32_t vtmp = vals[i0];
            vals[i0] = vals[i1];
            vals[i1] = vtmp;
            ++ i0;
        }
    }
    if (i0 == begin) {
        // The median of three was the smallest value, switch to have the
        // left side <= pivot.
        i1 = end;
        while (i0 < i1) {
            if (pivot.compare(keys[i1-1]) < 0) {
                -- i1;
            }
            else if (pivot.compare(keys[i0]) >= 0) {
                ++ i0;
            }
            else {
                // exchange i0, i1
                -- i1;
                keys[i0].swap(keys[i1]);
                uint32_t vtmp = vals[i0];
                vals[i0] = vals[i1];
                vals[i1] = vtmp;
                ++ i0;
            }
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    const uint32_t nelm = end - begin;
    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    lg() << "util::sortStrings_partition(keys[" << keys.size()
         << "], vals[" << vals.size() << "], " << begin << ", "
         << end << ") completed with i0 = " << i0 << " and i1 = " << i1
         << ", pivot = \"" << pivot << "\"";
    lg() << "\npartition 1, # elements = " << i0 << ": ";
    uint32_t j1 = (begin+iprt <= i0 ? begin+iprt : i0);
    for (uint32_t j0 = begin; j0 < j1; ++ j0)
        lg() << keys[j0] << " ";
    if (j1 < i0)
        lg() << " ... (" << i0 - j1 << " ommitted)";
    lg() << "\npartition 2, # elements = " << end - i0 << ": ";
    j1 = (i0+iprt < end ? i0+iprt : end);
    for (uint32_t j0 = i0; j0 < j1; ++ j0)
        lg() << keys[j0] << " ";
    if (end > j1)
        lg() << " ... (" << end - j1 << " ommitted)";
#endif
    return i0;
} // ibis::util::sortStrings_partition

/// It uses quick sort if there are more than FASTBIT_QSORT_MIN elements to
/// sort, otherwise, it uses shell sort.
///
/// @note This function operates completely in-memory; all arrays and
/// whatever auxiliary data must fit in memory.
void ibis::util::sortStrings(ibis::array_t<const char*> &keys,
                             array_t<uint32_t> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm >= FASTBIT_QSORT_MIN) {
        sortStrings(keys, vals, 0, nelm);
    }
    else if (nelm > 1) {
        sortStrings_shell(keys, vals, 0, nelm);
    }
} // ibis::util::sortStrings

/// Quick-sort for strings with shell sort as clean-up procedure.
void ibis::util::sortStrings(ibis::array_t<const char*> &keys,
                             ibis::array_t<uint32_t> &vals,
                             uint32_t begin, uint32_t end) {
    while (end >= begin+FASTBIT_QSORT_MIN) {
        uint32_t split = sortStrings_partition(keys, vals, begin, end);
        if (split < end) {
            if (split - begin <= end - split) {
                sortStrings(keys, vals, begin, split);
                begin = split;
            }
            else {
                sortStrings(keys, vals, split, end);
                end = split;
            }
        }
        else {
            begin = split;
        }
    }

    if (end > begin) { // clean up with shell sort
        sortStrings_shell(keys, vals, begin, end);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = begin+1; j < end; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = begin+(((end-begin) >> ibis::gVerbose) > 0 ?
                           (1 << ibis::gVerbose) : (end-begin));
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sortStrings(keys[" << keys.size()
         << "], vals[" << vals.size() << "], " << begin << ", " << end
         << ") completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = begin; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < end)
            lg() << "\n... " << end-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sortStrings

/// Shell sort of string values.  The string values are represented as
/// "const char*".  This function uses std::strcmp to perform the
/// comparisons of string.  Since strcmp can not accept nil pointers, this
/// function has to check for nil pointers, which effectively treats a nil
/// pointer as a special value of string that is less than all other string
/// values.
void ibis::util::sortStrings_shell(ibis::array_t<const char*> &keys,
                                   ibis::array_t<uint32_t> &vals,
                                   uint32_t begin, uint32_t end) {
    const uint32_t nelm = end - begin;
    uint32_t gap = nelm / 2;
    while (gap >= shellgaps[7]) {
        for (uint32_t j = begin+gap; j < end; ++ j) {
            const char *ktmp = keys[j];
            const uint32_t vtmp = vals[j];
            uint32_t i = j;
            // keys[i-gap] > keys[i]
            while (i >= begin+gap && keys[i-gap] != 0 &&
                   (keys[i] == 0 || std::strcmp(keys[i-gap], keys[i]) > 0)) {
                keys[i] = keys[i-gap];
                vals[i] = vals[i-gap];
                i -= gap;
            }
            keys[i] = ktmp;
            vals[i] = vtmp;
        }
        gap = (uint32_t) (gap / 2.25);
    }

    int ig = 7;
    while (ig > 0 && gap < shellgaps[ig]) -- ig;
    while (ig >= 0) {
        gap = shellgaps[ig];
        for (uint32_t j = begin+gap; j < end; ++ j) {
            const char *ktmp = keys[j];
            const uint32_t vtmp = vals[j];
            uint32_t i = j;
            // keys[i-gap] > ktmp
            while (i >= begin+gap && keys[i-gap] != 0 &&
                   (ktmp == 0 || std::strcmp(keys[i-gap], ktmp) > 0)) {
                keys[i] = keys[i-gap];
                vals[i] = vals[i-gap];
                i -= gap;
            }
            keys[i] = ktmp;
            vals[i] = vtmp;
#if DEBUG+0 > 3 || _DEBUG+0 > 3
            bool sorted = true;
            for (i = begin + (j-begin) % gap; i < j; i += gap) {
                // keys[i] > keys[i+gap]
                if (keys[i] != 0 && (keys[i+gap] == 0 ||
                                     std::strcmp(keys[i], keys[i+gap]) > 0)) {
                    sorted = false;
                    break;
                }
            }
            if (! sorted) {
                ibis::util::logger lg(4);
                lg() << "Warning -- util::sortStrings_shell expected the "
                    "following to be nonincreasing sequence, but it is not:\n";
                for (i = j; i > begin; i -= gap)
                    lg() << ' ' << keys[i];
            }
#endif
        }
        -- ig;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
        {
            ibis::util::logger lg(4);
            lg() << "DEBUG -- util::sortStrings_shell - gap = " << gap;
            for (unsigned j = 0; j < gap; ++ j) {
                lg() << "\n\t" << j << ": ";
                for (unsigned i = begin+j; i < end; i += gap)
                    lg() << keys[i] << " ";
            }
        }
#endif
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    bool sorted = true;
    for (uint32_t j = begin+1; j < end; ++ j) {
        // keys[j-1] > keys[j]
        if (keys[j-1] != 0 && (keys[j] == 0 ||
                               std::strcmp(keys[j-1], keys[j]) > 0)) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sortStrings_shell(keys[" << keys.size() << "], vals["
         << vals.size() << "], " << begin << ", " << end
         << ") completed " << (sorted?"successfully":"with errors");
    for (unsigned j = begin; j < begin+iprt; ++ j)
        // keys[j-1]>keys[j] --> '*'
        lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j << "]="
             << vals[j]
             << (j>begin ?
                 (keys[j-1] != && (keys[j] == 0 ||
                                   std::strcmp(keys[j-1],keys[j])>0)) ?
                 '*' : ' ') : ' ');
    if (iprt < nelm)
        lg() << "\n... " << nelm-iprt << " ommitted\n";
#endif
} // ibis::util::sortStrings_shell

/// Median-of-3 partitioning algorithm.  The string values are represented
/// as "const char*".  This function uses std::strcmp to perform the
/// comparisons of string.  Since strcmp can not accept nil pointers, this
/// function has to check for nil pointers, which effectively treats a nil
/// pointer as a special value of string that is less than all other string
/// values.
#if defined(_MSC_VER) && defined(_WIN32) && _MSC_VER < 1400
#pragma optimize("g", off)
#endif
uint32_t
ibis::util::sortStrings_partition(ibis::array_t<const char*> &keys,
                                  array_t<uint32_t> &vals, uint32_t begin,
                                  uint32_t end) {
    if (end < begin+7) {
        ibis::util::sortStrings_shell(keys, vals, begin, end);
        return end;
    }

    // sort three values at position 0, nelm/2, and nelm-1
    // keys[begin] > keys[(begin+end)/2]
    if (keys[begin] != 0 &&
        (keys[(begin+end)/2] == 0 ||
         std::strcmp(keys[begin], keys[(begin+end)/2]) > 0)) {
        const char *ktmp = keys[begin];
        keys[begin] = keys[(begin+end)/2];
        keys[(begin+end)/2] = ktmp;
        const uint32_t vtmp = vals[begin];
        vals[begin] = vals[(begin+end)/2];
        vals[(begin+end)/2] = vtmp;
    }
    // keys[(begin+end)/2] > keys[end-1]
    if (keys[(begin+end)/2] != 0 &&
        (keys[end-1] == 0 ||
         std::strcmp(keys[(begin+end)/2], keys[end-1]) > 0)) {
        const char *ktmp = keys[(begin+end)/2];
        keys[(begin+end)/2] = keys[end-1];
        keys[end-1] = ktmp;
        uint32_t vtmp = vals[(begin+end)/2];
        vals[(begin+end)/2] = vals[end-1];
        vals[end-1] = vtmp;
        // keys[begin] > keys[(begin+end)/2]
        if (keys[begin] != 0 &&
            (keys[(begin+end)/2] == 0 ||
             std::strcmp(keys[begin], keys[(begin+end)/2]) > 0)) {
            ktmp = keys[begin];
            keys[begin] = keys[(begin+end)/2];
            keys[(begin+end)/2] = ktmp;
            vtmp = vals[begin];
            vals[begin] = vals[(begin+end)/2];
            vals[(begin+end)/2] = vtmp;
        }
    }
    // pick the median of three values
    const char* pivot(keys[(begin+end)/2]);

    uint32_t i0 = begin;
    uint32_t i1 = end;
    // normal partition: left side < pivot, right side >= pivot
    while (i0 < i1) {
        // keys[i1-1] >= pivot
        if ((keys[i1-1] != 0 &&
             (pivot == 0 || std::strcmp(keys[i1-1], pivot) >= 0))
            || (keys[i1-1] == 0 && pivot == 0)) {
            -- i1;
        }
        else if (pivot != 0 &&
                 (keys[i0] == 0 || std::strcmp(pivot, keys[i0]) > 0)) {
            ++ i0;
        }
        else {
            // exchange i0, i1
            -- i1;
            const char *ktmp = keys[i0];
            keys[i0] = keys[i1];
            keys[i1] = ktmp;
            const uint32_t vtmp = vals[i0];
            vals[i0] = vals[i1];
            vals[i1] = vtmp;
            ++ i0;
        }
    }
    if (i0 == begin) {
        // The pivot was the smallest value, switch to have the
        // left side <= pivot.
        i1 = end;
        while (i0 < i1) {
            // keys[i1-1] > pivot
            if (keys[i1-1] != 0 &&
                (pivot == 0 || std::strcmp(keys[i1-1], pivot) > 0)) {
                -- i1;
            }
            // pivot >= keys[i0]
            else if ((pivot != 0 &&
                      (keys[i0] == 0 || std::strcmp(pivot, keys[i0]) >= 0))
                     || (pivot == 0 && keys[i0] == 0)) {
                ++ i0;
            }
            else {
                // exchange i0, i1
                -- i1;
                const char* ktmp = keys[i0];
                keys[i0] = keys[i1];
                keys[i1] = ktmp;
                const uint32_t vtmp = vals[i0];
                vals[i0] = vals[i1];
                vals[i1] = vtmp;
                ++ i0;
            }
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    const uint32_t nelm = end - begin;
    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    lg() << "util::sortStrings_partition(keys[" << keys.size()
         << "], vals[" << vals.size() << "], " << begin << ", "
         << end << ") completed with i0 = " << i0 << " and i1 = " << i1
         << ", pivot = \"" << pivot << "\"";
    lg() << "\npartition 1, # elements = " << i0 << ": ";
    uint32_t j1 = (begin+iprt <= i0 ? begin+iprt : i0);
    for (uint32_t j0 = begin; j0 < j1; ++ j0)
        lg() << keys[j0] << " ";
    if (j1 < i0)
        lg() << " ... (" << i0 - j1 << " ommitted)";
    lg() << "\npartition 2, # elements = " << end - i0 << ": ";
    j1 = (i0+iprt < end ? i0+iprt : end);
    for (uint32_t j0 = i0; j0 < j1; ++ j0)
        lg() << keys[j0] << " ";
    if (end > j1)
        lg() << " ... (" << end - j1 << " ommitted)";
#endif
    return i0;
} // ibis::util::sortStrings_partition

template <typename T>
void ibis::util::sort_radix(array_t<char> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    array_t<uint32_t> offsets(256, 0);
    bool sorted = true;
    ++ offsets[keys[0]+128];
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offsets[keys[j]+128];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    uint32_t maxv = offsets[0];
    uint32_t prev = offsets[0];
    offsets[0] = 0;
    for (uint32_t j = 1; j < 256; ++ j) {
        const uint32_t cnt = offsets[j];
        offsets[j] = prev;
        prev += cnt;
        if (maxv < cnt) maxv = cnt;
    }
    if (maxv < nelm) { // not all the same
        array_t<char> ktmp(nelm);
        array_t<T> vtmp(nelm);
        for (uint32_t j = 0; j < nelm; ++ j) {
            ktmp[offsets[keys[j]+128]] = keys[j];
            vtmp[offsets[keys[j]+128]] = vals[j];
            ++ offsets[keys[j]+128];
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << std::hex << keys[j]
                 << std::dec << ", vals[" << j << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<signed char> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    array_t<uint32_t> offsets(256, 0);
    if (nelm <= 1) return;

    bool sorted = true;
    ++ offsets[keys[0]+128];
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offsets[keys[j]+128];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    uint32_t maxv = offsets[0];
    uint32_t prev = offsets[0];
    offsets[0] = 0;
    for (uint32_t j = 1; j < 256; ++ j) {
        const uint32_t cnt = offsets[j];
        offsets[j] = prev;
        prev += cnt;
        if (maxv < cnt) maxv = cnt;
    }
    if (maxv < nelm) {
        array_t<signed char> ktmp(nelm);
        array_t<T> vtmp(nelm);
        for (uint32_t j = 0; j < nelm; ++ j) {
            ktmp[offsets[keys[j]+128]] = keys[j];
            vtmp[offsets[keys[j]+128]] = vals[j];
            ++ offsets[keys[j]+128];
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << std::hex << keys[j]
                 << std::dec << ", vals[" << j << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<unsigned char> &keys,
                            array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    array_t<uint32_t> offsets(256, 0);
    if (nelm <= 1) return;

    bool sorted = true;
    offsets[keys[0]] = 1;
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offsets[keys[j]];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    uint32_t maxv = offsets[0];
    uint32_t prev = offsets[0];
    offsets[0] = 0;
    for (uint32_t j = 1; j < 256; ++ j) {
        const uint32_t cnt = offsets[j];
        offsets[j] = prev;
        prev += cnt;
        if (cnt > maxv) maxv = cnt;
    }

    if (maxv < nelm) {
        array_t<unsigned char> ktmp(nelm);
        array_t<T> vtmp(nelm);
        for (uint32_t j = 0; j < nelm; ++ j) {
            ktmp[offsets[keys[j]]] = keys[j];
            vtmp[offsets[keys[j]]] = vals[j];
            ++ offsets[keys[j]];
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << std::hex << keys[j]
                 << std::dec << ", vals[" << j << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<int16_t> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    try { // try the one-pass approach first
        array_t<uint32_t> offsets(65536, 0);
        bool sorted = true;
        // count the number of values in each bucket
        offsets[keys[0]+32768] = 1;
        for (uint32_t j = 1; j < nelm; ++ j) {
            ++ offsets[keys[j]+32768];
            sorted = sorted && (keys[j] >= keys[j-1]);
        }
        if (sorted) return;

        // determine the starting positions for each bucket
        uint32_t max1 = offsets[0];
        uint32_t prev1 = offsets[0];
        offsets[0] = 0;
        for (uint32_t j = 1; j < 65536; ++ j) {
            const uint32_t cnt1 = offsets[j];
            offsets[j] = prev1;
            prev1 += cnt1;
            if (max1 < cnt1) max1 = cnt1;
        }

        array_t<int16_t> ktmp(nelm);
        array_t<T> vtmp(nelm);
        // distribution 1
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offsets[keys[j]+32768];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
    catch (...) { // now try to two-pass appraoch
        array_t<uint32_t> offset1(256, 0);
        array_t<uint32_t> offset2(256, 0);
        bool sorted = true;
        // count the number of values in each bucket
        offset1[keys[0]&255] = 1;
        offset2[(keys[0]>>8)+128] = 1;
        for (uint32_t j = 1; j < nelm; ++ j) {
            ++ offset1[keys[j]&255];
            ++ offset2[(keys[j]>>8)+128];
            sorted = sorted && (keys[j] >= keys[j-1]);
        }
        if (sorted) return;

        // determine the starting positions for each bucket
        uint32_t max1 = offset1[0];
        uint32_t max2 = offset2[0];
        uint32_t prev1 = offset1[0];
        uint32_t prev2 = offset2[0];
        offset1[0] = 0;
        offset2[0] = 0;
        for (uint32_t j = 1; j < 256; ++ j) {
            const uint32_t cnt1 = offset1[j];
            const uint32_t cnt2 = offset2[j];
            offset1[j] = prev1;
            offset2[j] = prev2;
            prev1 += cnt1;
            prev2 += cnt2;
            if (max1 < cnt1) max1 = cnt1;
            if (max2 < cnt2) max2 = cnt2;
        }
        if (max1 == nelm && max2 == nelm) return;

        array_t<int16_t> ktmp(nelm);
        array_t<T> vtmp(nelm);
        // distribution 1
        if (max1 < nelm) {
            for (uint32_t j = 0; j < nelm; ++ j) {
                uint32_t &pos = offset1[keys[j]&255];
                ktmp[pos] = keys[j];
                vtmp[pos] = vals[j];
                ++ pos;
            }
        }
        else {
            keys.swap(ktmp);
            vals.swap(vtmp);
        }

        // distribution 2
        if (max2 < nelm) {
            for (uint32_t j = 0; j < nelm; ++ j) {
                uint32_t &pos = offset2[(ktmp[j]>>8)+128];
                keys[pos] = ktmp[j];
                vals[pos] = vtmp[j];
                ++ pos;
            }
        }
        else {
            keys.swap(ktmp);
            vals.swap(vtmp);
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<uint16_t> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    try { // attempt to use 65536 bins
        array_t<uint32_t> offsets(65536, 0);
        bool sorted = true;
        // count the number of values in each bucket
        offsets[keys[0]] = 1;
        for (uint32_t j = 1; j < nelm; ++ j) {
            ++ offsets[keys[j]];
            sorted = sorted && (keys[j] >= keys[j-1]);
        }
        if (sorted) return;

        // determine the starting positions for each bucket
        uint32_t max1 = offsets[0];
        uint32_t prev1 = offsets[0];
        offsets[0] = 0;
        for (uint32_t j = 1; j < 65536; ++ j) {
            const uint32_t cnt1 = offsets[j];
            offsets[j] = prev1;
            prev1 += cnt1;
            if (cnt1 > max1) max1 = cnt1;
        }

        array_t<uint16_t> ktmp(nelm);
        array_t<T> vtmp(nelm);
        // distribution 1
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offsets[keys[j]];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
    catch (...) { // attempt to use 512 bins (256 x 2)
        array_t<uint32_t> offset1(256, 0);
        array_t<uint32_t> offset2(256, 0);
        bool sorted = true;
        // count the number of values in each bucket
        offset1[keys[0]&255] = 1;
        offset2[keys[0]>>8] = 1;
        for (uint32_t j = 1; j < nelm; ++ j) {
            ++ offset1[keys[j]&255];
            ++ offset2[keys[j]>>8];
            sorted = sorted && (keys[j] >= keys[j-1]);
        }
        if (sorted) return;

        // determine the starting positions for each bucket
        uint32_t max1 = offset1[0];
        uint32_t max2 = offset2[0];
        uint32_t prev1 = offset1[0];
        uint32_t prev2 = offset2[0];
        offset1[0] = 0;
        offset2[0] = 0;
        for (uint32_t j = 1; j < 256; ++ j) {
            const uint32_t cnt1 = offset1[j];
            const uint32_t cnt2 = offset2[j];
            offset1[j] = prev1;
            offset2[j] = prev2;
            prev1 += cnt1;
            prev2 += cnt2;
            if (cnt1 > max1) max1 = cnt1;
            if (cnt2 > max2) max2 = cnt2;
        }
        if (max1 == nelm && max2 == nelm) return;

        array_t<uint16_t> ktmp(nelm);
        array_t<T> vtmp(nelm);
        // distribution 1
        if (max1 < nelm) {
            for (uint32_t j = 0; j < nelm; ++ j) {
                uint32_t &pos = offset1[keys[j]&255];
                ktmp[pos] = keys[j];
                vtmp[pos] = vals[j];
                ++ pos;
            }
        }
        else {
            keys.swap(ktmp);
            vals.swap(vtmp);
        }

        // distribution 2
        if (max2 < nelm) {
            for (uint32_t j = 0; j < nelm; ++ j) {
                uint32_t &pos = offset2[ktmp[j]>>8];
                keys[pos] = ktmp[j];
                vals[pos] = vtmp[j];
                ++ pos;
            }
        }
        else {
            keys.swap(ktmp);
            vals.swap(vtmp);
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<int32_t> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    array_t<uint32_t> offset1(2048, 0); // 11-bit
    array_t<uint32_t> offset2(2048, 0); // 11-bit
    array_t<uint32_t> offset3(1024, 0); // 10-bit
    bool sorted = true;
    // count the number of values in each bucket
    offset1[keys[0]&2047] = 1;
    offset2[(keys[0]>>11)&2047] = 1;
    offset3[(keys[0]>>22)+512] = 1;
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offset1[keys[j]&2047];
        ++ offset2[(keys[j]>>11)&2047];
        ++ offset3[(keys[j]>>22)+512];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    // determine the starting positions for each bucket
    uint32_t max1 = offset1[0];
    uint32_t max2 = offset2[0];
    uint32_t max3 = offset3[0];
    uint32_t prev1 = offset1[0];
    uint32_t prev2 = offset2[0];
    uint32_t prev3 = offset3[0];
    offset1[0] = 0;
    offset2[0] = 0;
    offset3[0] = 0;
    for (uint32_t j = 1; j < 1024; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        if (cnt1 > max1) max1 = cnt1;
        if (cnt2 > max2) max2 = cnt2;
        if (cnt3 > max3) max3 = cnt3;
    }
    for (uint32_t j = 1024; j < 2048; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        prev1 += cnt1;
        prev2 += cnt2;
        if (cnt1 > max1) max1 = cnt1;
        if (cnt2 > max2) max2 = cnt2;
    }
    if (max1 == nelm && max2 == nelm && max2 == nelm) return;

    array_t<int32_t> ktmp(nelm);
    array_t<T> vtmp(nelm);
    // distribution 1
    if (max1 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset1[keys[j]&2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 2
    if (max2 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset2[(ktmp[j]>>11)&2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 3
    if (max3 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset3[(keys[j]>>22)+512];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<uint32_t> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    array_t<uint32_t> offset1(2048, 0); // 11-bit
    array_t<uint32_t> offset2(2048, 0); // 11-bit
    array_t<uint32_t> offset3(1024, 0); // 10-bit
    if (nelm <= 1) return;

    bool sorted = true;
    // count the number of values in each bucket
    ++ offset1[keys[0]&2047];
    ++ offset2[(keys[0]>>11)&2047];
    ++ offset3[(keys[0]>>22)];
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offset1[keys[j]&2047];
        ++ offset2[(keys[j]>>11)&2047];
        ++ offset3[(keys[j]>>22)];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    // determine the starting positions for each bucket
    uint32_t max1 = offset1[0];
    uint32_t max2 = offset2[0];
    uint32_t max3 = offset3[0];
    uint32_t prev1 = offset1[0];
    uint32_t prev2 = offset2[0];
    uint32_t prev3 = offset3[0];
    offset1[0] = 0;
    offset2[0] = 0;
    offset3[0] = 0;
    for (uint32_t j = 1; j < 1024; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
    }
    for (uint32_t j = 1024; j < 2048; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        prev1 += cnt1;
        prev2 += cnt2;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
    }
    if (max1 == nelm && max2 == nelm && max3 == nelm) return;

    array_t<uint32_t> ktmp(nelm);
    array_t<T> vtmp(nelm);
    // distribution 1
    if (max1 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset1[keys[j]&2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 2
    if (max2 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset2[(ktmp[j]>>11)&2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 3
    if (max3 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset3[(keys[j]>>22)];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<int64_t> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    array_t<uint32_t> offset1(2048, 0); // 11-bit
    array_t<uint32_t> offset2(2048, 0); // 11-bit
    array_t<uint32_t> offset3(2048, 0); // 11-bit
    array_t<uint32_t> offset4(2048, 0); // 11-bit
    array_t<uint32_t> offset5(1024, 0); // 10-bit
    array_t<uint32_t> offset6(1024, 0); // 10-bit
    bool sorted = true;
    // count the number of values in each bucket
    ++ offset1[keys[0]&2047];
    ++ offset2[(keys[0]>>11)&2047];
    ++ offset3[(keys[0]>>22)&2047];
    ++ offset4[(keys[0]>>33)&2047];
    ++ offset5[(keys[0]>>44)&1023];
    ++ offset6[(keys[0]>>54)+512];
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offset1[keys[j]&2047];
        ++ offset2[(keys[j]>>11)&2047];
        ++ offset3[(keys[j]>>22)&2047];
        ++ offset4[(keys[j]>>33)&2047];
        ++ offset5[(keys[j]>>44)&1023];
        ++ offset6[(keys[j]>>54)+512];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    // determine the starting positions for each bucket
    uint32_t max1 = offset1[0];
    uint32_t max2 = offset2[0];
    uint32_t max3 = offset3[0];
    uint32_t max4 = offset4[0];
    uint32_t max5 = offset5[0];
    uint32_t max6 = offset6[0];
    uint32_t prev1 = offset1[0];
    uint32_t prev2 = offset2[0];
    uint32_t prev3 = offset3[0];
    uint32_t prev4 = offset4[0];
    uint32_t prev5 = offset5[0];
    uint32_t prev6 = offset6[0];
    offset1[0] = 0;
    offset2[0] = 0;
    offset3[0] = 0;
    offset4[0] = 0;
    offset5[0] = 0;
    offset6[0] = 0;
    for (uint32_t j = 1; j < 1024; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        const uint32_t cnt4 = offset4[j];
        const uint32_t cnt5 = offset5[j];
        const uint32_t cnt6 = offset6[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        offset4[j] = prev4;
        offset5[j] = prev5;
        offset6[j] = prev6;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        prev4 += cnt4;
        prev5 += cnt5;
        prev6 += cnt6;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
        if (max4 < cnt4) max4 = cnt4;
        if (max5 < cnt5) max5 = cnt5;
        if (max6 < cnt6) max6 = cnt6;
    }
    for (uint32_t j = 1024; j < 2048; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        const uint32_t cnt4 = offset4[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        offset4[j] = prev4;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        prev4 += cnt4;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
        if (max4 < cnt4) max4 = cnt4;
    }
    if (max1 == nelm && max2 == nelm && max3 == nelm && max4 == nelm &&
        max5 == nelm && max6 == nelm) return;

    array_t<int64_t> ktmp(nelm);
    array_t<T> vtmp(nelm);
    // distribution 1
    if (max1 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset1[keys[j]&2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 2
    if (max2 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset2[(ktmp[j]>>11)&2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 3
    if (max3 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset3[(keys[j]>>22)&2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 4
    if (max4 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset4[(ktmp[j]>>33)&2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 5
    if (max5 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset5[(keys[j]>>44)&1023];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 6
    if (max6 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset6[(ktmp[j]>>54)+512];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<uint64_t> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    array_t<uint32_t> offset1(2048, 0); // 11-bit
    array_t<uint32_t> offset2(2048, 0); // 11-bit
    array_t<uint32_t> offset3(2048, 0); // 11-bit
    array_t<uint32_t> offset4(2048, 0); // 11-bit
    array_t<uint32_t> offset5(1024, 0); // 10-bit
    array_t<uint32_t> offset6(1024, 0); // 10-bit
    bool sorted = true;
    // count the number of values in each bucket
    ++ offset1[keys[0]&2047];
    ++ offset2[(keys[0]>>11)&2047];
    ++ offset3[(keys[0]>>22)&2047];
    ++ offset4[(keys[0]>>33)&2047];
    ++ offset5[(keys[0]>>44)&1023];
    ++ offset6[(keys[0]>>54)];
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offset1[keys[j]&2047];
        ++ offset2[(keys[j]>>11)&2047];
        ++ offset3[(keys[j]>>22)&2047];
        ++ offset4[(keys[j]>>33)&2047];
        ++ offset5[(keys[j]>>44)&1023];
        ++ offset6[(keys[j]>>54)];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    // determine the starting positions for each bucket
    uint32_t max1 = offset1[0];
    uint32_t max2 = offset2[0];
    uint32_t max3 = offset3[0];
    uint32_t max4 = offset4[0];
    uint32_t max5 = offset5[0];
    uint32_t max6 = offset6[0];
    uint32_t prev1 = offset1[0];
    uint32_t prev2 = offset2[0];
    uint32_t prev3 = offset3[0];
    uint32_t prev4 = offset4[0];
    uint32_t prev5 = offset5[0];
    uint32_t prev6 = offset6[0];
    offset1[0] = 0;
    offset2[0] = 0;
    offset3[0] = 0;
    offset4[0] = 0;
    offset5[0] = 0;
    offset6[0] = 0;
    for (uint32_t j = 1; j < 1024; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        const uint32_t cnt4 = offset4[j];
        const uint32_t cnt5 = offset5[j];
        const uint32_t cnt6 = offset6[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        offset4[j] = prev4;
        offset5[j] = prev5;
        offset6[j] = prev6;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        prev4 += cnt4;
        prev5 += cnt5;
        prev6 += cnt6;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
        if (max4 < cnt4) max4 = cnt4;
        if (max5 < cnt5) max5 = cnt5;
        if (max6 < cnt6) max6 = cnt6;
    }
    for (uint32_t j = 1024; j < 2048; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        const uint32_t cnt4 = offset4[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        offset4[j] = prev4;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        prev4 += cnt4;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
        if (max4 < cnt4) max4 = cnt4;
    }
    if (max1 == nelm && max2 == nelm && max3 == nelm && max4 == nelm &&
        max5 == nelm && max6 == nelm) return;

    array_t<uint64_t> ktmp(nelm);
    array_t<T> vtmp(nelm);
    // distribution 1
    if (max1 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset1[keys[j]&2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vtmp.swap(vals);
    }

    // distribution 2
    if (max2 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset2[(ktmp[j]>>11)&2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 3
    if (max3 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset3[(keys[j]>>22)&2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 4
    if (max4 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset4[(ktmp[j]>>33)&2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 5
    if (max5 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset5[(keys[j]>>44)&1023];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 6
    if (max6 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset6[(ktmp[j]>>54)];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<float> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    array_t<uint32_t> offset1(2048, 0); // 11-bit
    array_t<uint32_t> offset2(2048, 0); // 11-bit
    array_t<uint32_t> offset3(1024, 0); // 10-bit
    bool sorted = true;
    // count the number of values in each bucket
    const uint32_t *ikeys = reinterpret_cast<const uint32_t*>(keys.begin());
    offset1[(*ikeys)&2047] = 1;
    offset2[((*ikeys)>>11)&2047] = 1;
    offset3[((*ikeys)>>22)] = 1;
    for (uint32_t j = 1; j < nelm; ++ j) {
        const uint32_t &key = ikeys[j];
        ++ offset1[key&2047];
        ++ offset2[(key>>11)&2047];
        ++ offset3[(key>>22)];
        sorted = sorted && (keys[j]>=keys[j-1]);
    }
    if (sorted) return; // input keys are already sorted

    // determine the starting positions for each bucket
    uint32_t prev1 = offset1[0];
    uint32_t prev2 = offset2[0];
    uint32_t prev3 = offset3[1023];
    uint32_t max1 = offset1[0];
    uint32_t max2 = offset2[0];
    uint32_t max3 = offset3[1023];
    offset1[0] = 0;
    offset2[0] = 0;
    for (uint32_t j = 1; j < 2048; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        prev1 += cnt1;
        prev2 += cnt2;
        if (cnt1 > max1) max1 = cnt1;
        if (cnt2 > max2) max2 = cnt2;
    }
    for (uint32_t j = 1022; j > 511; -- j) {
        const uint32_t cnt3 = offset3[j];
        prev3 += cnt3;
        offset3[j] = prev3;
        if (cnt3 > max3) max3 = cnt3;
    }
    for (uint32_t j = 0; j < 512; ++ j) {
        const uint32_t cnt3 = offset3[j];
        offset3[j] = prev3;
        prev3 += cnt3;
        if (cnt3 > max3) max3 = cnt3;
    }
    if (max1 == nelm && max2 == nelm && max3 == nelm)
        return; // all values are the same

    array_t<float> ktmp(nelm);
    array_t<T> vtmp(nelm);
    // distribution 1
    if (max1 < nelm) { // need actual copying
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset1[ikeys[j] & 2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else { // swap the arrays to simplify the next pass
        ktmp.swap(keys);
        vtmp.swap(vals);
    }

    // distribution 2
    if (max2 < nelm) {
        ikeys = reinterpret_cast<const uint32_t *>(ktmp.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset2[(ikeys[j]>>11) & 2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 3
    if (max3 < nelm) {
        ikeys = reinterpret_cast<const uint32_t*>(keys.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t key = (ikeys[j]>>22);
            uint32_t &pos = offset3[key];
            if (key < 512) { // positive value
                ktmp[pos] = keys[j];
                vtmp[pos] = vals[j];
                ++ pos;
            }
            else {
                -- pos;
                ktmp[pos] = keys[j];
                vtmp[pos] = vals[j];
            }
        }
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    ikeys = reinterpret_cast<const uint32_t*>(keys.begin());
    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << "("
                 << std::hex << ikeys[j] << std::dec << ")"
                 << ", vals[" << j << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

template <typename T>
void ibis::util::sort_radix(array_t<double> &keys, array_t<T> &vals) {
    const uint32_t nelm = (keys.size() <= vals.size() ?
                           keys.size() : vals.size());
    if (nelm <= 1) return;

    array_t<uint32_t> offset1(2048, 0); // 11-bit
    array_t<uint32_t> offset2(2048, 0); // 11-bit
    array_t<uint32_t> offset3(2048, 0); // 11-bit
    array_t<uint32_t> offset4(2048, 0); // 11-bit
    array_t<uint32_t> offset5(1024, 0); // 10-bit
    array_t<uint32_t> offset6(1024, 0); // 10-bit
    bool sorted = true;
    const uint64_t *ikeys = reinterpret_cast<const uint64_t*>(keys.begin());
    // count the number of values in each bucket
    offset1[ikeys[0]&2047] = 1;
    offset2[(ikeys[0]>>11)&2047] = 1;
    offset3[(ikeys[0]>>22)&2047] = 1;
    offset4[(ikeys[0]>>33)&2047] = 1;
    offset5[(ikeys[0]>>44)&1023] = 1;
    offset6[(ikeys[0]>>54)] = 1;
    for (uint32_t j = 1; j < nelm; ++ j) {
        ++ offset1[ikeys[j]&2047];
        ++ offset2[(ikeys[j]>>11)&2047];
        ++ offset3[(ikeys[j]>>22)&2047];
        ++ offset4[(ikeys[j]>>33)&2047];
        ++ offset5[(ikeys[j]>>44)&1023];
        ++ offset6[(ikeys[j]>>54)];
        sorted = sorted && (keys[j] >= keys[j-1]);
    }
    if (sorted) return;

    // determine the starting positions for each bucket
    uint32_t max1 = offset1[0];
    uint32_t max2 = offset2[0];
    uint32_t max3 = offset3[0];
    uint32_t max4 = offset4[0];
    uint32_t max5 = offset5[0];
    uint32_t max6 = offset6[1023];
    uint32_t prev1 = offset1[0];
    uint32_t prev2 = offset2[0];
    uint32_t prev3 = offset3[0];
    uint32_t prev4 = offset4[0];
    uint32_t prev5 = offset5[0];
    uint32_t prev6 = offset6[1023];
    offset1[0] = 0;
    offset2[0] = 0;
    offset3[0] = 0;
    offset4[0] = 0;
    offset5[0] = 0;
    for (uint32_t j = 1; j < 1024; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        const uint32_t cnt4 = offset4[j];
        const uint32_t cnt5 = offset5[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        offset4[j] = prev4;
        offset5[j] = prev5;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        prev4 += cnt4;
        prev5 += cnt5;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
        if (max4 < cnt4) max4 = cnt4;
        if (max5 < cnt5) max5 = cnt5;
    }
    for (uint32_t j = 1024; j < 2048; ++ j) {
        const uint32_t cnt1 = offset1[j];
        const uint32_t cnt2 = offset2[j];
        const uint32_t cnt3 = offset3[j];
        const uint32_t cnt4 = offset4[j];
        offset1[j] = prev1;
        offset2[j] = prev2;
        offset3[j] = prev3;
        offset4[j] = prev4;
        prev1 += cnt1;
        prev2 += cnt2;
        prev3 += cnt3;
        prev4 += cnt4;
        if (max1 < cnt1) max1 = cnt1;
        if (max2 < cnt2) max2 = cnt2;
        if (max3 < cnt3) max3 = cnt3;
        if (max4 < cnt4) max4 = cnt4;
    }
    for (uint32_t j = 1022; j > 511; --j) {
        const uint32_t cnt6 = offset6[j];
        prev6 += cnt6;
        offset6[j] = prev6;
        if (max6 < cnt6) max6 = cnt6;
    }
    for (uint32_t j = 0; j < 512; ++j) {
        const uint32_t cnt6 = offset6[j];
        offset6[j] = prev6;
        prev6 += cnt6;
        if (max6 < cnt6) max6 = cnt6;
    }
    if (max1 == nelm && max2 == nelm && max3 == nelm && max4 == nelm &&
        max5 == nelm && max6 == nelm) return;

    array_t<double> ktmp(nelm);
    array_t<T> vtmp(nelm);
    // distribution 1
    if (max1 < nelm) {
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset1[ikeys[j] & 2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 2
    if (max2 < nelm) {
        ikeys = reinterpret_cast<const uint64_t*>(ktmp.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset2[(ikeys[j]>>11) & 2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 3
    if (max3 < nelm) {
        ikeys = reinterpret_cast<const uint64_t*>(keys.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset3[(ikeys[j]>>22) & 2047];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 4
    if (max4 < nelm) {
        ikeys = reinterpret_cast<const uint64_t*>(ktmp.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset4[(ikeys[j]>>33) & 2047];
            keys[pos] = ktmp[j];
            vals[pos] = vtmp[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 5
    if (max5 < nelm) {
        ikeys = reinterpret_cast<const uint64_t*>(keys.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t &pos = offset5[(ikeys[j]>>44) & 1023];
            ktmp[pos] = keys[j];
            vtmp[pos] = vals[j];
            ++ pos;
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }

    // distribution 6
    if (max6 < nelm) {
        ikeys = reinterpret_cast<const uint64_t*>(ktmp.begin());
        for (uint32_t j = 0; j < nelm; ++ j) {
            uint32_t key = (ikeys[j]>>54);
            uint32_t &pos = offset6[key];
            if (key < 512) { // positive numbers
                keys[pos] = ktmp[j];
                vals[pos] = vtmp[j];
                ++ pos;
            }
            else { // negative numbers
                -- pos;
                keys[pos] = ktmp[j];
                vals[pos] = vtmp[j];
            }
        }
    }
    else {
        keys.swap(ktmp);
        vals.swap(vtmp);
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    sorted = true;
    for (uint32_t j = 1; j < nelm; ++ j) {
        if (keys[j-1] > keys[j]) {
            sorted = false;
            break;
        }
    }

    uint32_t iprt = ((nelm >> ibis::gVerbose) > 0 ?
                     (1 << ibis::gVerbose) : nelm);
    ibis::util::logger lg(4);
    if (sorted != true) {
        lg() << "Warning -- ";
    }
    lg() << "util::sort_radix(keys[" << keys.size()
         << "], vals[" << vals.size() << "]) completed ";
    if (sorted) {
        lg() << "successfully";
    }
    else {
        lg() << "with errors";
        for (unsigned j = 0; j < iprt; ++ j)
            lg() << "\nkeys[" << j << "]=" << keys[j] << ", vals[" << j
                 << "]=" << vals[j];
        if (iprt < nelm)
            lg() << "\n... " << nelm-iprt << " ommitted\n";
    }
#endif
} // ibis::util::sort_radix

int64_t
ibis::util::sortMerge(std::vector<std::string> &valR, array_t<uint32_t> &indR,
                      std::vector<std::string> &valS, array_t<uint32_t> &indS) {
    if (valR.empty() || valS.empty()) return 0;

    try {
        indR.nosharing();
        if (valR.size() != indR.size()) {
            indR.resize(valR.size());
            for (uint32_t j = 0; j < valR.size(); ++ j)
                indR[j] = j;
        }
        ibis::util::sortStrings(valR, indR);

        indS.nosharing();
        if (valS.size() != indS.size()) {
            indS.resize(valS.size());
            for (uint32_t j = 0; j < valS.size(); ++ j)
                indS[j] = j;
        }
        ibis::util::sortStrings(valS, indS);
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- util::sortMerge(std::string["
            << valR.size() << "], std::string[" << valS.size()
            << "]) failed to sort the two values or to create index arrays";
        return -1;
    }

    int64_t cnt = 0;
    uint32_t ir = 0;
    uint32_t is = 0;
    const uint32_t nr = valR.size();
    const uint32_t ns = valS.size();
    while (ir < nr && is < ns) {
        int cmp = valR[ir].compare(valS[is]);
        if (cmp == 0) {
            const uint32_t ir0 = ir;
            const uint32_t is0 = is;
            for (++ ir; ir < nr && valR[ir].compare(valR[ir0]) == 0; ++ ir);
            for (++ is; is < ns && valS[is].compare(valS[is0]) == 0; ++ is);
            cnt += (ir-ir0) * (is-is0);
        }
        else if (cmp < 0) {
            ++ ir;
        }
        else {
            ++ is;
        }
    }
    return cnt;
} // ibis::util::sortMerge

/// @note This implementation is for elementary numberical data types only.
///
/// @note On input, if the size of indR is the same as that of valR, its
/// content is preserved, otherwise it is reset to 0..valR.size()-1.  The
/// array indS is similarly set to 0..valS.size()-1 if its size is
/// different from that of valS.
template <typename T> int64_t
ibis::util::sortMerge(array_t<T> &valR, array_t<uint32_t> &indR,
                      array_t<T> &valS, array_t<uint32_t> &indS) {
    if (valR.empty() || valS.empty()) return 0;

    try {
        valR.nosharing();
        indR.nosharing();
        if (valR.size() != indR.size()) {
            indR.resize(valR.size());
            for (uint32_t j = 0; j < valR.size(); ++ j)
                indR[j] = j;
        }
        ibis::util::sortKeys(valR, indR);

        valS.nosharing();
        indS.nosharing();
        if (valS.size() != indS.size()) {
            indS.resize(valS.size());
            for (uint32_t j = 0; j < valS.size(); ++ j)
                indS[j] = j;
        }
        ibis::util::sortKeys(valS, indS);
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- util::sortMerge(" << typeid(T).name() << "["
            << valR.size() << "], " << typeid(T).name() << "[" << valS.size()
            << "]) failed to sort the values or to create index arrays";
        return -1;
    }

    int64_t cnt = 0;
    uint32_t ir = 0;
    uint32_t is = 0;
    const uint32_t nr = valR.size();
    const uint32_t ns = valS.size();
    while (ir < nr && is < ns) {
        if (valR[ir] == valS[is]) {
            const uint32_t ir0 = ir;
            const uint32_t is0 = is;
            for (++ ir; ir < nr && valR[ir] == valR[ir0]; ++ ir);
            for (++ is; is < ns && valS[is] == valS[is0]; ++ is);
            cnt += (ir-ir0) * (is-is0);
        }
        else if (valR[ir] < valS[is]) {
            do {++ ir;} while (ir < nr && valR[ir] < valS[is]);
        }
        else {
            do {++ is;} while (is < ns && valS[is] < valR[ir]);
        }
    }
    return cnt;
} // ibis::util::sortMerge

/// @note This implementation is for elementary numberical data types only.
///
/// @note On input, if the size of indR is the same as that of valR, its
/// content is preserved, otherwise it is reset to 0..valR.size()-1.  The
/// array indS is similarly set to 0..valS.size()-1 if its size is
/// different from that of valS.
template <typename T> int64_t
ibis::util::sortMerge(array_t<T> &valR, array_t<uint32_t> &indR,
                      array_t<T> &valS, array_t<uint32_t> &indS,
                      double delta1, double delta2) {
    if (valR.empty() || valS.empty()) return 0;

    try {
        valR.nosharing();
        indR.nosharing();
        if (valR.size() != indR.size()) {
            indR.resize(valR.size());
            for (uint32_t j = 0; j < valR.size(); ++ j)
                indR[j] = j;
        }
        ibis::util::sortKeys(valR, indR);

        valS.nosharing();
        indS.nosharing();
        if (valS.size() != indS.size()) {
            indS.resize(valS.size());
            for (uint32_t j = 0; j < valS.size(); ++ j)
                indS[j] = j;
        }
        ibis::util::sortKeys(valS, indS);
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- util::sortMerge(" << typeid(T).name() << "["
            << valR.size() << "], " << typeid(T).name() << "[" << valS.size()
            << "]) failed to sort the values or to create index arrays";
        return -1;
    }

    int64_t cnt = 0;
    uint32_t ir0 = 0, ir1 = 0;
    uint32_t is = 0;
    const uint32_t nr = valR.size();
    const uint32_t ns = valS.size();
    while (ir0 < nr && is < ns) {
        while (ir0 < nr && valR[ir0] < valS[is]+delta1)
            ++ ir0;
        for (ir1=(ir1>=ir0?ir1:ir0);
             ir1 < nr && valR[ir1] <= valS[is]+delta2;
             ++ ir1);
        if (ir1 > ir0) {
            const uint32_t is0 = is;
            for (++ is; is < ns && valS[is] == valS[is0]; ++ is);
            cnt += (ir1-ir0) * (is-is0);
        }
        else {
            ++ is;
        }
    }
    return cnt;
} // ibis::util::sortMerge

/// Find the position of the first element that is no less than @c val.
/// The search starts with the given position @c i0.
/// Assuming @c ind was produced by the sort function,
/// it returns the smallest i such that @c operator[](ind[i]) >= @c val.
///
/// @note Because the explicit use of uint32_t to denote the positions,
/// that array can not have more than 2^32 elements.
template<class T>
uint32_t ibis::util::find(const array_t<T> &arr, const array_t<uint32_t> &ind,
                          const T &val, uint32_t i0) {
    const uint32_t ntot = arr.size();
    if (ntot == 0) return 0; // empty array
    else if (! (arr[ind[0]] < val))
        return 0; // 1st value is larger than val
    if (i0 >= ntot)
        i0 = ntot - 1;
    uint32_t i1, i2;
    double d0;
    if (arr[ind[i0]] < val) { // look for [i1] >= val
        i2 = 1;
        i1 = i0 + 1;
        while (i1 < ntot && arr[ind[i1]] < val) {
            if (arr[ind[i1]] > arr[ind[i0]]) {
                d0 = ceil(i2 * static_cast<double>(val - arr[ind[i1]]) /
                          (double)(arr[ind[i1]] - arr[ind[i0]]));
                i0 = i1;
                if (! (d0 < ntot - i1)) {
                    i2 = ntot - i1 - 1;
                    i1 = ntot - 1;
                    if (i2 == 0) {
                        i1 = ntot;
                        i2 = 1;
                    }
                }
                else if (d0 > 1.0) {
                    i1 += static_cast<size_t>(d0);
                    i2 = static_cast<size_t>(d0);
                }
                else {
                    i2 = 1;
                    ++ i1;
                }
            }
            else {
                i0 = i1;
                i2 += i2;
                i1 += i2;
            }
        }
        if (i1 >= ntot) {// all values less than val
            LOGGER(ibis::gVerbose > 0 && !(arr[ind[ntot-1]] < val))
                << "Warning -- util::find<" << typeid(T).name()
                << "> is to return " << ntot << ", but [" << ntot-1 << "] ("
                << arr[ind[ntot-1]] << ") is not less than " << val;
            return ntot;
        }
    }
    else { // look for [i0] < val
        i1 = i0;
        i0 = i1 - 1;
        i2 = 1;
        while (i0 > 0 && arr[ind[i0]] >= val) {
            if (arr[ind[i0]] < arr[ind[i1]]) {
                d0 = ceil(i2 * static_cast<double>(arr[ind[i0]] - val) /
                          (double)(arr[ind[i1]] - arr[ind[01]]));
                i1 = i0;
                if (! (d0 < i0)) {
                    i0 = 0;
                    i2 = i0;
                }
                else if (d0 > 1.0) {
                    i0 -= static_cast<size_t>(d0);
                    i2 = static_cast<size_t>(d0);
                }
                else {
                    i2 = 1;
                    -- i0;
                }
            }
            else {
                i1 = i0;
                i2 += i2;
                if (i2 < i0)
                    i0 -= i2;
                else
                    i0 = 0;
            }
        }
        // checked *arr at the beginning already, no need to check again
    }
    // invariant at this point: [i0] < val <= [i1]
    LOGGER(ibis::gVerbose > 7)
        << "util::find -- arr[ind[" << i0 << "]] (" << arr[ind[i0]] << ") < "
        << val << " <= arr[ind[" << i1 << "] (" << arr[ind[i1]] << ')';

    // attempt 1 to narrow the gap between i0 and i1: large gap, use
    // computed address
    while (i0+FASTBIT_QSORT_MIN < i1 && arr[ind[i1]] > val) {
        i2 = i0 + (i1 - i0) * (val - arr[ind[i0]]) /
            (arr[ind[i1]] - arr[ind[i0]]);
        if (i2 == i0)
            i2 = (i1 + i0) / 2;
        if (arr[ind[i2]] < val)
            i0 = i2;
        else
            i1 = i2;
    }
    if (arr[i1] == val) {
        for (i2 = 1; i0+i2 < i1; i2 += i2) {
            if (arr[i1-i2] < val) {
                i0 = i1 - i2;
                break;
            }
            else {
                i1 = i1 - i2;
            }
        }
    }
    // attempt 2 to narrow the gap between i0 and i1: basic binary search
    i2 = (i0 + i1) / 2;
    while (i0 < i2) {
        if (arr[ind[i2]] < val)
            i0 = i2;
        else
            i1 = i2;

        i2 = (i0 + i1) / 2;
    }

    LOGGER(ibis::gVerbose > 0 && !(arr[ind[i1]] >= val))
        << "Warning -- util::find<" << typeid(T).name() << "> is to return "
        << i1 << ", but [" << i1 << "] (" << arr[ind[i1]] << ") is less than "
        << val;

    return i1;
} // ibis::util::find

/// Find the first position where the value is no less than @c val.
/// Start the searching operation with position start.
/// Assuming the array is already sorted in the ascending order,
/// it returns the smallest i such that @c operator[](i) >= @c val.
template<class T> size_t
ibis::util::find(const array_t<T> &arr, const T &val, size_t i0) {
    const size_t ntot = arr.size();
    if (ntot == 0) return 0; // empty array
    else if (! (arr[0] < val)) return 0; // 1st value is larger than val
    if (i0 >= ntot)
        i0 = ntot - 1;
    size_t i1, i2;
    double d0;
    if (arr[i0] < val) { // look for [i1] >= val
        i2 = 1;
        i1 = i0 + 1;
        while (i1 < ntot && arr[i1] < val) {
            if (arr[i1] > arr[i0]) {
                d0 = ceil(i2 * static_cast<double>(val - arr[i1]) /
                          static_cast<double>(arr[i1] - arr[i0]));
                i0 = i1;
                if (! (d0 < ntot - i1)) {
                    i2 = ntot - i1 - 1;
                    i1 = ntot - 1;
                    if (i2 == 0) {
                        i1 = ntot;
                        i2 = 1;
                    }
                }
                else if (d0 > 1.0) {
                    i1 += static_cast<size_t>(d0);
                    i2 = static_cast<size_t>(d0);
                }
                else {
                    i2 = 1;
                    ++ i1;
                }
            }
            else {
                i0 = i1;
                i2 += i2;
                i1 += i2;
            }
        }
        if (i1 >= ntot) {// all values less than val
            LOGGER(ibis::gVerbose > 0 && !(arr[ntot-1] < val))
                << "Warning -- util::find<" << typeid(T).name()
                << "> is to return " << ntot << ", but [" << ntot-1
                << "] (" << arr[ntot-1] << ") is not less than " << val;
            return ntot;
        }
    }
    else { // look for [i0] < val
        i1 = i0;
        i0 = i1 - 1;
        i2 = 1;
        while (i0 > 0 && arr[i0] >= val) {
            if (arr[i0] < arr[i1]) {
                d0 = ceil(i2 * static_cast<double>(arr[i0] - val) /
                          static_cast<double>(arr[i1] - arr[01]));
                i1 = i0;
                if (! (d0 < i0)) {
                    i0 = 0;
                    i2 = i0;
                }
                else if (d0 > 1.0) {
                    i0 -= static_cast<size_t>(d0);
                    i2 = static_cast<size_t>(d0);
                }
                else {
                    i2 = 1;
                    -- i0;
                }
            }
            else {
                i1 = i0;
                i2 += i2;
                if (i2 < i0)
                    i0 -= i2;
                else
                    i0 = 0;
            }
        }
        // checked *arr at the beginning already, no need to check again
    }
    // invariant at this point: [i0] < val <= [i1]
    LOGGER(ibis::gVerbose > 7)
        << "util::find -- arr[" << i0 << "] (" << arr[i0] << ")< " << val
        << " <= arr[" << i1 << "] (" << arr[i1] << ')';

    // attempt 1 to narrow the gap between i0 and i1: large gap, use
    // computed address
    while (i0+FASTBIT_QSORT_MIN < i1 && arr[i1] > val) {
        i2 = i0 + (i1 - i0) * (val - arr[i0]) / (arr[i1] - arr[i0]);
        if (i2 == i0)
            i2 = (i1 + i0) / 2;
        if (arr[i2] < val)
            i0 = i2;
        else
            i1 = i2;
    }
    if (arr[i1] == val) {
        for (i2 = 1; i0+i2 < i1; i2 += i2) {
            if (arr[i1-i2] < val) {
                i0 = i1 - i2;
                break;
            }
            else {
                i1 = i1 - i2;
            }
        }
    }
    // attempt 2 to narrow the gap between i0 and i1: basic binary search
    i2 = (i0 + i1) / 2;
    while (i0 < i2) {
        if (arr[i2] < val)
            i0 = i2;
        else
            i1 = i2;

        i2 = (i0 + i1) / 2;
    }

    LOGGER(ibis::gVerbose > 0 && !(arr[i1] >= val))
        << "Warning -- util::find<" << typeid(T).name() << "> is to return "
        << i1 << ", but [" << i1 << "] (" << arr[i1] << ") is less than "
        << val;

    return i1;
} // ibis::util::find

/// Find the first position where the value is no less than @c val.
/// Start the searching operation with position start.
/// Assuming the array is already sorted in the ascending order,
/// it returns the smallest i such that @c operator[](i) >= @c val.
template<class T> size_t
ibis::util::find(const std::vector<T> &arr, const T &val, size_t i0) {
    const size_t ntot = arr.size();
    if (ntot == 0) return 0; // empty array
    else if (! (arr[0] < val)) return 0; // 1st value is larger than val
    if (i0 >= ntot)
        i0 = ntot - 1;
    size_t i1, i2;
    double d0;
    if (arr[i0] < val) { // look for [i1] >= val
        i2 = 1;
        i1 = i0 + 1;
        while (i1 < ntot && arr[i1] < val) {
            if (arr[i1] > arr[i0]) {
                d0 = ceil(i2 * static_cast<double>(val - arr[i1]) /
                          static_cast<double>(arr[i1] - arr[i0]));
                i0 = i1;
                if (! (d0 < ntot - i1)) {
                    i2 = ntot - i1 - 1;
                    i1 = ntot - 1;
                    if (i2 == 0) {
                        i1 = ntot;
                        i2 = 1;
                    }
                }
                else if (d0 > 1.0) {
                    i1 += static_cast<size_t>(d0);
                    i2 = static_cast<size_t>(d0);
                }
                else {
                    i2 = 1;
                    ++ i1;
                }
            }
            else {
                i0 = i1;
                i2 += i2;
                i1 += i2;
            }
        }
        if (i1 >= ntot) {// all values less than val
            LOGGER(ibis::gVerbose > 0 && !(arr[ntot-1] < val))
                << "Warning -- util::find<" << typeid(T).name()
                << "> is to return " << ntot << ", but [" << ntot-1
                << "] (" << arr[ntot-1] << ") is not less than " << val;
            return ntot;
        }
    }
    else { // look for [i0] < val
        i1 = i0;
        i0 = i1 - 1;
        i2 = 1;
        while (i0 > 0 && arr[i0] >= val) {
            if (arr[i0] < arr[i1]) {
                d0 = ceil(i2 * static_cast<double>(arr[i0] - val) /
                          static_cast<double>(arr[i1] - arr[01]));
                i1 = i0;
                if (! (d0 < i0)) {
                    i0 = 0;
                    i2 = i0;
                }
                else if (d0 > 1.0) {
                    i0 -= static_cast<size_t>(d0);
                    i2 = static_cast<size_t>(d0);
                }
                else {
                    i2 = 1;
                    -- i0;
                }
            }
            else {
                i1 = i0;
                i2 += i2;
                if (i2 < i0)
                    i0 -= i2;
                else
                    i0 = 0;
            }
        }
        // checked *arr at the beginning already, no need to check again
    }
    // invariant at this point: [i0] < val <= [i1]
    LOGGER(ibis::gVerbose > 7)
        << "util::find -- arr[" << i0 << "] (" << arr[i0] << ")< " << val
        << " <= arr[" << i1 << "] (" << arr[i1] << ')';

    // attempt 1 to narrow the gap between i0 and i1: large gap, use
    // computed address
    while (i0+FASTBIT_QSORT_MIN < i1 && arr[i1] > val) {
        i2 = i0 + (i1 - i0) * (val - arr[i0]) / (arr[i1] - arr[i0]);
        if (i2 == i0)
            i2 = (i1 + i0) / 2;
        if (arr[i2] < val)
            i0 = i2;
        else
            i1 = i2;
    }
    if (arr[i1] == val) {
        for (i2 = 1; i0+i2 < i1; i2 += i2) {
            if (arr[i1-i2] < val) {
                i0 = i1 - i2;
                break;
            }
            else {
                i1 = i1 - i2;
            }
        }
    }
    // attempt 2 to narrow the gap between i0 and i1: basic binary search
    i2 = (i0 + i1) / 2;
    while (i0 < i2) {
        if (arr[i2] < val)
            i0 = i2;
        else
            i1 = i2;

        i2 = (i0 + i1) / 2;
    }

    LOGGER(ibis::gVerbose > 0 && !(arr[i1] >= val))
        << "Warning -- util::find<" << typeid(T).name() << "> is to return "
        << i1 << ", but [" << i1 << "] (" << arr[i1] << ") is less than "
        << val;

    return i1;
} // ibis::util::find

// explicit template instantiations
template int64_t
ibis::util::sortMerge<signed char>(array_t<signed char>&, array_t<uint32_t>&,
                                   array_t<signed char>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<unsigned char>
(array_t<unsigned char>&, array_t<uint32_t>&,
 array_t<unsigned char>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<int16_t>(array_t<int16_t>&, array_t<uint32_t>&,
                               array_t<int16_t>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<uint16_t>(array_t<uint16_t>&, array_t<uint32_t>&,
                                array_t<uint16_t>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<int32_t>(array_t<int32_t>&, array_t<uint32_t>&,
                               array_t<int32_t>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<uint32_t>(array_t<uint32_t>&, array_t<uint32_t>&,
                                array_t<uint32_t>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<int64_t>(array_t<int64_t>&, array_t<uint32_t>&,
                               array_t<int64_t>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<uint64_t>(array_t<uint64_t>&, array_t<uint32_t>&,
                                array_t<uint64_t>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<float>(array_t<float>&, array_t<uint32_t>&,
                             array_t<float>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<double>(array_t<double>&, array_t<uint32_t>&,
                              array_t<double>&, array_t<uint32_t>&);
template int64_t
ibis::util::sortMerge<signed char>(array_t<signed char>&, array_t<uint32_t>&,
                                   array_t<signed char>&, array_t<uint32_t>&,
                                   double, double);
template int64_t
ibis::util::sortMerge<unsigned char>
(array_t<unsigned char>&, array_t<uint32_t>&,
 array_t<unsigned char>&, array_t<uint32_t>&,
 double, double);
template int64_t
ibis::util::sortMerge<int16_t>(array_t<int16_t>&, array_t<uint32_t>&,
                               array_t<int16_t>&, array_t<uint32_t>&,
                               double, double);
template int64_t
ibis::util::sortMerge<uint16_t>(array_t<uint16_t>&, array_t<uint32_t>&,
                                array_t<uint16_t>&, array_t<uint32_t>&,
                                double, double);
template int64_t
ibis::util::sortMerge<int32_t>(array_t<int32_t>&, array_t<uint32_t>&,
                               array_t<int32_t>&, array_t<uint32_t>&,
                               double, double);
template int64_t
ibis::util::sortMerge<uint32_t>(array_t<uint32_t>&, array_t<uint32_t>&,
                                array_t<uint32_t>&, array_t<uint32_t>&,
                                double, double);
template int64_t
ibis::util::sortMerge<int64_t>(array_t<int64_t>&, array_t<uint32_t>&,
                               array_t<int64_t>&, array_t<uint32_t>&,
                               double, double);
template int64_t
ibis::util::sortMerge<uint64_t>(array_t<uint64_t>&, array_t<uint32_t>&,
                                array_t<uint64_t>&, array_t<uint32_t>&,
                                double, double);
template int64_t
ibis::util::sortMerge<float>(array_t<float>&, array_t<uint32_t>&,
                             array_t<float>&, array_t<uint32_t>&,
                             double, double);
template int64_t
ibis::util::sortMerge<double>(array_t<double>&, array_t<uint32_t>&,
                              array_t<double>&, array_t<uint32_t>&,
                              double, double);

template void
ibis::util::reorder<signed char>(array_t<signed char>&,
                                 const array_t<uint32_t>&);
template void
ibis::util::reorder<unsigned char>(array_t<unsigned char>&,
                                   const array_t<uint32_t>&);
template void
ibis::util::reorder<char>(array_t<char>&,
                          const array_t<uint32_t>&);
template void
ibis::util::reorder<int16_t>(array_t<int16_t>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<int32_t>(array_t<int32_t>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<int64_t>(array_t<int64_t>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<uint16_t>(array_t<uint16_t>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<uint32_t>(array_t<uint32_t>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<uint64_t>(array_t<uint64_t>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<float>(array_t<float>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<double>(array_t<double>&, const array_t<uint32_t>&);
template void
ibis::util::reorder<ibis::rid_t>(array_t<ibis::rid_t>&,
                                 const array_t<uint32_t>&);
template void
ibis::util::reorder<ibis::array_t<ibis::rid_t> >
(ibis::array_t<ibis::array_t<ibis::rid_t>*>&,
 const ibis::array_t<uint32_t>&);

template void
ibis::util::sortAll<int32_t, int32_t>(array_t<int32_t>&, array_t<int32_t>&);
template void
ibis::util::sortAll<uint32_t, int32_t>(array_t<uint32_t>&, array_t<int32_t>&);
template void
ibis::util::sortAll<int64_t, int32_t>(array_t<int64_t>&, array_t<int32_t>&);
template void
ibis::util::sortAll<uint64_t, int32_t>(array_t<uint64_t>&, array_t<int32_t>&);
template void
ibis::util::sortAll<float, int32_t>(array_t<float>&, array_t<int32_t>&);
template void
ibis::util::sortAll<double, int32_t>(array_t<double>&, array_t<int32_t>&);
template void
ibis::util::sortAll<int32_t, uint32_t>(array_t<int32_t>&, array_t<uint32_t>&);
template void
ibis::util::sortAll<uint32_t, uint32_t>(array_t<uint32_t>&, array_t<uint32_t>&);
template void
ibis::util::sortAll<int64_t, uint32_t>(array_t<int64_t>&, array_t<uint32_t>&);
template void
ibis::util::sortAll<uint64_t, uint32_t>(array_t<uint64_t>&, array_t<uint32_t>&);
template void
ibis::util::sortAll<float, uint32_t>(array_t<float>&, array_t<uint32_t>&);
template void
ibis::util::sortAll<double, uint32_t>(array_t<double>&, array_t<uint32_t>&);
template void
ibis::util::sortAll<int32_t, int64_t>(array_t<int32_t>&, array_t<int64_t>&);
template void
ibis::util::sortAll<uint32_t, int64_t>(array_t<uint32_t>&, array_t<int64_t>&);
template void
ibis::util::sortAll<int64_t, int64_t>(array_t<int64_t>&, array_t<int64_t>&);
template void
ibis::util::sortAll<uint64_t, int64_t>(array_t<uint64_t>&, array_t<int64_t>&);
template void
ibis::util::sortAll<float, int64_t>(array_t<float>&, array_t<int64_t>&);
template void
ibis::util::sortAll<double, int64_t>(array_t<double>&, array_t<int64_t>&);
template void
ibis::util::sortAll<int32_t, uint64_t>(array_t<int32_t>&, array_t<uint64_t>&);
template void
ibis::util::sortAll<uint32_t, uint64_t>(array_t<uint32_t>&, array_t<uint64_t>&);
template void
ibis::util::sortAll<int64_t, uint64_t>(array_t<int64_t>&, array_t<uint64_t>&);
template void
ibis::util::sortAll<uint64_t, uint64_t>(array_t<uint64_t>&, array_t<uint64_t>&);
template void
ibis::util::sortAll<float, uint64_t>(array_t<float>&, array_t<uint64_t>&);
template void
ibis::util::sortAll<double, uint64_t>(array_t<double>&, array_t<uint64_t>&);
template void
ibis::util::sortAll<int32_t, float>(array_t<int32_t>&, array_t<float>&);
template void
ibis::util::sortAll<uint32_t, float>(array_t<uint32_t>&, array_t<float>&);
template void
ibis::util::sortAll<int64_t, float>(array_t<int64_t>&, array_t<float>&);
template void
ibis::util::sortAll<uint64_t, float>(array_t<uint64_t>&, array_t<float>&);
template void
ibis::util::sortAll<float, float>(array_t<float>&, array_t<float>&);
template void
ibis::util::sortAll<double, float>(array_t<double>&, array_t<float>&);
template void
ibis::util::sortAll<int32_t, double>(array_t<int32_t>&, array_t<double>&);
template void
ibis::util::sortAll<uint32_t, double>(array_t<uint32_t>&, array_t<double>&);
template void
ibis::util::sortAll<int64_t, double>(array_t<int64_t>&, array_t<double>&);
template void
ibis::util::sortAll<uint64_t, double>(array_t<uint64_t>&, array_t<double>&);
template void
ibis::util::sortAll<float, double>(array_t<float>&, array_t<double>&);
template void
ibis::util::sortAll<double, double>(array_t<double>&, array_t<double>&);

template void
ibis::util::sort_quick3(array_t<signed char>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<unsigned char>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<int16_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<uint16_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<int32_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<uint32_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<int64_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<uint64_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<float>&, array_t<uint32_t>&);
template void
ibis::util::sort_quick3(array_t<double>&, array_t<uint32_t>&);

template void
ibis::util::sort_shell(array_t<signed char>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<unsigned char>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<int16_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<uint16_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<int32_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<uint32_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<int64_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<uint64_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<float>&, array_t<uint32_t>&);
template void
ibis::util::sort_shell(array_t<double>&, array_t<uint32_t>&);

template void
ibis::util::sort_insertion(array_t<signed char>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<unsigned char>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<int16_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<uint16_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<int32_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<uint32_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<int64_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<uint64_t>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<float>&, array_t<uint32_t>&);
template void
ibis::util::sort_insertion(array_t<double>&, array_t<uint32_t>&);

template void
ibis::util::sortKeys(array_t<char>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<signed char>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<unsigned char>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<int16_t>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<uint16_t>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<int32_t>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<uint32_t>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<int64_t>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<uint64_t>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<float>&, array_t<uint32_t>&);
template void
ibis::util::sortKeys(array_t<double>&, array_t<uint32_t>&);

template void
ibis::util::sortKeys(array_t<signed char>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<unsigned char>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<int16_t>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<uint16_t>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<int32_t>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<uint32_t>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<int64_t>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<uint64_t>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<float>&, array_t<ibis::rid_t>&);
template void
ibis::util::sortKeys(array_t<double>&, array_t<ibis::rid_t>&);

template size_t
ibis::util::find(const std::vector<signed char>&, const signed char&, size_t);
template size_t
ibis::util::find(const std::vector<unsigned char>&, const unsigned char&, size_t);
template size_t
ibis::util::find(const std::vector<int16_t>&, const int16_t&, size_t);
template size_t
ibis::util::find(const std::vector<uint16_t>&, const uint16_t&, size_t);
template size_t
ibis::util::find(const std::vector<int32_t>&, const int32_t&, size_t);
template size_t
ibis::util::find(const std::vector<uint32_t>&, const uint32_t&, size_t);
template size_t
ibis::util::find(const std::vector<int64_t>&, const int64_t&, size_t);
template size_t
ibis::util::find(const std::vector<uint64_t>&, const uint64_t&, size_t);
template size_t
ibis::util::find(const std::vector<float>&, const float&, size_t);
template size_t
ibis::util::find(const std::vector<double>&, const double&, size_t);

template size_t
ibis::util::find(const array_t<signed char>&, const signed char&, size_t);
template size_t
ibis::util::find(const array_t<unsigned char>&, const unsigned char&, size_t);
template size_t
ibis::util::find(const array_t<int16_t>&, const int16_t&, size_t);
template size_t
ibis::util::find(const array_t<uint16_t>&, const uint16_t&, size_t);
template size_t
ibis::util::find(const array_t<int32_t>&, const int32_t&, size_t);
template size_t
ibis::util::find(const array_t<uint32_t>&, const uint32_t&, size_t);
template size_t
ibis::util::find(const array_t<int64_t>&, const int64_t&, size_t);
template size_t
ibis::util::find(const array_t<uint64_t>&, const uint64_t&, size_t);
template size_t
ibis::util::find(const array_t<float>&, const float&, size_t);
template size_t
ibis::util::find(const array_t<double>&, const double&, size_t);

template uint32_t
ibis::util::find(const array_t<signed char>&, const array_t<uint32_t>&,
                 const signed char&, uint32_t);
template uint32_t
ibis::util::find(const array_t<unsigned char>&, const array_t<uint32_t>&,
                 const unsigned char&, uint32_t);
template uint32_t
ibis::util::find(const array_t<int16_t>&, const array_t<uint32_t>&,
                 const int16_t&, uint32_t);
template uint32_t
ibis::util::find(const array_t<uint16_t>&, const array_t<uint32_t>&,
                 const uint16_t&, uint32_t);
template uint32_t
ibis::util::find(const array_t<int32_t>&, const array_t<uint32_t>&,
                 const int32_t&, uint32_t);
template uint32_t
ibis::util::find(const array_t<uint32_t>&, const array_t<uint32_t>&,
                 const uint32_t&, uint32_t);
template uint32_t
ibis::util::find(const array_t<int64_t>&, const array_t<uint32_t>&,
                 const int64_t&, uint32_t);
template uint32_t
ibis::util::find(const array_t<uint64_t>&, const array_t<uint32_t>&,
                 const uint64_t&, uint32_t);
template uint32_t
ibis::util::find(const array_t<float>&, const array_t<uint32_t>&,
                 const float&, uint32_t);
template uint32_t
ibis::util::find(const array_t<double>&, const array_t<uint32_t>&,
                 const double&, uint32_t);
