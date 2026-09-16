#ifndef C4_STD_STRING_VIEW_FWD_HPP_
#define C4_STD_STRING_VIEW_FWD_HPP_

/** @file string_view_fwd.hpp Provides forward declaration of std::string_view
 * to enable order-independent includes for use with ref @ref
 * c4::to_chars() and @ref c4::from_chars(). */

#ifndef C4CORE_SINGLE_HEADER
#include "c4/language.hpp"
#endif


#if (C4_CPP >= 17) || defined(__DOXYGEN__)

#ifndef C4CORE_SINGLE_HEADER
#include "c4/substr_fwd.hpp"
#endif

#include <string_view> // AFAICT it's not possible to fwd-declare std::string_view


namespace c4 {

template<class T> struct is_string;

// mark std::string_view as a string type
template<> struct is_string<std::string_view>;
template<> struct is_string<const std::string_view>;


//-----------------------------------------------------------------------------

c4::csubstr to_csubstr(std::string_view s) noexcept;


//-----------------------------------------------------------------------------

bool operator== (c4::csubstr ss, std::string_view s);
bool operator!= (c4::csubstr ss, std::string_view s);
bool operator>= (c4::csubstr ss, std::string_view s);
bool operator>  (c4::csubstr ss, std::string_view s);
bool operator<= (c4::csubstr ss, std::string_view s);
bool operator<  (c4::csubstr ss, std::string_view s);

bool operator== (std::string_view s, c4::csubstr ss);
bool operator!= (std::string_view s, c4::csubstr ss);
bool operator<= (std::string_view s, c4::csubstr ss);
bool operator<  (std::string_view s, c4::csubstr ss);
bool operator>= (std::string_view s, c4::csubstr ss);
bool operator>  (std::string_view s, c4::csubstr ss);


//-----------------------------------------------------------------------------

size_t to_chars(c4::substr buf, std::string_view s);

} // namespace c4

#endif // STRING_VIEW_AVAILABLE

#endif // C4_STD_STRING_VIEW_FWD_HPP_
