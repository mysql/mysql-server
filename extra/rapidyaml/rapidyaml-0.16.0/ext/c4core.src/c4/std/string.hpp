#ifndef C4_STD_STRING_HPP_
#define C4_STD_STRING_HPP_

/** @file string.hpp */

#ifndef C4CORE_SINGLE_HEADER
#include "c4/substr.hpp"
#endif

#include <string>

namespace c4 {

// mark std::string as a string type
template<class T> struct is_string;
template<> struct is_string<std::string> : public std::true_type {};
template<> struct is_string<const std::string> : public std::true_type {};

// mark std::string as a writeable string type
template<class T> struct is_writeable_string;
template<> struct is_writeable_string<std::string> : public std::true_type {};


//-----------------------------------------------------------------------------

/** get a writeable view to an existing std::string.
 * When the string is empty, the returned view will be pointing
 * at the character with value '\0', but the size will be zero.
 * @see https://en.cppreference.com/w/cpp/string/basic_string/operator_at
 */
C4_ALWAYS_INLINE c4::substr to_substr(std::string &s) noexcept
{
    #if C4_CPP < 11
    #error this function requires c++11
    #endif
    // since c++11 it is legal to call s[s.size()].
    return c4::substr(&s[0], s.size()); // NOLINT
}

/** get a readonly view to an existing std::string.
 * When the string is empty, the returned view will be pointing
 * at the character with value '\0', but the size will be zero.
 * @see https://en.cppreference.com/w/cpp/string/basic_string/operator_at
 */
C4_ALWAYS_INLINE c4::csubstr to_csubstr(std::string const& s) noexcept
{
    #if C4_CPP < 11
    #error this function requires c++11
    #endif
    // since c++11 it is legal to call s[s.size()].
    return c4::csubstr(&s[0], s.size()); // NOLINT
}


//-----------------------------------------------------------------------------

C4_ALWAYS_INLINE bool operator== (c4::csubstr ss, std::string const& s) { return ss.compare(to_csubstr(s)) == 0; }
C4_ALWAYS_INLINE bool operator!= (c4::csubstr ss, std::string const& s) { return ss.compare(to_csubstr(s)) != 0; }
C4_ALWAYS_INLINE bool operator>= (c4::csubstr ss, std::string const& s) { return ss.compare(to_csubstr(s)) >= 0; }
C4_ALWAYS_INLINE bool operator>  (c4::csubstr ss, std::string const& s) { return ss.compare(to_csubstr(s)) >  0; }
C4_ALWAYS_INLINE bool operator<= (c4::csubstr ss, std::string const& s) { return ss.compare(to_csubstr(s)) <= 0; }
C4_ALWAYS_INLINE bool operator<  (c4::csubstr ss, std::string const& s) { return ss.compare(to_csubstr(s)) <  0; }

C4_ALWAYS_INLINE bool operator== (std::string const& s, c4::csubstr ss) { return ss.compare(to_csubstr(s)) == 0; }
C4_ALWAYS_INLINE bool operator!= (std::string const& s, c4::csubstr ss) { return ss.compare(to_csubstr(s)) != 0; }
C4_ALWAYS_INLINE bool operator>= (std::string const& s, c4::csubstr ss) { return ss.compare(to_csubstr(s)) <= 0; }
C4_ALWAYS_INLINE bool operator>  (std::string const& s, c4::csubstr ss) { return ss.compare(to_csubstr(s)) <  0; }
C4_ALWAYS_INLINE bool operator<= (std::string const& s, c4::csubstr ss) { return ss.compare(to_csubstr(s)) >= 0; }
C4_ALWAYS_INLINE bool operator<  (std::string const& s, c4::csubstr ss) { return ss.compare(to_csubstr(s)) >  0; }


//-----------------------------------------------------------------------------

/** copy a std::string to a writeable substr */
inline size_t to_chars(c4::substr buf, std::string const& s)
{
    size_t sz = s.size();
    size_t len = buf.len < sz ? buf.len : sz;
    buf.copy_from(csubstr(s.data(), len)); // copy only available chars
    return sz; // return the number of needed chars
}

/** copy a csubstr to an existing std::string */
inline bool from_chars(c4::csubstr buf, std::string * s)
{
    s->resize(buf.len);
    substr(&(*s)[0], buf.len).copy_from(buf); // NOLINT
    return true;
}

} // namespace c4

#endif // C4_STD_STRING_HPP_
