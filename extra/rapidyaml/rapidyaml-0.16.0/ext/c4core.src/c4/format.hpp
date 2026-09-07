#ifndef C4_FORMAT_HPP_
#define C4_FORMAT_HPP_

/** @file format.hpp provides type-safe facilities for formatting arguments
 * to string buffers */

#include "c4/charconv.hpp"
#include "c4/blob.hpp"


#if defined(_MSC_VER) && !defined(__clang__)
#   pragma warning(push)
#   if C4_MSVC_VERSION != C4_MSVC_VERSION_2017
#       pragma warning(disable: 4800) // forcing value to bool 'true' or 'false' (performance warning)
#   endif
#   pragma warning(disable: 4996) // snprintf/scanf: this function or variable may be unsafe
#elif defined(__clang__)
#   pragma clang diagnostic push
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,*avoid-goto*)

/** @defgroup doc_format_utils Format utilities
 *
 * @brief Provides generic and type-safe formatting/scanning utilities
 * built on top of @ref doc_to_chars() and @ref doc_from_chars,
 * forwarding the arguments to these functions, which in turn use the
 * @ref doc_charconv utilities. Like @ref doc_charconv, the formatting
 * facilities are very efficient and many times faster than printf().
 *
 * @see [a formatting sample in rapidyaml's docs](https://rapidyaml.readthedocs.io/latest/doxygen/group__doc__quickstart.html#gac2425b515eb552589708cfff70c52b14)
 * */

/** @defgroup doc_format_specifiers Format specifiers
 *
 * @brief Format specifiers are tag types and functions that are used
 * together with @ref doc_to_chars and @ref doc_from_chars
 *
 * @see [a formatting sample in rapidyaml's docs](https://rapidyaml.readthedocs.io/latest/doxygen/group__doc__quickstart.html#gac2425b515eb552589708cfff70c52b14)
 * @ingroup doc_format_utils */

namespace c4 {

/** @addtogroup doc_format_utils
 * @{ */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// formatting truthy types as booleans

namespace fmt {

/** @addtogroup doc_format_specifiers
 * @{ */

/** @defgroup doc_boolean_specifiers boolean specifiers
 * @{ */

struct boolalpha_
{
    bool val;
};

/** tag function to mark a variable to be written as an alphabetic
 * boolean, ie as either true or false */
template<class T>
boolalpha_ boolalpha(T const& val=false)
{
    return boolalpha_{val ? true : false};
}

/** @} */

/** @} */

} // namespace fmt

/** write a variable as an alphabetic boolean, ie as either true or
 * false
 * @ingroup doc_to_chars */
inline size_t to_chars(substr buf, fmt::boolalpha_ fmt)
{
    return to_chars(buf, fmt.val ? "true" : "false");
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// formatting integral types

namespace fmt {

/** @addtogroup doc_format_specifiers
 * @{ */

/** @defgroup doc_integer_specifiers Integer specifiers
 * @{ */

/** format an integral type with a custom radix */
template<typename T>
struct integral_
{
    C4_STATIC_ASSERT(std::is_integral<T>::value);
    T val;
    T radix;
    C4_ALWAYS_INLINE integral_(T val_, T radix_) : val(val_), radix(radix_) {}
};

/** format an integral type with a custom radix, and pad with zeroes on the left */
template<typename T>
struct integral_padded_
{
    C4_STATIC_ASSERT(std::is_integral<T>::value);
    T val;
    T radix;
    size_t num_digits;
    C4_ALWAYS_INLINE integral_padded_(T val_, T radix_, size_t nd) : val(val_), radix(radix_), num_digits(nd) {}
};


/** format an integral type with a custom radix */
template<class T>
C4_ALWAYS_INLINE integral_<T> integral(T val, T radix=10)
{
    return integral_<T>(val, radix);
}
/** format an integral type with a custom radix */
template<class T>
C4_ALWAYS_INLINE integral_<intptr_t> integral(T const* val, T radix=10)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(val), static_cast<intptr_t>(radix));
}
/** format an integral type with a custom radix */
template<class T>
C4_ALWAYS_INLINE integral_<intptr_t> integral(std::nullptr_t, T radix=10)
{
    return integral_<intptr_t>(intptr_t(0), static_cast<intptr_t>(radix));
}


/** format the pointer as an hexadecimal value */
template<class T>
inline integral_<intptr_t> hex(T * v)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(v), intptr_t(16));
}
/** format the pointer as an hexadecimal value */
template<class T>
inline integral_<intptr_t> hex(T const* v)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(v), intptr_t(16));
}
/** format null as an hexadecimal value
 * @overload hex */
inline integral_<intptr_t> hex(std::nullptr_t)
{
    return integral_<intptr_t>(0, intptr_t(16));
}
/** format the integral_ argument as an hexadecimal value
 * @overload hex */
template<class T>
inline integral_<T> hex(T v)
{
    return integral_<T>(v, T(16));
}

/** format the pointer as an octal value */
template<class T>
inline integral_<intptr_t> oct(T const* v)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(v), intptr_t(8));
}
/** format the pointer as an octal value */
template<class T>
inline integral_<intptr_t> oct(T * v)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(v), intptr_t(8));
}
/** format null as an octal value */
inline integral_<intptr_t> oct(std::nullptr_t)
{
    return integral_<intptr_t>(intptr_t(0), intptr_t(8));
}
/** format the integral_ argument as an octal value */
template<class T>
inline integral_<T> oct(T v)
{
    return integral_<T>(v, T(8));
}

/** format the pointer as a binary 0-1 value
 * @see c4::raw() if you want to use a binary memcpy instead of 0-1 formatting */
template<class T>
inline integral_<intptr_t> bin(T const* v)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(v), intptr_t(2));
}
/** format the pointer as a binary 0-1 value
 * @see c4::raw() if you want to use a binary memcpy instead of 0-1 formatting */
template<class T>
inline integral_<intptr_t> bin(T * v)
{
    return integral_<intptr_t>(reinterpret_cast<intptr_t>(v), intptr_t(2));
}
/** format null as a binary 0-1 value
 * @see c4::raw() if you want to use a binary memcpy instead of 0-1 formatting */
inline integral_<intptr_t> bin(std::nullptr_t)
{
    return integral_<intptr_t>(intptr_t(0), intptr_t(2));
}
/** format the integral_ argument as a binary 0-1 value
 * @see c4::raw() if you want to use a raw memcpy-based binary dump instead of 0-1 formatting */
template<class T>
inline integral_<T> bin(T v)
{
    return integral_<T>(v, T(2));
}

/** @} */ // integer_specifiers


/** @defgroup doc_zpad Pad the number with zeroes on the left
 * @{ */

/** pad the argument with zeroes on the left, with decimal radix */
template<class T>
C4_ALWAYS_INLINE integral_padded_<T> zpad(T val, size_t num_digits)
{
    return integral_padded_<T>(val, T(10), num_digits);
}
/** pad the argument with zeroes on the left */
template<class T>
C4_ALWAYS_INLINE integral_padded_<T> zpad(integral_<T> val, size_t num_digits)
{
    return integral_padded_<T>(val.val, val.radix, num_digits);
}
/** pad the argument with zeroes on the left */
C4_ALWAYS_INLINE integral_padded_<intptr_t> zpad(std::nullptr_t, size_t num_digits)
{
    return integral_padded_<intptr_t>(0, 16, num_digits);
}
/** pad the argument with zeroes on the left */
template<class T>
C4_ALWAYS_INLINE integral_padded_<intptr_t> zpad(T const* val, size_t num_digits)
{
    return integral_padded_<intptr_t>(reinterpret_cast<intptr_t>(val), 16, num_digits);
}
template<class T>
C4_ALWAYS_INLINE integral_padded_<intptr_t> zpad(T * val, size_t num_digits)
{
    return integral_padded_<intptr_t>(reinterpret_cast<intptr_t>(val), 16, num_digits);
}

/** @} */ // zpad


/** @defgroup doc_overflow_checked Check read for overflow
 * @{ */

template<class T>
struct overflow_checked_
{
    static_assert(std::is_integral<T>::value, "range checking only for integral types");
    C4_ALWAYS_INLINE overflow_checked_(T &val_) : val(&val_) {}
    T *val;
};
template<class T>
C4_ALWAYS_INLINE overflow_checked_<T> overflow_checked(T &val)
{
   return overflow_checked_<T>(val);
}

/** @} */ // overflow_checked

/** @} */ // format_specifiers


} // namespace fmt

/** format an integer signed type
 * @ingroup doc_to_chars */
template<typename T>
C4_ALWAYS_INLINE auto to_chars(substr buf, fmt::integral_<T> fmt)
    -> typename std::enable_if<std::is_signed<T>::value, size_t>::type
{
    return itoa(buf, fmt.val, fmt.radix);
}
/** format an integer signed type, pad with zeroes
 * @ingroup doc_to_chars */
template<typename T>
C4_ALWAYS_INLINE auto to_chars(substr buf, fmt::integral_padded_<T> fmt)
    -> typename std::enable_if<std::is_signed<T>::value, size_t>::type
{
    return itoa(buf, fmt.val, fmt.radix, fmt.num_digits);
}

/** format an integer unsigned type
 * @ingroup doc_to_chars */
template<typename T>
C4_ALWAYS_INLINE auto to_chars(substr buf, fmt::integral_<T> fmt)
    -> typename std::enable_if<std::is_unsigned<T>::value, size_t>::type
{
    return utoa(buf, fmt.val, fmt.radix);
}
/** format an integer unsigned type, pad with zeroes
 * @ingroup doc_to_chars */
template<typename T>
C4_ALWAYS_INLINE auto to_chars(substr buf, fmt::integral_padded_<T> fmt)
    -> typename std::enable_if<std::is_unsigned<T>::value, size_t>::type
{
    return utoa(buf, fmt.val, fmt.radix, fmt.num_digits);
}

/** read an integer type, detecting overflow (returns false on overflow)
 * @ingroup doc_from_chars */
template<class T>
C4_ALWAYS_INLINE bool from_chars(csubstr s, fmt::overflow_checked_<T> wrapper)
{
    if C4_LIKELY(!overflows<T>(s))
        return atox(s, wrapper.val);
    return false;
}
/** read an integer type, detecting overflow (returns false on overflow)
 * @ingroup doc_from_chars */
template<class T>
C4_ALWAYS_INLINE bool from_chars(csubstr s, fmt::overflow_checked_<T> *wrapper)
{
    if C4_LIKELY(!overflows<T>(s))
        return atox(s, wrapper->val);
    return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// formatting real types

namespace fmt {

/** @addtogroup doc_format_specifiers
 * @{ */

/** @defgroup doc_real_specifiers Real specifiers
 * @{ */

template<class T>
struct real_
{
    T val;
    int precision;
    RealFormat_e fmt;
    real_(T v, int prec=-1, RealFormat_e f=FTOA_FLOAT) : val(v), precision(prec), fmt(f)  {}
};

template<class T>
real_<T> real(T val, int precision, RealFormat_e fmt=FTOA_FLOAT)
{
    return real_<T>(val, precision, fmt);
}

/** @} */ // real_specifiers

/** @} */ // format_specifiers

} // namespace fmt

/** @ingroup doc_to_chars */
inline size_t to_chars(substr buf, fmt::real_< float> fmt) { return ftoa(buf, fmt.val, fmt.precision, fmt.fmt); }
/** @ingroup doc_to_chars */
inline size_t to_chars(substr buf, fmt::real_<double> fmt) { return dtoa(buf, fmt.val, fmt.precision, fmt.fmt); }


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// writing raw binary data

namespace fmt {

/** @addtogroup doc_format_specifiers
 * @{ */

/** @defgroup doc_raw_binary_specifiers Raw binary data
 * @{ */

/** @see blob_ */
template<class T>
struct raw_wrapper_ : public blob_<T>
{
    size_t alignment;

    C4_ALWAYS_INLINE raw_wrapper_(blob_<T> data, size_t alignment_) noexcept
        :
        blob_<T>(data),
        alignment(alignment_)
    {
        C4_ASSERT_MSG(alignment > 0 && (alignment & (alignment - 1)) == 0, "alignment must be a power of two");
    }
};

using const_raw_wrapper = raw_wrapper_<cbyte>;
using raw_wrapper = raw_wrapper_<byte>;

/** mark a variable to be written in raw binary format, using memcpy
 * @see blob_ */
inline const_raw_wrapper craw(cblob data, size_t alignment=alignof(max_align_t))
{
    return const_raw_wrapper(data, alignment);
}
/** mark a variable to be written in raw binary format, using memcpy
 * @see blob_ */
inline const_raw_wrapper raw(cblob data, size_t alignment=alignof(max_align_t))
{
    return const_raw_wrapper(data, alignment);
}
/** mark a variable to be written in raw binary format, using memcpy
 * @see blob_ */
template<class T>
inline const_raw_wrapper craw(T const& C4_RESTRICT data, size_t alignment=alignof(T))
{
    return const_raw_wrapper(cblob(data), alignment);
}
/** mark a variable to be written in raw binary format, using memcpy
 * @see blob_ */
template<class T>
inline const_raw_wrapper raw(T const& C4_RESTRICT data, size_t alignment=alignof(T))
{
    return const_raw_wrapper(cblob(data), alignment);
}

/** mark a variable to be read in raw binary format, using memcpy */
inline raw_wrapper raw(blob data, size_t alignment=alignof(max_align_t))
{
    return raw_wrapper(data, alignment);
}
/** mark a variable to be read in raw binary format, using memcpy */
template<class T>
inline raw_wrapper raw(T & C4_RESTRICT data, size_t alignment=alignof(T))
{
    return raw_wrapper(blob(data), alignment);
}

/** @} */ // raw_binary_specifiers

/** @} */ // format_specifiers

} // namespace fmt


/** write a variable in raw binary format, using memcpy
 * @ingroup doc_to_chars */
C4CORE_EXPORT size_t to_chars(substr buf, fmt::const_raw_wrapper r);

/** read a variable in raw binary format, using memcpy
 * @ingroup doc_from_chars */
C4CORE_EXPORT bool from_chars(csubstr buf, fmt::raw_wrapper *r);
/** read a variable in raw binary format, using memcpy
 * @ingroup doc_from_chars */
inline bool from_chars(csubstr buf, fmt::raw_wrapper r)
{
    return from_chars(buf, &r);
}

/** read a variable in raw binary format, using memcpy
 * @ingroup doc_from_chars_first */
inline size_t from_chars_first(csubstr buf, fmt::raw_wrapper *r)
{
    return from_chars(buf, r);
}
/** read a variable in raw binary format, using memcpy
 * @ingroup doc_from_chars_first */
inline size_t from_chars_first(csubstr buf, fmt::raw_wrapper r)
{
    return from_chars(buf, &r);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// formatting aligned to left/right/center

namespace fmt {

/** @addtogroup doc_format_specifiers
 * @{ */

/** @defgroup doc_alignment_specifiers Alignment specifiers
 * @{ */

template<class T> struct left_   { T val; size_t width; char padchar; C4_ALWAYS_INLINE left_  (T v, size_t w, char c) noexcept : val(v), width(w), padchar(c) {} };
template<class T> struct center_ { T val; size_t width; char padchar; C4_ALWAYS_INLINE center_(T v, size_t w, char c) noexcept : val(v), width(w), padchar(c) {} };
template<class T> struct right_  { T val; size_t width; char padchar; C4_ALWAYS_INLINE right_ (T v, size_t w, char c) noexcept : val(v), width(w), padchar(c) {} };

/** tag type to mark an argument to be aligned left.
 * @param val the argument to be aligned left
 * @param width the length of the field
 * @param padchar the padding character, defaults to ' '
 *
 * @note This function (and the return structure) uses value semantics. To
 * avoid copies on larger types, you can use std::cref(). For example:
 * @code{.cpp}
 * char buf[512];
 * std::string large_string = .....;
 * size_t len = cat(buf, fmt::left(std::cref(large_string), 100));
 * @endcode
 */
template<class T>
left_<T> left(T val, size_t width, char padchar=' ')
{
    return left_<T>(val, width, padchar);
}

/** tag function to mark an argument to be aligned center
 * @param val the argument to be aligned center
 * @param width the length of the field
 * @param padchar the padding character, defaults to ' '
 *
 * @note This function (and the return value) uses value semantics. To
 * avoid copies on larger types, you can use std::cref(). For example:
 * @code{.cpp}
 * char buf[512];
 * std::string large_string = .....;
 * size_t len = cat(buf, fmt::center(std::cref(large_string), 100));
 * @endcode
 */
template<class T>
center_<T> center(T val, size_t width, char padchar=' ')
{
    return center_<T>(val, width, padchar);
}

/** tag function to mark an argument to be aligned right
 * @param val the argument to be aligned right
 * @param width the length of the field
 * @param padchar the padding character, defaults to ' '
 *
 * @note This function (and the return value) uses value semantics. To
 * avoid copies on larger types, you can use std::cref(). For example:
 * @code{.cpp}
 * char buf[512];
 * std::string large_string = .....;
 * size_t len = cat(buf, fmt::right(std::cref(large_string), 100));
 * @endcode
 */
template<class T>
right_<T> right(T val, size_t width, char padchar=' ')
{
    return right_<T>(val, width, padchar);
}

/** @} */ // alignment_specifiers

/** @} */ // format_specifiers

} // namespace fmt


/** @ingroup doc_to_chars */
template<class T>
size_t to_chars(substr buf, fmt::left_<T> const& C4_RESTRICT align)
{
    size_t ret = to_chars(buf, align.val);
    if(ret >= buf.len || ret >= align.width)
        return ret > align.width ? ret : align.width;
    buf.first(align.width).sub(ret).fill(align.padchar);
    return align.width;
}

/** @ingroup doc_to_chars */
template<class T>
size_t to_chars(substr buf, fmt::right_<T> const& C4_RESTRICT align)
{
    size_t ret = to_chars(buf, align.val);
    if(ret >= buf.len || ret >= align.width)
        return ret > align.width ? ret : align.width;
    size_t rem = align.width - ret;
    if(ret)
        memmove(buf.str + rem, buf.str, ret);
    buf.first(rem).fill(align.padchar);
    return align.width;
}

/** @ingroup doc_to_chars */
template<class T>
size_t to_chars(substr buf, fmt::center_<T> const& C4_RESTRICT align)
{
    size_t ret = to_chars(buf, align.val);
    if(ret >= buf.len || ret >= align.width)
        return ret > align.width ? ret : align.width;
    size_t first = (align.width - ret) / 2u;
    if(ret)
        memmove(buf.str + first, buf.str, ret);
    buf.first(first).fill(align.padchar);
    buf.sub(first + ret).fill(align.padchar);
    return align.width;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @defgroup doc_cat cat: concatenate arguments to string
 * @{ */


/** @cond dev */
// terminates the variadic recursion
inline size_t cat(substr /*buf*/)
{
    return 0;
}
/** @endcond */


/** serialize the arguments, concatenating them to the given fixed-size buffer.
 * The buffer size is strictly respected: no writes will occur beyond its end.
 * @return the number of characters needed to write all the arguments into the buffer.
 * @see @ref c4::catrs() if instead of a fixed-size buffer, a resizeable container is desired
 * @see @ref c4::uncat() for the inverse function
 * @see @ref c4::catsep() if a separator between each argument is to be used
 * @see @ref c4::format() if a format string is desired
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class Arg, class... Args>
size_t cat(substr buf, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t num = to_chars(buf, a);
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num += cat(buf, more...);
    return num;
}

/** like @ref c4::cat() but return a substr instead of a size */
template<class... Args>
substr cat_sub(substr buf, Args const& C4_RESTRICT ...args)
{
    size_t sz = cat(buf, args...);
    C4_CHECK(sz <= buf.len);
    return {buf.str, sz <= buf.len ? sz : buf.len};
}

/** @} */


//-----------------------------------------------------------------------------


/** @defgroup doc_uncat uncat: read concatenated arguments from string
 * @{ */

/** @cond dev */
// terminates the variadic recursion
inline size_t uncat(csubstr /*buf*/)
{
    return 0;
}
/** @endcond */


/** deserialize the arguments from the given buffer.
 *
 * @return the number of characters read from the buffer, or csubstr::npos
 *   if a conversion was not successful.
 * @see @ref c4::cat(). @ref c4::uncat() is the inverse of @ref c4::cat(). */
template<class Arg, class... Args>
size_t uncat(csubstr buf, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    size_t out = from_chars_first(buf, &a);
    if C4_UNLIKELY(out == csubstr::npos)
        return csubstr::npos;
    buf  = buf.len >= out ? buf.sub(out) : substr{};
    size_t num = uncat(buf, more...);
    if C4_UNLIKELY(num == csubstr::npos)
        return csubstr::npos;
    return out + num;
}

/** @} */



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/** @defgroup doc_catsep catsep: cat arguments to string with separator
 * @{ */

/** @cond dev */
namespace detail {
template<class Sep>
C4_ALWAYS_INLINE size_t catsep_more(substr /*buf*/, Sep const& C4_RESTRICT /*sep*/)
{
    return 0;
}

template<class Sep, class Arg, class... Args>
size_t catsep_more(substr buf, Sep const& C4_RESTRICT sep, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t ret = to_chars(buf, sep);
    size_t num = ret;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = to_chars(buf, a);
    num += ret;
    buf  = buf.len >= ret ? buf.sub(ret) : substr{};
    ret  = catsep_more(buf, sep, more...);
    num += ret;
    return num;
}
} // namespace detail

// terminates the variadic recursion
template<class Sep>
size_t catsep(substr /*buf*/, Sep const& C4_RESTRICT /*sep*/)
{
    return 0;
}
/** @endcond */


/** serialize the arguments, concatenating them to the given fixed-size
 * buffer, using a separator between each argument.
 * The buffer size is strictly respected: no writes will occur beyond its end.
 * @return the number of characters needed to write all the arguments into the buffer.
 * @see @ref c4::catseprs() if instead of a fixed-size buffer, a resizeable container is desired
 * @see @ref c4::uncatsep() for the inverse function (ie, reading instead of writing)
 * @see @ref c4::cat() if no separator is needed
 * @see @ref c4::format() if a format string is desired
 *
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class Sep, class Arg, class... Args>
size_t catsep(substr buf, Sep const& C4_RESTRICT sep, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t num = to_chars(buf, a);
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num += detail::catsep_more(buf, sep, more...);
    return num;
}

/** like @ref c4::catsep() but return a substr instead of a size @see
 * @ref c4::catsep(). @ref c4::uncatsep() is the inverse of @ref
 * c4::catsep(). */
template<class... Args>
substr catsep_sub(substr buf, Args && ...args)
{
    size_t sz = catsep(buf, std::forward<Args>(args)...);
    C4_CHECK(sz <= buf.len);
    return {buf.str, sz <= buf.len ? sz : buf.len};
}

/** @} */



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @defgroup doc_uncatsep uncatsep: deserialize the separated arguments from a string
 * @{ */

/** @cond dev */
template<class Arg>
inline size_t uncatsep(csubstr buf, csubstr /*sep*/, Arg &C4_RESTRICT a)
{
    return from_chars(buf, &a) ? buf.len : csubstr::npos;
}
/** @endcond */

/** deserialize the arguments from the given buffer, using a separator.
 *
 * @return the number of characters read from the buffer, or csubstr::npos
 *   if a conversion was not successful
 *
 * @see c4::catsep(). @ref c4::uncatsep() is the inverse of @ref c4::catsep(). */
template<class Arg, class... Args>
size_t uncatsep(csubstr buf, csubstr sep, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    if C4_LIKELY(sep.len > 0)
    {
        size_t pos = buf.find(sep);
        if C4_LIKELY(pos != csubstr::npos)
        {
            if C4_LIKELY(from_chars(buf.first(pos), &a))
            {
                pos += sep.len;
                size_t num = uncatsep(buf.sub(pos), sep, more...);
                if C4_LIKELY(num != csubstr::npos)
                    return pos + num;
            }
        }
    }
    return csubstr::npos;
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @defgroup doc_format format: formatted string interpolation
 * @{ */

/// @cond dev
// terminates the variadic recursion
inline size_t format(substr buf, csubstr fmt)
{
    return to_chars(buf, fmt);
}
/// @endcond


/** Using a format string, serialize the arguments into the given
 * fixed-size buffer. The buffer size is strictly respected: no writes
 * will occur beyond its end. In the format string, each argument is
 * marked with a compact curly-bracket pair "{}". This pair does not
 * take any interior sequence numbers or extra formatting arguments
 * inside it (contrary to eg the C++20 std::format implementation or
 * the Python formatting facilities). To enable argument
 * customization, use the formatting facilities in @ref
 * doc_format_specifiers wrapping the arguments passed to this
 * function.
 *
 * @return the number of bytes needed to write into the buffer.
 *
 * @see @ref c4::formatrs() if instead of a fixed-size buffer, a resizeable container is desired
 * @see @ref c4::unformat() for the inverse function
 * @see @ref c4::cat() if no format or separator is needed
 * @see @ref c4::catsep() if no format is needed, but a separator must be used
 *
 * For example:
 * @code{.cpp}
 * c4::format(buf, "the {} drank {} {}", "partier", 5, "beers"); // the partier drank 5 beers
 * c4::format(buf, "the {} drank {} {}", "programmer", 6, "coffees"); // the programmer drank 6 coffees
 * @endcode
 *
 * Using @ref
 * doc_format_specifiers enables control of the result. For example:
 * @code{.cpp}
 * c4::format(buf, "the {} drank {} {}", "partier", c4::fmt::real(5, 3), "beers"); // the partier drank 5.000 beers
 * c4::format(buf, "the {} drank {} {}", "partier", c4::fmt::zpad(5, 3), "beers"); // the partier drank 005 beers
 * c4::format(buf, "the {} drank {} {}", "partier", c4::fmt::bin(5), "beers"); // the partier drank 0b101 beers
 * c4::format(buf, "the {} drank {} {}", "partier", c4::fmt::oct(5), "beers"); // the partier drank 0o6 beers
 * c4::format(buf, "the {} drank {} {}", "partier", c4::fmt::hex(5), "beers"); // the partier drank 0x6 beers
 * c4::format(buf, "the {} drank {} {}", "programmer", c4::fmt::real(6, 3), "coffees"); // the programmer drank 6.000 coffees
 * c4::format(buf, "the {} drank {} {}", "programmer", c4::fmt::zpad(6, 3), "coffees"); // the programmer drank 006 coffees
 * c4::format(buf, "the {} drank {} {}", "programmer", c4::fmt::bin(6), "coffees"); // the programmer drank 0b110 coffees
 * c4::format(buf, "the {} drank {} {}", "programmer", c4::fmt::oct(6), "coffees"); // the programmer drank 0o6 coffees
 * c4::format(buf, "the {} drank {} {}", "programmer", c4::fmt::hex(6), "coffees"); // the programmer drank 0x6 coffees
 * @endcode
 *
 * @note Arguments beyond the last curly bracket pair are silently
 * ignored. Curly bracket pairs without a corresponding argument are
 * printed as part of the result.
 * @code{.cpp}
 * // note "and nothing else" being ignored
 * c4::format(buf, "the {} drank {} {}", "partier", 5, "beers", "and nothing else"); // the partier drank 5 beers
 *
 * // note "this is ignored {}" being part of the result
 * c4::format(buf, "the {} drank {} {} this is ignored: {}", "programmer", 6, "coffees"); // the programmer drank 6 coffees this is ignored: {}
 * @endcode
 *
 * @note The curly bracket pair cannot be escaped, but can of course
 * be placed into the result by passing an "{}" argument in its place,
 * or if it is provided beyond the last argument passed to the
 * function (see prior note).
 * @code{.cpp}
 * // as above: no argument given, so no substitution made:
 * c4::format(buf, "let's show {} on the result"); // let's show {} on the result
 * // or just pass "{}" as an argument to force the substitution:
 * c4::format(buf, "let's show {} on the result and then {}", "{}", "this"); // let's show {} on the result and then this
 * @endcode
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class Arg, class... Args>
size_t format(substr buf, csubstr fmt, Arg const& C4_RESTRICT a, Args const& C4_RESTRICT ...more)
{
    size_t pos = fmt.find("{}");
    if C4_UNLIKELY(pos == csubstr::npos)
        return to_chars(buf, fmt);
    size_t num = to_chars(buf, fmt.first(pos));
    size_t out = num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = to_chars(buf, a);
    out += num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = format(buf, fmt.sub(pos + 2), more...);
    out += num;
    return out;
}

/** like @ref c4::format() but return a substr instead of a size
 * @see c4::format()
 * @see c4::catsep(). @ref c4::uncatsep() is the inverse of @ref c4::catsep(). */
template<class... Args>
substr format_sub(substr buf, csubstr fmt, Args const& C4_RESTRICT ...args)
{
    size_t sz = c4::format(buf, fmt, args...);
    C4_CHECK(sz <= buf.len);
    return {buf.str, sz <= buf.len ? sz : buf.len};
}

/** @} */


//-----------------------------------------------------------------------------

/** @defgroup doc_unformat unformat: formatted read from string
 * @{ */

/// @cond dev
// terminates the variadic recursion
inline size_t unformat(csubstr /*buf*/, csubstr fmt)
{
    return fmt.len;
}
/// @endcond


/** using a format string, deserialize the arguments from the given
 * buffer. This is the inverse function to @ref c4::format().
 *
 * @return the number of characters read from the buffer, or npos if a conversion failed.
 *
 * @see @ref c4::format(). */
template<class Arg, class... Args>
size_t unformat(csubstr buf, csubstr fmt, Arg & C4_RESTRICT a, Args & C4_RESTRICT ...more)
{
    const size_t pos = fmt.find("{}");
    if C4_UNLIKELY(pos == csubstr::npos)
        return unformat(buf, fmt);
    size_t num = pos;
    size_t out = num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = from_chars_first(buf, &a);
    if C4_UNLIKELY(num == csubstr::npos)
        return csubstr::npos;
    out += num;
    buf  = buf.len >= num ? buf.sub(num) : substr{};
    num  = unformat(buf, fmt.sub(pos + 2), more...);
    if C4_UNLIKELY(num == csubstr::npos)
        return csubstr::npos;
    out += num;
    return out;
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** cat+resize: like @ref c4::cat(), but receives a container, and
 * resizes it as needed to contain the result. The container is
 * overwritten. To append to it, use @ref c4::catrs_append().
 *
 * @see @ref c4::cat()
 * @see @ref c4::catrs_append()
 * @ingroup doc_cat
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class... Args>
inline void catrs(CharOwningContainer * C4_RESTRICT cont, Args const& C4_RESTRICT ...args)
{
    cont->resize(cont->capacity()); // improve the odds of fitting in the original buffer
retry:
    substr buf = to_substr(*cont);
    size_t ret = cat(buf, args...);
    cont->resize(ret);
    if(ret > buf.len)
        goto retry;
}

/** cat+resize: like @ref c4::cat(), but creates and returns a new
 * container sized as needed to contain the result.
 *
 * @see @ref c4::cat()
 * @ingroup doc_cat
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class... Args>
inline CharOwningContainer catrs(Args const& C4_RESTRICT ...args)
{
    CharOwningContainer cont;
    catrs(&cont, args...);
    return cont;
}

/** cat+resize+append: like @ref c4::cat(), but receives a container,
 * and appends to it instead of overwriting it. The container is
 * resized as needed to contain the result.
 *
 * @return the region newly appended to the original container
 * @see @ref c4::cat()
 * @see @ref c4::catrs()
 * @ingroup doc_cat
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class... Args>
inline csubstr catrs_append(CharOwningContainer * C4_RESTRICT cont, Args const& C4_RESTRICT ...args)
{
    const size_t pos = cont->size();
    cont->resize(cont->capacity()); // improve the odds of fitting in the original buffer
retry:
    substr buf = to_substr(*cont).sub(pos);
    size_t ret = cat(buf, args...);
    cont->resize(pos + ret);
    if(ret > buf.len)
        goto retry;
    return to_csubstr(*cont).range(pos, cont->size());
}


//-----------------------------------------------------------------------------

/** catsep+resize: like @ref c4::catsep(), but receives a container,
 * and resizes it as needed to contain the result.  The container is
 * overwritten. To append to the container use @ref
 * c4::catseprs_append().
 *
 * @see @ref c4::catsep()
 * @see @ref c4::catseprs_append()
 * @ingroup doc_catsep
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class Sep, class... Args>
inline void catseprs(CharOwningContainer * C4_RESTRICT cont, Sep const& C4_RESTRICT sep, Args const& C4_RESTRICT ...args)
{
    cont->resize(cont->capacity()); // improve the odds of fitting in the original buffer
retry:
    substr buf = to_substr(*cont);
    size_t ret = catsep(buf, sep, args...);
    cont->resize(ret);
    if(ret > buf.len)
        goto retry;
}

/** catsep+resize: like @ref c4::catsep(), but create a new container
 * with the result.
 *
 * @return the requested container
 * @ingroup doc_catsep
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class Sep, class... Args>
inline CharOwningContainer catseprs(Sep const& C4_RESTRICT sep, Args const& C4_RESTRICT ...args)
{
    CharOwningContainer cont;
    catseprs(&cont, sep, args...);
    return cont;
}


/** catsep+resize+append: like @ref c4::catsep(), but receives a
 * container, and appends the arguments, resizing the container as
 * needed to contain the result. The buffer is appended to.
 *
 * @return a csubstr of the appended part
 * @ingroup doc_catsep
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class Sep, class... Args>
inline csubstr catseprs_append(CharOwningContainer * C4_RESTRICT cont, Sep const& C4_RESTRICT sep, Args const& C4_RESTRICT ...args)
{
    const size_t pos = cont->size();
    cont->resize(cont->capacity()); // improve the odds of fitting in the original buffer
retry:
    substr buf = to_substr(*cont).sub(pos);
    size_t ret = catsep(buf, sep, args...);
    cont->resize(pos + ret);
    if(ret > buf.len)
        goto retry;
    return to_csubstr(*cont).range(pos, cont->size());
}


//-----------------------------------------------------------------------------

/** format+resize: like @ref c4::format(), but receives a container,
 * and resizes it as needed to contain the result.  The container is
 * overwritten. To append to the container use @ref
 * c4::formatrs_append().
 *
 * @see @ref c4::format()
 * @see @ref c4::formatrs_append()
 * @ingroup doc_format
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class... Args>
inline void formatrs(CharOwningContainer * C4_RESTRICT cont, csubstr fmt, Args const& C4_RESTRICT ...args)
{
    cont->resize(cont->capacity()); // improve the odds of fitting in the original buffer
retry:
    substr buf = to_substr(*cont);
    size_t ret = format(buf, fmt, args...);
    cont->resize(ret);
    if(ret > buf.len)
        goto retry;
}

/** format+resize: like @ref c4::format(), but create a new container
 * with the result.
 *
 * @return the requested container
 * @ingroup doc_format
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class... Args>
inline CharOwningContainer formatrs(csubstr fmt, Args const& C4_RESTRICT ...args)
{
    CharOwningContainer cont;
    formatrs(&cont, fmt, args...);
    return cont;
}

/** format+resize+append: like @ref c4::format(), but receives a
 * container, and appends the arguments, resizing the container as
 * needed to contain the result. The buffer is appended to.
 *
 * @return the region newly appended to the original container
 * @ingroup doc_format
 *
 * @note The arguments to format are restricted (legal because they
 * are rvalues). This may require a workaround when arguments of type
 * char[]/const char[] are passed repeatedly to the function. For
 * example,
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * cat(buf, str, str, str); // compile error: 'passing argument 2 to ‘restrict’-qualified parameter aliases with arguments 3, 4'
 * @endcode
 * It is possible to work around the problem by suppressing -Wrestrict
 * or by using the decayed type char* or const char*, or even wrapping
 * the argument in a csubstr():
 * @code{.cpp}
 * const char str[] = "Hi! ";
 * csubstr ss = to_csubstr(str);
 * cat(buf, ss, ss, ss); // ok! compiles cleanly
 * @endcode
 */
template<class CharOwningContainer, class... Args>
inline csubstr formatrs_append(CharOwningContainer * C4_RESTRICT cont, csubstr fmt, Args const& C4_RESTRICT ...args)
{
    const size_t pos = cont->size();
    C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Warray-bounds")
    cont->resize(cont->capacity()); // improve the odds of fitting in the original buffer
    C4_SUPPRESS_WARNING_GCC_POP
retry:
    substr buf = to_substr(*cont).sub(pos);
    size_t ret = format(buf, fmt, args...);
    cont->resize(pos + ret);
    if(ret > buf.len)
        goto retry;
    return to_csubstr(*cont).range(pos, cont->size());
}

/** @} */

} // namespace c4

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,*avoid-goto*)
#ifdef _MSC_VER
#   pragma warning(pop)
#elif defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#endif /* C4_FORMAT_HPP_ */
