//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_DICTIONARY_H
#define IBIS_DICTIONARY_H
///@file
/// Define a dictionary data structure used by ibis::category.
#include "array_t.h"
#if defined(HAVE_UNORDERED_MAP)
#include <unordered_map>
#elif defined(__GNUC__) && (__GNUC__+0 <= 4 || ( __GNUC__+0 == 4 && __GNUC_MINOR__+0 <= 5))
#include <backward/hash_map>
#else
#include <unordered_map>
#endif

namespace std {
    // specialization of hash<> on const char*
    template <> struct hash< const char* > {
	size_t operator()(const char* x) const;
    };
}

/// Provide a dual-directional mapping between strings and integers.  A
/// utility class used by ibis::category.  The integer values are always
/// treated as 32-bit unsigned integers.  The NULL string is always mapped
/// to 0xFFFFFFFF (-1U) and is NOT counted as an entry in a dictionary.
///
/// This version uses an in-memory hash_map to provide a mapping from a
/// string to an integer.
///
/// @note The integer returned from this class is a unsigned 32-bit integer
/// (uint32_t).  This limits the size of the dictionary to be no more than
/// 2^32 entries.  The dictionary file is written with 64-bit internal
/// pointers.  However, since the dictionary has to be read into memory
/// completely before any use, the size of a dictionary is generally
/// limited by the size of the computer memory.
///
/// @note If FASTBIT_CASE_SENSITIVE_COMPARE is defined to be 0, the values
/// stored in a dictionary will be folded to the upper case.  This will
/// allow the words in the dictionary to be stored in a simple sorted
/// order.  By default, the dictionary is case sensitive.
class FASTBIT_CXX_DLLSPEC ibis::dictionary {
public:
    ~dictionary() {clear();}
    dictionary();
    dictionary(const dictionary& dic);

    /// Return the number of entries in the dictionary.  May have undefined
    /// entries.
    uint32_t size() const {return raw_.size();}

    const char* operator[](uint32_t i) const;
    uint32_t operator[](const char* str) const;
    const char* find(const char* str) const;
    void patternSearch(const char* pat, array_t<uint32_t>& matches) const;

    uint32_t insert(const char*, uint32_t);
    uint32_t insert(const char*);
    uint32_t insertRaw(char*);
    uint32_t appendOrdered(const char*);

    void clear();
    void swap(dictionary&);

    int  read(const char*);
    int  write(const char*) const;

    int  fromASCII(std::istream &);
    void toASCII(std::ostream &) const;

    void sort(array_t<uint32_t>&);
    int  merge(const dictionary&);
    int  morph(const dictionary&, array_t<uint32_t>&) const;

    bool equal_to(const ibis::dictionary&) const;

    void copy(const dictionary& rhs);

protected:

    /// Member variable raw_ contains the string values in the order of the
    /// code assignment.
    array_t<const char*> raw_;
    /// Member varaible buffer_ contains a list of pointers to the memory
    /// that holds the strings.
    array_t<char*> buffer_;
    /// Member variable key_ contains the hash_map that connects a string
    /// value to an integer.
    typedef
#if defined(HAVE_UNORDERED_MAP)
        std::unordered_map
#elif defined(__GNUC__) && (__GNUC__+0 <= 4 || ( __GNUC__+0 == 4 && __GNUC_MINOR__+0 <= 5))
        __gnu_cxx::hash_map
#else
        std::unordered_map
#endif
        <const char*, uint32_t, std::hash<const char*>,
         std::equal_to<const char*> > MYMAP;
    MYMAP key_;

    int  readRaw(const char*, FILE *);
    int  readKeys0(const char*, FILE *);
    int  readKeys1(const char*, FILE *);
    int  readKeys2(const char*, FILE *);
    void mergeBuffers() const;
    int  writeKeys(FILE*, uint32_t, array_t<uint64_t>&,
                   array_t<uint32_t>&) const;
    int  writeBuffer(FILE*, uint32_t, array_t<uint64_t>&,
                     array_t<uint32_t>&) const;

private:
    dictionary& operator=(const dictionary&);
}; // ibis::dictionary

/// Swap the content of two dictionaries.
inline void ibis::dictionary::swap(ibis::dictionary& rhs) {
    raw_.swap(rhs.raw_);
    key_.swap(rhs.key_);
    buffer_.swap(rhs.buffer_);
} // ibis::dictionary::swap

/// Return a string corresponding to the integer.  If the index is beyond
/// the valid range, i.e., i > size(), then a null pointer will be
/// returned.
inline const char* ibis::dictionary::operator[](uint32_t i) const {
    return (i < raw_.size() ? raw_[i] : static_cast<const char*>(0));
} // int to string

/// Find the given string in the dictionary.  If the input string is found
/// in the dictionary, it returns the string.  Otherwise it returns null
/// pointer.  This function makes a little easier to determine whether a
/// string is in a dictionary.
inline const char* ibis::dictionary::find(const char* str) const {
    const char* ret = 0;
    const uint32_t ind = operator[](str);
    if (ind < raw_.size())
	ret = raw_[ind];
    return ret;
} // ibis::dictionary::find
#endif // IBIS_DICTIONARY_H
