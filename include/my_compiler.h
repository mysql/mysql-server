#ifndef MY_COMPILER_INCLUDED
#define MY_COMPILER_INCLUDED

/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  Header for compiler-dependent features.

  Intended to contain a set of reusable wrappers for preprocessor
  macros, attributes, pragmas, and any other features that are
  specific to a target compiler.
*/

#if defined __GNUC__
/*
  Convenience macro to test the minimum required GCC version.
  These should be used with care as Clang also sets __GNUC__ and
  __GNUC_MINOR__ (currently to 4.2). Prefer using feature specific
  CMake checks in configure.cmake instead.
*/
#  define MY_GNUC_PREREQ(maj, min) \
    ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#else
#  define MY_GNUC_PREREQ(maj, min) (0)
#endif

/* Comunicate to the compiler the unreachability of the code. */
#ifdef HAVE_BUILTIN_UNREACHABLE
#  define MY_ASSERT_UNREACHABLE()   __builtin_unreachable()
#else
#  define MY_ASSERT_UNREACHABLE()  do { assert(0); } while (0)
#endif

#if defined __GNUC__ || defined __SUNPRO_C || defined __SUNPRO_CC
/* Specifies the minimum alignment of a type. */
# define MY_ALIGNOF(type)   __alignof__(type)
/* Determine the alignment requirement of a type. */
# define MY_ALIGNED(n)      __attribute__((__aligned__((n))))
/* Microsoft Visual C++ */
#elif defined _MSC_VER
# define MY_ALIGNOF(type)   __alignof(type)
# define MY_ALIGNED(n)      __declspec(align(n))
#else /* Make sure they are defined for other compilers. */
# define MY_ALIGNOF(type)
# define MY_ALIGNED(size)
#endif


/**
  C++ Type Traits
*/
#ifdef __cplusplus

/**
  Opaque storage with a particular alignment.
  Partial specialization used due to MSVC++.
*/
template<size_t alignment> struct my_alignment_imp;
template<> struct MY_ALIGNED(1) my_alignment_imp<1> {};
template<> struct MY_ALIGNED(2) my_alignment_imp<2> {};
template<> struct MY_ALIGNED(4) my_alignment_imp<4> {};
template<> struct MY_ALIGNED(8) my_alignment_imp<8> {};
template<> struct MY_ALIGNED(16) my_alignment_imp<16> {};

/**
  A POD type with a given size and alignment.

  @remark If the compiler does not support a alignment attribute
          (MY_ALIGN macro), the default alignment of a double is
          used instead.

  @tparam size        The minimum size.
  @tparam alignment   The desired alignment: 1, 2, 4, 8 or 16.
*/
template <size_t size, size_t alignment>
struct my_aligned_storage
{
  union
  {
    char data[size];
    my_alignment_imp<alignment> align;
  };
};

#endif /* __cplusplus */

/*
  Disable __attribute__ for Sun Studio and Visual Studio.
  Note that Sun Studio supports some __attribute__ variants,
  but not format or unused which we use quite a lot.

  Sic: We should not (re-)define identifiers that begin with
  an underscore followed by an uppercase letter or another underscore.
*/
#if defined __SUNPRO_C || defined __SUNPRO_CC || defined _MSC_VER
# define __attribute__(A)
#endif

#endif /* MY_COMPILER_INCLUDED */
