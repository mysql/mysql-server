// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013-2024.
// Modifications copyright (c) 2013-2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_TURN_INFO_HELPERS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_TURN_INFO_HELPERS_HPP

#include <boost/geometry/algorithms/detail/direction_code.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/policies/relate/intersection_policy.hpp>
#include <boost/geometry/strategies/intersection_result.hpp>

namespace boost { namespace geometry {

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay {

enum turn_position { position_middle, position_front, position_back };

template <typename Point, typename SegmentRatio>
struct turn_operation_linear
    : public turn_operation<Point, SegmentRatio>
{
    turn_operation_linear()
        : position(position_middle)
        , is_collinear(false)
    {}

    turn_position position;
    bool is_collinear; // valid only for Linear geometry
};

template
<
    typename UniqueSubRange1,
    typename UniqueSubRange2,
    typename Strategy
>
struct side_calculator
{
    using side_strategy_type = decltype(std::declval<Strategy>().side());

    inline side_calculator(UniqueSubRange1 const& range_p,
                           UniqueSubRange2 const& range_q,
                           Strategy const& strategy)
        : m_side_strategy(strategy.side())
        , m_range_p(range_p)
        , m_range_q(range_q)
    {}

    inline int pi_wrt_q1() const { return m_side_strategy.apply(get_qi(), get_qj(), get_pi()); }

    inline int pj_wrt_q1() const { return m_side_strategy.apply(get_qi(), get_qj(), get_pj()); }
    inline int pj_wrt_q2() const { return m_side_strategy.apply(get_qj(), get_qk(), get_pj()); }
    inline int qj_wrt_p1() const { return m_side_strategy.apply(get_pi(), get_pj(), get_qj()); }
    inline int qj_wrt_p2() const { return m_side_strategy.apply(get_pj(), get_pk(), get_qj()); }

    inline int pk_wrt_p1() const { return m_side_strategy.apply(get_pi(), get_pj(), get_pk()); }
    inline int pk_wrt_q1() const { return m_side_strategy.apply(get_qi(), get_qj(), get_pk()); }
    inline int qk_wrt_p1() const { return m_side_strategy.apply(get_pi(), get_pj(), get_qk()); }
    inline int qk_wrt_q1() const { return m_side_strategy.apply(get_qi(), get_qj(), get_qk()); }

    inline int pk_wrt_q2() const { return m_side_strategy.apply(get_qj(), get_qk(), get_pk()); }
    inline int qk_wrt_p2() const { return m_side_strategy.apply(get_pj(), get_pk(), get_qk()); }

    inline auto const& get_pi() const { return m_range_p.at(0); }
    inline auto const& get_pj() const { return m_range_p.at(1); }
    inline auto const& get_pk() const { return m_range_p.at(2); }

    inline auto const& get_qi() const { return m_range_q.at(0); }
    inline auto const& get_qj() const { return m_range_q.at(1); }
    inline auto const& get_qk() const { return m_range_q.at(2); }

    // Used side-strategy, owned by the calculator
    side_strategy_type m_side_strategy;

    // Used ranges - owned by get_turns or (for points) by intersection_info_base
    UniqueSubRange1 const& m_range_p;
    UniqueSubRange2 const& m_range_q;
};

template
<
    typename UniqueSubRange1, typename UniqueSubRange2,
    typename TurnPoint, typename UmbrellaStrategy
>
class intersection_info_base
{
public:

    using intersection_point_type = segment_intersection_points<TurnPoint>;
    using intersection_policy_type = policies::relate::segments_intersection_policy
        <
            intersection_point_type
        >;

    using result_type = typename intersection_policy_type::return_type;

    using side_calculator_type = side_calculator
        <
            UniqueSubRange1, UniqueSubRange2, UmbrellaStrategy
        >;

    using swapped_side_calculator_type = side_calculator
        <
            UniqueSubRange2, UniqueSubRange1, UmbrellaStrategy
        >;

    intersection_info_base(UniqueSubRange1 const& range_p,
                           UniqueSubRange2 const& range_q,
                           UmbrellaStrategy const& umbrella_strategy)
        : m_range_p(range_p)
        , m_range_q(range_q)
        , m_side_calc(range_p, range_q, umbrella_strategy)
        , m_swapped_side_calc(range_q, range_p, umbrella_strategy)
        , m_result(umbrella_strategy.relate()
                        .apply(range_p, range_q, intersection_policy_type()))
    {}

    inline bool p_is_last_segment() const { return m_range_p.is_last_segment(); }
    inline bool q_is_last_segment() const { return m_range_q.is_last_segment(); }

    inline auto const& rpi() const { return m_side_calc.get_pi(); }
    inline auto const& rpj() const { return m_side_calc.get_pj(); }
    inline auto const& rpk() const { return m_side_calc.get_pk(); }

    inline auto const& rqi() const { return m_side_calc.get_qi(); }
    inline auto const& rqj() const { return m_side_calc.get_qj(); }
    inline auto const& rqk() const { return m_side_calc.get_qk(); }

    inline side_calculator_type const& sides() const { return m_side_calc; }
    inline swapped_side_calculator_type const& swapped_sides() const
    {
        return m_swapped_side_calc;
    }

private :
    // Owned by get_turns
    UniqueSubRange1 const& m_range_p;
    UniqueSubRange2 const& m_range_q;

    // Owned by this class
    side_calculator_type m_side_calc;
    swapped_side_calculator_type m_swapped_side_calc;

protected :
    result_type m_result;
};


template
<
    typename UniqueSubRange1, typename UniqueSubRange2,
    typename TurnPoint,
    typename UmbrellaStrategy
>
class intersection_info
    : public intersection_info_base<UniqueSubRange1, UniqueSubRange2,
        TurnPoint, UmbrellaStrategy>
{
    using base = intersection_info_base<UniqueSubRange1, UniqueSubRange2,
        TurnPoint, UmbrellaStrategy>;

public:

    using cs_tag = typename UmbrellaStrategy::cs_tag;

    using side_calculator_type = typename base::side_calculator_type;
    using result_type = typename base::result_type;

    using i_info_type = typename result_type::intersection_points_type;
    using d_info_type = typename result_type::direction_type;

    intersection_info(UniqueSubRange1 const& range_p,
                      UniqueSubRange2 const& range_q,
                      UmbrellaStrategy const& umbrella_strategy)
        : base(range_p, range_q, umbrella_strategy)
        , m_umbrella_strategy(umbrella_strategy)
    {}

    inline result_type const& result() const { return base::m_result; }
    inline i_info_type const& i_info() const { return base::m_result.intersection_points; }
    inline d_info_type const& d_info() const { return base::m_result.direction; }

    // TODO: it's more like is_spike_ip_p
    inline bool is_spike_p() const
    {
        if (base::p_is_last_segment())
        {
            return false;
        }
        if (base::sides().pk_wrt_p1() == 0)
        {
            // p:  pi--------pj--------pk
            // or: pi----pk==pj

            if (! is_ip_j<0>())
            {
                return false;
            }

            // TODO: why is q used to determine spike property in p?
            bool const has_qk = ! base::q_is_last_segment();
            int const qk_p1 = has_qk ? base::sides().qk_wrt_p1() : 0;
            int const qk_p2 = has_qk ? base::sides().qk_wrt_p2() : 0;

            if (qk_p1 == -qk_p2)
            {
                if (qk_p1 == 0)
                {
                    // qk is collinear with both p1 and p2,
                    // verify if pk goes backwards w.r.t. pi/pj
                    return direction_code<cs_tag>(base::rpi(), base::rpj(), base::rpk()) == -1;
                }

                // qk is at opposite side of p1/p2, therefore
                // p1/p2 (collinear) are opposite and form a spike
                return true;
            }
        }

        return false;
    }

    inline bool is_spike_q() const
    {
        if (base::q_is_last_segment())
        {
            return false;
        }

        // See comments at is_spike_p
        if (base::sides().qk_wrt_q1() == 0)
        {
            if (! is_ip_j<1>())
            {
                return false;
            }

            // TODO: why is p used to determine spike property in q?
            bool const has_pk = ! base::p_is_last_segment();
            int const pk_q1 = has_pk ? base::sides().pk_wrt_q1() : 0;
            int const pk_q2 = has_pk ? base::sides().pk_wrt_q2() : 0;

            if (pk_q1 == -pk_q2)
            {
                if (pk_q1 == 0)
                {
                    return direction_code<cs_tag>(base::rqi(), base::rqj(), base::rqk()) == -1;
                }

                return true;
            }
        }

        return false;
    }

    UmbrellaStrategy const& strategy() const
    {
        return m_umbrella_strategy;
    }

private:
    template <std::size_t OpId>
    bool is_ip_j() const
    {
        int arrival = d_info().arrival[OpId];
        bool same_dirs = d_info().dir_a == 0 && d_info().dir_b == 0;

        if (same_dirs)
        {
            if (i_info().count == 2)
            {
                return arrival != -1;
            }
            else
            {
                return arrival == 0;
            }
        }
        else
        {
            return arrival == 1;
        }
    }

    UmbrellaStrategy const& m_umbrella_strategy;
};

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_TURN_INFO_HELPERS_HPP
