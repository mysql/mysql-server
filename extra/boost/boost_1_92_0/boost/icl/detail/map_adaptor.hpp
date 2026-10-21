/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Joaquin M Lopez Munoz
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_DETAIL_MAP_ADAPTOR_HPP_JMLM_260321
#define BOOST_ICL_DETAIL_MAP_ADAPTOR_HPP_JMLM_260321

#include <boost/icl/detail/assoc_container_adaptor.hpp>

#include ICL_IMPL_PATH(map)

// let boostdep know about this potential dependency hidden by ICL_IMPL_PATH
#if 0
#include <boost/container/map.hpp>
#endif

namespace boost{namespace icl{namespace detail
{

template<class K, class T, class Compare, class Allocator>
using map_adaptor = assoc_container_adaptor<
    ICL_IMPL_SPACE::map<K, T, transparent_compare<Compare>, Allocator>>;

}}} // namespace detail icl boost

#endif
