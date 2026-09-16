#ifndef C4_SUBSTR_FWD_HPP_
#define C4_SUBSTR_FWD_HPP_

namespace c4 {

#ifndef __DOXYGEN__
template<class C> struct basic_substring;
using csubstr = basic_substring<const char>;
using substr = basic_substring<char>;
template<class T> struct is_string;
template<class T> struct is_writeable_string;
#endif // !__DOXYGEN__

} // namespace c4

#endif /* C4_SUBSTR_FWD_HPP_ */
