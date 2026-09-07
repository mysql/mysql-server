#ifndef C4_STD_VECTOR_FWD_HPP_
#define C4_STD_VECTOR_FWD_HPP_

/** @file vector_fwd.hpp Provides forward declaration of std::vector
 * to enable order-independent includes for use with ref
 * @ref c4::to_chars() and @ref c4::from_chars(). */

#include <cstddef>

// NOLINTBEGIN(cert-dcl58-cpp)

// forward declarations for std::vector
#if defined(__GLIBCXX__) || defined(__GLIBCPP__) || defined(_MSC_VER)
#if defined(_MSC_VER)
__pragma(warning(push))
__pragma(warning(disable : 4643))
#endif
namespace std {
template<typename> class allocator; // NOLINT
#ifdef _GLIBCXX_DEBUG
inline namespace __debug {
template<typename T, typename Alloc> class vector; // NOLINT
}
#else
template<typename T, typename Alloc> class vector; // NOLINT
#endif
} // namespace std
#if defined(_MSC_VER)
__pragma(warning(pop))
#endif
#elif defined(_LIBCPP_ABI_NAMESPACE)
namespace std {
inline namespace _LIBCPP_ABI_NAMESPACE {
template<typename> class allocator; // NOLINT
template<typename T, typename Alloc> class vector; // NOLINT
} // namespace _LIBCPP_ABI_NAMESPACE
} // namespace std
#else
#error "unknown standard library"
#endif

#ifndef C4CORE_SINGLE_HEADER
#include "c4/substr_fwd.hpp"
#endif

namespace c4 {

template<class T> struct is_string;
template<class T> struct is_writeable_string;
template<class Alloc> struct is_string<std::vector<char,Alloc>>;
template<class Alloc> struct is_string<const std::vector<char,Alloc>>;
template<class Alloc> struct is_writeable_string<std::vector<char,Alloc>>;

template<class Alloc> c4::substr to_substr(std::vector<char, Alloc> &vec);
template<class Alloc> c4::csubstr to_csubstr(std::vector<char, Alloc> const& vec);

template<class Alloc> bool operator!= (c4::csubstr ss, std::vector<char, Alloc> const& s);
template<class Alloc> bool operator== (c4::csubstr ss, std::vector<char, Alloc> const& s);
template<class Alloc> bool operator>= (c4::csubstr ss, std::vector<char, Alloc> const& s);
template<class Alloc> bool operator>  (c4::csubstr ss, std::vector<char, Alloc> const& s);
template<class Alloc> bool operator<= (c4::csubstr ss, std::vector<char, Alloc> const& s);
template<class Alloc> bool operator<  (c4::csubstr ss, std::vector<char, Alloc> const& s);

template<class Alloc> bool operator!= (std::vector<char, Alloc> const& s, c4::csubstr ss);
template<class Alloc> bool operator== (std::vector<char, Alloc> const& s, c4::csubstr ss);
template<class Alloc> bool operator>= (std::vector<char, Alloc> const& s, c4::csubstr ss);
template<class Alloc> bool operator>  (std::vector<char, Alloc> const& s, c4::csubstr ss);
template<class Alloc> bool operator<= (std::vector<char, Alloc> const& s, c4::csubstr ss);
template<class Alloc> bool operator<  (std::vector<char, Alloc> const& s, c4::csubstr ss);

template<class Alloc> size_t to_chars(c4::substr buf, std::vector<char, Alloc> const& s);
template<class Alloc> bool from_chars(c4::csubstr buf, std::vector<char, Alloc> * s);

} // namespace c4

// NOLINTEND(cert-dcl58-cpp)

#endif // C4_STD_VECTOR_FWD_HPP_
