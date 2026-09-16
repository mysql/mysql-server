#ifndef C4_STD_STRING_VIEW_HPP_
#define C4_STD_STRING_VIEW_HPP_

/** @file string_view.hpp */

#ifndef C4CORE_SINGLE_HEADER
#ifndef C4_LANGUAGE_HPP_
#include "c4/language.hpp"
#endif
#endif

#if (C4_CPP >= 17 && defined(__cpp_lib_string_view)) || defined(__DOXYGEN__)

#ifndef C4CORE_SINGLE_HEADER
#ifndef C4_SUBSTR_HPP_
#include "c4/substr.hpp"
#endif
#endif

#include <string_view>


namespace c4 {

// mark std::string_view as a string type
template<class T> struct is_string;
template<> struct is_string<std::string_view> : public std::true_type {};
template<> struct is_string<const std::string_view> : public std::true_type {};


//-----------------------------------------------------------------------------

/** create a csubstr from an existing std::string_view. */
C4_ALWAYS_INLINE c4::csubstr to_csubstr(std::string_view s) noexcept
{
    return c4::csubstr(s.data(), s.size());
}


//-----------------------------------------------------------------------------

C4_ALWAYS_INLINE bool operator== (c4::csubstr ss, std::string_view s) { return ss.compare(s.data(), s.size()) == 0; }
C4_ALWAYS_INLINE bool operator!= (c4::csubstr ss, std::string_view s) { return ss.compare(s.data(), s.size()) != 0; }
C4_ALWAYS_INLINE bool operator>= (c4::csubstr ss, std::string_view s) { return ss.compare(s.data(), s.size()) >= 0; }
C4_ALWAYS_INLINE bool operator>  (c4::csubstr ss, std::string_view s) { return ss.compare(s.data(), s.size()) >  0; }
C4_ALWAYS_INLINE bool operator<= (c4::csubstr ss, std::string_view s) { return ss.compare(s.data(), s.size()) <= 0; }
C4_ALWAYS_INLINE bool operator<  (c4::csubstr ss, std::string_view s) { return ss.compare(s.data(), s.size()) <  0; }

C4_ALWAYS_INLINE bool operator== (std::string_view s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) == 0; }
C4_ALWAYS_INLINE bool operator!= (std::string_view s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) != 0; }
C4_ALWAYS_INLINE bool operator<= (std::string_view s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) >= 0; }
C4_ALWAYS_INLINE bool operator<  (std::string_view s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) >  0; }
C4_ALWAYS_INLINE bool operator>= (std::string_view s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) <= 0; }
C4_ALWAYS_INLINE bool operator>  (std::string_view s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) <  0; }


//-----------------------------------------------------------------------------

/** copy an std::string_view to a writeable substr */
inline size_t to_chars(c4::substr buf, std::string_view s)
{
    size_t sz = s.size();
    size_t len = buf.len < sz ? buf.len : sz;
    buf.copy_from(csubstr(s.data(), len)); // copy only available chars
    return sz; // return the number of needed chars
}

} // namespace c4

#endif // STRING_VIEW_AVAILABLE

#endif // C4_STD_STRING_VIEW_HPP_
