#ifndef C4_YML_SCALAR_CHARCONV_HPP_
#define C4_YML_SCALAR_CHARCONV_HPP_

/** @file scalar_charconv.hpp */

#ifndef C4_YML_COMMON_HPP_
#include "c4/yml/common.hpp"
#endif
#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif
#ifndef C4_CHARCONV_HPP_
#include <c4/charconv.hpp>
#endif

namespace c4 {
namespace yml {


/** @addtogroup doc_scalar_charconv
 *
 * @{
 */

/** YAML-sense query of nullity. returns true if the scalar points
 * to `nullptr` or is otherwise equal to one of the strings
 * `"~"`,`"null"`,`"Null"`,`"NULL"` */
inline bool scalar_is_null(csubstr s) noexcept
{
    return s.str == nullptr ||
        (s.len == 1 && (s.str[0] == '~')) ||
        (s.len == 4 && ((0 == memcmp("null", s.str, 4))
                        || (0 == memcmp("Null", s.str, 4))
                        || (0 == memcmp("NULL", s.str, 4))));
}


/** JSON-sense query of plain number */
inline bool scalar_is_plain_number_json(csubstr s) noexcept
{
    return s.is_number()
        &&
        (
            // quote integral numbers if they have a leading 0
            // https://github.com/biojppm/rapidyaml/issues/291
            (!(s.len > 1 && s.begins_with('0')))
            // do not quote reals with leading 0
            // https://github.com/biojppm/rapidyaml/issues/313
            || (s.find('.') != csubstr::npos)
        );
}


/** Query if a scalar is inf (inf, Inf, INF)
 * @warning length must be 3
 */
inline bool scalar_is_inf3(const char *C4_RESTRICT s) noexcept
{
    switch(s[0])
    {
    case 'i': return ((s[1] == 'n') && (s[2] == 'f'));
    case 'I': return ((s[1] == 'n') && (s[2] == 'f')) ||
                     ((s[1] == 'N') && (s[2] == 'F'));
    }
    return false;
}


/** Query if a scalar is nan (nan, NaN, Nan, NAN)
 * @warning length must be 3
 */
inline bool scalar_is_nan3(const char *C4_RESTRICT s) noexcept
{
    switch(s[0])
    {
    case 'n': return ((s[1] == 'a') && (s[2] == 'n'));
    case 'N': return ((s[1] == 'a') && (s[2] == 'N')) ||
                     ((s[1] == 'a') && (s[2] == 'n')) ||
                     ((s[1] == 'A') && (s[2] == 'N'));
    }
    return false;
}


/** Same as scalar_is_inf3() || scalar_is_nan3()
 * @warning length must be 3
 */
inline bool scalar_is_inf_or_nan3(const char *C4_RESTRICT s) noexcept
{
    switch(s[0])
    {
    case 'i': return ((s[1] == 'n') && (s[2] == 'f'));
    case 'I': return ((s[1] == 'n') && (s[2] == 'f')) ||
                     ((s[1] == 'N') && (s[2] == 'F'));
    case 'n': return ((s[1] == 'a') && (s[2] == 'n'));
    case 'N': return ((s[1] == 'a') && (s[2] == 'N')) ||
                     ((s[1] == 'a') && (s[2] == 'n')) ||
                     ((s[1] == 'A') && (s[2] == 'N'));
    }
    return false;
}


/** Query if a scalar is plain, eg, true, false, null, +-.inf or .nan */
inline bool scalar_is_special_json(csubstr s)  noexcept
{
    if(s.len == 4)
        return 0 == memcmp("true", s.str, 4)
            || 0 == memcmp("null", s.str, 4)
            || ((s[0] == '.') && scalar_is_inf_or_nan3(s.str + 1))
            || ((s[0] == '-' || s[0] == '+') && scalar_is_inf3(s.str + 1));
    else if(s.len == 5)
        return 0 == memcmp("false", s.str, 5)
            || ((s[0] == '-' || s[0] == '+') && s[1] == '.' && scalar_is_inf3(s.str + 2));
    else if(s.len == 3)
        return scalar_is_inf_or_nan3(s.str);
    return false;
}


//-----------------------------------------------------------------------------

/** @cond dev */
namespace detail {
template<class T>
C4_NODISCARD C4_NO_INLINE bool from_chars_float_yaml_special(csubstr buf, T *C4_RESTRICT val) RYML_NOEXCEPT
{
    static_assert(std::is_floating_point<T>::value, "must be floating point");
    RYML_ASSERT_BASIC_(buf.len);
    RYML_ASSERT_BASIC_(!buf.begins_with('+'));
    switch(buf.str[0])
    {
    case '.':
        if(buf.len == 4)
        {
            if(scalar_is_nan3(buf.str + 1))
            {
                *val = std::numeric_limits<T>::quiet_NaN();
                goto ok; // NOLINT
            }
            else if(scalar_is_inf3(buf.str + 1))
            {
                *val = std::numeric_limits<T>::infinity();
                goto ok; // NOLINT
            }
        }
        break;
    case '-':
        if(buf.len == 5 && buf.str[1] == '.' && scalar_is_inf3(buf.str + 2))
        {
            *val = -std::numeric_limits<T>::infinity();
            goto ok; // NOLINT
        }
    }
    return false;
ok:
    return true;
}
} // namespace detail
/** @endcond */


/** Deserialize a floating point from string. Accepts special values:
 * .nan, .inf, -.inf, with upper-case variations.
 *
 * Unlike non-floating types, only the leading part of the string that
 * may constitute a number is processed. This happens because the
 * float parsing is delegated to fast_float, which is implemented that
 * way. Consequently, for example, all of `"34"`, `"34 "` `"34hg"`
 * `"34 gh"` will be read as 34. If you are not sure about the
 * contents of the data, you can use @ref csubstr::first_real_span() to
 * check before deserializing, for example like this:
 *
 * @code{c++}
 * csubstr val = node.val();
 * if(val.first_real_span() == val)
 *     node >> v; // or call from_chars_float(val, &v);
 * else
 *     ERROR("not a real")
 * @endcode
 */
template<class T>
C4_NODISCARD bool from_chars_float(csubstr scalar, T *C4_RESTRICT val) RYML_NOEXCEPT
{
    static_assert(std::is_floating_point<T>::value, "must be floating point");
    if C4_LIKELY(scalar.len > 0)
    {
        if(scalar.str[0] == '+')
            scalar = scalar.sub(1);
        if C4_LIKELY(from_chars(scalar, val))
            return true;
        else if(scalar.len == 4 || scalar.len == 5)
            return detail::from_chars_float_yaml_special(scalar, val);
    }
    return false;
}


/** Serialize a floating point value to a string.
 */
template<class T>
size_t to_chars_float(substr buf, T val) RYML_NOEXCEPT
{
    static_assert(std::is_floating_point<T>::value, "must be floating point");
    C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wfloat-equal");
    if C4_UNLIKELY(std::isnan(val))
        return to_chars(buf, csubstr(".nan"));
    else if C4_UNLIKELY(val == std::numeric_limits<T>::infinity())
        return to_chars(buf, csubstr(".inf"));
    else if C4_UNLIKELY(val == -std::numeric_limits<T>::infinity())
        return to_chars(buf, csubstr("-.inf"));
    return to_chars(buf, val);
    C4_SUPPRESS_WARNING_GCC_CLANG_POP
}


//-----------------------------------------------------------------------------

/** Deserialize an integral scalar. Trims leading `+`, and then
 * dispatches to @ref from_chars<T>(). The full string is used.
 *
 * @note Unlike with the floating case (see @ref to_chars_float()),
 * there is no need for an analogous `to_chars_integral()`, because
 * the lower-level @ref atoi() and @ref atou() functions used by @ref
 * to_chars() do not produce leading `+` characters. */
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE bool from_chars_integral(csubstr scalar, T *val) RYML_NOEXCEPT
{
    if C4_LIKELY(scalar.len > 0)
    {
        if(scalar.str[0] == '+')
            scalar = scalar.sub(1);
        return from_chars(scalar, val);
    }
    return false;
}


//-----------------------------------------------------------------------------
// scalar_deserialize

#if (C4_CPP >= 17) || defined(__DOXYGEN__)

/** Deserialize a scalar from its string representation, dispatching
 * to one of @ref from_chars(), @ref from_chars_float() or @ref
 * from_chars_integral() as appropriate.
 *
 * @note When using a standard older than C++17, `if constexpr` is not
 * available, and the implementation reverts to SFINAE to achieve the
 * compile-time dispatch.
 *
 * @return true if the deserialization succeeded */
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE bool scalar_deserialize(csubstr str, T *val)
{
    if constexpr (std::is_floating_point<T>::value)
        return from_chars_float(str, val);
    else if constexpr (std::is_arithmetic<T>::value)
        return from_chars_integral(str, val);
    else
        return from_chars(str, val);
}

#else // pre-C++17 implementation: need to use SFINAE

// float
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE auto scalar_deserialize(csubstr str, T *val) RYML_NOEXCEPT
    -> typename std::enable_if<std::is_floating_point<T>::value, bool>::type
{
    return from_chars_float(str, val);
}

// integral
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE auto scalar_deserialize(csubstr str, T *val) RYML_NOEXCEPT
    -> typename std::enable_if<std::is_arithmetic<T>::value && !std::is_floating_point<T>::value, bool>::type
{
    return from_chars_integral(str, val);
}

// other types
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE auto scalar_deserialize(csubstr str, T *val)
    -> typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_floating_point<T>::value, bool>::type
{
    return from_chars(str, val);
}

#endif // pre-C++17 implementation


//-----------------------------------------------------------------------------
// scalar_serialize

#if (C4_CPP >= 17) || defined(__DOXYGEN__)

/** Serialize a scalar to the buffer, dispatching to @ref to_chars() or
 * @ref to_chars_float() as appropriate.
 *
 * @note When using a standard older than C++17, `if constexpr` is not
 * available, and the implementation reverts to SFINAE to achieve the
 * compile-time dispatch.
 *
 * @return the size of the serialization, (the buffer size is strictly
 * respected and never overflowed) */
template<class T>
C4_ALWAYS_INLINE size_t scalar_serialize(substr buf, T const& C4_RESTRICT a)
{
    if constexpr (std::is_floating_point<T>::value)
        return to_chars_float(buf, a);
    else
        return to_chars(buf, a);
}

#else // pre-C++17 implementation: need to use SFINAE

// float
template<class T>
C4_ALWAYS_INLINE auto scalar_serialize(substr buf, T const& C4_RESTRICT a) RYML_NOEXCEPT
    -> typename std::enable_if<std::is_floating_point<T>::value, size_t>::type
{
    return to_chars_float(buf, a);
}

// other scalar types
template<class T>
C4_ALWAYS_INLINE auto scalar_serialize(substr buf, T const& C4_RESTRICT a)
    -> typename std::enable_if< ! std::is_floating_point<T>::value, size_t>::type
{
    return to_chars(buf, a);
}

#endif // pre-C++17 implementation


/** @} */


} // namespace yml
} // namespace c4


#endif /* C4_YML_SCALAR_CHARCONV_HPP_ */
