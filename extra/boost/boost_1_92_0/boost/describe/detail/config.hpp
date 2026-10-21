#ifndef BOOST_DESCRIBE_DETAIL_CONFIG_HPP_INCLUDED
#define BOOST_DESCRIBE_DETAIL_CONFIG_HPP_INCLUDED

// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if __cplusplus >= 201402L

# define BOOST_DESCRIBE_CXX14
# define BOOST_DESCRIBE_CXX11

#elif defined(_MSC_VER) && _MSC_VER >= 1900

# define BOOST_DESCRIBE_CXX14
# define BOOST_DESCRIBE_CXX11

#elif __cplusplus >= 201103L

# define BOOST_DESCRIBE_CXX11

# if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ == 7
#  undef BOOST_DESCRIBE_CXX11
# endif

#endif

#if defined(BOOST_DESCRIBE_CXX11)
# define BOOST_DESCRIBE_CONSTEXPR_OR_CONST constexpr
#else
# define BOOST_DESCRIBE_CONSTEXPR_OR_CONST const
#endif

#if defined(__clang__)
# define BOOST_DESCRIBE_MAYBE_UNUSED __attribute__((unused))
#else
# define BOOST_DESCRIBE_MAYBE_UNUSED
#endif

#if defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L
# define BOOST_DESCRIBE_INLINE_VARIABLE inline
#else
# define BOOST_DESCRIBE_INLINE_VARIABLE
#endif

#define BOOST_DESCRIBE_INLINE_CONSTEXPR BOOST_DESCRIBE_INLINE_VARIABLE BOOST_DESCRIBE_CONSTEXPR_OR_CONST

#if __cplusplus >= 202002L || ( defined(_MSVC_LANG) && _MSVC_LANG >= 202002L )

# define BOOST_DESCRIBE_CXX20

// Clang 13.0 is needed for unevaluated lambdas
# if defined(__clang__) && __clang_major__ < 13
#  undef BOOST_DESCRIBE_CXX20
# endif

// Apple Clang 13.1 is Clang 13.0
# if defined(__clang__) && defined(__apple_build_version__) && __clang_major__ == 13 && __clang_minor__ < 1
#  undef BOOST_DESCRIBE_CXX20
# endif

#endif

#endif // #ifndef BOOST_DESCRIBE_DETAIL_CONFIG_HPP_INCLUDED
