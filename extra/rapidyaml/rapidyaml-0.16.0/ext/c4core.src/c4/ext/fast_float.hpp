#ifndef C4_EXT_FAST_FLOAT_HPP_
#define C4_EXT_FAST_FLOAT_HPP_

#if defined(_MSC_VER) && !defined(__clang__)
#   pragma warning(push)
#   pragma warning(disable: 4127) // conditional expression is constant
#   pragma warning(disable: 4365) // '=': conversion from 'const _Ty' to 'fast_float::limb', signed/unsigned mismatch
#   pragma warning(disable: 4996) // snprintf/scanf: this function or variable may be unsafe
#elif defined(__clang__) || defined(__APPLE_CC__) || defined(_LIBCPP_VERSION)
#   pragma clang diagnostic push
#   if (defined(__clang_major__) && (__clang_major__ >= 9)) || defined(__APPLE_CC__)
#       pragma clang diagnostic ignored "-Wfortify-source"
#   endif
#   pragma clang diagnostic ignored "-Wshift-count-overflow"
#   pragma clang diagnostic ignored "-Wold-style-cast"
#   pragma clang diagnostic ignored "-Wpadded"
#   pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#   if (defined(__clang_major__) && (__clang_major__ >= 21))
#       pragma clang diagnostic ignored "-Wnrvo"
#   endif
#   if (defined(__clang_major__) && (__clang_major__ >= 13))
#       pragma clang diagnostic ignored "-Wreserved-identifier"
#   endif
#   if (defined(__clang_major__) && (__clang_major__ >= 8))
#       pragma clang diagnostic ignored "-Wextra-semi-stmt"
#   endif
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wnarrowing"
#   pragma GCC diagnostic ignored "-Wconversion"
#   pragma GCC diagnostic ignored "-Wsign-conversion"
#   pragma GCC diagnostic ignored "-Wuseless-cast"
#   pragma GCC diagnostic ignored "-Wold-style-cast"
#   pragma GCC diagnostic ignored "-Warray-bounds"
#   if __GNUC__ >= 5
#       pragma GCC diagnostic ignored "-Wshift-count-overflow"
#   endif
#   if __GNUC__ >= 6
#       pragma GCC diagnostic ignored "-Wnull-dereference"
#   endif
#endif

#ifndef C4CORE_SYS_FAST_FLOAT
#include "c4/ext/fast_float_all.h"
#else
#include <fast_float/fast_float.h>
#endif

#ifdef _MSC_VER
#   pragma warning(pop)
#elif defined(__clang__) || defined(__APPLE_CC__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#endif // C4_EXT_FAST_FLOAT_HPP_
