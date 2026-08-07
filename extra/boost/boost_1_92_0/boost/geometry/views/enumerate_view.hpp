// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2024 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_VIEWS_ENUMERATE_VIEW_HPP
#define BOOST_GEOMETRY_VIEWS_ENUMERATE_VIEW_HPP

#include <algorithm>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_categories.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/difference_type.hpp>
#include <boost/range/reference.hpp>
#include <boost/range/value_type.hpp>

#include <boost/geometry/util/type_traits_std.hpp>

namespace boost { namespace geometry
{

namespace util
{

// This view is a range of values, each with an index
// It is used to iterate over a range, and to get the index of the value
// It is used in the enumerate function
// The typename Range can either be const or non-const
template <typename Range>
struct enumerated_view
{
    // The return value of the iterator
    struct value_with_index
    {
        using type = util::transcribe_const_t
            <
                Range,
                typename boost::range_value<Range>::type
            >;

        // Member variable index contains the zero-based index of the value in the range
        std::size_t const index;

        // Member variable value contains a const or non-const reference to the value itself
        type& value;  
    };

private:
    // Iterator implementation, not exposed.
    struct enumerating_iterator
        : public boost::iterator_facade
        <
            enumerating_iterator,
            value_with_index const,
            boost::random_access_traversal_tag,
            value_with_index const,
            typename boost::range_difference<Range>::type
        >
    {
        using reference = value_with_index;
        using difference_type = typename boost::range_difference<Range>::type;

        // Constructor with the range it handles
        explicit inline enumerating_iterator(Range& range)
            : m_begin(boost::begin(range))
            , m_end(boost::end(range))
            , m_iterator(boost::begin(range))
        {}

        // Constructor to indicate the end of a range
        explicit inline enumerating_iterator(Range& range, bool)
            : m_begin(boost::begin(range))
            , m_end(boost::end(range))
            , m_iterator(boost::end(range))
        {}

        // There is no default constructor
        enumerating_iterator() = delete;

        inline reference dereference() const
        {
            constexpr difference_type zero = 0;
            const std::size_t index = (std::max)(zero, std::distance(m_begin, m_iterator));
            const value_with_index result{index, *m_iterator};
            return result;
        }

        inline difference_type distance_to(enumerating_iterator const& other) const
        {
            return std::distance(other.m_iterator, m_iterator);
        }

        inline bool equal(enumerating_iterator const& other) const
        {
            return
                m_begin == other.m_begin 
                && m_end == other.m_end 
                && m_iterator == other.m_iterator;
        }

        inline void increment()
        {
            ++m_iterator;
        }

        inline void decrement()
        {
            --m_iterator;
        }

        inline void advance(difference_type n)
        {
            std::advance(m_iterator, n);
        }

        const typename boost::range_iterator<Range>::type m_begin;
        const typename boost::range_iterator<Range>::type m_end;

        typename boost::range_iterator<Range>::type m_iterator;
    };

public:
    using iterator = enumerating_iterator;
    using const_iterator = enumerating_iterator;

    explicit inline enumerated_view(Range& range)
        : m_begin(range)
        , m_end(range, true)
    {}

    inline iterator begin() const { return m_begin; }
    inline iterator end() const { return m_end; }

private:
    const iterator m_begin;
    const iterator m_end;
};

// Helper function to create the enumerated view, for a const range
template <typename Range>
inline auto enumerate(Range const& range)
{
    return util::enumerated_view<Range const>(range);    
}

// Helper function to create the enumerated view, for a non-const range
template <typename Range>
inline auto enumerate(Range& range)
{
    return util::enumerated_view<Range>(range);    
}

}}} // boost::geometry::util


#endif // BOOST_GEOMETRY_VIEWS_ENUMERATE_VIEW_HPP
