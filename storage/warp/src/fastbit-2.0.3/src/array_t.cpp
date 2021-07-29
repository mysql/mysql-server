// File: $Id$
// Author: K. John Wu <John.Wu at acm.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 Univeristy of California
//
// the implementation file of the ibis::array_t<T> class
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
// #elif defined(__GNUC__)
// #pragma implementation
#endif
#include "array_t.h"
#include "table.h"      // ibis::TYPE_T

#include <memory>       // std::unique_ptr
#include <algorithm>    // std::sort
#include <iomanip>      // std::setw
#include <typeinfo>     // typeid

// When the number of elements in an array to be sorted by qsort is less
// than QSORT_MIN, the insert sort routine will be used.
#ifndef QSORT_MIN
#define QSORT_MIN 64
#endif
// When the number of recursion levels is more than QSORT_MAX_DEPTH, switch
// to use the heap sort routine.
#ifndef QSORT_MAX_DEPTH
#define QSORT_MAX_DEPTH 20
#endif

/// The default constructor.  It constructs an empty array.
template<class T>
ibis::array_t<T>::array_t()
    : actual(new ibis::fileManager::storage), m_begin(0), m_end(0) {
    if (actual != 0) {
        m_begin = reinterpret_cast<T*>(actual->begin());
        m_end   = m_begin;
        actual->beginUse();
        LOGGER(ibis::gVerbose > 9)
            << "array_t<" << typeid(T).name() << "> constructed at "
            << static_cast<void*>(this) << " with actual="
            << static_cast<void*>(actual) << ", m_begin="
            << static_cast<void*>(m_begin) << " and m_end="
            << static_cast<void*>(m_end);
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- array_t<" << typeid(T).name()
            << "> failed to allocate an empty array";
        throw ibis::bad_alloc("array_t<T>::ctor failed" IBIS_FILE_LINE);
    }
}

/// Constructor.  Construct an array with n elements.
template<class T>
ibis::array_t<T>::array_t(size_t n)
    : actual(new ibis::fileManager::storage(n*sizeof(T))),
      m_begin(0), m_end(0) {
    if (actual != 0) {
        m_begin = reinterpret_cast<T*>(actual->begin());
        m_end   = m_begin + n;
        actual->beginUse();
        LOGGER(ibis::gVerbose > 9)
            << "array_t<" << typeid(T).name() << "> constructed at "
            << static_cast<void*>(this) << " with " << n << " element"
            << (n>1?"s":"") << ", actual="
            << static_cast<void*>(actual) << ", m_begin="
            << static_cast<void*>(m_begin) << " and m_end="
            << static_cast<void*>(m_end);
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- array_t<" << typeid(T).name()
            << "> failed to allocate an array with "
            << n << " element" << (n > 1 ? "s" : "");
        throw ibis::bad_alloc("array_t<T>::ctor failed" IBIS_FILE_LINE);
    }
}

/// Constructor.  Construct an array with @c n elements of value @c val.
template<class T>
ibis::array_t<T>::array_t(size_t n, const T& val)
    : actual(new ibis::fileManager::storage(n*sizeof(T))),
      m_begin(0), m_end(0){
    if (actual != 0) {
        m_begin = reinterpret_cast<T*>(actual->begin());
        m_end   = m_begin + n;
        actual->beginUse();
        for (size_t i = 0; i < n; ++ i) {
            m_begin[i] = val;
        }
        LOGGER(ibis::gVerbose > 9)
            << "array_t<" << typeid(T).name() << "> constructed at "
            << static_cast<void*>(this) << " with " << n << " element"
            << (n>1?"s":"") << " of " << val << ", actual="
            << static_cast<void*>(actual) << ", m_begin="
            << static_cast<void*>(m_begin) << " and m_end="
            << static_cast<void*>(m_end);
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- array_t<" << typeid(T).name()
            << "> failed to allocate memory for copying "
            << n << " element" << (n > 1 ? "s" : "");
        throw ibis::bad_alloc("array_t<T>::ctor failed" IBIS_FILE_LINE);
    }
}

/// Constructor.  Copy the values from a vector to array_t, i.e., deep
/// copy.
template<class T>
ibis::array_t<T>::array_t(const std::vector<T>& rhs)
    : actual(new ibis::fileManager::storage(rhs.size()*sizeof(T))),
      m_begin(0), m_end(0) {
    if (actual != 0) {
        actual->beginUse();
        m_begin = reinterpret_cast<T*>(actual->begin());
        m_end   = m_begin + rhs.size();
        std::copy(rhs.begin(), rhs.end(), m_begin);
    }
    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << ", m_begin="
        << static_cast<void*>(m_begin) << " and m_end="
        << static_cast<void*>(m_end) << ", copied from std::vector @ "
        << static_cast<const void*>(&rhs);
}

/// Copy constructor.  Makes a shallow copy.  Should not throw any
/// exception.
template<class T>
ibis::array_t<T>::array_t(const array_t<T>& rhs)
    : actual(rhs.actual), m_begin(rhs.m_begin), m_end(rhs.m_end) {
    if (actual != 0)
        actual->beginUse();

    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << ", m_begin="
        << static_cast<void*>(m_begin) << " and m_end="
        << static_cast<void*>(m_end) << ", copied from array_t @ "
        << static_cast<const void*>(&rhs);
}

/// A shallow copy constructor.  It makes a new array out of a section of
/// the existing array.  The values of begin and end are indices to the
/// array rhs.
///
/// @note If end is less than begin, the array section is assumed to
/// extend to the end of rhs.
template<class T>
ibis::array_t<T>::array_t(const array_t<T>& rhs, const size_t begin,
                          const size_t end)
    : actual(rhs.actual), m_begin(rhs.m_begin+begin), m_end(rhs.m_begin+end) {
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(end < begin && end != 0 && ibis::gVerbose > 0)
        << "Warning -- array_t<" << typeid(T).name()
        << "> called with a suspicious looking section [" << begin
        << ", " << end << ')';
#endif
    if (m_begin > rhs.m_end)
        m_begin = rhs.m_end;
    if (m_end > rhs.m_end || m_end < m_begin)
        m_end = rhs.m_end;
    if (actual != 0)
        actual->beginUse();

    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << " m_begin="
        << static_cast<void*>(m_begin) << " and m_end="
        << static_cast<void*>(m_end) << " using section [" << begin << ", "
        << end << ") from " << static_cast<const void*>(&rhs);
}

/// Turn a raw storage object into an array_t object.  The input storage
/// object is used by the array.  No new storage is allocated, i.e.,
/// shallow copy.
template<class T>
ibis::array_t<T>::array_t(ibis::fileManager::storage* rhs)
    : actual(rhs), m_begin((T*)(rhs->begin())), m_end((T*)(rhs->end())) {
    if (actual != 0)
        actual->beginUse();
    difference_type diff = m_end - m_begin;

    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << " m_begin="
        << static_cast<void*>(m_begin) << " and m_end="
        << static_cast<void*>(m_end);
}

/// Construct an array from a section of the raw storage.  No new storage
/// is allocated, i.e., shallow copy.
///
/// @note The arguments @c start and @c end are offsets into the raw
/// storage measured in number of bytes, NOT the number of elements of type
/// T!
template<class T>
ibis::array_t<T>::array_t(ibis::fileManager::storage* rhs,
                          const size_t start, const size_t end)
    : actual(rhs),
      m_begin((T*)(rhs != 0 ? rhs->begin()+start : 0)),
      m_end((T*)(rhs != 0 ? rhs->begin()+end : 0)) {
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(end < start && end != 0 && ibis::gVerbose > 0)
        << "Warning -- array_t<" << typeid(T).name()
        << "> called with a suspicious looking section [" << start
        << ", " << end << ')';
#endif
    if (actual != 0 && m_begin != 0 && m_end != 0) {
        if ((const char*)(m_begin) > rhs->end()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- the constructor of array_t<" << typeid(T).name()
                << "> has received an empty range of bytes (begin=" << start
                << ", end=" << end << "), please check the calling sequence";

            m_begin = (T*)(rhs->end());
        }
        else if ((const char*)(m_end) > rhs->end()) {
            m_end = (T*)(rhs->end());
        }
        actual->beginUse();
    }
    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<const void*>(this) << " with actual="
        << static_cast<const void*>(actual) << ", m_begin="
        << static_cast<const void*>(m_begin) << " and m_end="
        << static_cast<const void*>(m_end) << " using section [" << start
        << ", " << end << ") from " << static_cast<const void*>(rhs);
}

/// Construct a new array by reading a part of a binary file.  The argument
/// @c fdes must be a valid file descriptor (as defined in unistd.h).  It
/// attempt to read @c end - @c begin bytes from the file starting at
/// offset @c begin.
template<class T>
ibis::array_t<T>::array_t(const int fdes, const off_t begin, const off_t end)
    : actual(new ibis::fileManager::storage(fdes, begin, end)),
      m_begin((T*) (actual != 0 ? actual->begin() : 0)),
      m_end((T*) (actual != 0 ? actual->end() : 0)) {
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(end < begin && end != 0 && ibis::gVerbose > 0)
        << "Warning -- array_t<" << typeid(T).name()
        << "> called with a suspicious looking section [" << begin
        << ", " << end << ')';
#endif
    if (m_begin == 0 || m_begin + (end - begin)/sizeof(T) != m_end) {
        delete actual;
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- array_t<" << typeid(T).name() << "> failed to read "
            << "from file descriptor " << fdes << " between " << begin
            << " and " << end;
        throw ibis::bad_alloc("array_t failed to read file segment"
                              IBIS_FILE_LINE);
    }
    if (actual != 0)
        actual->beginUse();

    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << " and m_begin="
        << static_cast<void*>(m_begin) << ", content from file descriptor "
        << fdes << " beginning at " << begin << " ending at " << end;
}

/// Constructor.  Reads a portion of the named file.
template<class T>
ibis::array_t<T>::array_t(const char *fn, const off_t begin, const off_t end)
    : actual(new ibis::fileManager::storage(fn, begin, end)),
      m_begin(actual ? (T*) actual->begin() : (T*)0),
      m_end(actual ? (T*) actual->end() : (T*)0) {
    if (m_begin == 0 || m_begin + (end - begin)/sizeof(T) != m_end) {
        delete actual;
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- array_t<" << typeid(T).name() << "> failed to read "
            << "from file \"" << fn << "\" between " << begin
            << " and " << end;
        throw ibis::bad_alloc("array_t failed to read file segment"
                              IBIS_FILE_LINE);
    }
    if (actual != 0)
        actual->beginUse();
    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << " and m_begin="
        << static_cast<void*>(m_begin) << ", content from file " << fn
        << " beginning at " << begin << " ending at " << end;
}

/// Constructor.  Reads a portion of the named file through the specified
/// file descriptor.
template<class T>
ibis::array_t<T>::array_t(const char *fn, const int fdes,
                          const off_t begin, const off_t end)
    : actual(ibis::fileManager::getFileSegment(fn, fdes, begin, end)),
      m_begin(actual ? (T*) actual->begin() : (T*)0),
      m_end(actual ? (T*) actual->end() : (T*)0) {
    if (m_begin == 0 || m_begin + (end - begin)/sizeof(T) != m_end) {
        delete actual;
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- array_t<" << typeid(T).name() << "> failed to read "
            << "from file \"" << fn << "\" (" << fdes << ") between " << begin
            << " and " << end;
        throw ibis::bad_alloc("array_t failed to read file segment"
                              IBIS_FILE_LINE);
    }
    if (actual != 0)
        actual->beginUse();
    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << " and m_begin="
        << static_cast<void*>(m_begin) << ", content from file " << fn
        << " beginning at " << begin << " ending at " << end;
}

/// Constructor.  Directly use the raw pointer provided, i.e., shallow
/// copy.
///
/// @note The second argument is the number of elements starting at the
/// given address.
///
/// @note This object does not copy the content at the given address, nor
/// takes the responsibility of freeing the content at the given address.
/// It merely provide a reference to the content at the given address.  The
/// caller needs to free the memory after use.
template <class T>
ibis::array_t<T>::array_t(T *addr, size_t nelm)
    : actual(0), m_begin(addr), m_end(addr+nelm) {
    LOGGER(ibis::gVerbose > 9)
        << "array_t<" << typeid(T).name() << "> constructed at "
        << static_cast<void*>(this) << " with actual="
        << static_cast<void*>(actual) << " and m_begin="
        << static_cast<void*>(m_begin) << ", content from " << nelm
        << " element" << (nelm>1?"s":"") << " beginning at " << addr;
}

/// Assignment operator.  It performs a shallow copy.
template<class T>
ibis::array_t<T>& ibis::array_t<T>::operator=(const array_t<T>& rhs) {
    array_t<T> tmp(rhs); // make a shallow copy
    swap(tmp); // swap, let compiler clean up the old content
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 10)
        << "array_t<" << typeid(T).name() << ">::operator= copied ("
        << static_cast<const void*>(&rhs) << ", "
        << static_cast<const void*>(rhs.m_begin) << ") to ("
        << static_cast<const void*>(this) << ", "
        << static_cast<const void*>(m_begin) << ")";
#endif
    return *this;
}

/// The copy function.  It performs a shallow copy.
template<class T> 
void ibis::array_t<T>::copy(const array_t<T>& rhs) {
    array_t<T> tmp(rhs);
    swap(tmp);
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 10)
        << "array_t<" << typeid(T).name() << ">::copy copied ("
        << static_cast<const void*>(&rhs) << ", "
        << static_cast<const void*>(rhs.m_begin) << ") to ("
        << static_cast<const void*>(this) << ", "
        << static_cast<const void*>(m_begin) << ")";
#endif
} // ibis::array_t<T>::copy

/// The deep copy function.  It makes an in-memory copy of @c rhs.
/// If @c rhs is empty or uninitialized, the current content is cleared
/// resulting in an empty array.
template<class T> 
void ibis::array_t<T>::deepCopy(const array_t<T>& rhs) {
    if (rhs.m_begin != 0 && rhs.m_end != 0) { // valid rhs
        if (actual != 0 && actual->inUse() < 2U &&
            actual->end() >= rhs.size() * sizeof(T) + actual->begin()) {
            // already has enough memory allocated, stay with it
            const size_t n = rhs.size();
            m_begin = (T*)(actual->begin());
            m_end = m_begin + n;
            for (size_t i = 0; i < n; ++ i)
                m_begin[i] = rhs[i];
        }
        else {
            array_t<T> tmp(rhs.size()); // allocate memory
            for (size_t i = 0; i < rhs.size(); ++ i)
                tmp[i] = rhs[i];
            swap(tmp);
        }
    }
    else { // invalid rhs, mark existing array as empty
        m_end = m_begin;
    }
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 10)
        << "array_t<" << typeid(T).name() << ">::deepCopy copied ("
        << static_cast<const void*>(&rhs) << ", "
        << static_cast<const void*>(rhs.m_begin) << ") to ("
        << static_cast<const void*>(this) << ", "
        << static_cast<const void*>(m_begin) << ")";
#endif
} // ibis::array_t<T>::deepCopy

/// Make a non-shared copy of the array.
/// This function makes a copy of the current content if the content is
/// shared by two or more clients.  This does not guarantee that it would
/// not become shared later.  The complete solution is to implement
/// copy-on-write in all functions that modifies an array, however, there
/// are plenty of cases where copying is unnecessary.
///
/// If this object is not well-formed, it will be reset to be an empty
/// array.
template<class T>
void ibis::array_t<T>::nosharing() {
    if (m_begin != 0 && m_end >= m_begin) { // a well-formed object
        if (actual == 0 || m_begin != (T*)actual->begin() ||
            actual->filename() != 0 || actual->inUse() > 1) {
            // copy-and-swap
            std::unique_ptr<ibis::fileManager::storage>
                tmp(new ibis::fileManager::storage
                    (reinterpret_cast<const char*>(m_begin),
                     reinterpret_cast<const char*>(m_end)));
#if defined(DEBUG) || defined(_DEBUG)
            LOGGER(ibis::gVerbose > 10)
                << "array_t<" << typeid(T).name() << ">::nosharing copied ("
                << static_cast<const void*>(this) << ", "
                << static_cast<const void*>(m_begin) << ") to ("
                << static_cast<const void*>(this) << ", "
                << static_cast<const void*>(tmp->begin()) << ")";
#endif

            freeMemory();
            tmp->beginUse();
            m_begin = (T*)(tmp->begin());
            m_end = (T*)(tmp->end());
            actual = tmp.release();
        }
    }
    else { // ill-formed array_t object, clean up
        freeMemory();
    }
} // ibis::array_t<T>::nosharing

/// Free the memory associated with the fileManager::storage.
template<class T>
void ibis::array_t<T>::freeMemory() {
    if (actual != 0) {
        actual->endUse();
        LOGGER(ibis::gVerbose > 9)
            << "array_t<" << typeid(T).name()
            << ">::freeMemory this=" << static_cast<void*>(this)
            << " actual=" << static_cast<void*>(actual)
            << " and m_begin=" << static_cast<const void*>(m_begin)
            << " (active references: " << actual->inUse()
            << ", past references: " << actual->pastUse() << ')';

        if (0 == actual->filename() && 0 == actual->inUse()) {
            delete actual;
        }
        actual = 0;
    }
    m_begin = 0;
    m_end = 0;
} // ibis::array_t<T>::freeMemory

/// Find the position of the first element that is no less than @c val.
/// Assuming @c ind was produced by the sort function,
/// it returns the smallest i such that @c operator[](ind[i]) >= @c val.
template<class T>
uint32_t ibis::array_t<T>::find(const array_t<uint32_t>& ind,
                                const T& val) const {
    if (m_begin[ind[0]] >= val)
        return 0;

    uint32_t i = 0, j = size();
    if (j < QSORT_MIN) { // linear search
        for (i = 0; i < j; ++ i)
            if (m_begin[ind[i]] >= val) return i;
    }
    else { // binary search
        uint32_t m = (i + j) / 2;
        while (i < m) { // m_begin[ind[j]] >= val
            if (m_begin[ind[m]] < val)
                i = m;
            else
                j = m;

            m = (i + j) / 2;
        }
    }
    return j;
} // ibis::array_t<T>::find

/// Find the first position where the value is no less than @c val.
/// Assuming the array is already sorted in the ascending order,
/// it returns the smallest i such that @c operator[](i) >= @c val.
template<class T>
size_t ibis::array_t<T>::find(const T& val) const {
    if (m_end <= m_begin) return 0; // empty array
    else if (! (*m_begin < val)) return 0; // 1st value is larger than val

    size_t i = 0, j = size();
    if (j < QSORT_MIN) { // linear search
        for (i = 0; i < j; ++ i)
            if (m_begin[i] >= val) return i;
    }
    else {
        size_t m = (i + j) / 2;
        while (i < m) { // m_begin[j] >= val
            if (m_begin[m] < val)
                i = m;
            else
                j = m;

            m = (i + j) / 2;
        }
    }
    return j;
} // ibis::array_t<T>::find

/// Find the first position where the value is greater than @c val.
/// Assuming the array is already sorted in the ascending order, it returns
/// the smallest i such that @c operator[](i) > @c val.
///
/// @note The word upper is used in the same sense as in the STL function
/// std::upper_bound.
template<class T>
size_t ibis::array_t<T>::find_upper(const T& val) const {
    if (m_end <= m_begin) return 0; // empty array
    else if (*m_begin > val) return 0; // 1st value is larger than val

    size_t i = 0, j = size();
    if (j < QSORT_MIN) { // linear search
        for (i = 0; i < j; ++ i)
            if (m_begin[i] > val) return i;
    }
    else {
        size_t m = (i + j) / 2;
        while (i < m) { // m_begin[j] > val
            if (m_begin[m] <= val)
                i = m;
            else
                j = m;

            m = (i + j) / 2;
        }
    }
    return j;
} // ibis::array_t<T>::find_upper

/// A stable sort using the provided workspace.  The current content is
/// modified to be in the ascending order.  The argument @c tmp is only
/// used as temporary storage.  It uses the merge sort algorithm.
template<class T>
void ibis::array_t<T>::stableSort(array_t<T>& tmp) {
    const size_t n = size();
    if (n < 2) return;

    if (tmp.size() != n)
        tmp.resize(n);

    size_t stride = 1;
    while (stride < n) {
        size_t i;
        for (i = 0; i+stride < n; i += stride+stride) {
            if (stride > 1) { // larger strides
                size_t i0 = i;
                size_t i1 = i + stride;
                const size_t i0max = i1;
                const size_t i1max = (i1+stride <= n ? i1+stride : n);
                size_t j = i;
                while (i0 < i0max || i1 < i1max) {
                    if (i0 < i0max) {
                        if (i1 < i1max) {
                            if (m_begin[i0] <= m_begin[i1]) {
                                tmp[j] = m_begin[i0];
                                ++ i0;
                            }
                            else {
                                tmp[j] = m_begin[i1];
                                ++ i1;
                            }
                        }
                        else {
                            tmp[j] = m_begin[i0];
                            ++ i0;
                        }
                    }
                    else {
                        tmp[j] = m_begin[i1];
                        ++ i1;
                    }
                    ++ j;
                }
            }
            else if (m_begin[i] <= m_begin[i+1]) { // stride 1
                tmp[i] = m_begin[i];
                tmp[i+1] = m_begin[i+1];
            }
            else { // stride 1
                tmp[i] = m_begin[i+1];
                tmp[i+1] = m_begin[i];
            }
        }

        while (i < n) {
            tmp[i] = m_begin[i];
            ++ i;
        }
        swap(tmp);
        stride += stride; // double the stride every iteration
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    (void) isSorted();
#endif
} // ibis::array_t<T>::stableSort

/// A stable sort that does not modify the current array.  It uses two
/// additional arrays for temporary storage.  It uses the merge sort
/// algorithm to produce an index array so that array[ind[i]] would in
/// ascending order.
template<class T>
void ibis::array_t<T>::stableSort(array_t<uint32_t>& ind) const {
    if (size() > 2) {
        if (size() > 0xFFFFFFFFUL) {
            ind.clear();
            return;
        }

        array_t<T> tmp1, tmp2;
        array_t<uint32_t> itmp;
        tmp1.deepCopy(*this);
        ibis::array_t<T>::stableSort(tmp1, ind, tmp2, itmp);
    }
    else if (size() == 2) {
        ind.resize(2);
        if (m_begin[1] < m_begin[0]) {
            ind[0] = 1;
            ind[1] = 0;
        }
        else {
            ind[0] = 0;
            ind[1] = 1;
        }
    }
    else if (size() == 1) {
        ind.resize(1);
        ind[0] = 0;
    }
    else {
        ind.clear();
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    (void) isSorted(ind);
#endif
} // stableSort

/// A stable sort.  It does not change this array, but produces a
/// sorted version in @c sorted.
/// The content of array @c ind will be simply moved together with @ this
/// array if it is the same size as this array.  Otherwise, it is
/// initialized to consecutive integers starting from 0 before the actual
/// sorting is performed.
template<class T>
void ibis::array_t<T>::stableSort(array_t<uint32_t>& ind,
                                  array_t<T>& sorted) const {
    if (size() > 2) {
        if (size() > 0xFFFFFFFFUL) {
            sorted.clear();
            ind.clear();
            return;
        }

        array_t<T> tmp;
        array_t<uint32_t> itmp;
        sorted.resize(size());
        ind.resize(size());
        for (size_t i = 0; i < size(); ++ i) {
            sorted[i] = m_begin[i];
            ind[i] = i;
        }
        ibis::array_t<T>::stableSort(sorted, ind, tmp, itmp);
    }
    else if (size() == 2) {
        sorted.resize(2);
        ind.resize(2);
        if (m_begin[1] > m_begin[0]) {
            sorted[0] = m_begin[1];
            sorted[1] = m_begin[0];
            ind[0] = 1;
            ind[1] = 0;
        }
        else {
            sorted[0] = m_begin[0];
            sorted[1] = m_begin[1];
            ind[0] = 0;
            ind[1] = 1;
        }
    }
    else if (size() == 1) {
        sorted.resize(1);
        ind.resize(1);
        sorted[0] = m_begin[0];
        ind[0] = 0;
    }
    else {
        sorted.clear();
        ind.clear();
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    (void) sorted.isSorted();
#endif
} // ibis::array_t<T>::stableSort

/// This function sorts the content of array @c val.  The values of @c
/// ind[i] will be reordered in the same way as the array @c val.  The two
/// other arrays are used as temporary storage.
/// @note
/// On input, if array @c ind has the same size as array @c val, the
/// content of array @c ind will be directly used.  Otherwise, array @c ind
/// will be initialized to be consecutive integers starting from 0.
/// @note
/// If the input array @c val has less than two elements, this function
/// does nothing, i.e., does not change any of the four arguments.
template<class T>
void ibis::array_t<T>::stableSort(array_t<T>& val, array_t<uint32_t>& ind,
                                  array_t<T>& tmp, array_t<uint32_t>& itmp) {
    const size_t n = val.size();
    if (n < 2)
        return; // nothing to do
    if (n > 0xFFFFFFFFUL) {
        val.clear();
        ind.clear();
        return;
    }

    if (ind.size() != n) {
        ind.resize(n);
        for (uint32_t i = 0; i < n; ++ i)
            ind[i] = i;
    }
    tmp.resize(n);
    itmp.resize(n);

    std::less<T> cmp;
    size_t stride = 1;
    while (stride < n) {
        size_t i;
        for (i = 0; i+stride < n; i += stride+stride) {
            if (stride > 1) { // larger strides
                size_t i0 = i;
                size_t i1 = i + stride;
                const size_t i0max = i1;
                const size_t i1max = (i1+stride <= n ? i1+stride : n);
                size_t j = i;
                while (i0 < i0max || i1 < i1max) {
                    if (i0 < i0max) {
                        if (i1 < i1max) {
                            if (cmp(val[i1], val[i0])) {
                                itmp[j] = ind[i1];
                                tmp[j] = val[i1];
                                ++ i1;
                            }
                            else {
                                itmp[j] = ind[i0];
                                tmp[j] = val[i0];
                                ++ i0;
                            }
                        }
                        else {
                            itmp[j] = ind[i0];
                            tmp[j] = val[i0];
                            ++ i0;
                        }
                    }
                    else {
                        itmp[j] = ind[i1];
                        tmp[j] = val[i1];
                        ++ i1;
                    }
                    ++ j;
                }
            }
            else if (cmp(val[i+1], val[i])) { // stride 1
                tmp[i] = val[i+1];
                tmp[i+1] = val[i];
                itmp[i] = ind[i+1];
                itmp[i+1] = ind[i];
            }
            else { // stride 1
                tmp[i] = val[i];
                itmp[i] = ind[i];
                tmp[i+1] = val[i+1];
                itmp[i+1] = ind[i+1];
            }
        }
        while (i < n) { // copy the left over elements
            tmp[i] = val[i];
            itmp[i] = ind[i];
            ++ i;
        }
        val.swap(tmp);
        ind.swap(itmp);
        stride += stride; // double the stride every iteration
    }
} // ibis::array_t<T>::stableSort

/// Does this array have the same content as the other?  Return true is
/// yes, otherwise false.
template <typename T> bool
ibis::array_t<T>::equal_to(const ibis::array_t<T>& other) const {
    if (size() != other.size()) return false;
    for (size_t j = 0; j < size(); ++ j)
        if (m_begin[j] != other.m_begin[j])
            return false;
    return true;
} // ibis::array_t<T>::equal_to

/// Verify the values are in ascending order.  Returns true if yes,
/// otherwise no.
template<class T>
bool ibis::array_t<T>::isSorted() const {
    const size_type ni = size();
    bool sorted = true;
    for (size_t i = 1; i < ni; ++ i) {
        if (m_begin[i] < m_begin[i-1]) {
            sorted = false;
            break;
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "DEBUG -- array_t<" << typeid(T).name() << ">::isSorted"
             " found the array in the ascending order";
    }
    else {
        lg() << "Warning -- array_t<" << typeid(T).name() << ">::isSorted"
             " found the array not in the ascending order";
        for (size_t i = 0; i < ni; ++i) {
            lg() << "\n" << i << "\t" << m_begin[i];
            if (i > 0 && m_begin[i] < m_begin[i-1]) {
                lg() << "\t*";
            }
        }
    }
#endif
    return sorted;
} // ibis::array_t<T>::isSorted

/// Verify the index array orders the values in the ascending order.
/// Returns true if yes, otherwise false.
template<class T>
bool ibis::array_t<T>::isSorted(const array_t<uint32_t>& ind) const {
    const size_type ni = ind.size();
    bool sorted = true;
    for (size_t i = 1; i < ni; ++ i) {
        if (m_begin[ind[i]] < m_begin[ind[i-1]]) {
            sorted = false;
            break;
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    if (sorted) {
        lg() << "DEBUG -- array_t<" << typeid(T).name() << ">::isSorted(ind["
             << ni << "]) is true";
    }
    else {
        lg() << "Warning -- array_t<" << typeid(T).name() << ">::isSorted(ind["
             << ni << "]) is false";
        for (size_t i = 0; i < ni; ++i) {
            lg() << "\nind[" << i << "]=" << ind[i] << "\t"
                 << m_begin[ind[i]];
            if (i > 0 && m_begin[ind[i]] < m_begin[ind[i-1]]) {
                lg() << "\t*";
            }
        }
    }
#endif
    return sorted;
} // ibis::array_t<T>::isSorted

/// Remove the duplicate values.  It sorts the values first and then
/// removes any entry that is not in strictly assending order.
///
/// @warning This function uses std::sort to order the values first before
/// removing duplicates.  The function std::sort normally would places
/// not-a-number (nan) at the end of the array which allows this function
/// to remove them from the list of values.  However, should std::sort
/// actually places nans at the beginning of the sorted list, this function
/// would produce an empty array.
template<class T>
void ibis::array_t<T>::deduplicate() {
    const size_t oldsize = size();
    if (oldsize <= 1) return;

    int flag = 2; // 2 == unique, 1 == sorted
    for (size_t j = 1; j < oldsize && flag > 0; ++ j) {
        if (m_begin[j-1] < m_begin[j]) {
            // nothing to do
        }
        else if (m_begin[j-1] == m_begin[j]) {
            flag = (flag > 0);
        }
        else {
            flag = 0;
        }
    }
    if (flag > 1) return; // nothing to do

    nosharing(); // prepare for changes
    if (flag == 0) // need to sort values
        std::sort(m_begin, m_end);

    // deduplicate
    size_t sz = 0;
    for (size_t j = 1; j < oldsize; ++ j) {
        if (m_begin[sz] < m_begin[j]) {
            ++ sz;
            m_begin[sz] = m_begin[j];
        }
    }
    resize(sz+1);
} // ibis::array_t<T>::deduplicate

/// Produce index for ascending order.
/// Sort the array to produce @c ind so that array_t[ind[i]] is in
/// ascending order.  Uses the quicksort algorithm with introspection.  On
/// entering this function, if the values in ind are less than size(), then
/// this function only sorts the subset of elements identified by the
/// indices.  Otherwise, it is fill with values between 0 and size()-1.  It
/// returns an empty array ind to indicate errors.
template<class T>
void ibis::array_t<T>::sort(array_t<uint32_t>& ind) const {
    const size_t na = size();
    size_t ni = ind.size();
    bool keepind = (ni > 0);
    for (size_t j = 0; keepind && j < ni; ++ j)
        keepind = (ind[j] < na);
    if (! keepind) { // initalize ind to [0:na-1]
        ni = na;
        ind.resize(na);
        for (size_t i = 0; i < na; ++i)
            ind[i] = i;
    }
    if (ni < 2) { // no need to sort
        return;
    }
    if (ni > 0xFFFFFFFFUL) { // do not support arrays of this size?
        ind.clear();
        return;
    }

    // call qsort to do the actual work
    qsort(ind, 0, ni);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    (void) isSorted(ind);
#endif
} // ibis::array_t<T>::sort

/// Return the positions of the @c k largest elements.
/// Sort the @c k largest elements of the array.  Return the indices of the
/// in sorted values.
///
///@note The resulting array @c ind may have more than @c k elements if the
/// <code>k</code>th largest value is not a single value.  The array @c
/// ind may have less than @c k elements if this array has less than @c k
/// elements.
///
///@note The values are sorted in the ascending order, i.e., <code>[ind[i]] <=
/// [ind[i+1]]</code>.  This is done so that all sorting routines produce
/// indices in the same ascending order.  It should be easy to reverse the
/// order the indices since it only contains the largest values.
template<class T>
void ibis::array_t<T>::topk(uint32_t k, array_t<uint32_t>& ind) const {
    if (k == 0 || size() > 0xFFFFFFFFUL) {
        ind.clear();
        return;
    }

    uint32_t front = 0;
    uint32_t back = size();
    // initialize ind array
    ind.resize(back);
    for (uint32_t i = 0; i < back; ++ i)
        ind[i] = i;
    if (back <= k) {
        qsort(ind, front, back);
        return;
    }

    const uint32_t mark = back - k;
    // main loop to deal with the case of having more than QSORT_MIN elements
    while (back > front + QSORT_MIN && back > mark) {
        // find a pivot and partition the values into two groups
        uint32_t p = partition(ind, front, back);
        if (p >= mark) { // sort [p, back-1]
            qsort(ind, p, back);
            back = p;
        }
        else { // do not sort the smaller numbers
            front = p;
        }
    }
    if (back > mark) {
        // use insertion sort to clean up the few elements
        isort(ind, front, back);
    }
    // find the first value before [mark] and is equal to it
    for (front = mark;
         front > 0 && m_begin[front-1] == m_begin[mark];
         -- front);
    if (front > 0) { // not all are sorted
        // copy the sorted indices to the front of array ind
        for (back = 0; front < size(); ++ front, ++ back)
            ind[back] = ind[front];
        ind.resize(back); // return only the sorted values
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "DEBUG -- topk(" << k << ")\n";
    for (size_t i = 0; i < back; ++i)
        lg() << ind[i] << "\t" << m_begin[ind[i]] << "\n";
    std::flush(lg());
#endif
} // ibis::array_t<T>::topk

/// Return the positions of the @c k smallest elements.
/// Sort the first @c k elemnent of the array.  Return the indices of the
/// smallest values in array @c ind.
///
///@note The resulting array @c ind may have more than @c k elements if the
/// <code>k</code>th smallest value is not a single value.  The array @c
/// ind may have less than @c k elements if this array has less than @c k
/// elements.
template<class T>
void ibis::array_t<T>::bottomk(uint32_t k, array_t<uint32_t>& ind) const {
    if (k == 0 || size() > 0xFFFFFFFFUL) {
        ind.clear();
        return;
    }

    uint32_t front = 0;
    uint32_t back = size();
    // initialize ind array
    ind.resize(back);
    for (size_t i = 0; i < back; ++ i)
        ind[i] = i;
    if (back <= k) {
        qsort(ind, front, back);
        return;
    }

    // main loop to deal with the case of having more than QSORT_MIN elements
    while (back > front + QSORT_MIN && k > front) {
        // find a pivot and partition the values into two groups
        uint32_t p = partition(ind, front, back);
        if (p <= k) { // sort [front, p-1]
            qsort(ind, front, p);
            front = p;
        }
        else { // do not sort the larger numbers
            back = p;
        }
    }
    if (k > front) {
        // use insertion sort to clean up the few elements
        isort(ind, front, back);
    }
    // where to cut off -- only include values that are exactly the same as
    // [k-1]
    for (back = k;
         back < size() && m_begin[ind[back]] == m_begin[k-1];
         ++ back);
    ind.resize(back); // drop the indices of partially sorted indices
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "DEBUG -- bottomk(" << k << ")\n";
    for (size_t i = 0; i < back; ++i)
        lg() << ind[i] << "\t" << m_begin[ind[i]] << "\n";
    std::flush(lg());
#endif
} // ibis::array_t<T>::bottomk

/// The quick sort procedure.  Sort @c [ind[front:back]] assuming the array
/// @c ind has been properly initialized.  This is the main function that
/// implements @c ibis::array_t<T>::sort.
template<class T>
void ibis::array_t<T>::qsort(array_t<uint32_t>& ind, uint32_t front,
                             uint32_t back, uint32_t lvl) const {
    while (back > front + QSORT_MIN) { // more than QSORT_MIN elements
        // find the pivot
        uint32_t p = partition(ind, front, back);
        // make sure the smaller half is sorted
        if (p >= back) {
            front = back; // all values are the same
        }
        else if (p - front <= back - p) {
            // sort [front, p-1]
            if (p > front + QSORT_MIN) { // use quick sort
                if (lvl >= QSORT_MAX_DEPTH)
                    hsort(ind, front, p);
                else
                    qsort(ind, front, p, lvl+1);
            }
            else if (p > front + 2) { // between 2 and QSORT_MIN elements
                isort(ind, front, p);
            }
            else if (p == front + 2) { // two elements only
                if (m_begin[ind[front]] > m_begin[ind[front+1]]) {
                    uint32_t tmp = ind[front];
                    ind[front] = ind[front+1];
                    ind[front+1] = tmp;
                }
            }
            front = p;
        }
        else { // sort [p, back-1]
            if (p + QSORT_MIN < back) { // more than QSORT_MIN elements
                if (lvl >= QSORT_MAX_DEPTH) {
                    hsort(ind, p, back);
                }
                else {
                    qsort(ind, p, back, lvl+1);
                }
            }
            else if (p + 2 < back) { // 2 to QSORT_MIN elements
                isort(ind, p, back);
            }
            else if (p + 2 == back) { // two elements only
                if (m_begin[ind[p]] > m_begin[ind[p+1]]) {
                    back = ind[p];
                    ind[p] = ind[p+1];
                    ind[p+1] = back;
                }
            }
            back = p;
        }
    }
    // use insertion sort to clean up the few left over elements
    isort(ind, front, back);
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg(4);
    bool sorted = true;
    for (size_t i = front+1; i < back; ++ i) {
        if (m_begin[ind[i]] < m_begin[ind[i-1]]) {
            sorted = false;
            break;
        }
    }
    if (sorted) {
        lg() << "DEBUG -- qsort(" << front << ", " << back
             << ") verified results to be in the ascending order";
    }
    else {
        lg() << "Warning -- qsort(" << front << ", " << back
             << ") did not sort correctly";
        for (size_t i = front; i < back; ++i) {
            lg() << "\nind[" << i << "]=" << ind[i] << "\t"
                 << m_begin[ind[i]];
            if (i > front && m_begin[ind[i]] < m_begin[ind[i-1]]) {
                lg() << "\t*";
            }
        }
    }
#endif
} // qsort

/// A heapsort function.  This is used as the backup option in case the
/// quicksort has been consistently picking bad pivots.
template<class T>
void ibis::array_t<T>::hsort(array_t<uint32_t>& ind, uint32_t front,
                             uint32_t back) const {
    uint32_t n = back;
    uint32_t parent = front + (back-front)/2;
    uint32_t curr, child;
    uint32_t itmp; // temporary index value
    while (true) {
        if (parent > front) {
            // stage 1 -- form heap
            -- parent;
            itmp = ind[parent];
        }
        else {
            // stage 2 -- extract element from the heap
            -- n;
            if (n <= front) break;
            itmp = ind[n];
            ind[n] = ind[front];
        }

        // push-down procedure
        curr = parent;
        child = (curr-front)*2 + 1 + front; // the left child
        while (child < n) {
            // choose the child with larger value
            if (child+1 < n &&
                m_begin[ind[child+1]] > m_begin[ind[child]])
                ++ child; // the right child has larger value
            if (m_begin[itmp] < m_begin[ind[child]]) {
                // the child has larger value than the parent
                ind[curr] = ind[child];
                curr = child;
                child = (child-front)*2 + 1 + front;
            }
            else { // the parent has larger value
                break;
            }
        } // while (child < n)
        // the temporary index goes to its final location
        ind[curr] = itmp;
    } // while (1)
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg(4);
    lg() << "DEBUG -- hsort(" << front << ", " << back << ")\n";
    for (size_t i = front; i < back; ++i) {
        lg() << ind[i] << "\t" << m_begin[ind[i]];
        if (i > front && m_begin[ind[i-1]] > m_begin[ind[i]])
            lg() << "\t*** error [ind[" << i-1 << "]] > [ind[" << i
                 << "]] ***";
        lg() << "\n";
    }
#endif
} // ibis::array_t<T>::hsort

/// Simple insertion sort.  Not a stable sort.  This function is used
/// instead of the quick sort function ibis::array_t<T>::qsort for small
/// arrays.
template<class T>
void ibis::array_t<T>::isort(array_t<uint32_t>& ind, uint32_t front,
                             uint32_t back) const {
    uint32_t i, j, k;
    for (i = front; i < back-1; ++ i) {
        // go through [i+1, back-1] to find the smallest element
        k = i + 1;
        for (j = k + 1; j < back; ++ j) {
            if (m_begin[ind[k]] > m_begin[ind[j]])
                k = j;
        }
        // place the index of the smallest element at position ind[i]
        if (m_begin[ind[i]] > m_begin[ind[k]]) {
            j = ind[i];
            ind[i] = ind[k];
            ind[k] = j;
        }
        else { // ind[i] has the right value, ind[k] goes to ind[i+1]
            ++ i;
            if (m_begin[ind[i]] > m_begin[ind[k]]) {
                j = ind[i];
                ind[i] = ind[k];
                ind[k] = j;
            }
        }
    }
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg(4);
    lg() << "DEBUG -- isort(" << front << ", " << back << ")\n";
    for (i = front; i < back; ++i)
        lg() << ind[i] << "\t" << m_begin[ind[i]] << "\n";
#endif
} // ibis::array_t<T>::isort

/// A median-of-3 method partition algorithm to find a pivot for quick
/// sort.  Upon returning from this function, val[ind[front, pivot-1]] <
/// val[ind[pivot, back-1]].  This implementation of the median-of-3 tries
/// to find the median of the three values, the goal is to find at least
/// one value that is smaller than others.  If all values are equal, this
/// function returns the value 'back'.  Caller is to ensure back > front+3.
template<class T> 
uint32_t ibis::array_t<T>::partition(array_t<uint32_t>& ind, uint32_t front,
                                     uint32_t back) const {
    uint32_t i, j, pivot, tmp;
    T target;
    i = front;
    j = back - 1;
    pivot = (front + back) / 2; // temporary usage, index to target

    // find the median of three values
    if (m_begin[ind[i]] < m_begin[ind[pivot]]) {
        if (m_begin[ind[pivot]] > m_begin[ind[j]]) {
            tmp = ind[j];
            ind[j] = ind[pivot];
            ind[pivot] = tmp;
            if (m_begin[ind[i]] > m_begin[ind[pivot]]) {
                tmp = ind[i];
                ind[i] = ind[pivot];
                ind[pivot] = tmp;
            }
            else if (m_begin[ind[i]] == m_begin[ind[pivot]]) {
                // move ind[pivot] to ind[front+1] and use the largest
                // element (now located at ind[j]) as the reference for
                // partitioning
                ++ i;
                if (pivot > i) {
                    tmp = ind[pivot];
                    ind[pivot] = ind[i];
                    ind[i] = tmp;
                }
                pivot = j;
            }
        }
    }
    else if (m_begin[ind[i]] > m_begin[ind[pivot]]) {
        tmp = ind[i];
        ind[i] = ind[pivot];
        ind[pivot] = tmp;
        if (m_begin[ind[pivot]] > m_begin[ind[j]]) {
            tmp = ind[j];
            ind[j] = ind[pivot];
            ind[pivot] = tmp;
            if (m_begin[ind[i]] > m_begin[ind[pivot]]) {
                tmp = ind[i];
                ind[i] = ind[pivot];
                ind[pivot] = tmp;
            }
            else if (m_begin[ind[i]] == m_begin[ind[pivot]]) {
                ++ i;
                if (i < pivot) {
                    tmp = ind[pivot];
                    ind[pivot] = ind[i];
                    ind[i] = tmp;
                }
                pivot = j;
            }
        }
    }
    else if (m_begin[ind[i]] > m_begin[ind[j]]) {
        // m_begin[ind[i]] == m_begin[ind[pivot]]
        tmp = ind[i];
        ind[i] = ind[j];
        ind[j] = tmp;
    }
    else if (m_begin[ind[i]] < m_begin[ind[j]]) {
        // m_begin[ind[i]] == m_begin[ind[pivot]]
        ++ i;
        if (i < pivot) {
            tmp = ind[pivot];
            ind[pivot] = ind[i];
            ind[i] = tmp;
        }
        pivot = j;
    }
    else { // all three values are the same
        target = m_begin[ind[i]];
        // attempt to find a value that is different
        for (tmp=i+1; tmp<back && target==m_begin[ind[tmp]]; ++tmp);
        if (tmp >= back) {
            return back;
        }
        else if (m_begin[ind[tmp]] > target) {
            // swap the larger value to the end -- to make sure it is in
            // the right position for the operation '--j' at the beginning
            // of the next while block
            i = ind[j];
            ind[j] = ind[tmp];
            ind[tmp] = i;
            if (tmp < pivot) { // move the middle element forward
                ++ tmp;
                i = ind[tmp];
                ind[tmp] = ind[pivot];
                ind[pivot] = i;
            }
            pivot = j; // use the last element as the reference
            i = tmp;
        }
        else { // found a smaller element, move it to the front
            i = ind[front];
            ind[front] = ind[tmp];
            ind[tmp] = i;
            i = front;
        }
    }
    target = m_begin[ind[pivot]];

    // loop to partition the values according the value target
    ++ i;
    -- j;
    bool left = (m_begin[ind[i]] < target);
    bool right = (m_begin[ind[j]] >= target);
    while (i < j) {
        if (left) {
            ++ i;
            left = (m_begin[ind[i]] < target);
            if (right) {
                -- j;
                right = (m_begin[ind[j]] >= target);
            }
        }
        else if (right) {
            -- j;
            right = (m_begin[ind[j]] >= target);
        }
        else { // swap ind[i] and ind[j]
            tmp = ind[j];
            ind[j] = ind[i];
            ind[i] = tmp;
            ++ i;
            -- j;
            left = (m_begin[ind[i]] < target);
            right = (m_begin[ind[j]] >= target);
        }
    }

    // assigned the correct value to the variable pivot
    if (left)
        pivot = i + 1;
    else
        pivot = i;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    ibis::util::logger lg(4);
    lg() << "DEBUG -- partition(" << front << ", " << back << ") = "
         << pivot << ", target = " << target << "\nfirst half: ";
    for (i = front; i < pivot; ++i)
        lg() << m_begin[ind[i]] << " ";
    lg() << "\nsecond half: ";
    for (i = pivot; i < back; ++i)
        lg() << m_begin[ind[i]] << " ";
#endif
    return pivot;
} // ibis::array_t<T>::partition

/// Replace the current array with nnew rows.  The first row kept has the
/// row number start in the current content.  Note the the rows are
/// numbered from 0.
template<class T>
void ibis::array_t<T>::truncate(size_t nnew, size_t start) {
    if (nnew == 0U || start >= static_cast<size_t>(m_end-m_begin)) {
        m_end = m_begin;
    }
    else if (start == 0U) {
        if (m_begin+nnew < m_end) {
            nosharing();
            m_end = m_begin + nnew;
        }
    }
    else {
        nosharing();
        if (nnew+start > static_cast<size_t>(m_end-m_begin))
            nnew = (m_end-m_begin) - start;
        for (size_t j = 0; j < nnew; ++ j)
            m_begin[j] = m_begin[j+start];
        m_end = m_begin + nnew;
    }
} // ibis::array_t<T>::truncate

/// Change the size of the array to have @n elements.  If n is greater than
/// its current capacity, it allocates exactly enough space for the new
/// array and copy the existing content to the new space.  The existing
/// content is copied while the new elements are left uninitialized.  If
/// the user needs to perform a series of resize operations with
/// progressively larger sizes, call the function reserve with the final
/// size to increase the underlying storage and avoid copying the data
/// multiple times.
///
/// @note New space is allocated through the function reserve.  The new
/// elements of the array are not initalized.  They contain random
/// unspecified values, don't rely their content.
template<class T>
void ibis::array_t<T>::resize(size_t n) {
    if (n > 0x7FFFFFFFU) {
        throw "array_t must have less than 2^31 elements";
    }

    if (n == 0) {
        m_end = m_begin;
        return;
    }

    if (actual == 0 || m_begin == 0 || m_end < m_begin ||
        (const T*)actual->end() < m_begin + n)
        reserve(n);

    if (actual == 0 || (const T*)actual->end() < m_begin + n) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- array_t::resize(" << n << ") failed to allocate "
            " sufficient space";
        throw ibis::bad_alloc("failed to resize array" IBIS_FILE_LINE);
    }
    else {
        m_end = m_begin + n;
    }
} // ibis::array_t<T>::resize

/// Increase the size of the array_t to have the capacity to store at least
/// @c n elements.  If the current storage object already has n elements,
/// nothing is done, otherwise new storage is allocated.  If it fails to
/// allocate new storage, the current array is unchanged.  The newly
/// allocated space is not initalized to any specific value.
///
/// @note If the incoming argument is 0, this function will either allocate
/// 32 / sizeof(T) elements if sizeof(T) is less than 32, or 2 elements if
/// sizeof(T) is greater or equal to 32.
///
/// @note If the underlying storage could not be modified (i.e., is
/// read-only), new storage will be allocated regardless of the current
/// available storage.  In this case, the newly reserved piece of memory
/// will be at least as large as the existing one.
template<class T>
void ibis::array_t<T>::reserve(size_t n) {
    if (n > 0x7FFFFFFFU) {
        throw "array_t must have less than 2^31 elements";
    }
    if (n == 0) { // special case with n = 0
        n = 32 / sizeof(T);
        if (n == 0)
            n = 2;
    }

    size_t n0 = 0;
    if (actual != 0)
        n0 = (reinterpret_cast<T const *>(actual->end()) - m_begin);
    if (m_begin != 0 && m_end >= m_begin) { // a valid existing array
        if (n > n0 || (actual != 0 && actual->filename() != 0)) {
            // attempt to allocate new storage space
            n0 = (m_end-m_begin);
            if (n < n0) n = n0;
            std::unique_ptr<ibis::fileManager::storage>
                tmp(new ibis::fileManager::storage(n*sizeof(T)));
            if (tmp.get() != 0) { // copy and swap
                (void) memcpy(tmp->begin(), m_begin, n0*sizeof(T));
                if (n > n0)
                    memset(tmp->begin()+n0*sizeof(T), 0, (n-n0)*sizeof(T));
                freeMemory(); // free the old content
                actual = tmp.release();
                actual->beginUse();
                m_begin = (T*)(actual->begin());
                m_end = m_begin + n0;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- array_t::reserve failed to allocate " << n
                    << ' ' << sizeof(T) << "-byte elements";
            }
        }
    }
    else {
        freeMemory(); // just in case actual is not nil
        actual = new ibis::fileManager::storage(n*sizeof(T));
        (void) memset(actual->begin(), 0, n*sizeof(T));
        actual->beginUse();
        m_begin = (T*)(actual->begin());
        m_end = m_begin;
    }
} // ibis::array_t<T>::reserve

/// Release the memory under management to the caller as a raw pointer.
/// The caller takes the responsibility to free to memory.  This object is
/// emptied.
template<class T>
T* ibis::array_t<T>::release() {
    nosharing();
    T* ret = (actual != 0 ? static_cast<T*>(actual->release())
              : static_cast<T*>(0));
    if (ret != 0) {
        m_begin = 0;
        m_end = 0;
    }
    return ret;
} // ibis::array_t<T>::release

/// Insert a single value to the specified location.  It inserts the value
/// right infront of position p and returns the iterator pointing to the
/// new element.
///
/// In case of failure, it returns the same value as the function end().
/// The incoming iterator p must be between begin() and end() of this array.
/// If it is outside of this valid range, the return value will be end().
///
/// It throws a string exception if the new array has more than
/// 0x7FFFFFFF (2^31-1) elements.
template<class T> typename ibis::array_t<T>::iterator
ibis::array_t<T>::insert(typename ibis::array_t<T>::iterator p, const T& val) {
    if (p < m_begin || p > m_end) {
        return m_end;
    }

    const difference_type n0 = m_end - m_begin;
    if (n0 >= 0x7FFFFFFFU) {
        throw "array_t must have less than 2^31 elements";
    }

    if (actual != 0 && actual->filename() == 0 &&
        (const T*)actual->end() > m_end) {
        // use the existing space
        iterator i = m_end;
        while (i > p) {
            *i = i[-1]; --i;
        }
        *p = val;
        ++ m_end;
    }
    else { // copy-and-swap
        const difference_type ip = p - m_begin;
        size_t newsize = static_cast<size_t>((n0 >= 7 ? n0 : 7) + n0);
        if (newsize > 0x7FFFFFFFU)
            newsize = 0x7FFFFFFFU;

        array_t<T> copy(newsize);
        copy.resize(static_cast<size_t>(n0+1));
        for (difference_type j = 0; j < ip; ++ j)
            copy.m_begin[j] = m_begin[j];
        copy.m_begin[ip] = val;
        for (difference_type j = ip; j < n0; ++ j)
            copy.m_begin[j+1] = m_begin[j];
        swap(copy);
    }
    return p;
} // ibis::array<T>::insert

/// Insert n copies of a value (val) before p.   Nothing is done if n is
/// zero, or p is not between m_begin and m_end.
template<class T> void
ibis::array_t<T>::insert(typename ibis::array_t<T>::iterator p, size_t n,
                         const T& val) {
    if (n == 0 || p < m_begin || p > m_end) return;

    if (m_begin == 0 || m_end < m_begin) {
        reserve(n);
        for (size_t j = 0; j < n; ++ j, ++ m_end)
            *m_end = val;
    }
    else if (actual != 0 && actual->filename() == 0 &&
             m_end+n <= (T*)(actual->end())) {
        // use the existing space
        m_end += n;
        // copy all values after p to p+n
        iterator i = m_end - 1;
        while (i >= p+n) {
            *i = *(i-n);
            --i;
        }
        // insert incoming value between p and p+n-1
        while (i >= p) {
            *i = val;
            --i;
        }
    }
    else { // need new memory
        // copy and swap
        const difference_type nold = m_end - m_begin;
        size_t nnew = static_cast<size_t>(nold + (nold>=(long)n?nold:n));
        if (nnew > 0x7FFFFFFFU) {
            throw "array_t must have less than 2^31 elements";
        }

        const size_t jp = p - m_begin;
        ibis::array_t<T> tmp(nnew);
        tmp.resize(nold+n);
        for (size_t j = 0; j < jp; ++ j)
            tmp[j] = m_begin[j];
        for (size_t j = 0; j < n; ++ j)
            tmp[jp+j] = val;
        for (size_t j = jp; j < (nold>0?(size_t)nold:0U); ++ j)
            tmp[n+j] = m_begin[j];
        swap(tmp); // swap this and tmp
    }
} // ibis::array<T>::insert

/// Insert all values in [front, back) before p.  Nothing is done if front
/// and back does not define a valid range, or p is not between m_begin and
/// m_end.
template<class T> void
ibis::array_t<T>::insert(typename ibis::array_t<T>::iterator p,
                         typename ibis::array_t<T>::const_iterator front,
                         typename ibis::array_t<T>::const_iterator back) {
    if (back <= front || p < m_begin || p > m_end) return;
    const difference_type n = back - front;

    if (m_begin == 0 || m_end < m_begin) { // no space
        reserve(n);
        for (const_iterator j = front; j < back; ++ j, ++ m_end)
            *m_end = *j;
    }
    else if (actual != 0 && actual->filename() == 0 &&
             m_end+n <= (T*)(actual->end())) {
        // enough space, simply copy the values
        m_end += n;
        iterator i = m_end - 1;
        while (i >= p+n) {
            *i = i[-n]; --i;
        }
        for (--back; i >= p; --back, --i) {
            *i = *back;
        }
    }
    else {      // need new memory, copy-and-swap
        const difference_type nold = (m_begin > 0 && m_end > m_begin ?
                                      m_end - m_begin : 0);
        size_t nnew = static_cast<size_t>(nold + (nold>=n ? nold : n));
        if (nnew > 0x7FFFFFFFU) {
            throw "array_t must have less than 2^31 elements";
        }

        const size_t jp = p - m_begin;
        ibis::array_t<T> tmp(nnew);
        tmp.resize(nold+n);
        for (size_t j = 0; j < jp; ++ j)
            tmp[j] = m_begin[j];
        for (size_t j = 0; j < (size_t)n; ++ j)
            tmp[jp+j] = front[j];
        for (size_t j = jp; j < (nold>0?(size_t)nold:0U); ++ j)
            tmp[n+j] = m_begin[j];
        swap(tmp); // swap this and tmp
    }
} // ibis::array_t<T>::insert

/// Erase one element.
template<class T> typename ibis::array_t<T>::iterator
ibis::array_t<T>::erase(typename ibis::array_t<T>::iterator p) {
    LOGGER(actual->inUse() > 1 && ibis::gVerbose >= 0)
        << "Warning -- array_t<" << typeid(T).name()
        << ">::erase -- should not erase part of a shared array";

    if (p >= m_begin && p < m_end) {
        iterator i = p, j = p+1;
        while (j < m_end) {
            *i = *j;
            i = j; ++ j;
        }
        -- m_end;
    }
    else {
        p = m_end;
    }
    return p;
} // ibis::array_t<T>::earse

/// Erase a list of elements.
template<class T> typename ibis::array_t<T>::iterator
ibis::array_t<T>::erase(typename ibis::array_t<T>::iterator i,
                        typename ibis::array_t<T>::iterator j) {
    LOGGER(actual->inUse() > 1 && ibis::gVerbose >= 0)
        << "Warning -- array_t<" << typeid(T).name()
        << ">::erase -- should not erase part of a shared array";

    if (i >= j) {
        return m_begin;
    }
    else {
        if (i < m_begin) i = m_begin;
        if (j > m_end) j = m_end;
        iterator p;
        for (p=i; j<m_end; ++j, ++p)
            *p = *j;
        m_end = p;
        return i;
    }
} // ibis::array_t<T>::earse

/// Read an array from the name file.  This version may use either explicit
/// read or on-deman read (i.e. memory map) depending on the file size and
/// available file pointers.
template<class T>
void ibis::array_t<T>::read(const char* file) {
    if (file == 0 || *file == 0) return;
    freeMemory();
    int ierr = ibis::fileManager::instance().getFile(file, &actual);
    if (ierr == 0) {
        m_begin = (T*)(actual->begin());
        m_end = (T*)(actual->end());
        actual->beginUse();
    }
    else {
        LOGGER(ibis::gVerbose > 3)
            << "array_t<" << typeid(T).name()
            << ">::read(" << file << ") failed with ierr=" << ierr;
    }
} // ibis::array_t<T>::read

/// Read a portion of the named file.  This version always uses the
/// explicit read function to extract the content of file.
template<class T>
off_t ibis::array_t<T>::read(const char* fname, const off_t begin,
                             const off_t end) {
    if (fname == 0 || *fname == 0) return -2;
    off_t nread = actual->read(fname, begin, end);
    if (begin+nread == end) {
        m_begin = (T*)(actual->begin());
        m_end = (T*)(actual->begin()+nread);
    }
    else {
        LOGGER(ibis::gVerbose > 3)
            << "Warning -- array_t<" << typeid(T).name() << ">::read("
            << fname << ", " << begin << ", " << end << ") expected to read "
            << (end-begin) << " bytes, but acutally read " << nread;
    }
    return nread;
} // ibis::array_t<T>::read

/// Read an array from a file already open.
template<class T>
off_t ibis::array_t<T>::read(const int fdes, const off_t begin,
                             const off_t end) {
    if (fdes < 0) return -2;
    off_t nread = actual->read(fdes, begin, end);
    if (begin+nread == end) {
        m_begin = (T*)(actual->begin());
        m_end = (T*)(actual->begin()+nread);
    }
    else {
        LOGGER(ibis::gVerbose > 3)
            << "Warning -- array_t<" << typeid(T).name() << ">::read("
            << fdes << ", " << begin << ", " << end << ") expected to read "
            << (end-begin) << " bytes, but acutally read " << nread;
    }
    return nread;
} // ibis::array_t<T>::read

/// Write the content of array to the named file.
///
/// Returns 0 for success, a negative number for errors.
template<class T>
int ibis::array_t<T>::write(const char* file) const {
    if (m_end <= m_begin) return 0; // nothing to write

    off_t n, i;
    FILE *out = fopen(file, "wb");
    if (out == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "array_t<T>::write is unable open file \"" << file << "\" ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -1;
    }

    n = m_end - m_begin;
    i = fwrite(reinterpret_cast<void*>(m_begin), sizeof(T), n, out);
    fclose(out); // close the file
    if (i != n) {
        LOGGER(ibis::gVerbose >= 0)
            << "array_t<T>::write expects to write " << n << ' '
            << sizeof(T) << "-byte element" << (n>1?"s":"")
            << " to \"" << file << "\", but actually wrote " << i;
        return -2;
    }
    return 0;
} // ibis::array_t<T>::write

/// Write the content of the array to a file already opened.  The content
/// of the array is written out in binary.
template<class T>
int ibis::array_t<T>::write(FILE* fptr) const {
    if (fptr == 0) return -1;
    if (m_end <= m_begin) return 0;

    off_t n, i;
    n = m_end - m_begin;
    i = fwrite(reinterpret_cast<void*>(m_begin), sizeof(T), n, fptr);
    if (i != n) {
        LOGGER(ibis::gVerbose >= 0)
            << "array_t<T>::write() expects to write " << n << ' '
            << sizeof(T) << "-byte element" << (n>1?"s":"")
            << ", but actually wrote " << i;
        return -2;
    }
    return 0;
} // ibis::array_t<T>::write

/// Print internal pointer addresses.
template<class T>
void ibis::array_t<T>::printStatus(std::ostream &out) const {
    out << "array_t: m_begin = " << static_cast<void*>(m_begin)
        << ", m_end = " <<  static_cast<void*>(m_end) << ", size = "
        << m_end - m_begin << "\n";
#if defined(DEBUG) || defined(_DEBUG)
    if (actual != 0 && ibis::gVerbose > 6)
        actual->printStatus(out);
#else
    if (actual != 0 && ibis::gVerbose > 16)
        actual->printStatus(out);
#endif
} // ibis::array_t<T>::printStatus

/// Print out the content of the array to the given output stream.  The
/// elements are given on a single line separated by comas.
template<class T>
void ibis::array_t<T>::print(std::ostream &out) const {
    const_iterator it = m_begin;
    if (m_begin < m_end) {
        out << *it;
        for (++ it; it < m_end; ++ it)
            out << ", " << *it;
    }
    out << "\n";
} // ibis::array_t<T>::print

// explicit instantiation required and have to appear after the definitions
template class FASTBIT_CXX_DLLSPEC ibis::array_t<char>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<signed char>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<unsigned char>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<float>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<double>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<int16_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<int32_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<int64_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<uint16_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<uint32_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<uint64_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<ibis::rid_t>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<ibis::TYPE_T>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<void*>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<char*>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<const char*>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<ibis::bitvector*>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<ibis::bitvector const*>;
template class FASTBIT_CXX_DLLSPEC ibis::array_t<ibis::array_t<ibis::rid_t>*>;
