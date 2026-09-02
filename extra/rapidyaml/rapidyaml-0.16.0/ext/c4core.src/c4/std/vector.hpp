#ifndef C4_STD_VECTOR_HPP_
#define C4_STD_VECTOR_HPP_

/** @file vector.hpp provides conversion and comparison facilities
 * from/between std::vector<char> to c4::substr and c4::csubstr.
 * @todo add to_span() and friends
 */

#ifndef C4CORE_SINGLE_HEADER
#include "c4/substr.hpp"
#endif

#if defined(__GNUC__) && (__GNUC__ >= 6)
C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wnull-dereference")
#endif
#include <vector>
#if defined(__GNUC__) && (__GNUC__ >= 6)
C4_SUPPRESS_WARNING_GCC_POP
#endif

namespace c4 {

// mark std::string as a string type
template<class T> struct is_string;
template<class Alloc> struct is_string<std::vector<char,Alloc>> : public std::true_type {};
template<class Alloc> struct is_string<const std::vector<char,Alloc>> : public std::true_type {};

// mark std::string as a writeable string type
template<class T> struct is_writeable_string;
template<class Alloc> struct is_writeable_string<std::vector<char,Alloc>> : public std::true_type {};


//-----------------------------------------------------------------------------

/** get a substr (writeable string view) of an existing std::vector<char> */
template<class Alloc>
c4::substr to_substr(std::vector<char, Alloc> &vec)
{
    char *data = vec.empty() ? nullptr : vec.data(); // data() may or may not return a null pointer.
    return c4::substr(data, vec.size());
}

/** get a csubstr (read-only string) view of an existing std::vector<char> */
template<class Alloc>
c4::csubstr to_csubstr(std::vector<char, Alloc> const& vec)
{
    const char *data = vec.empty() ? nullptr : vec.data(); // data() may or may not return a null pointer.
    return c4::csubstr(data, vec.size());
}


//-----------------------------------------------------------------------------
// comparisons between substrings and std::vector<char>

template<class Alloc> C4_ALWAYS_INLINE bool operator!= (c4::csubstr ss, std::vector<char, Alloc> const& s) { return ss != to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator== (c4::csubstr ss, std::vector<char, Alloc> const& s) { return ss == to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator>= (c4::csubstr ss, std::vector<char, Alloc> const& s) { return ss >= to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator>  (c4::csubstr ss, std::vector<char, Alloc> const& s) { return ss >  to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator<= (c4::csubstr ss, std::vector<char, Alloc> const& s) { return ss <= to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator<  (c4::csubstr ss, std::vector<char, Alloc> const& s) { return ss <  to_csubstr(s); }

template<class Alloc> C4_ALWAYS_INLINE bool operator!= (std::vector<char, Alloc> const& s, c4::csubstr ss) { return ss != to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator== (std::vector<char, Alloc> const& s, c4::csubstr ss) { return ss == to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator>= (std::vector<char, Alloc> const& s, c4::csubstr ss) { return ss <= to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator>  (std::vector<char, Alloc> const& s, c4::csubstr ss) { return ss <  to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator<= (std::vector<char, Alloc> const& s, c4::csubstr ss) { return ss >= to_csubstr(s); }
template<class Alloc> C4_ALWAYS_INLINE bool operator<  (std::vector<char, Alloc> const& s, c4::csubstr ss) { return ss >  to_csubstr(s); }


//-----------------------------------------------------------------------------

/** copy a std::vector<char> to a substr */
template<class Alloc>
inline size_t to_chars(c4::substr buf, std::vector<char, Alloc> const& s)
{
    size_t sz = s.size();
    size_t len = buf.len < sz ? buf.len : sz;
    buf.copy_from(csubstr(s.data(), len)); // copy only available chars
    return sz; // return the number of needed chars
}

/** copy a csubstr to an existing std::vector<char> */
template<class Alloc>
inline bool from_chars(c4::csubstr buf, std::vector<char, Alloc> * s)
{
    s->resize(buf.len);
    substr(s->data(), buf.len).copy_from(buf);
    return true;
}

} // namespace c4

#endif // C4_STD_VECTOR_HPP_
