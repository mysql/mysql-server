/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Joaquin M Lopez Munoz
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_DETAIL_SET_ADAPTOR_HPP_JMLM_260321
#define BOOST_ICL_DETAIL_SET_ADAPTOR_HPP_JMLM_260321

#include <boost/icl/detail/assoc_container_adaptor.hpp>

#include ICL_IMPL_PATH(set)

// let boostdep know about this potential dependency hidden by ICL_IMPL_PATH
#if 0
#include <boost/container/set.hpp>
#endif

namespace boost{namespace icl{namespace detail
{

template<class K, class Compare, class Allocator>
struct set_adaptor: assoc_container_adaptor<
    ICL_IMPL_SPACE::set<K, transparent_compare<Compare>, Allocator>>
{
    using super = assoc_container_adaptor<
        ICL_IMPL_SPACE::set<K, transparent_compare<Compare>, Allocator>>;

    using super::super;

    // Boost.ICL assumes set::iterator is the same type as set::const_iterator,
    // which is not guaranteed generally and is certainly not the case for
    // boost::container::set. We extend to reverse_iterator out of sympathy.

    using const_iterator = typename super::const_iterator;
    using iterator = const_iterator;
    using const_reverse_iterator = typename super::const_reverse_iterator;
    using reverse_iterator = const_reverse_iterator;
};

}}} // namespace detail icl boost

#endif
