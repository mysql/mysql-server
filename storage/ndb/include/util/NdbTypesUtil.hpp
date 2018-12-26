/*
  Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_TYPES_UTIL_HPP
#define NDB_TYPES_UTIL_HPP

#include <assert.h>
#include <string.h>


#include "ndb_constants.h"
#include "ndb_types.h"

/*
 * Traits and Helper classes for NDB data types.
 */

// ---------------------------------------------------------------------------
// Traits classes providing information on NDB (column) data types
// ---------------------------------------------------------------------------

/*
 * These Traits classes support code genericity by parametrizing over
 * NDB (column) data types.  They provide compile-time information on
 * - array types:               [long][var](binary|char)
 * - numeric types:             [U]Int8..[U]Int64|float|double
 *
 * For instance, Traits functions
 * - isFixedSized()             for array types
 * - lowest(), highest()        for numeric types
 * allow for the generic handling of arrays or numeric limits.
 *
 * Notes: the Traits classes
 * - provide uniform access to type meta-data
 * - are used as a type argument to a class or function template
 * - have pure compile-time scope, lack instantiation at runtime
 * - have _no_ link or library dependencies upon C++ stdlib code
 *   (compare to the bounds definitions in std::numeric_limits)
 * - are defined below as inline template specializations.
 */

/**
 * Common Traits of NDB array types.
 */
template< int TID > // the NDB type id as defined in ndb_constants.h
struct ArrayTypeTraits {
  // whether this array type is a binary or character type
  static bool isBinary();

  // whether this array type is fixed-or variable-sized
  static bool isFixedSized();

  // the size of the length prefix in bytes, or zero if a fixed-sized array
  static Uint32 lengthPrefixSize();
};

// aliases for array type traits
typedef ArrayTypeTraits< NDB_TYPE_CHAR > Tchar;
typedef ArrayTypeTraits< NDB_TYPE_BINARY > Tbinary;
typedef ArrayTypeTraits< NDB_TYPE_VARCHAR > Tvarchar;
typedef ArrayTypeTraits< NDB_TYPE_VARBINARY > Tvarbinary;
typedef ArrayTypeTraits< NDB_TYPE_LONGVARCHAR > Tlongvarchar;
typedef ArrayTypeTraits< NDB_TYPE_LONGVARBINARY > Tlongvarbinary;

// internal helper class
template< typename T >
struct NumTypeMap {};

/**
 * Common Traits of NDB numeric types.
 *
 * Notes: the C++ stdlib offers limits as part of std::numeric_limits;
 * its bounds definitions result in a non-uniform usage over different
 * data types, with min() referring to the smallest positive value for
 * float and double, but lowest negative value for integral types.
 * In contrast, this Traits class's functions lowest() and smallest()
 * support a uniform usage.
 */
template< typename T >
struct NumTypeTraits {
  // the domain type T
  typedef typename NumTypeMap< T >::DomainT DomainT;

  // if T is integral, the signed type of same width; otherwise T
  typedef typename NumTypeMap< T >::SignedT SignedT;

  // if T is integral, the unsigned type of same width; otherwise T
  typedef typename NumTypeMap< T >::UnsignedT UnsignedT;

  // whether the domain type is an integer type
  static bool isIntegral() { return NumTypeMap< T >::isIntegral(); };

  // whether the domain type is signed or unsigned
  static bool isSigned() { return NumTypeMap< T >::isSigned(); };

  // the width of the type in bytes
  static Uint32 size();

  // the minimum finite value
  static T lowest();

  // the maximum finite value
  static T highest();

  // the minimum positive normalized value, or 0 for integral types
  static T smallest();
};

// aliases for standard numeric type traits
typedef NumTypeTraits< Int8 > Tint8;
typedef NumTypeTraits< Int16 > Tint16;
typedef NumTypeTraits< Int32 > Tint32;
typedef NumTypeTraits< Int64 > Tint64;
typedef NumTypeTraits< Uint8 > Tuint8;
typedef NumTypeTraits< Uint16 > Tuint16;
typedef NumTypeTraits< Uint32 > Tuint32;
typedef NumTypeTraits< Uint64 > Tuint64;
// not implemented yet: float, double
// ansi C type 'long double' is not a supported numeric NDB type

/**
 * Common Traits of non-standard NDB numeric types.
 *
 * Unless distinct [U]Int24 value types are defined to represent these
 * proper subsets of [U]Int32 numbers, the correspoding Traits classes
 * need to be defined as separate types (not just mere specializations).
 * Using a derived class does that and allows to partially override.
 */
template< typename T >
struct NonStdNumTypeTraits : NumTypeTraits< T > {
  // the minimum finite value
  static T lowest();

  // the maximum finite value
  static T highest();
};

// aliases for standard numeric type traits
typedef NonStdNumTypeTraits< Int32 > Tint24;
typedef NonStdNumTypeTraits< Uint32 > Tuint24;

// ---------------------------------------------------------------------------
// Helper classes providing common functions on NDB (column) data
// ---------------------------------------------------------------------------

/*
 * These Helper classes provide basic utility functions on NDB types.
 *
 * For example, Helper functions
 * - read/writeLengthPrefix()   for array types
 * - load(), store()            for numeric types
 * allow to abstract from the details of writing an array's length prefix
 * or from reading/writing a numeric value from/to an unaligned buffer.
 *
 * Notes: the Helper classes
 * - extend Traits classes for convenience
 * - only add basic utility functions that
 * - have _no_ link or library dependencies upon MySQL code
 *   (in contrast to other SQL utility code like ./NdbSqlUtil)
 * - are defined below as inline template specializations.
 */

/**
 * Basic Helper functions for NDB array types.
 */
template< int ID >
struct ArrayTypeHelper : ArrayTypeTraits< ID > {
  // read the length prefix (not available if a fixed-sized array)
  static Uint32 readLengthPrefix(const void * a);

  // write the length prefix (not available if a fixed-sized array)
  // the non-length-prefix bytes of 'l' must be zero
  static void writeLengthPrefix(void * a, Uint32 l);
};

// aliases for array type helpers
typedef ArrayTypeHelper< NDB_TYPE_CHAR > Hchar;
typedef ArrayTypeHelper< NDB_TYPE_BINARY > Hbinary;
typedef ArrayTypeHelper< NDB_TYPE_VARCHAR > Hvarchar;
typedef ArrayTypeHelper< NDB_TYPE_VARBINARY > Hvarbinary;
typedef ArrayTypeHelper< NDB_TYPE_LONGVARCHAR > Hlongvarchar;
typedef ArrayTypeHelper< NDB_TYPE_LONGVARBINARY > Hlongvarbinary;

/**
 * Basic Helper functions for numeric NDB types.
 *
 * As another design option, these helper functions could be defined as
 * individual function templates, which'd allow for implicit function
 * resolution based on the parameter type but, on the other hand, required
 * distinct value types for all data (i.e., an Int24 value type).
 */
template< typename T >
struct NumTypeHelper : NumTypeTraits< T > {
  // convenience aliases
  typedef typename NumTypeTraits< T >::SignedT SignedT;
  typedef typename NumTypeTraits< T >::UnsignedT UnsignedT;

  // casts a value to the signed numerical type of same width
  static SignedT asSigned(T t) { return static_cast< SignedT >(t); }

  // casts a value to the unsigned numerical type of same width
  static UnsignedT asUnsigned(T t) { return static_cast< UnsignedT >(t); }

  // read a single value from an unaligned buffer; s, t must not overlap
  static void load(T * t, const char * s);

  // write a single value to an unaligned buffer; s, t must not overlap
  static void store(char * t, const T * s);
};

// aliases for numeric type helpers
typedef NumTypeHelper< Int8 > Hint8;
typedef NumTypeHelper< Int16 > Hint16;
typedef NumTypeHelper< Int32 > Hint32;
typedef NumTypeHelper< Int64 > Hint64;
typedef NumTypeHelper< Uint8 > Huint8;
typedef NumTypeHelper< Uint16 > Huint16;
typedef NumTypeHelper< Uint32 > Huint32;
typedef NumTypeHelper< Uint64 > Huint64;
// not implemented yet: float, double
// ansi C type 'long double' is not a supported numeric NDB type

/**
 * Basic Helper functions of non-standard NDB numeric types.
 *
 * Unless distinct [U]Int24 value types are defined to represent these
 * proper subsets of [U]Int32 numbers, the correspoding Helper classes
 * need to be defined as separate types (not just mere specializations).
 * This class only derives from the Traits class to avoid member access
 * ambiguities resulting from multiple inheritance.
 */
template< typename T >
struct NonStdNumTypeHelper : NonStdNumTypeTraits< T > {
  // convenience alias
  typedef typename NonStdNumTypeTraits< T >::SignedT SignedT;
  typedef typename NonStdNumTypeTraits< T >::UnsignedT UnsignedT;

  // casts a value to the signed numerical type of same width
  static SignedT asSigned(T t) { return static_cast< SignedT >(t); }

  // casts a value to the unsigned numerical type of same width
  static UnsignedT asUnsigned(T t) { return static_cast< UnsignedT >(t); }

  // read a single value from an unaligned buffer; s, t must not overlap
  static void load(T * t, const char * s);

  // write a single value to an unaligned buffer; s, t must not overlap
  static void store(char * t, const T * s);
};

// aliases for non-standard numeric type helpers
typedef NonStdNumTypeHelper< Int32 > Hint24;
typedef NonStdNumTypeHelper< Uint32 > Huint24;

// ---------------------------------------------------------------------------
// Definitions/Specializations of Traits classes
// ---------------------------------------------------------------------------

// specialize the Traits template members for array types
#define NDB_SPECIALIZE_ARRAY_TYPE_TRAITS( TR, B, FS, LPS )              \
  template<> inline bool TR::isBinary() { return B; }                   \
  template<> inline bool TR::isFixedSized() { return FS; }              \
  template<> inline Uint32 TR::lengthPrefixSize() { return LPS; }

// coincidentally, we could use ndb constants
//   NDB_ARRAYTYPE_FIXED, NDB_ARRAYTYPE_SHORT_VAR, NDB_ARRAYTYPE_MEDIUM_VAR
// instead of literals, but let's not confuse ordinal/cardinal numbers
NDB_SPECIALIZE_ARRAY_TYPE_TRAITS(Tchar, false, true, 0)
NDB_SPECIALIZE_ARRAY_TYPE_TRAITS(Tbinary, true, true, 0)
NDB_SPECIALIZE_ARRAY_TYPE_TRAITS(Tvarchar, false, false, 1)
NDB_SPECIALIZE_ARRAY_TYPE_TRAITS(Tvarbinary, true, false, 1)
NDB_SPECIALIZE_ARRAY_TYPE_TRAITS(Tlongvarchar, false, false, 2)
NDB_SPECIALIZE_ARRAY_TYPE_TRAITS(Tlongvarbinary, true, false, 2)
#undef NDB_SPECIALIZE_ARRAY_TYPE_TRAITS

// specialize the TypeMap template for numeric types
#define NDB_SPECIALIZE_NUM_TYPE_MAP( DT, ST, UT, I, S )                 \
  template<> struct NumTypeMap< DT > {                                  \
    typedef DT DomainT;                                                 \
    typedef ST SignedT;                                                 \
    typedef UT UnsignedT;                                               \
    static bool isIntegral() { return S; };                             \
    static bool isSigned() { return S; };                               \
  };

NDB_SPECIALIZE_NUM_TYPE_MAP(Int8, Int8, Uint8, true, true)
NDB_SPECIALIZE_NUM_TYPE_MAP(Uint8, Int8, Uint8, true, false)
NDB_SPECIALIZE_NUM_TYPE_MAP(Int16, Int16, Uint16, true, true)
NDB_SPECIALIZE_NUM_TYPE_MAP(Uint16, Int16, Uint16, true, false)
NDB_SPECIALIZE_NUM_TYPE_MAP(Int32, Int32, Uint32, true, true)
NDB_SPECIALIZE_NUM_TYPE_MAP(Uint32, Int32, Uint32, true, false)
NDB_SPECIALIZE_NUM_TYPE_MAP(Int64, Int64, Uint64, true, true)
NDB_SPECIALIZE_NUM_TYPE_MAP(Uint64, Int64, Uint64, true, false)

NDB_SPECIALIZE_NUM_TYPE_MAP(float, float, float, false, true)
NDB_SPECIALIZE_NUM_TYPE_MAP(double, double, double, false, true)
#undef NDB_SPECIALIZE_NUM_TYPE_MAP

// specialize the Traits template members for numeric types
#define NDB_SPECIALIZE_NUM_TYPE_TRAITS( TR, T, SZ, LO, HI, SM )         \
  template<> inline Uint32 TR::size() { return SZ; }                    \
  template<> inline T TR::lowest() { return LO; }                       \
  template<> inline T TR::highest() { return HI; }                      \
  template<> inline T TR::smallest() { return SM; }

NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tint8, Int8, 1, INT_MIN8, INT_MAX8, 0)
NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tint16, Int16, 2, INT_MIN16, INT_MAX16, 0)
NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tint32, Int32, 4, INT_MIN32, INT_MAX32, 0)
NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tint64, Int64, 8, INT_MIN64, INT_MAX64, 0)

NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tuint8, Uint8, 1, 0, UINT_MAX8, 0)
NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tuint16, Uint16, 2, 0, UINT_MAX16, 0)
NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tuint32, Uint32, 4, 0, UINT_MAX32, 0)
NDB_SPECIALIZE_NUM_TYPE_TRAITS(Tuint64, Uint64, 8, 0, UINT_MAX64, 0)
// not implemented yet: float, double
#undef NDB_SPECIALIZE_NUM_TYPE_TRAITS

// specialize the Traits template members for non-standard numeric types
#define NDB_SPECIALIZE_NON_STD_NUM_TYPE_TRAITS( TR, T, LO, HI )         \
  template<> inline T TR::lowest() { return LO; }                       \
  template<> inline T TR::highest() { return HI; }

NDB_SPECIALIZE_NON_STD_NUM_TYPE_TRAITS(Tint24, Int32, INT_MIN24, INT_MAX24)
NDB_SPECIALIZE_NON_STD_NUM_TYPE_TRAITS(Tuint24, Uint32, 0, UINT_MAX24)
#undef NDB_SPECIALIZE_NON_STD_NUM_TYPE_TRAITS

// ---------------------------------------------------------------------------
// Definitions/Specializations of Helper classes
// ---------------------------------------------------------------------------

// specialize the Helper template members for fixed-sized arrays
#define NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS0( H )                      \
  template<> inline Uint32 H::readLengthPrefix(const void * a) {        \
    assert(false);                                                      \
    (void)a;                                                            \
    return 0;                                                           \
  };                                                                    \
  template<> inline void H::writeLengthPrefix(void * a, Uint32 l) {     \
    assert(false);                                                      \
    (void)a; (void)l;                                                   \
  }

NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS0(Hchar)
NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS0(Hbinary)
#undef NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS0

// specialize the Helper template members for short-var arrays
#define NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS1( H )                      \
  template<> inline Uint32 H::readLengthPrefix(const void * a) {        \
    assert(a);                                                          \
    const Uint8 * s = static_cast<const Uint8 *>(a);                    \
    return s[0];                                                        \
  };                                                                    \
  template<> inline void H::writeLengthPrefix(void * a, Uint32 l) {     \
    assert(a);                                                          \
    assert(l >> (lengthPrefixSize() * 8) == 0);                         \
    Uint8 * t = static_cast<Uint8 *>(a);                                \
    t[0] = l & 0x000000FF;                                              \
  }

NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS1(Hvarchar)
NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS1(Hvarbinary)
#undef NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS1

// specialize the Helper template members for medium-var arrays
#define NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS2( H )                      \
  template<> inline Uint32 H::readLengthPrefix(const void * a) {        \
    assert(a);                                                          \
    const Uint8 * s = static_cast<const Uint8 *>(a);                    \
    return static_cast<Uint32>(s[0] + (s[1] << 8));                     \
  };                                                                    \
  template<> inline void H::writeLengthPrefix(void * a, Uint32 l) {     \
    assert(a);                                                          \
    assert(l >> (lengthPrefixSize() * 8) == 0);                         \
    Uint8 * t = static_cast<Uint8 *>(a);                                \
    t[0] = static_cast<Uint8>(l & 0x000000FF);                          \
    t[1] = static_cast<Uint8>((l & 0x0000FF00) >> 8);                   \
  }

NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS2(Hlongvarchar)
NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS2(Hlongvarbinary)
#undef NDB_SPECIALIZE_ARRAY_TYPE_HELPER_LPS2

// specialize the Helper template members for single-byte types
#define NDB_SPECIALIZE_NUM_TYPE_HELPER_BYTE( H, T )                     \
  template<> inline void H::load(T * t, const char * s) {               \
    assert(t); assert(s); assert(t != (const T *)s);                    \
    *t = static_cast<T>(*s);                                            \
  }                                                                     \
  template<> inline void H::store(char * t, const T * s) {              \
    H::load(reinterpret_cast<T *>(t),                                   \
            reinterpret_cast<const char *>(s));                         \
  }

NDB_SPECIALIZE_NUM_TYPE_HELPER_BYTE(Hint8, Int8);
NDB_SPECIALIZE_NUM_TYPE_HELPER_BYTE(Huint8, Uint8);
#undef NDB_SPECIALIZE_NUM_TYPE_HELPER_BYTE

// specialize the Helper template members for numeric types
#define NDB_SPECIALIZE_NUM_TYPE_HELPER( H, T )                          \
  template<> inline void H::load(T * t, const char * s) {               \
    assert(t); assert(s); assert(t != (const T *)s);                    \
    memcpy(t, s, H::size());                                            \
  }                                                                     \
  template<> inline void H::store(char * t, const T * s) {              \
    H::load(reinterpret_cast<T *>(t),                                   \
            reinterpret_cast<const char *>(s));                         \
  }

NDB_SPECIALIZE_NUM_TYPE_HELPER(Hint16, Int16);
NDB_SPECIALIZE_NUM_TYPE_HELPER(Hint32, Int32);
NDB_SPECIALIZE_NUM_TYPE_HELPER(Hint64, Int64);

NDB_SPECIALIZE_NUM_TYPE_HELPER(Huint16, Uint16);
NDB_SPECIALIZE_NUM_TYPE_HELPER(Huint32, Uint32);
NDB_SPECIALIZE_NUM_TYPE_HELPER(Huint64, Uint64);
// not implemented yet: float, double
#undef NDB_SPECIALIZE_NUM_TYPE_HELPER

// specialize the Helper template members for non-standard numeric types
#define NDB_SPECIALIZE_NON_STD_NUM_TYPE_HELPER( H, T, INT3KORR )        \
  template<> inline void H::load(T * t, const char * s) {               \
    assert(t); assert(s); assert(t != (const T *)s);                    \
    *t = (INT3KORR(s));                                                 \
  }                                                                     \
  template<> inline void H::store(char * t, const T * s) {              \
    assert(t); assert(s); assert((const T *)t != s);                    \
    int3store(t, (*s));                                                 \
  }

NDB_SPECIALIZE_NON_STD_NUM_TYPE_HELPER(Hint24, Int32, sint3korr)
NDB_SPECIALIZE_NON_STD_NUM_TYPE_HELPER(Huint24, Uint32, uint3korr)
#undef NDB_SPECIALIZE_NON_STD_NUM_TYPE_HELPER

#endif /* !NDB_TYPES_UTIL_HPP */
