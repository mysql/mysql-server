// Boost.Geometry

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_PROJECTIONS_IAU2000_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IAU2000_HPP


#include <algorithm>

#include <boost/geometry/srs/projections/code.hpp>


namespace boost { namespace geometry { namespace projections
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

    inline srs::dpar::parameters<> iau2000_to_parameters(int code)
    {
        using namespace srs::dpar;

        using p = code_parameter;
        using ps = std::array<p, 13>;
        using ce = code_element;

        static constexpr std::array<code_element, 1400> arr = {
            ce(19900, ps{p(proj_longlat), p(r,2439700), p(no_defs)}),
            ce(19901, ps{p(proj_longlat), p(r,2439700), p(no_defs)}),
            //{19910}
            //{19911}
            //{19912}
            //{19913}
            ce(19914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            //{19968}
            //{19969}
            //{19970}
            //{19971}
            ce(19972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            ce(19973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2439700), p(units_m), p(no_defs)}),
            //{19974}
            //{19975}
            //{19976}
            //{19977}
            //{19978}
            //{19979}
            ce(29900, ps{p(proj_longlat), p(r,6051800), p(no_defs)}),
            ce(29901, ps{p(proj_longlat), p(r,6051800), p(no_defs)}),
            //{29910}
            //{29911}
            //{29912}
            //{29913}
            ce(29914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            //{29968}
            //{29969}
            //{29970}
            //{29971}
            ce(29972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            ce(29973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,6051800), p(units_m), p(no_defs)}),
            //{29974}
            //{29975}
            //{29976}
            //{29977}
            //{29978}
            //{29979}
            ce(30100, ps{p(proj_longlat), p(r,1737400), p(no_defs)}),
            ce(30101, ps{p(proj_longlat), p(r,1737400), p(no_defs)}),
            //{30110}
            //{30111}
            //{30112}
            //{30113}
            ce(30114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            //{30168}
            //{30169}
            //{30170}
            //{30171}
            ce(30172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            ce(30173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1737400), p(units_m), p(no_defs)}),
            //{30174}
            //{30175}
            //{30176}
            //{30177}
            //{30178}
            //{30179}
            ce(39900, ps{p(proj_longlat), p(a,6378140), p(b,6356750), p(no_defs)}),
            ce(39901, ps{p(proj_longlat), p(a,6378140), p(b,6356750), p(no_defs)}),
            //{39910}
            //{39911}
            //{39912}
            //{39913}
            ce(39914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            //{39968}
            //{39969}
            //{39970}
            //{39971}
            ce(39972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            ce(39973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,6378140), p(b,6356750), p(units_m), p(no_defs)}),
            //{39974}
            //{39975}
            //{39976}
            //{39977}
            //{39978}
            //{39979}
            ce(40100, ps{p(proj_longlat), p(a,13400), p(b,9200), p(no_defs)}),
            ce(40101, ps{p(proj_longlat), p(a,13400), p(b,9200), p(no_defs)}),
            //{40110}
            //{40111}
            //{40112}
            //{40113}
            ce(40114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            //{40168}
            //{40169}
            //{40170}
            //{40171}
            ce(40172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            ce(40173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,13400), p(b,9200), p(units_m), p(no_defs)}),
            //{40174}
            //{40175}
            //{40176}
            //{40177}
            //{40178}
            //{40179}
            ce(40200, ps{p(proj_longlat), p(a,7500), p(b,5200), p(no_defs)}),
            ce(40201, ps{p(proj_longlat), p(a,7500), p(b,5200), p(no_defs)}),
            //{40210}
            //{40211}
            //{40212}
            //{40213}
            ce(40214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            //{40268}
            //{40269}
            //{40270}
            //{40271}
            ce(40272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            ce(40273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,7500), p(b,5200), p(units_m), p(no_defs)}),
            //{40274}
            //{40275}
            //{40276}
            //{40277}
            //{40278}
            //{40279}
            ce(49900, ps{p(proj_longlat), p(a,3396190), p(b,3376200), p(no_defs)}),
            ce(49901, ps{p(proj_longlat), p(a,3396190), p(b,3376200), p(no_defs)}),
            //{49910}
            //{49911}
            //{49912}
            //{49913}
            ce(49914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            //{49968}
            //{49969}
            //{49970}
            //{49971}
            ce(49972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            ce(49973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,3396190), p(b,3376200), p(units_m), p(no_defs)}),
            //{49974}
            //{49975}
            //{49976}
            //{49977}
            //{49978}
            //{49979}
            ce(50100, ps{p(proj_longlat), p(a,1829400), p(b,1815700), p(no_defs)}),
            ce(50101, ps{p(proj_longlat), p(a,1829400), p(b,1815700), p(no_defs)}),
            //{50110}
            //{50111}
            //{50112}
            //{50113}
            ce(50114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            //{50168}
            //{50169}
            //{50170}
            //{50171}
            ce(50172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            ce(50173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1829400), p(b,1815700), p(units_m), p(no_defs)}),
            //{50174}
            //{50175}
            //{50176}
            //{50177}
            //{50178}
            //{50179}
            ce(50200, ps{p(proj_longlat), p(a,1564130), p(b,1560930), p(no_defs)}),
            ce(50201, ps{p(proj_longlat), p(a,1564130), p(b,1560930), p(no_defs)}),
            //{50210}
            //{50211}
            //{50212}
            //{50213}
            ce(50214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            //{50268}
            //{50269}
            //{50270}
            //{50271}
            ce(50272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            ce(50273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,1564130), p(b,1560930), p(units_m), p(no_defs)}),
            //{50274}
            //{50275}
            //{50276}
            //{50277}
            //{50278}
            //{50279}
            ce(50300, ps{p(proj_longlat), p(a,2632400), p(b,2632350), p(no_defs)}),
            ce(50301, ps{p(proj_longlat), p(a,2632400), p(b,2632350), p(no_defs)}),
            //{50310}
            //{50311}
            //{50312}
            //{50313}
            ce(50314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            //{50368}
            //{50369}
            //{50370}
            //{50371}
            ce(50372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            ce(50373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2632400), p(b,2632350), p(units_m), p(no_defs)}),
            //{50374}
            //{50375}
            //{50376}
            //{50377}
            //{50378}
            //{50379}
            ce(50400, ps{p(proj_longlat), p(a,2409400), p(b,2409300), p(no_defs)}),
            ce(50401, ps{p(proj_longlat), p(a,2409400), p(b,2409300), p(no_defs)}),
            //{50410}
            //{50411}
            //{50412}
            //{50413}
            ce(50414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            //{50468}
            //{50469}
            //{50470}
            //{50471}
            ce(50472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            ce(50473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,2409400), p(b,2409300), p(units_m), p(no_defs)}),
            //{50474}
            //{50475}
            //{50476}
            //{50477}
            //{50478}
            //{50479}
            ce(50500, ps{p(proj_longlat), p(a,125000), p(b,64000), p(no_defs)}),
            ce(50501, ps{p(proj_longlat), p(a,125000), p(b,64000), p(no_defs)}),
            //{50510}
            //{50511}
            //{50512}
            //{50513}
            ce(50514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            //{50568}
            //{50569}
            //{50570}
            //{50571}
            ce(50572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            ce(50573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,125000), p(b,64000), p(units_m), p(no_defs)}),
            //{50574}
            //{50575}
            //{50576}
            //{50577}
            //{50578}
            //{50579}
            ce(50600, ps{p(proj_longlat), p(r,85000), p(no_defs)}),
            ce(50601, ps{p(proj_longlat), p(r,85000), p(no_defs)}),
            //{50610}
            //{50611}
            //{50612}
            //{50613}
            ce(50614, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50615, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50616, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50617, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50618, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50619, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50620, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50621, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50660, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50661, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50662, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50663, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50664, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50665, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50666, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50667, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            //{50668}
            //{50669}
            //{50670}
            //{50671}
            ce(50672, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            ce(50673, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,85000), p(units_m), p(no_defs)}),
            //{50674}
            //{50675}
            //{50676}
            //{50677}
            //{50678}
            //{50679}
            ce(50700, ps{p(proj_longlat), p(r,40000), p(no_defs)}),
            ce(50701, ps{p(proj_longlat), p(r,40000), p(no_defs)}),
            //{50710}
            //{50711}
            //{50712}
            //{50713}
            ce(50714, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50715, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50716, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50717, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50718, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50719, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50720, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50721, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50760, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50761, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50762, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50763, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50764, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50765, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50766, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50767, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            //{50768}
            //{50769}
            //{50770}
            //{50771}
            ce(50772, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(50773, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            //{50774}
            //{50775}
            //{50776}
            //{50777}
            //{50778}
            //{50779}
            ce(50800, ps{p(proj_longlat), p(r,18000), p(no_defs)}),
            ce(50801, ps{p(proj_longlat), p(r,18000), p(no_defs)}),
            //{50810}
            //{50811}
            //{50812}
            //{50813}
            ce(50814, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50815, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50816, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50817, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50818, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50819, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50820, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50821, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50860, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50861, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50862, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50863, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50864, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50865, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50866, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50867, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            //{50868}
            //{50869}
            //{50870}
            //{50871}
            ce(50872, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            ce(50873, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,18000), p(units_m), p(no_defs)}),
            //{50874}
            //{50875}
            //{50876}
            //{50877}
            //{50878}
            //{50879}
            ce(50900, ps{p(proj_longlat), p(r,14000), p(no_defs)}),
            ce(50901, ps{p(proj_longlat), p(r,14000), p(no_defs)}),
            //{50910}
            //{50911}
            //{50912}
            //{50913}
            ce(50914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            //{50968}
            //{50969}
            //{50970}
            //{50971}
            ce(50972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            ce(50973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,14000), p(units_m), p(no_defs)}),
            //{50974}
            //{50975}
            //{50976}
            //{50977}
            //{50978}
            //{50979}
            ce(51000, ps{p(proj_longlat), p(r,12000), p(no_defs)}),
            ce(51001, ps{p(proj_longlat), p(r,12000), p(no_defs)}),
            //{51010}
            //{51011}
            //{51012}
            //{51013}
            ce(51014, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51015, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51016, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51017, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51018, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51019, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51020, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51021, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51060, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51061, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51062, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51063, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51064, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51065, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51066, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51067, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            //{51068}
            //{51069}
            //{51070}
            //{51071}
            ce(51072, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            ce(51073, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,12000), p(units_m), p(no_defs)}),
            //{51074}
            //{51075}
            //{51076}
            //{51077}
            //{51078}
            //{51079}
            ce(51100, ps{p(proj_longlat), p(r,15000), p(no_defs)}),
            ce(51101, ps{p(proj_longlat), p(r,15000), p(no_defs)}),
            //{51110}
            //{51111}
            //{51112}
            //{51113}
            ce(51114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            //{51168}
            //{51169}
            //{51170}
            //{51171}
            ce(51172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(51173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            //{51174}
            //{51175}
            //{51176}
            //{51177}
            //{51178}
            //{51179}
            ce(51200, ps{p(proj_longlat), p(r,10000), p(no_defs)}),
            ce(51201, ps{p(proj_longlat), p(r,10000), p(no_defs)}),
            //{51210}
            //{51211}
            //{51212}
            //{51213}
            ce(51214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            //{51268}
            //{51269}
            //{51270}
            //{51271}
            ce(51272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(51273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            //{51274}
            //{51275}
            //{51276}
            //{51277}
            //{51278}
            //{51279}
            ce(51300, ps{p(proj_longlat), p(r,5000), p(no_defs)}),
            ce(51301, ps{p(proj_longlat), p(r,5000), p(no_defs)}),
            //{51310}
            //{51311}
            //{51312}
            //{51313}
            ce(51314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            //{51368}
            //{51369}
            //{51370}
            //{51371}
            ce(51372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            ce(51373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,5000), p(units_m), p(no_defs)}),
            //{51374}
            //{51375}
            //{51376}
            //{51377}
            //{51378}
            //{51379}
            ce(51400, ps{p(proj_longlat), p(a,58000), p(b,42000), p(no_defs)}),
            ce(51401, ps{p(proj_longlat), p(a,58000), p(b,42000), p(no_defs)}),
            //{51410}
            //{51411}
            //{51412}
            //{51413}
            ce(51414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            //{51468}
            //{51469}
            //{51470}
            //{51471}
            ce(51472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            ce(51473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,58000), p(b,42000), p(units_m), p(no_defs)}),
            //{51474}
            //{51475}
            //{51476}
            //{51477}
            //{51478}
            //{51479}
            ce(51500, ps{p(proj_longlat), p(a,10000), p(b,7000), p(no_defs)}),
            ce(51501, ps{p(proj_longlat), p(a,10000), p(b,7000), p(no_defs)}),
            //{51510}
            //{51511}
            //{51512}
            //{51513}
            ce(51514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            //{51568}
            //{51569}
            //{51570}
            //{51571}
            ce(51572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            ce(51573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,10000), p(b,7000), p(units_m), p(no_defs)}),
            //{51574}
            //{51575}
            //{51576}
            //{51577}
            //{51578}
            //{51579}
            ce(51600, ps{p(proj_longlat), p(r,21500), p(no_defs)}),
            ce(51601, ps{p(proj_longlat), p(r,21500), p(no_defs)}),
            //{51610}
            //{51611}
            //{51612}
            //{51613}
            ce(51614, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51615, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51616, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51617, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51618, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51619, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51620, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51621, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51660, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51661, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51662, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51663, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51664, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51665, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51666, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51667, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            //{51668}
            //{51669}
            //{51670}
            //{51671}
            ce(51672, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            ce(51673, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21500), p(units_m), p(no_defs)}),
            //{51674}
            //{51675}
            //{51676}
            //{51677}
            //{51678}
            //{51679}
            ce(59900, ps{p(proj_longlat), p(a,71492000), p(b,66854000), p(no_defs)}),
            ce(59901, ps{p(proj_longlat), p(a,71492000), p(b,66854000), p(no_defs)}),
            //{59910}
            //{59911}
            //{59912}
            //{59913}
            ce(59914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            //{59968}
            //{59969}
            //{59970}
            //{59971}
            ce(59972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            ce(59973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,71492000), p(b,66854000), p(units_m), p(no_defs)}),
            //{59974}
            //{59975}
            //{59976}
            //{59977}
            //{59978}
            //{59979}
            ce(60100, ps{p(proj_longlat), p(a,209100), p(b,191400), p(no_defs)}),
            ce(60101, ps{p(proj_longlat), p(a,209100), p(b,191400), p(no_defs)}),
            //{60110}
            //{60111}
            //{60112}
            //{60113}
            ce(60114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            //{60168}
            //{60169}
            //{60170}
            //{60171}
            ce(60172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            ce(60173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,209100), p(b,191400), p(units_m), p(no_defs)}),
            //{60174}
            //{60175}
            //{60176}
            //{60177}
            //{60178}
            //{60179}
            ce(60200, ps{p(proj_longlat), p(a,256300), p(b,244600), p(no_defs)}),
            ce(60201, ps{p(proj_longlat), p(a,256300), p(b,244600), p(no_defs)}),
            //{60210}
            //{60211}
            //{60212}
            //{60213}
            ce(60214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            //{60268}
            //{60269}
            //{60270}
            //{60271}
            ce(60272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            ce(60273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,256300), p(b,244600), p(units_m), p(no_defs)}),
            //{60274}
            //{60275}
            //{60276}
            //{60277}
            //{60278}
            //{60279}
            ce(60300, ps{p(proj_longlat), p(a,535600), p(b,525800), p(no_defs)}),
            ce(60301, ps{p(proj_longlat), p(a,535600), p(b,525800), p(no_defs)}),
            //{60310}
            //{60311}
            //{60312}
            //{60313}
            ce(60314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            //{60368}
            //{60369}
            //{60370}
            //{60371}
            ce(60372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            ce(60373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,535600), p(b,525800), p(units_m), p(no_defs)}),
            //{60374}
            //{60375}
            //{60376}
            //{60377}
            //{60378}
            //{60379}
            ce(60400, ps{p(proj_longlat), p(r,560000), p(no_defs)}),
            ce(60401, ps{p(proj_longlat), p(r,560000), p(no_defs)}),
            //{60410}
            //{60411}
            //{60412}
            //{60413}
            ce(60414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            //{60468}
            //{60469}
            //{60470}
            //{60471}
            ce(60472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            ce(60473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,560000), p(units_m), p(no_defs)}),
            //{60474}
            //{60475}
            //{60476}
            //{60477}
            //{60478}
            //{60479}
            ce(60500, ps{p(proj_longlat), p(r,764000), p(no_defs)}),
            ce(60501, ps{p(proj_longlat), p(r,764000), p(no_defs)}),
            //{60510}
            //{60511}
            //{60512}
            //{60513}
            ce(60514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            //{60568}
            //{60569}
            //{60570}
            //{60571}
            ce(60572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            ce(60573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,764000), p(units_m), p(no_defs)}),
            //{60574}
            //{60575}
            //{60576}
            //{60577}
            //{60578}
            //{60579}
            ce(60600, ps{p(proj_longlat), p(r,2575000), p(no_defs)}),
            ce(60601, ps{p(proj_longlat), p(r,2575000), p(no_defs)}),
            //{60610}
            //{60611}
            //{60612}
            //{60613}
            ce(60614, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60615, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60616, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60617, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60618, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60619, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60620, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60621, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60660, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60661, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60662, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60663, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60664, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60665, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60666, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60667, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            //{60668}
            //{60669}
            //{60670}
            //{60671}
            ce(60672, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            ce(60673, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,2575000), p(units_m), p(no_defs)}),
            //{60674}
            //{60675}
            //{60676}
            //{60677}
            //{60678}
            //{60679}
            ce(60700, ps{p(proj_longlat), p(a,164000), p(b,107000), p(no_defs)}),
            ce(60701, ps{p(proj_longlat), p(a,164000), p(b,107000), p(no_defs)}),
            //{60710}
            //{60711}
            //{60712}
            //{60713}
            ce(60714, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60715, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60716, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60717, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60718, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60719, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60720, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60721, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60760, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60761, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60762, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60763, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60764, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60765, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60766, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60767, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            //{60768}
            //{60769}
            //{60770}
            //{60771}
            ce(60772, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            ce(60773, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,164000), p(b,107000), p(units_m), p(no_defs)}),
            //{60774}
            //{60775}
            //{60776}
            //{60777}
            //{60778}
            //{60779}
            ce(60800, ps{p(proj_longlat), p(r,718000), p(no_defs)}),
            ce(60801, ps{p(proj_longlat), p(r,718000), p(no_defs)}),
            //{60810}
            //{60811}
            //{60812}
            //{60813}
            ce(60814, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60815, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60816, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60817, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60818, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60819, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60820, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60821, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60860, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60861, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60862, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60863, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60864, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60865, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60866, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60867, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            //{60868}
            //{60869}
            //{60870}
            //{60871}
            ce(60872, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            ce(60873, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,718000), p(units_m), p(no_defs)}),
            //{60874}
            //{60875}
            //{60876}
            //{60877}
            //{60878}
            //{60879}
            ce(60900, ps{p(proj_longlat), p(a,115000), p(b,105000), p(no_defs)}),
            ce(60901, ps{p(proj_longlat), p(a,115000), p(b,105000), p(no_defs)}),
            //{60910}
            //{60911}
            //{60912}
            //{60913}
            ce(60914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            //{60968}
            //{60969}
            //{60970}
            //{60971}
            ce(60972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            ce(60973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,115000), p(b,105000), p(units_m), p(no_defs)}),
            //{60974}
            //{60975}
            //{60976}
            //{60977}
            //{60978}
            //{60979}
            ce(61000, ps{p(proj_longlat), p(a,97000), p(b,77000), p(no_defs)}),
            ce(61001, ps{p(proj_longlat), p(a,97000), p(b,77000), p(no_defs)}),
            //{61010}
            //{61011}
            //{61012}
            //{61013}
            ce(61014, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61015, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61016, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61017, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61018, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61019, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61020, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61021, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61060, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61061, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61062, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61063, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61064, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61065, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61066, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61067, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            //{61068}
            //{61069}
            //{61070}
            //{61071}
            ce(61072, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            ce(61073, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,97000), p(b,77000), p(units_m), p(no_defs)}),
            //{61074}
            //{61075}
            //{61076}
            //{61077}
            //{61078}
            //{61079}
            ce(61100, ps{p(proj_longlat), p(a,69000), p(b,55000), p(no_defs)}),
            ce(61101, ps{p(proj_longlat), p(a,69000), p(b,55000), p(no_defs)}),
            //{61110}
            //{61111}
            //{61112}
            //{61113}
            ce(61114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            //{61168}
            //{61169}
            //{61170}
            //{61171}
            ce(61172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            ce(61173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,69000), p(b,55000), p(units_m), p(no_defs)}),
            //{61174}
            //{61175}
            //{61176}
            //{61177}
            //{61178}
            //{61179}
            ce(61200, ps{p(proj_longlat), p(r,16000), p(no_defs)}),
            ce(61201, ps{p(proj_longlat), p(r,16000), p(no_defs)}),
            //{61210}
            //{61211}
            //{61212}
            //{61213}
            ce(61214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            //{61268}
            //{61269}
            //{61270}
            //{61271}
            ce(61272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            ce(61273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,16000), p(units_m), p(no_defs)}),
            //{61274}
            //{61275}
            //{61276}
            //{61277}
            //{61278}
            //{61279}
            ce(61300, ps{p(proj_longlat), p(a,15000), p(b,7500), p(no_defs)}),
            ce(61301, ps{p(proj_longlat), p(a,15000), p(b,7500), p(no_defs)}),
            //{61310}
            //{61311}
            //{61312}
            //{61313}
            ce(61314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            //{61368}
            //{61369}
            //{61370}
            //{61371}
            ce(61372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            ce(61373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,7500), p(units_m), p(no_defs)}),
            //{61374}
            //{61375}
            //{61376}
            //{61377}
            //{61378}
            //{61379}
            ce(61400, ps{p(proj_longlat), p(a,15000), p(b,8000), p(no_defs)}),
            ce(61401, ps{p(proj_longlat), p(a,15000), p(b,8000), p(no_defs)}),
            //{61410}
            //{61411}
            //{61412}
            //{61413}
            ce(61414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            //{61468}
            //{61469}
            //{61470}
            //{61471}
            ce(61472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            ce(61473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,15000), p(b,8000), p(units_m), p(no_defs)}),
            //{61474}
            //{61475}
            //{61476}
            //{61477}
            //{61478}
            //{61479}
            ce(61500, ps{p(proj_longlat), p(a,18500), p(b,13500), p(no_defs)}),
            ce(61501, ps{p(proj_longlat), p(a,18500), p(b,13500), p(no_defs)}),
            //{61510}
            //{61511}
            //{61512}
            //{61513}
            ce(61514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            //{61568}
            //{61569}
            //{61570}
            //{61571}
            ce(61572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            ce(61573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,18500), p(b,13500), p(units_m), p(no_defs)}),
            //{61574}
            //{61575}
            //{61576}
            //{61577}
            //{61578}
            //{61579}
            ce(61600, ps{p(proj_longlat), p(a,74000), p(b,34000), p(no_defs)}),
            ce(61601, ps{p(proj_longlat), p(a,74000), p(b,34000), p(no_defs)}),
            //{61610}
            //{61611}
            //{61612}
            //{61613}
            ce(61614, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61615, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61616, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61617, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61618, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61619, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61620, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61621, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61660, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61661, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61662, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61663, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61664, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61665, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61666, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61667, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            //{61668}
            //{61669}
            //{61670}
            //{61671}
            ce(61672, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            ce(61673, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,74000), p(b,34000), p(units_m), p(no_defs)}),
            //{61674}
            //{61675}
            //{61676}
            //{61677}
            //{61678}
            //{61679}
            ce(61700, ps{p(proj_longlat), p(a,55000), p(b,31000), p(no_defs)}),
            ce(61701, ps{p(proj_longlat), p(a,55000), p(b,31000), p(no_defs)}),
            //{61710}
            //{61711}
            //{61712}
            //{61713}
            ce(61714, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61715, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61716, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61717, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61718, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61719, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61720, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61721, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61760, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61761, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61762, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61763, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61764, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61765, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61766, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61767, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            //{61768}
            //{61769}
            //{61770}
            //{61771}
            ce(61772, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            ce(61773, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,55000), p(b,31000), p(units_m), p(no_defs)}),
            //{61774}
            //{61775}
            //{61776}
            //{61777}
            //{61778}
            //{61779}
            ce(61800, ps{p(proj_longlat), p(r,10000), p(no_defs)}),
            ce(61801, ps{p(proj_longlat), p(r,10000), p(no_defs)}),
            //{61810}
            //{61811}
            //{61812}
            //{61813}
            ce(61814, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61815, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61816, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61817, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61818, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61819, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61820, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61821, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61860, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61861, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61862, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61863, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61864, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61865, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61866, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61867, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            //{61868}
            //{61869}
            //{61870}
            //{61871}
            ce(61872, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            ce(61873, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,10000), p(units_m), p(no_defs)}),
            //{61874}
            //{61875}
            //{61876}
            //{61877}
            //{61878}
            //{61879}
            ce(69900, ps{p(proj_longlat), p(a,60268000), p(b,54364000), p(no_defs)}),
            ce(69901, ps{p(proj_longlat), p(a,60268000), p(b,54364000), p(no_defs)}),
            //{69910}
            //{69911}
            //{69912}
            //{69913}
            ce(69914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            //{69968}
            //{69969}
            //{69970}
            //{69971}
            ce(69972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            ce(69973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,60268000), p(b,54364000), p(units_m), p(no_defs)}),
            //{69974}
            //{69975}
            //{69976}
            //{69977}
            //{69978}
            //{69979}
            ce(70100, ps{p(proj_longlat), p(a,581100), p(b,577700), p(no_defs)}),
            ce(70101, ps{p(proj_longlat), p(a,581100), p(b,577700), p(no_defs)}),
            //{70110}
            //{70111}
            //{70112}
            //{70113}
            ce(70114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            //{70168}
            //{70169}
            //{70170}
            //{70171}
            ce(70172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            ce(70173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,581100), p(b,577700), p(units_m), p(no_defs)}),
            //{70174}
            //{70175}
            //{70176}
            //{70177}
            //{70178}
            //{70179}
            ce(70200, ps{p(proj_longlat), p(r,584700), p(no_defs)}),
            ce(70201, ps{p(proj_longlat), p(r,584700), p(no_defs)}),
            //{70210}
            //{70211}
            //{70212}
            //{70213}
            ce(70214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            //{70268}
            //{70269}
            //{70270}
            //{70271}
            ce(70272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            ce(70273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,584700), p(units_m), p(no_defs)}),
            //{70274}
            //{70275}
            //{70276}
            //{70277}
            //{70278}
            //{70279}
            ce(70300, ps{p(proj_longlat), p(r,788900), p(no_defs)}),
            ce(70301, ps{p(proj_longlat), p(r,788900), p(no_defs)}),
            //{70310}
            //{70311}
            //{70312}
            //{70313}
            ce(70314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            //{70368}
            //{70369}
            //{70370}
            //{70371}
            ce(70372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            ce(70373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,788900), p(units_m), p(no_defs)}),
            //{70374}
            //{70375}
            //{70376}
            //{70377}
            //{70378}
            //{70379}
            ce(70400, ps{p(proj_longlat), p(r,761400), p(no_defs)}),
            ce(70401, ps{p(proj_longlat), p(r,761400), p(no_defs)}),
            //{70410}
            //{70411}
            //{70412}
            //{70413}
            ce(70414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            //{70468}
            //{70469}
            //{70470}
            //{70471}
            ce(70472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            ce(70473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,761400), p(units_m), p(no_defs)}),
            //{70474}
            //{70475}
            //{70476}
            //{70477}
            //{70478}
            //{70479}
            ce(70500, ps{p(proj_longlat), p(a,240400), p(b,232900), p(no_defs)}),
            ce(70501, ps{p(proj_longlat), p(a,240400), p(b,232900), p(no_defs)}),
            //{70510}
            //{70511}
            //{70512}
            //{70513}
            ce(70514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            //{70568}
            //{70569}
            //{70570}
            //{70571}
            ce(70572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            ce(70573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,240400), p(b,232900), p(units_m), p(no_defs)}),
            //{70574}
            //{70575}
            //{70576}
            //{70577}
            //{70578}
            //{70579}
            ce(70600, ps{p(proj_longlat), p(r,13000), p(no_defs)}),
            ce(70601, ps{p(proj_longlat), p(r,13000), p(no_defs)}),
            //{70610}
            //{70611}
            //{70612}
            //{70613}
            ce(70614, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70615, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70616, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70617, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70618, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70619, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70620, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70621, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70660, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70661, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70662, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70663, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70664, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70665, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70666, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70667, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            //{70668}
            //{70669}
            //{70670}
            //{70671}
            ce(70672, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            ce(70673, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,13000), p(units_m), p(no_defs)}),
            //{70674}
            //{70675}
            //{70676}
            //{70677}
            //{70678}
            //{70679}
            ce(70700, ps{p(proj_longlat), p(r,15000), p(no_defs)}),
            ce(70701, ps{p(proj_longlat), p(r,15000), p(no_defs)}),
            //{70710}
            //{70711}
            //{70712}
            //{70713}
            ce(70714, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70715, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70716, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70717, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70718, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70719, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70720, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70721, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70760, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70761, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70762, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70763, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70764, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70765, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70766, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70767, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            //{70768}
            //{70769}
            //{70770}
            //{70771}
            ce(70772, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            ce(70773, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,15000), p(units_m), p(no_defs)}),
            //{70774}
            //{70775}
            //{70776}
            //{70777}
            //{70778}
            //{70779}
            ce(70800, ps{p(proj_longlat), p(r,21000), p(no_defs)}),
            ce(70801, ps{p(proj_longlat), p(r,21000), p(no_defs)}),
            //{70810}
            //{70811}
            //{70812}
            //{70813}
            ce(70814, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70815, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70816, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70817, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70818, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70819, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70820, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70821, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70860, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70861, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70862, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70863, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70864, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70865, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70866, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70867, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            //{70868}
            //{70869}
            //{70870}
            //{70871}
            ce(70872, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            ce(70873, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,21000), p(units_m), p(no_defs)}),
            //{70874}
            //{70875}
            //{70876}
            //{70877}
            //{70878}
            //{70879}
            ce(70900, ps{p(proj_longlat), p(r,31000), p(no_defs)}),
            ce(70901, ps{p(proj_longlat), p(r,31000), p(no_defs)}),
            //{70910}
            //{70911}
            //{70912}
            //{70913}
            ce(70914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            //{70968}
            //{70969}
            //{70970}
            //{70971}
            ce(70972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            ce(70973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,31000), p(units_m), p(no_defs)}),
            //{70974}
            //{70975}
            //{70976}
            //{70977}
            //{70978}
            //{70979}
            ce(71000, ps{p(proj_longlat), p(r,27000), p(no_defs)}),
            ce(71001, ps{p(proj_longlat), p(r,27000), p(no_defs)}),
            //{71010}
            //{71011}
            //{71012}
            //{71013}
            ce(71014, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71015, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71016, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71017, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71018, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71019, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71020, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71021, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71060, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71061, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71062, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71063, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71064, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71065, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71066, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71067, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            //{71068}
            //{71069}
            //{71070}
            //{71071}
            ce(71072, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71073, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            //{71074}
            //{71075}
            //{71076}
            //{71077}
            //{71078}
            //{71079}
            ce(71100, ps{p(proj_longlat), p(r,42000), p(no_defs)}),
            ce(71101, ps{p(proj_longlat), p(r,42000), p(no_defs)}),
            //{71110}
            //{71111}
            //{71112}
            //{71113}
            ce(71114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            //{71168}
            //{71169}
            //{71170}
            //{71171}
            ce(71172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            ce(71173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,42000), p(units_m), p(no_defs)}),
            //{71174}
            //{71175}
            //{71176}
            //{71177}
            //{71178}
            //{71179}
            ce(71200, ps{p(proj_longlat), p(r,54000), p(no_defs)}),
            ce(71201, ps{p(proj_longlat), p(r,54000), p(no_defs)}),
            //{71210}
            //{71211}
            //{71212}
            //{71213}
            ce(71214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            //{71268}
            //{71269}
            //{71270}
            //{71271}
            ce(71272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            ce(71273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,54000), p(units_m), p(no_defs)}),
            //{71274}
            //{71275}
            //{71276}
            //{71277}
            //{71278}
            //{71279}
            ce(71300, ps{p(proj_longlat), p(r,27000), p(no_defs)}),
            ce(71301, ps{p(proj_longlat), p(r,27000), p(no_defs)}),
            //{71310}
            //{71311}
            //{71312}
            //{71313}
            ce(71314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            //{71368}
            //{71369}
            //{71370}
            //{71371}
            ce(71372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            ce(71373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,27000), p(units_m), p(no_defs)}),
            //{71374}
            //{71375}
            //{71376}
            //{71377}
            //{71378}
            //{71379}
            ce(71400, ps{p(proj_longlat), p(r,33000), p(no_defs)}),
            ce(71401, ps{p(proj_longlat), p(r,33000), p(no_defs)}),
            //{71410}
            //{71411}
            //{71412}
            //{71413}
            ce(71414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            //{71468}
            //{71469}
            //{71470}
            //{71471}
            ce(71472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            ce(71473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,33000), p(units_m), p(no_defs)}),
            //{71474}
            //{71475}
            //{71476}
            //{71477}
            //{71478}
            //{71479}
            ce(71500, ps{p(proj_longlat), p(r,77000), p(no_defs)}),
            ce(71501, ps{p(proj_longlat), p(r,77000), p(no_defs)}),
            //{71510}
            //{71511}
            //{71512}
            //{71513}
            ce(71514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            //{71568}
            //{71569}
            //{71570}
            //{71571}
            ce(71572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            ce(71573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,77000), p(units_m), p(no_defs)}),
            //{71574}
            //{71575}
            //{71576}
            //{71577}
            //{71578}
            //{71579}
            ce(79900, ps{p(proj_longlat), p(a,25559000), p(b,24973000), p(no_defs)}),
            ce(79901, ps{p(proj_longlat), p(a,25559000), p(b,24973000), p(no_defs)}),
            //{79910}
            //{79911}
            //{79912}
            //{79913}
            ce(79914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            //{79968}
            //{79969}
            //{79970}
            //{79971}
            ce(79972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            ce(79973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,25559000), p(b,24973000), p(units_m), p(no_defs)}),
            //{79974}
            //{79975}
            //{79976}
            //{79977}
            //{79978}
            //{79979}
            ce(80100, ps{p(proj_longlat), p(r,1352600), p(no_defs)}),
            ce(80101, ps{p(proj_longlat), p(r,1352600), p(no_defs)}),
            //{80110}
            //{80111}
            //{80112}
            //{80113}
            ce(80114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            //{80168}
            //{80169}
            //{80170}
            //{80171}
            ce(80172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            ce(80173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1352600), p(units_m), p(no_defs)}),
            //{80174}
            //{80175}
            //{80176}
            //{80177}
            //{80178}
            //{80179}
            ce(80200, ps{p(proj_longlat), p(r,170000), p(no_defs)}),
            ce(80201, ps{p(proj_longlat), p(r,170000), p(no_defs)}),
            //{80210}
            //{80211}
            //{80212}
            //{80213}
            ce(80214, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80215, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80216, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80217, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80218, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80219, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80220, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80221, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80260, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80261, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80262, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80263, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80264, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80265, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80266, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80267, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            //{80268}
            //{80269}
            //{80270}
            //{80271}
            ce(80272, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            ce(80273, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,170000), p(units_m), p(no_defs)}),
            //{80274}
            //{80275}
            //{80276}
            //{80277}
            //{80278}
            //{80279}
            ce(80300, ps{p(proj_longlat), p(r,29000), p(no_defs)}),
            ce(80301, ps{p(proj_longlat), p(r,29000), p(no_defs)}),
            //{80310}
            //{80311}
            //{80312}
            //{80313}
            ce(80314, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80315, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80316, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80317, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80318, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80319, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80320, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80321, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80360, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80361, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80362, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80363, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80364, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80365, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80366, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80367, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            //{80368}
            //{80369}
            //{80370}
            //{80371}
            ce(80372, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            ce(80373, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,29000), p(units_m), p(no_defs)}),
            //{80374}
            //{80375}
            //{80376}
            //{80377}
            //{80378}
            //{80379}
            ce(80400, ps{p(proj_longlat), p(r,40000), p(no_defs)}),
            ce(80401, ps{p(proj_longlat), p(r,40000), p(no_defs)}),
            //{80410}
            //{80411}
            //{80412}
            //{80413}
            ce(80414, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80415, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80416, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80417, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80418, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80419, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80420, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80421, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80460, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80461, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80462, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80463, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80464, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80465, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80466, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80467, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            //{80468}
            //{80469}
            //{80470}
            //{80471}
            ce(80472, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            ce(80473, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,40000), p(units_m), p(no_defs)}),
            //{80474}
            //{80475}
            //{80476}
            //{80477}
            //{80478}
            //{80479}
            ce(80500, ps{p(proj_longlat), p(r,74000), p(no_defs)}),
            ce(80501, ps{p(proj_longlat), p(r,74000), p(no_defs)}),
            //{80510}
            //{80511}
            //{80512}
            //{80513}
            ce(80514, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80515, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80516, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80517, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80518, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80519, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80520, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80521, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80560, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80561, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80562, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80563, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80564, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80565, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80566, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80567, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            //{80568}
            //{80569}
            //{80570}
            //{80571}
            ce(80572, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            ce(80573, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,74000), p(units_m), p(no_defs)}),
            //{80574}
            //{80575}
            //{80576}
            //{80577}
            //{80578}
            //{80579}
            ce(80600, ps{p(proj_longlat), p(r,79000), p(no_defs)}),
            ce(80601, ps{p(proj_longlat), p(r,79000), p(no_defs)}),
            //{80610}
            //{80611}
            //{80612}
            //{80613}
            ce(80614, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80615, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80616, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80617, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80618, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80619, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80620, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80621, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80660, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80661, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80662, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80663, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80664, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80665, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80666, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80667, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            //{80668}
            //{80669}
            //{80670}
            //{80671}
            ce(80672, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            ce(80673, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,79000), p(units_m), p(no_defs)}),
            //{80674}
            //{80675}
            //{80676}
            //{80677}
            //{80678}
            //{80679}
            ce(80700, ps{p(proj_longlat), p(r,96000), p(no_defs)}),
            ce(80701, ps{p(proj_longlat), p(r,96000), p(no_defs)}),
            //{80710}
            //{80711}
            //{80712}
            //{80713}
            ce(80714, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80715, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80716, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80717, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80718, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80719, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80720, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80721, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80760, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80761, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80762, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80763, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80764, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80765, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80766, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80767, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            //{80768}
            //{80769}
            //{80770}
            //{80771}
            ce(80772, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            ce(80773, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,96000), p(units_m), p(no_defs)}),
            //{80774}
            //{80775}
            //{80776}
            //{80777}
            //{80778}
            //{80779}
            ce(80800, ps{p(proj_longlat), p(a,218000), p(b,201000), p(no_defs)}),
            ce(80801, ps{p(proj_longlat), p(a,218000), p(b,201000), p(no_defs)}),
            //{80810}
            //{80811}
            //{80812}
            //{80813}
            ce(80814, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80815, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80816, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80817, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80818, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80819, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80820, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80821, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80860, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80861, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80862, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80863, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80864, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80865, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80866, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80867, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            //{80868}
            //{80869}
            //{80870}
            //{80871}
            ce(80872, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            ce(80873, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,218000), p(b,201000), p(units_m), p(no_defs)}),
            //{80874}
            //{80875}
            //{80876}
            //{80877}
            //{80878}
            //{80879}
            ce(89900, ps{p(proj_longlat), p(a,24764000), p(b,24341000), p(no_defs)}),
            ce(89901, ps{p(proj_longlat), p(a,24764000), p(b,24341000), p(no_defs)}),
            //{89910}
            //{89911}
            //{89912}
            //{89913}
            ce(89914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            //{89968}
            //{89969}
            //{89970}
            //{89971}
            ce(89972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            ce(89973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(a,24764000), p(b,24341000), p(units_m), p(no_defs)}),
            //{89974}
            //{89975}
            //{89976}
            //{89977}
            //{89978}
            //{89979}
            ce(90100, ps{p(proj_longlat), p(r,593000), p(no_defs)}),
            ce(90101, ps{p(proj_longlat), p(r,593000), p(no_defs)}),
            //{90110}
            //{90111}
            //{90112}
            //{90113}
            ce(90114, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90115, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90116, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90117, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90118, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90119, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90120, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90121, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90160, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90161, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90162, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90163, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90164, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90165, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90166, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90167, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            //{90168}
            //{90169}
            //{90170}
            //{90171}
            ce(90172, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            ce(90173, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,593000), p(units_m), p(no_defs)}),
            //{90174}
            //{90175}
            //{90176}
            //{90177}
            //{90178}
            //{90179}
            ce(99900, ps{p(proj_longlat), p(r,1195000), p(no_defs)}),
            ce(99901, ps{p(proj_longlat), p(r,1195000), p(no_defs)}),
            //{99910}
            //{99911}
            //{99912}
            //{99913}
            ce(99914, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99915, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99916, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99917, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99918, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99919, ps{p(proj_stere), p(lat_0,90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99920, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99921, ps{p(proj_stere), p(lat_0,-90), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99960, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99961, ps{p(proj_sinu), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99962, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99963, ps{p(proj_stere), p(lat_0,0), p(lon_0,0), p(k,1), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99964, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99965, ps{p(proj_tmerc), p(lat_0,0), p(lon_0,0), p(k,0.9996), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99966, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99967, ps{p(proj_ortho), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            //{99968}
            //{99969}
            //{99970}
            //{99971}
            ce(99972, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)}),
            ce(99973, ps{p(proj_laea), p(lat_0,90), p(lon_0,0), p(x_0,0), p(y_0,0), p(r,1195000), p(units_m), p(no_defs)})
            //{99974}
            //{99975}
            //{99976}
            //{99977}
            //{99978}
            //{99979}

        };

        const auto it = std::lower_bound(arr.begin(), arr.end(), code, code_element_less{});
        return it != arr.end() && it->code == code ? it->to_parameters() : srs::dpar::parameters<>();
    }

}
#endif // DOXYGEN_NO_DETAIL


}}} // namespace boost::geometry::projections

#endif
