#ifndef BOOST_ICL_DETAIL_CO_EQUAL_FWD_HPP_JOFA_20251205
#define BOOST_ICL_DETAIL_CO_EQUAL_FWD_HPP_JOFA_20251205

// Lightweight forward-declarations of co_equal overloads that match the real
// SFINAE-enabled definitions. This avoids an unconstrained generic template
// competing with the real overloads and producing ambiguous calls.
//
// Includes mirror the headers needed for the traits used in the enable_if
// conditions. Adjust includes if your project places these traits elsewhere.

#include <boost/icl/detail/design_config.hpp>
#include <boost/icl/type_traits/is_combinable.hpp> // provides is_map, is_key_compare_equal, etc.
#include <boost/mpl/not.hpp>
#include <boost/mpl/and.hpp>
#include <boost/utility/enable_if.hpp> // boost::enable_if

namespace boost { namespace icl {

// Overload used when both containers are maps and key-compare-equal:
template<class Type, class CoType>
typename boost::enable_if<
    boost::mpl::and_<
        is_key_compare_equal<Type, CoType>,
        boost::mpl::and_< is_map<Type>, is_map<CoType> >
    >,
    bool
>::type
co_equal(typename Type::const_iterator left_,
         typename CoType::const_iterator right_,
         const Type* = 0, const CoType* = 0);

// Overload used when key-compare-equal but not both maps
template<class Type, class CoType>
typename boost::enable_if<
    boost::mpl::and_<
        is_key_compare_equal<Type, CoType>,
        boost::mpl::not_< boost::mpl::and_< is_map<Type>, is_map<CoType> > >
    >,
    bool
>::type
co_equal(typename Type::const_iterator,
         typename CoType::const_iterator,
         const Type* = 0, const CoType* = 0);

}} // namespace boost::icl

#endif // BOOST_ICL_DETAIL_CO_EQUAL_FWD_HPP_JOFA_20251205