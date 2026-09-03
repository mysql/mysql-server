#ifndef C4_FORMAT_BASE64_HPP_
#define C4_FORMAT_BASE64_HPP_

/** @file format_base64.hpp Utilities for formatting data as base64 */

#ifndef C4_SUBSTR_HPP_
#include "c4/substr.hpp"
#endif
#ifndef C4_BLOB_HPP_
#include "c4/blob.hpp"
#endif
#ifndef C4_BASE64_HPP_
#include "c4/base64.hpp"
#endif


namespace c4 {
namespace fmt {

/** @defgroup doc_base64_fmt Base64 format specifiers
 *
 *  ```c++
 *  // given these variables:
 *  T var = {};
 *  T &ref = var;
 *  std::vector<T> vec = ...;
 *  substr buf = ...;
 *
 *  // there are different approaches to encode/decode base64:
 *  size_t numchars = to_chars(buf, base64(var)); // encode var as base64
 *  size_t numchars = to_chars(buf, base64(ref)); // same as above
 *  size_t numchars = to_chars(buf, base64(&var, 1)); // same as above
 *  size_t numchars = to_chars(buf, base64(vec.data(), vec.size())); // for the container, but same call form as above
 *  size_t numchars = to_chars(buf, base64(vec)); // same effect as prev call
 *
 *  // using cbase64() (the `c` prefix is for const) is equivalent
 *  // for encoding, but quicker to compile:
 *  size_t numchars = to_chars(buf, cbase64(var)); // encode var as base64
 *  size_t numchars = to_chars(buf, cbase64(ref)); // same as above
 *  size_t numchars = to_chars(buf, cbase64(&var, 1)); // same as above
 *  size_t numchars = to_chars(buf, cbase64(vec.data(), vec.size())); // for the container, but same call as above
 *  size_t numchars = to_chars(buf, cbase64(vec)); // same effect as prev call
 *
 *  // to decode:
 *  csubstr buf = ...;
 *  size_t reqbytes = 0; // number of bytes of decoded data
 *  bool ok = from_chars(buf, base64(var)); // decode base64 to var
 *  bool ok = from_chars(buf, base64(var, &reqbytes)); // same. reqbytes will be sizeof(var)
 *  bool ok = from_chars(buf, base64(ref)); // same as above
 *  bool ok = from_chars(buf, base64(ref, &reqbytes)); // same. reqbytes will be sizeof(var)
 *  bool ok = from_chars(buf, base64(&var, 1, &reqbytes)); // same
 *  bool ok = from_chars(buf, base64(vec.data(), vec.size(), &reqbytes)); // for the container, but same call as above
 *  bool ok = from_chars(buf, base64(vec, &reqbytes)); // same effect as prev call
 *  ```
 * @ingroup doc_format_specifiers
 * @ingroup doc_base64
 * @{ */

/** @cond dev */
namespace detail {
template<class> struct sfinae_true : std::true_type{};
template<class T, class A0> static auto test_resize(int) -> sfinae_true<decltype(std::declval<T>().resize(std::declval<A0>()))>;
template<class, class A0> static auto test_resize(int64_t) -> std::false_type;
/// a traits class to report when a type as a member method named resize
/// @see https://stackoverflow.com/a/9154394
template<class T, class Arg=size_t>
struct has_resize : decltype(detail::test_resize<typename std::remove_cv<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::type, Arg>(0)){};

template<typename CharOrConstChar>
struct base64_wrapper_
{
    blob_<CharOrConstChar> data;
    size_t *required_size;
    base64_wrapper_(blob_<CharOrConstChar> blob, size_t *len_=nullptr) noexcept
        : data(blob)
        , required_size(len_)
    {}
};

template<class Container, typename CharOrConstChar>
struct base64_container_wrapper_
{
    using value_type = typename Container::value_type;
    Container *container;
    size_t *required_size;
    base64_container_wrapper_(Container *c, size_t *len_=nullptr) noexcept
        : container(c)
        , required_size(len_)
    {}
    blob_<CharOrConstChar> data() const noexcept
    {
        size_t sz = container->size();
        CharOrConstChar *first = sz ? reinterpret_cast<CharOrConstChar*>(&(*container)[0]) : nullptr; // NOLINT
        return blob_<CharOrConstChar>(first, sizeof(value_type) * sz);
    }
};
} // namespace detail
/** @endcond */



/** a tag type to mark a payload to be encoded as base64 */
using const_base64_wrapper = detail::base64_wrapper_<cbyte>;
/** a tag type to mark a payload to be decoded as base64 */
using base64_wrapper = detail::base64_wrapper_<byte>;


/** a tag type to mark a payload as base64-encoded */
template<class Container>
using const_base64_container_wrapper = detail::base64_container_wrapper_<const Container, cbyte>;
/** a tag type to mark a payload to be encoded as base64 */
template<class Container>
using base64_container_wrapper = detail::base64_container_wrapper_<Container, byte>;


/** a tag function to mark a csubstr payload to be encoded in base64 format */
C4_ALWAYS_INLINE const_base64_wrapper cbase64(csubstr s, size_t *reqsize=nullptr)
{
    return const_base64_wrapper(cblob(s.str, s.len), reqsize);
}
/** a tag function to mark a csubstr payload to be encoded in base64 format */
C4_ALWAYS_INLINE const_base64_wrapper base64(csubstr s, size_t *reqsize=nullptr)
{
    return const_base64_wrapper(cblob(s.str, s.len), reqsize);
}
/** a tag function to mark a variable to be decoded from base64 */
C4_ALWAYS_INLINE base64_wrapper base64(substr s, size_t *reqsize=nullptr)
{
    return base64_wrapper(blob(s.str, s.len), reqsize);
}


/** a tag function to mark a payload to be encoded in base64 format */
template<class T>
C4_ALWAYS_INLINE const_base64_wrapper cbase64(T const* arg, size_t sz, size_t *reqsize=nullptr) // NOLINT
{
    return const_base64_wrapper(cblob(arg, sz), reqsize);
}
/** a tag function to mark a payload to be encoded in base64 format */
template<class T>
C4_ALWAYS_INLINE auto base64(T * arg, size_t sz, size_t *reqsize=nullptr) // NOLINT
    -> typename std::conditional<std::is_const<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::value,
                                 const_base64_wrapper,
                                 base64_wrapper>::type
{
    using U = typename std::remove_reference<typename std::remove_pointer<T>::type>::type;
    using ret_type = typename std::conditional<std::is_const<U>::value,
                                               const_base64_wrapper,
                                               base64_wrapper>::type;
    using blob_type = typename std::conditional<std::is_const<U>::value,
                                               cblob,
                                               blob>::type;
    return ret_type(blob_type(arg, sz), reqsize);
}


/** a tag function to mark a payload to be encoded in base64 format */
template<class T>
C4_ALWAYS_INLINE auto cbase64(T const& arg, size_t *reqsize=nullptr) // NOLINT
    -> typename std::enable_if< ! detail::has_resize<T>::value, const_base64_wrapper>::type
{
    return const_base64_wrapper(cblob(arg), reqsize);
}
/** a tag function to mark a payload to be encoded or decoded in base64 format */
template<class T>
C4_ALWAYS_INLINE auto base64(T & arg, size_t *reqsize=nullptr) // NOLINT
    -> typename std::enable_if< ! detail::has_resize<T>::value,
                                typename std::conditional<std::is_const<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::value,
                                                          const_base64_wrapper,
                                                          base64_wrapper>::type>::type
{
    using U = typename std::remove_reference<typename std::remove_pointer<T>::type>::type;
    using ret_type = typename std::conditional<std::is_const<U>::value,
                                               const_base64_wrapper,
                                               base64_wrapper>::type;
    using blob_type = typename std::conditional<std::is_const<U>::value,
                                               cblob,
                                               blob>::type;
    return ret_type(blob_type(arg), reqsize);
}

/** a tag function to mark a container (payload with a .resize()
 * method) to be encoded in base64 format. */
template<class T>
C4_ALWAYS_INLINE auto cbase64(T const& arg, size_t *reqsize=nullptr) // NOLINT
    -> typename std::enable_if<detail::has_resize<T>::value, const_base64_container_wrapper<T>>::type
{
    return const_base64_container_wrapper<T>(&arg, reqsize);
}
/** a tag function to mark a container (payload with a .resize()
 * method) to be encoded or decoded in base64 format. Subsequently
 * when decoding, from_chars() will resize the container to fit the
 * decoded data. */
template<class T>
C4_ALWAYS_INLINE auto base64(T & arg, size_t *reqsize=nullptr) // NOLINT
    -> typename std::enable_if<detail::has_resize<T>::value,
                               typename std::conditional<std::is_const<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::value,
                                                         const_base64_container_wrapper<T>,
                                                         base64_container_wrapper<T>>::type>::type
{
    using ret_type = typename std::conditional<std::is_const<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::value,
                                               const_base64_container_wrapper<T>,
                                               base64_container_wrapper<T>>::type;
    return ret_type(&arg, reqsize);
}

/** @} */ // base64_fmt

} // namespace fmt


//-----------------------------------------------------------------------------

/** @addtogroup doc_base64
 * @{ */

/** (1) read a variable in base64 format
 * @ingroup doc_from_chars */
inline bool from_chars(csubstr buf, fmt::base64_wrapper const& b)
{
    size_t reqsize = 0;
    bool ok = base64_decode(buf.str, buf.len, b.data.buf, b.data.len, &reqsize);
    if(b.required_size)
        *b.required_size = reqsize;
    return ok;
}
/** (2) read a variable in base64 format
 * @ingroup doc_from_chars */
inline bool from_chars(csubstr buf, fmt::base64_wrapper *b)
{
    return from_chars(buf, *b);
}


/** write a variable or buffer in base64 format
 * @ingroup doc_to_chars */
template<typename CharOrConstChar>
inline size_t to_chars(substr buf, fmt::detail::base64_wrapper_<CharOrConstChar> const& b)
{
    size_t reqsize = base64_encode(buf.str, buf.len, b.data.buf, b.data.len);
    if(b.required_size)
        *b.required_size = reqsize;
    return reqsize;
}
/** write a container in base64 format
 * @ingroup doc_to_chars */
template<class Container, typename CharOrConstChar>
size_t to_chars(substr buf, fmt::detail::base64_container_wrapper_<Container, CharOrConstChar> const& b)
{
    cblob data = b.data();
    size_t reqsize = base64_encode(buf.str, buf.len, data.buf, data.len);
    if(b.required_size)
        *b.required_size = reqsize;
    return reqsize;
}

/** read a container in base64 format, resizing it as needed to
 * accomodate the result
 * @ingroup doc_from_chars */
template<class T>
bool from_chars(csubstr buf, fmt::base64_container_wrapper<T> const& b)
{
    enum : size_t { elm_sz = sizeof(typename fmt::base64_container_wrapper<T>::value_type) }; // NOLINT
    blob data = b.data();
    size_t required_size = 0;
    bool ok = base64_decode(buf.str, buf.len, data.buf, data.len, &required_size);
    if(b.required_size)
        *b.required_size = required_size;
    if(!required_size)
        return ok;
    else if(!ok && ((required_size < data.len) || (required_size % elm_sz)))
        return false;
    size_t num_elms = required_size / elm_sz;
    b.container->resize(num_elms);
    if(required_size > data.len)
    {
        data = b.data();
        ok = base64_decode(buf.str, buf.len, data.buf, data.len, &required_size);
        if(b.required_size)
            *b.required_size = required_size;
    }
    return ok;
}
/** read a container in base64 format, resizing it as needed to
 * accomodate the result
 * @ingroup doc_from_chars */
template<class T>
bool from_chars(csubstr buf, fmt::base64_container_wrapper<T> const* b)
{
    return from_chars(buf, *b);
}

/** @} */ // base64

} // namespace c4

#endif /* C4_FORMAT_BASE64_HPP_ */
