/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Joaquin M Lopez Munoz
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef BOOST_ICL_DETAIL_ASSOC_CONTAINER_ADAPTOR_HPP_JMLM_260320
#define BOOST_ICL_DETAIL_ASSOC_CONTAINER_ADAPTOR_HPP_JMLM_260320

#include <boost/icl/impl_config.hpp>
#include <iterator>
#include <type_traits>
#include <utility>

namespace boost{namespace icl{namespace detail
{

/*-----------------------------------------------------------------------------+
| Interval comparison is generally a partial order rather than a strict weak   |
| order (SWO). This does not pose any problem with associative containers as   |
| long as the intervals in the container are disjoint, since the induced order |
| restricted to those is a SWO (indeed, a total order). A difficulty may arise |
| when doing a lookup operation for an interval k that overlaps with           |
| E = elements(container), as the induced order over {k} U E may not be a SWO. |
| All stdlib implementations support this case except libc++ v22 or higher:    |
|   https://github.com/llvm/llvm-project/issues/183189                         |
|   https://github.com/boostorg/icl/issues/51 .                                |
| Whether libc++'s behavior is conformant or not is contested, see             |
|   https://github.com/llvm/llvm-project/issues/187667 ,                       |
| but, regardless, we can solve the problem by resorting to heterogeneous      |
| lookup. When k is of a type other than key_type and the compare predicate is |
| transparent, lookup operations on k are defined by the standard in terms of  |
| elements being _partitioned_ by k, without any reference to SWO compliance.  |
|                                                                              |
| assoc_container_adaptor, in combination with transparent_compare, forces     |
| lookup operations on the adapted container to be routed through its          |
| heterogeneous lookup overloads, and does nothing for C++11 containers        |
| without het lookup. This circumvents libc++'s singular behavior except when  |
| in C++11 mode: in this case, we use Boost.Container, which supports het      |
| lookup even in C++11 (see impl_config.hpp). Insert functions are also        |
| provided that prevent UB when the element would violate SWO (this is UB      |
| regardless of the resolution of libc++'s issue).                             |
|                                                                              |
| Additionally, Boost.ICL interval find functions are documented to return the |
| _first_ eligible element, which is not guaranteed by std::(set|map)::find;   |
| assoc_container_adaptor fixes that.
+-----------------------------------------------------------------------------*/

template<class Compare>
struct transparent_compare: Compare
{
    using is_transparent = void;
    using super = Compare;

    using super::super;
};

template<class AssocContainer>
using assoc_container_is_set = std::is_same<
    typename AssocContainer::key_type, typename AssocContainer::value_type>;

template<typename InputIterator>
using assoc_container_enable_if_is_input_iterator_t =
    typename std::enable_if<
        std::is_convertible<
            typename std::iterator_traits<InputIterator>::iterator_category,
            std::input_iterator_tag
        >::value
    >::type;

template<class AssocContainer>
struct assoc_container_adaptor: AssocContainer
{
    using key_type = typename AssocContainer::key_type;
    using value_type = typename AssocContainer::value_type;
    using size_type = typename AssocContainer::size_type;
    using key_compare = typename AssocContainer::key_compare::super;
    using iterator = typename AssocContainer::iterator;
    using const_iterator = typename AssocContainer::const_iterator;

    using AssocContainer::AssocContainer;

    template<
        typename InputIterator,
        typename = assoc_container_enable_if_is_input_iterator_t<InputIterator>
    >
    void insert(InputIterator first, InputIterator last)
    {
        while(first != last) insert(*first++);
    }

    std::pair<iterator, bool> insert(const value_type& x)
    {
        auto it = lower_bound(key_from_value(x));
        if(it == AssocContainer::end() || AssocContainer::value_comp()(x, *it)) {
            return {AssocContainer::insert(it, x), true};
        }
        else {
            return {it, false};
        }
    }

    iterator insert(const_iterator pos, const value_type& x)
    {
        if((pos == AssocContainer::end() || AssocContainer::value_comp()(x, *pos)) &&
           (pos == AssocContainer::begin() || AssocContainer::value_comp()(*std::prev(pos), x))){
            return AssocContainer::insert(pos, x);
        }
        return insert(x).first;
    }

    size_type count(const key_type& key)
    {
        return AssocContainer::count(std::cref(key));
    }

    size_type count(const key_type& key) const
    {
        return AssocContainer::count(std::cref(key));
    }

    iterator find(const key_type& key)
    {
        auto it = AssocContainer::lower_bound(std::cref(key));
        return 
            it == AssocContainer::end() ||
            AssocContainer::key_comp()(key, key_from_value(*it))?
            AssocContainer::end(): it;
    }

    const_iterator find(const key_type& key) const
    {
        return const_cast<assoc_container_adaptor*>(this)->find(key);
    }

    std::pair<iterator, iterator> equal_range(const key_type& key)
    {
        return AssocContainer::equal_range(std::cref(key));
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        return AssocContainer::equal_range(std::cref(key));
    }

    iterator lower_bound(const key_type& key)
    {
        return AssocContainer::lower_bound(std::cref(key));
    }

    const_iterator lower_bound(const key_type& key) const
    {
        return AssocContainer::lower_bound(std::cref(key));
    }

    iterator upper_bound(const key_type& key)
    {
        return AssocContainer::upper_bound(std::cref(key));
    }

    const_iterator upper_bound(const key_type& key) const
    {
        return AssocContainer::upper_bound(std::cref(key));
    }

private:
    template<
        class IsSet = assoc_container_is_set<AssocContainer>,
        typename std::enable_if<IsSet::value>::type* = nullptr>
    static const key_type& key_from_value(const value_type& x)
    {
        return x;
    }

    template<
        class IsSet = assoc_container_is_set<AssocContainer>,
        typename std::enable_if<!IsSet::value>::type* = nullptr>
    static const key_type& key_from_value(const value_type& x)
    {
        return x.first;
    }
};

}}} // namespace detail icl boost

#endif
