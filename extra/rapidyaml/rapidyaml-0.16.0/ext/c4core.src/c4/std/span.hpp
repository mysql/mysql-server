#ifndef C4_STD_SPAN_HPP_
#define C4_STD_SPAN_HPP_

/** @file span.hpp */

#ifndef C4CORE_SINGLE_HEADER
#include "c4/language.hpp"
#endif


#if (C4_CPP >= 20) || defined(__DOXYGEN__)

#ifndef C4CORE_SINGLE_HEADER
#include "c4/substr.hpp"
#endif

#include <span>


namespace c4 {

template<class T> struct is_string;
template<class T> struct is_writeable_string;

// mark std::span<const char> as a string type
template<> struct is_string<std::span<const char>> : public std::true_type {};
template<> struct is_string<const std::span<const char>> : public std::true_type {};

// mark std::span<char> as a string type
template<> struct is_string<std::span<char>> : public std::true_type {};
template<> struct is_string<const std::span<char>> : public std::true_type {};
template<> struct is_writeable_string<std::span<char>> : public std::true_type {};
template<> struct is_writeable_string<const std::span<char>> : public std::true_type {};


//-----------------------------------------------------------------------------

/** create a csubstr from an existing std::span<const char> */
C4_ALWAYS_INLINE c4::csubstr to_csubstr(std::span<const char> s) noexcept
{
    return c4::csubstr(s.data(), s.size());
}

/** create a csubstr from an existing std::span<char> */
C4_ALWAYS_INLINE c4::csubstr to_csubstr(std::span<char> s) noexcept
{
    return c4::csubstr(s.data(), s.size());
}
/** create a substr from an existing std::span<char> */
C4_ALWAYS_INLINE c4::substr to_substr(std::span<char> s) noexcept
{
    return c4::substr(s.data(), s.size());
}


//-----------------------------------------------------------------------------

C4_ALWAYS_INLINE bool operator== (c4::csubstr ss, std::span<char> s) { return ss.compare(s.data(), s.size()) == 0; }
C4_ALWAYS_INLINE bool operator!= (c4::csubstr ss, std::span<char> s) { return ss.compare(s.data(), s.size()) != 0; }
C4_ALWAYS_INLINE bool operator>= (c4::csubstr ss, std::span<char> s) { return ss.compare(s.data(), s.size()) >= 0; }
C4_ALWAYS_INLINE bool operator>  (c4::csubstr ss, std::span<char> s) { return ss.compare(s.data(), s.size()) >  0; }
C4_ALWAYS_INLINE bool operator<= (c4::csubstr ss, std::span<char> s) { return ss.compare(s.data(), s.size()) <= 0; }
C4_ALWAYS_INLINE bool operator<  (c4::csubstr ss, std::span<char> s) { return ss.compare(s.data(), s.size()) <  0; }

C4_ALWAYS_INLINE bool operator== (std::span<char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) == 0; }
C4_ALWAYS_INLINE bool operator!= (std::span<char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) != 0; }
C4_ALWAYS_INLINE bool operator<= (std::span<char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) >= 0; }
C4_ALWAYS_INLINE bool operator<  (std::span<char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) >  0; }
C4_ALWAYS_INLINE bool operator>= (std::span<char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) <= 0; }
C4_ALWAYS_INLINE bool operator>  (std::span<char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) <  0; }


C4_ALWAYS_INLINE bool operator== (c4::csubstr ss, std::span<const char> s) { return ss.compare(s.data(), s.size()) == 0; }
C4_ALWAYS_INLINE bool operator!= (c4::csubstr ss, std::span<const char> s) { return ss.compare(s.data(), s.size()) != 0; }
C4_ALWAYS_INLINE bool operator>= (c4::csubstr ss, std::span<const char> s) { return ss.compare(s.data(), s.size()) >= 0; }
C4_ALWAYS_INLINE bool operator>  (c4::csubstr ss, std::span<const char> s) { return ss.compare(s.data(), s.size()) >  0; }
C4_ALWAYS_INLINE bool operator<= (c4::csubstr ss, std::span<const char> s) { return ss.compare(s.data(), s.size()) <= 0; }
C4_ALWAYS_INLINE bool operator<  (c4::csubstr ss, std::span<const char> s) { return ss.compare(s.data(), s.size()) <  0; }

C4_ALWAYS_INLINE bool operator== (std::span<const char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) == 0; }
C4_ALWAYS_INLINE bool operator!= (std::span<const char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) != 0; }
C4_ALWAYS_INLINE bool operator<= (std::span<const char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) >= 0; }
C4_ALWAYS_INLINE bool operator<  (std::span<const char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) >  0; }
C4_ALWAYS_INLINE bool operator>= (std::span<const char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) <= 0; }
C4_ALWAYS_INLINE bool operator>  (std::span<const char> s, c4::csubstr ss) { return ss.compare(s.data(), s.size()) <  0; }


//-----------------------------------------------------------------------------

/** copy a std::span<const char> to a writeable substr */
C4_ALWAYS_INLINE size_t to_chars(c4::substr buf, std::span<const char> s)
{
    size_t sz = s.size();
    size_t len = buf.len < sz ? buf.len : sz;
    buf.copy_from(csubstr(s.data(), len)); // copy only available chars
    return sz; // return the number of needed chars
}

/** copy a std::span<char> to a writeable substr */
inline size_t to_chars(c4::substr buf, std::span<char> s)
{
    size_t sz = s.size();
    size_t len = buf.len < sz ? buf.len : sz;
    buf.copy_from(csubstr(s.data(), len)); // copy only available chars
    return sz; // return the number of needed chars
}

/** copy a csubstr to an existing std::span<char> */
inline bool from_chars(c4::csubstr buf, std::span<char> * s)
{
    if(buf.len <= s->size())
    {
        substr(s->data(), buf.len).copy_from(buf);
        *s = s->first(buf.len);
        return true;
    }
    return false;
}

} // namespace c4

#endif // SPAN_AVAILABLE

#endif // C4_STD_SPAN_HPP_
