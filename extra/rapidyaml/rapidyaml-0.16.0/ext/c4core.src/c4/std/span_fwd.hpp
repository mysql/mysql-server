#ifndef C4_STD_SPAN_FWD_HPP_
#define C4_STD_SPAN_FWD_HPP_

/** @file span_fwd.hpp Provides forward declaration of std::span
 * to enable order-independent includes for use with ref @ref
 * c4::to_chars() and @ref c4::from_chars(). */

#ifndef C4CORE_SINGLE_HEADER
#include "c4/language.hpp"
#endif


#if (C4_CPP >= 20) || defined(__DOXYGEN__)

#ifndef C4CORE_SINGLE_HEADER
#include "c4/substr_fwd.hpp"
#endif

#include <span> // AFAICT it's not possible to fwd-declare std::span


namespace c4 {

template<class T> struct is_string;
template<class T> struct is_writeable_string;

// mark std::span<const char> as a string type
template<> struct is_string<std::span<const char>>;
template<> struct is_string<const std::span<const char>>;

// mark std::span<char> as a string type
template<> struct is_string<std::span<char>>;
template<> struct is_string<const std::span<char>>;
template<> struct is_writeable_string<std::span<char>>;
template<> struct is_writeable_string<const std::span<char>>;


//-----------------------------------------------------------------------------

c4::csubstr to_csubstr(std::span<const char> s) noexcept;
c4::csubstr to_csubstr(std::span<char> s) noexcept;
c4::substr to_substr(std::span<char> s) noexcept;


//-----------------------------------------------------------------------------

bool operator== (c4::csubstr ss, std::span<char> s);
bool operator!= (c4::csubstr ss, std::span<char> s);
bool operator>= (c4::csubstr ss, std::span<char> s);
bool operator>  (c4::csubstr ss, std::span<char> s);
bool operator<= (c4::csubstr ss, std::span<char> s);
bool operator<  (c4::csubstr ss, std::span<char> s);

bool operator== (std::span<char> s, c4::csubstr ss);
bool operator!= (std::span<char> s, c4::csubstr ss);
bool operator<= (std::span<char> s, c4::csubstr ss);
bool operator<  (std::span<char> s, c4::csubstr ss);
bool operator>= (std::span<char> s, c4::csubstr ss);
bool operator>  (std::span<char> s, c4::csubstr ss);


bool operator== (c4::csubstr ss, std::span<const char> s);
bool operator!= (c4::csubstr ss, std::span<const char> s);
bool operator>= (c4::csubstr ss, std::span<const char> s);
bool operator>  (c4::csubstr ss, std::span<const char> s);
bool operator<= (c4::csubstr ss, std::span<const char> s);
bool operator<  (c4::csubstr ss, std::span<const char> s);

bool operator== (std::span<const char> s, c4::csubstr ss);
bool operator!= (std::span<const char> s, c4::csubstr ss);
bool operator<= (std::span<const char> s, c4::csubstr ss);
bool operator<  (std::span<const char> s, c4::csubstr ss);
bool operator>= (std::span<const char> s, c4::csubstr ss);
bool operator>  (std::span<const char> s, c4::csubstr ss);


//-----------------------------------------------------------------------------

size_t to_chars(c4::substr buf, std::span<const char> s);
size_t to_chars(c4::substr buf, std::span<char> s);
bool from_chars(c4::csubstr buf, std::span<char> * s);

} // namespace c4

#endif // SPAN_AVAILABLE

#endif // C4_STD_SPAN_FWD_HPP_
