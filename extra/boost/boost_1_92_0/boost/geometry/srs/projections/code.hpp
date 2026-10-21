// Boost.Geometry

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_CODE_HPP
#define BOOST_GEOMETRY_PROJECTIONS_CODE_HPP


#include <array>
#include <cmath>

#include <boost/geometry/srs/projections/dpar.hpp>


namespace boost { namespace geometry { namespace projections
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

    struct code_parameter {
        constexpr code_parameter() : id{-1}, val_i{-1} {}
        constexpr code_parameter(srs::dpar::name_f n, double v) : id{n}, val_d{v}, var_type{2} {}
        constexpr code_parameter(srs::dpar::name_r n, double v) : id{n}, val_d{v}, var_type{2} {}
        constexpr code_parameter(srs::dpar::name_i n, int v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_be n, bool v = true) : id{n}, val_b{v} {}
        constexpr code_parameter(srs::dpar::name_datum n, srs::dpar::value_datum v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_datum v) : id{srs::dpar::datum}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_ellps n, srs::dpar::value_ellps v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_ellps v) : id{srs::dpar::ellps}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_mode n, srs::dpar::value_mode v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_mode v) : id{srs::dpar::mode}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_orient n, srs::dpar::value_orient v) : id{srs::dpar::mode}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_orient v) : id{srs::dpar::orient}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_pm n, srs::dpar::value_pm v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_pm v) : id{srs::dpar::pm}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_pm n, double v) : id{n}, val_d{v}, var_type{2} {}
        constexpr code_parameter(srs::dpar::name_proj n, srs::dpar::value_proj v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_proj v) : id{srs::dpar::proj}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_sweep n, srs::dpar::value_sweep v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_sweep v) : id{srs::dpar::sweep}, val_i{v} {}
        constexpr code_parameter(srs::dpar::name_units n, srs::dpar::value_units v) : id{n}, val_i{v} {}
        constexpr code_parameter(srs::dpar::value_units v) : id{srs::dpar::units}, val_i{v} {}
        int id;
        union {
            bool val_b;
            int val_i;
            double val_d;
        };
        int var_type = 1;
        inline explicit operator srs::dpar::parameter<double>() const {
            using namespace srs::dpar;
            using p = parameter<double>;
            if (id == -1) return p{};
            if (id <= y_0) return p(static_cast<name_f>(id), val_d);
            if (id <= tilt) return p(static_cast<name_r>(id), val_d);
            if (id <= zone) return p(static_cast<name_i>(id), val_i);
            if (id <= south) return p(static_cast<name_be>(id), val_b);
            if (id == datum) return p(static_cast<name_datum>(id), static_cast<value_datum>(val_i));
            if (id == ellps) return p(static_cast<name_ellps>(id), static_cast<value_ellps>(val_i));
            if (id == mode) return p(static_cast<name_mode>(id), static_cast<value_mode>(val_i));
            if (id == orient) return p(static_cast<name_orient>(id), static_cast<value_orient>(val_i));
            if (id == pm && var_type == 1) return p(static_cast<name_pm>(id), static_cast<value_pm>(val_i));
            if (id == pm && var_type == 2) return p(static_cast<name_pm>(id), val_d);
            if (id <= proj) return p(static_cast<name_proj>(id), static_cast<value_proj>(val_i));
            if (id == name_sweep::sweep) return p(static_cast<name_sweep>(id), static_cast<value_sweep>(val_i));
            if (id == units) return p(static_cast<name_units>(id), static_cast<value_units>(val_i));
            return p{};
        }
    };

    struct code_element
    {
        constexpr code_element(int code,
                               std::array<code_parameter, 13> const& parameters,
                               std::array<double, 7> towgs84 =
                                {INFINITY, INFINITY, INFINITY, INFINITY, INFINITY, INFINITY, INFINITY})
            : code{code}, parameters{parameters}, towgs84{towgs84} {}
        constexpr code_element() : code{-1}, parameters{}, towgs84{} {}
        int code;
        std::array<code_parameter, 13> parameters;
        std::array<double, 7> towgs84;
        srs::dpar::parameters<double> to_parameters() const {
            srs::dpar::parameters<double> out;
            bool towgs84_to_be_inserted = towgs84[0] != INFINITY;
            for (const auto& p : parameters)
            {
                if (towgs84_to_be_inserted && (p.id == srs::dpar::units || p.id == srs::dpar::no_defs))
                {
                    towgs84_to_be_inserted = false;
                    out(srs::dpar::towgs84, towgs84);
                }
                out.add(p);
                if (p.id == srs::dpar::no_defs) break;
            }
            return out;
        }
    };

    struct code_element_less
    {
        inline bool operator()(code_element const& l, int code) const
        {
            return l.code < code;
        }
    };

}
#endif // DOXYGEN_NO_DETAIL


}}} // namespace boost::geometry::projections

#endif
