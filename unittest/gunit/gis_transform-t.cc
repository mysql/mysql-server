// Copyright (c) 2022, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include "my_config.h"

#include <gtest/gtest.h>
#include <memory>  // unique_ptr

#include "sql/dd/dd.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/relops.h"
#include "sql/gis/transform.h"
#include "unittest/gunit/gis_setops_testshapes.h"
#include "unittest/gunit/gis_typeset.h"

#include <boost/geometry.hpp>

namespace gis_transform_unittest {

// geo SRS

dd::String_type wgs84 =
    "GEOGCS[\"WGS 84\",DATUM[\"World Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,"
    "0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lon\",EAST],AXIS[\"Lat\",NORTH],AUTHORITY["
    "\"EPSG\",\"4326\"]]";
dd::String_type geogcs2985 =
    "GEOGCS[\"Petrels 1972\",DATUM[\"Petrels 1972\",SPHEROID[\"International "
    "1924\",6378388,297,AUTHORITY[\"EPSG\",\"7022\"]],TOWGS84[365,194,166,0,0,"
    "0,0],AUTHORITY[\"EPSG\",\"6636\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4636\"]]";
dd::String_type modairy =
    "GEOGCS[\"modairy\",DATUM[\"modairy\",SPHEROID[\"Bessel 1841 "
    "84\",6377340.189,299.324937365,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[0,0,"
    "0,0,0,0,0],AUTHORITY[\"EPSG\",\"6120\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lon\",EAST],AXIS[\"Lat\",NORTH],AUTHORITY["
    "\"EPSG\",\"4120\"]]";

// projections SRS
// 1024
dd::String_type webmerc3857 =
    "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"World "
    "Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Popular Visualisation Pseudo "
    "Mercator\",AUTHORITY[\"EPSG\",\"1024\"]],PARAMETER[\"Latitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"3857\"]]";
dd::String_type webmerc_modairy =
    "PROJCS[\"modairy / Pseudo-Mercator\",GEOGCS[\"modairy\",DATUM[\" "
    "modairy\",SPHEROID[\"Bessel "
    "1841\",6377340.189,299.324937365],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["
    "\"Greenwich\",0],UNIT[\"degree\",0.017453292519943278],AXIS[\"Lat\",NORTH]"
    ",AXIS[\"Lon\",EAST]],PROJECTION[\"Popular Visualisation Pseudo "
    "Mercator\",AUTHORITY[\"EPSG\",\"1024\"]],PARAMETER[\"Latitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1],AXIS[\"X\","
    "EAST],AXIS[\"Y\",NORTH]]";
// 1027
dd::String_type epsg2163 =
    "PROJCS[\"US National Atlas Equal "
    "Area\",GEOGCS[\"Unspecified\",DATUM[\"Not specified\",SPHEROID[\"Clarke "
    "1866 Authalic "
    "Sphere\",6370997,0,AUTHORITY[\"EPSG\",\"7052\"]],TOWGS84[0,0,0,0,0,0,0],"
    "AUTHORITY[\"EPSG\",\"6052\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\","
    "\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\","
    "\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\","
    "\"4052\"]],PROJECTION[\"Lambert Azimuthal Equal "
    "Area\",AUTHORITY[\"EPSG\",\"1027\"]],PARAMETER[\"Latitude of natural "
    "origin\",45,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",-100,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"2163\"]]";
// 1028
dd::String_type epsg4087 =
    "PROJCS[\"WGS 84 / World Equidistant Cylindrical\",GEOGCS[\"WGS "
    "84\",DATUM[\"World Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Equidistant "
    "Cylindrical\",AUTHORITY[\"EPSG\",\"1028\"]],PARAMETER[\"Latitude of 1st "
    "standard parallel\",0,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER[\"Longitude "
    "of natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"4087\"]]";
// 1029
dd::String_type epsg4088 =
    "PROJCS[\"World Equidistant Cylindrical (Sphere)\",GEOGCS[\"Unspecified "
    "datum based upon the GRS 1980 Authalic Sphere\",DATUM[\"Not specified "
    "(based on GRS 1980 Authalic Sphere)\",SPHEROID[\"GRS 1980 Authalic "
    "Sphere\",6371007,0,AUTHORITY[\"EPSG\",\"7048\"]],TOWGS84[0,0,0,0,0,0,0],"
    "AUTHORITY[\"EPSG\",\"6047\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\","
    "\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\","
    "\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\","
    "\"4047\"]],PROJECTION[\"Equidistant Cylindrical "
    "(Spherical)\",AUTHORITY[\"EPSG\",\"1029\"]],PARAMETER[\"Latitude of 1st "
    "standard parallel\",0,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER[\"Longitude "
    "of natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"4088\"]]";
// 1041
dd::String_type epsg5514 =
    "PROJCS[\"S-JTSK / Krovak East North\",GEOGCS[\"S-JTSK\",DATUM[\"System of "
    "the Unified Trigonometrical Cadastral Network\",SPHEROID[\"Bessel "
    "1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[589,"
    "76,480,0,0,0,0],AUTHORITY[\"EPSG\",\"6156\"]],PRIMEM[\"Greenwich\",0,"
    "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4156\"]],PROJECTION[\"Krovak (North "
    "Orientated)\",AUTHORITY[\"EPSG\",\"1041\"]],PARAMETER[\"Latitude of "
    "projection "
    "centre\",49.5111111111111,AUTHORITY[\"EPSG\",\"8811\"]],PARAMETER["
    "\"Longitude of "
    "origin\",24.8333333333333,AUTHORITY[\"EPSG\",\"8833\"]],PARAMETER[\"Co-"
    "latitude of cone "
    "axis\",30.2881397222222,AUTHORITY[\"EPSG\",\"1036\"]],PARAMETER["
    "\"Latitude of pseudo standard "
    "parallel\",78.5111111111111,AUTHORITY[\"EPSG\",\"8818\"]],PARAMETER["
    "\"Scale factor on pseudo standard "
    "parallel\",0.9999,AUTHORITY[\"EPSG\",\"8819\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"5514\"]]";
// 1051
dd::String_type epsg6201 =
    "PROJCS[\"NAD27 / Michigan Central\",GEOGCS[\"NAD27\",DATUM[\"North "
    "American Datum 1927\",SPHEROID[\"Clarke "
    "1866\",6378206.4,294.9786982138982,AUTHORITY[\"EPSG\",\"7008\"]],TOWGS84[-"
    "32.3841359,180.4090461,120.8442577,-2.1545854,-0.1498782,0.5742915,8."
    "1049164],AUTHORITY[\"EPSG\",\"6267\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4267\"]],PROJECTION[\"Lambert Conic Conformal (2SP "
    "Michigan)\",AUTHORITY[\"EPSG\",\"1051\"]],PARAMETER[\"Latitude of false "
    "origin\",43.3277777777778,AUTHORITY[\"EPSG\",\"8821\"]],PARAMETER["
    "\"Longitude of false "
    "origin\",-84.3333333333333,AUTHORITY[\"EPSG\",\"8822\"]],PARAMETER["
    "\"Latitude of 1st standard "
    "parallel\",44.1944444444444,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER["
    "\"Latitude of 2nd standard "
    "parallel\",45.7,AUTHORITY[\"EPSG\",\"8824\"]],PARAMETER[\"Easting at "
    "false origin\",2000000,AUTHORITY[\"EPSG\",\"8826\"]],PARAMETER[\"Northing "
    "at false origin\",0,AUTHORITY[\"EPSG\",\"8827\"]],PARAMETER[\"Ellipsoid "
    "scaling factor\",1.0000382,AUTHORITY[\"EPSG\",\"1038\"]],UNIT[\"US survey "
    "foot\",0.30480060960121924,AUTHORITY[\"EPSG\",\"9003\"]],AXIS[\"X\",EAST],"
    "AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\",\"6201\"]]";
// 1052
dd::String_type epsg6247 =
    "PROJCS[\"MAGNA-SIRGAS / Bogota urban "
    "grid\",GEOGCS[\"MAGNA-SIRGAS\",DATUM[\"Marco Geocentrico Nacional de "
    "Referencia\",SPHEROID[\"GRS "
    "1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,"
    "0,0,0,0],AUTHORITY[\"EPSG\",\"6686\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4686\"]],PROJECTION[\"Colombia "
    "Urban\",AUTHORITY[\"EPSG\",\"1052\"]],PARAMETER[\"Latitude of natural "
    "origin\",4.68048611111111,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",-74.1465916666667,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER["
    "\"False "
    "easting\",92334.879,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",109320.965,AUTHORITY[\"EPSG\",\"8807\"]],PARAMETER["
    "\"Projection plane origin "
    "height\",2550,AUTHORITY[\"EPSG\",\"1039\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"N\",NORTH],AXIS[\"E\",EAST],AUTHORITY[\"EPSG\","
    "\"6247\"]]";
// 9801
dd::String_type epsg24200 =
    "PROJCS[\"JAD69 / Jamaica National Grid\",GEOGCS[\"JAD69\",DATUM[\"Jamaica "
    "1969\",SPHEROID[\"Clarke "
    "1866\",6378206.4,294.9786982138982,AUTHORITY[\"EPSG\",\"7008\"]],TOWGS84[-"
    "33.722,153.789,94.959,-8.581,-4.478,4.54,8.95],AUTHORITY[\"EPSG\","
    "\"6242\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT["
    "\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4242\"]],"
    "PROJECTION[\"Lambert Conic Conformal "
    "(1SP)\",AUTHORITY[\"EPSG\",\"9801\"]],PARAMETER[\"Latitude of natural "
    "origin\",18,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",-77,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale "
    "factor at natural "
    "origin\",1,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",250000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",150000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"24200\"]]";
// 9802
dd::String_type epsg32040 =
    "PROJCS[\"NAD27 / Texas South Central\",GEOGCS[\"NAD27\",DATUM[\"North "
    "American Datum 1927\",SPHEROID[\"Clarke "
    "1866\",6378206.4,294.9786982138982,AUTHORITY[\"EPSG\",\"7008\"]],TOWGS84[-"
    "32.3841359,180.4090461,120.8442577,-2.1545854,-0.1498782,0.5742915,8."
    "1049164],AUTHORITY[\"EPSG\",\"6267\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4267\"]],PROJECTION[\"Lambert Conic Conformal "
    "(2SP)\",AUTHORITY[\"EPSG\",\"9802\"]],PARAMETER[\"Latitude of false "
    "origin\",27.8333333333333,AUTHORITY[\"EPSG\",\"8821\"]],PARAMETER["
    "\"Longitude of false "
    "origin\",-99,AUTHORITY[\"EPSG\",\"8822\"]],PARAMETER[\"Latitude of 1st "
    "standard "
    "parallel\",28.3833333333333,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER["
    "\"Latitude of 2nd standard "
    "parallel\",30.2833333333333,AUTHORITY[\"EPSG\",\"8824\"]],PARAMETER["
    "\"Easting at false "
    "origin\",2000000,AUTHORITY[\"EPSG\",\"8826\"]],PARAMETER[\"Northing at "
    "false origin\",0,AUTHORITY[\"EPSG\",\"8827\"]],UNIT[\"US survey "
    "foot\",0.30480060960121924,AUTHORITY[\"EPSG\",\"9003\"]],AXIS[\"X\",EAST],"
    "AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\",\"32040\"]]";
// 9803
dd::String_type epsg31300 =
    "PROJCS[\"Belge 1972 / Belge Lambert 72\",GEOGCS[\"Belge "
    "1972\",DATUM[\"Reseau National Belge 1972\",SPHEROID[\"International "
    "1924\",6378388,297,AUTHORITY[\"EPSG\",\"7022\"]],TOWGS84[-106.8686,52."
    "2978,-103.7239,0.3366,-0.457,1.8422,-1.2747],AUTHORITY[\"EPSG\",\"6313\"]]"
    ",PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4313\"]],PROJECTION[\"Lambert Conic "
    "Conformal (2SP "
    "Belgium)\",AUTHORITY[\"EPSG\",\"9803\"]],PARAMETER[\"Latitude of false "
    "origin\",90,AUTHORITY[\"EPSG\",\"8821\"]],PARAMETER[\"Longitude of false "
    "origin\",4.35693972222222,AUTHORITY[\"EPSG\",\"8822\"]],PARAMETER["
    "\"Latitude of 1st standard "
    "parallel\",49.8333333333333,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER["
    "\"Latitude of 2nd standard "
    "parallel\",51.1666666666667,AUTHORITY[\"EPSG\",\"8824\"]],PARAMETER["
    "\"Easting at false "
    "origin\",150000.01256,AUTHORITY[\"EPSG\",\"8826\"]],PARAMETER[\"Northing "
    "at false "
    "origin\",5400088.4378,AUTHORITY[\"EPSG\",\"8827\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],"
    "AUTHORITY[\"EPSG\",\"31300\"]]";
// 9804
dd::String_type epsg3002 =
    "PROJCS[\"Makassar / "
    "NEIEZ\",GEOGCS[\"Makassar\",DATUM[\"Makassar\",SPHEROID[\"Bessel "
    "1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[-587."
    "8,519.75,145.76,0,0,0,0],AUTHORITY[\"EPSG\",\"6257\"]],PRIMEM["
    "\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4257\"]],PROJECTION[\"Mercator "
    "(variant A)\",AUTHORITY[\"EPSG\",\"9804\"]],PARAMETER[\"Latitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",110,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale "
    "factor at natural "
    "origin\",0.997,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",3900000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",900000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],"
    "AUTHORITY[\"EPSG\",\"3002\"]]";
// 9805
dd::String_type epsg3388 =
    "PROJCS[\"Pulkovo 1942 / Caspian Sea Mercator\",GEOGCS[\"Pulkovo "
    "1942\",DATUM[\"Pulkovo 1942\",SPHEROID[\"Krassowsky "
    "1940\",6378245,298.3,AUTHORITY[\"EPSG\",\"7024\"]],TOWGS84[25,-141,-78.5,"
    "0,0.35,0.736,0],AUTHORITY[\"EPSG\",\"6284\"]],PRIMEM[\"Greenwich\",0,"
    "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4284\"]],PROJECTION[\"Mercator (variant "
    "B)\",AUTHORITY[\"EPSG\",\"9805\"]],PARAMETER[\"Latitude of 1st standard "
    "parallel\",42,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER[\"Longitude of "
    "natural origin\",51,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"none\",NORTH],AXIS[\"none\",EAST],AUTHORITY["
    "\"EPSG\",\"3388\"]]";
// 9806
dd::String_type epsg30200 =
    "PROJCS[\"Trinidad 1903 / Trinidad Grid\",GEOGCS[\"Trinidad "
    "1903\",DATUM[\"Trinidad 1903\",SPHEROID[\"Clarke "
    "1858\",6378293.645208759,294.26067636926103,AUTHORITY[\"EPSG\",\"7007\"]],"
    "TOWGS84[-61.702,284.488,472.052,0,0,0,0],AUTHORITY[\"EPSG\",\"6302\"]],"
    "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4302\"]],PROJECTION[\"Cassini-"
    "Soldner\",AUTHORITY[\"EPSG\",\"9806\"]],PARAMETER[\"Latitude of natural "
    "origin\",10.4416666666667,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",-61.3333333333333,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER["
    "\"False easting\",430000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",325000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"Clarke's "
    "link\",0.201166195164,AUTHORITY[\"EPSG\",\"9039\"]],AXIS[\"E\",EAST],AXIS["
    "\"N\",NORTH],AUTHORITY[\"EPSG\",\"30200\"]]";
// 9807
dd::String_type epsg27700 =
    "PROJCS[\"OSGB 1936 / British National Grid\",GEOGCS[\"OSGB "
    "1936\",DATUM[\"OSGB 1936\",SPHEROID[\"Airy "
    "1830\",6377563.396,299.3249646,AUTHORITY[\"EPSG\",\"7001\"]],TOWGS84[446."
    "448,-125.157,542.06,0.15,0.247,0.842,-20.489],AUTHORITY[\"EPSG\",\"6277\"]"
    "],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4277\"]],PROJECTION[\"Transverse "
    "Mercator\",AUTHORITY[\"EPSG\",\"9807\"]],PARAMETER[\"Latitude of natural "
    "origin\",49,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",-2,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale "
    "factor at natural "
    "origin\",0.9996012717,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",400000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",-100000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"27700\"]]";
dd::String_type wgs84tmerc =
    "PROJCS[\"WGS 84 / TM 36 SE\",GEOGCS[\"WGS 84\",DATUM[\"World Geodetic "
    "System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Transverse "
    "Mercator\",AUTHORITY[\"EPSG\",\"9807\"]],PARAMETER[\"Latitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of natural "
    "origin\",36,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale factor at "
    "natural origin\",0.9996,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",500000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",10000000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"32766\"]]";
// 9808
dd::String_type epsg2053 =
    "PROJCS[\"Hartebeesthoek94 / "
    "Lo29\",GEOGCS[\"Hartebeesthoek94\",DATUM[\"Hartebeesthoek94\",SPHEROID["
    "\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,"
    "0,0,0],AUTHORITY[\"EPSG\",\"6148\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4148\"]],PROJECTION[\"Transverse Mercator (South "
    "Orientated)\",AUTHORITY[\"EPSG\",\"9808\"]],PARAMETER[\"Latitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",29,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale "
    "factor at natural "
    "origin\",1,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"Y\",WEST],AXIS[\"X\",SOUTH],AUTHORITY[\"EPSG\","
    "\"2053\"]]";
// 9809
dd::String_type epsg28992 =
    "PROJCS[\"Amersfoort / RD "
    "New\",GEOGCS[\"Amersfoort\",DATUM[\"Amersfoort\",SPHEROID[\"Bessel "
    "1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[565."
    "4171,50.3319,465.5524,-0.398957388243134,0.343987817378283,-1."
    "87740163998045,4.0725],AUTHORITY[\"EPSG\",\"6289\"]],PRIMEM[\"Greenwich\","
    "0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4289\"]],PROJECTION[\"Oblique "
    "Stereographic\",AUTHORITY[\"EPSG\",\"9809\"]],PARAMETER[\"Latitude of "
    "natural "
    "origin\",52.1561605555556,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",5.38763888888889,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale "
    "factor at natural "
    "origin\",0.9999079,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",155000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",463000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],"
    "AUTHORITY[\"EPSG\",\"28992\"]]";
// 9810
dd::String_type epsg5041 =
    "PROJCS[\"WGS 84 / UPS North (E,N)\",GEOGCS[\"WGS 84\",DATUM[\"World "
    "Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Polar Stereographic (variant "
    "A)\",AUTHORITY[\"EPSG\",\"9810\"]],PARAMETER[\"Latitude of natural "
    "origin\",90,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale factor "
    "at natural origin\",0.994,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",2000000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",2000000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",SOUTH],AXIS[\"N\",SOUTH],"
    "AUTHORITY[\"EPSG\",\"5041\"]]";
// 9811
dd::String_type epsg27200 =
    "PROJCS[\"NZGD49 / New Zealand Map Grid\",GEOGCS[\"NZGD49\",DATUM[\"New "
    "Zealand Geodetic Datum 1949\",SPHEROID[\"International "
    "1924\",6378388,297,AUTHORITY[\"EPSG\",\"7022\"]],TOWGS84[59.47,-5.04,187."
    "44,0.47,-0.1,1.024,-4.5993],AUTHORITY[\"EPSG\",\"6272\"]],PRIMEM["
    "\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4272\"]],PROJECTION[\"New Zealand Map "
    "Grid\",AUTHORITY[\"EPSG\",\"9811\"]],PARAMETER[\"Latitude of natural "
    "origin\",-41,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",173,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",2510000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",6023150,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"27200\"]]";
// 9812
dd::String_type epsg3079 =
    "PROJCS[\"NAD83(HARN) / Michigan Oblique "
    "Mercator\",GEOGCS[\"NAD83(HARN)\",DATUM[\"NAD83 (High Accuracy Reference "
    "Network)\",SPHEROID[\"GRS "
    "1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,"
    "0,0,0,0],AUTHORITY[\"EPSG\",\"6152\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4152\"]],PROJECTION[\"Hotine Oblique Mercator (variant "
    "A)\",AUTHORITY[\"EPSG\",\"9812\"]],PARAMETER[\"Latitude of projection "
    "centre\",45.3091666666667,AUTHORITY[\"EPSG\",\"8811\"]],PARAMETER["
    "\"Longitude of projection "
    "centre\",-86,AUTHORITY[\"EPSG\",\"8812\"]],PARAMETER[\"Azimuth of initial "
    "line\",337.25556,AUTHORITY[\"EPSG\",\"8813\"]],PARAMETER[\"Angle from "
    "Rectified to Skew "
    "Grid\",337.25556,AUTHORITY[\"EPSG\",\"8814\"]],PARAMETER[\"Scale factor "
    "on initial line\",0.9996,AUTHORITY[\"EPSG\",\"8815\"]],PARAMETER[\"False "
    "easting\",2546731.496,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",-4354009.816,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],"
    "AUTHORITY[\"EPSG\",\"3079\"]]";
// 9813
dd::String_type epsg8441 =
    "PROJCS[\"Tananarive / Laborde "
    "Grid\",GEOGCS[\"Tananarive\",DATUM[\"Tananarive "
    "1925\",SPHEROID[\"International "
    "1924\",6378388,297,AUTHORITY[\"EPSG\",\"7022\"]],TOWGS84[-198.383,-240."
    "517,-107.909,0,0,0,0],AUTHORITY[\"EPSG\",\"6297\"]],PRIMEM[\"Greenwich\","
    "0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4297\"]],PROJECTION[\"Laborde Oblique "
    "Mercator\",AUTHORITY[\"EPSG\",\"9813\"]],PARAMETER[\"Latitude of "
    "projection "
    "centre\",-18.9111111111111,AUTHORITY[\"EPSG\",\"8811\"]],PARAMETER["
    "\"Longitude of projection "
    "centre\",46.4372291666667,AUTHORITY[\"EPSG\",\"8812\"]],PARAMETER["
    "\"Azimuth of initial "
    "line\",18.9111111111111,AUTHORITY[\"EPSG\",\"8813\"]],PARAMETER[\"Scale "
    "factor on initial "
    "line\",0.9995,AUTHORITY[\"EPSG\",\"8815\"]],PARAMETER[\"False "
    "easting\",400000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",800000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",NORTH],AXIS[\"Y\",EAST],"
    "AUTHORITY[\"EPSG\",\"8441\"]]";
// 9815
dd::String_type epsg29873 =
    "PROJCS[\"Timbalai 1948 / RSO Borneo (m)\",GEOGCS[\"Timbalai "
    "1948\",DATUM[\"Timbalai 1948\",SPHEROID[\"Everest 1830 (1967 "
    "Definition)\",6377298.556,300.8017,AUTHORITY[\"EPSG\",\"7016\"]],TOWGS84[-"
    "679,669,-48,0,0,0,0],AUTHORITY[\"EPSG\",\"6298\"]],PRIMEM[\"Greenwich\",0,"
    "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4298\"]],PROJECTION[\"Hotine Oblique Mercator "
    "(variant B)\",AUTHORITY[\"EPSG\",\"9815\"]],PARAMETER[\"Latitude of "
    "projection centre\",4,AUTHORITY[\"EPSG\",\"8811\"]],PARAMETER[\"Longitude "
    "of projection "
    "centre\",115,AUTHORITY[\"EPSG\",\"8812\"]],PARAMETER[\"Azimuth of initial "
    "line\",53.3158204722222,AUTHORITY[\"EPSG\",\"8813\"]],PARAMETER[\"Angle "
    "from Rectified to Skew "
    "Grid\",53.1301023611111,AUTHORITY[\"EPSG\",\"8814\"]],PARAMETER[\"Scale "
    "factor on initial "
    "line\",0.99984,AUTHORITY[\"EPSG\",\"8815\"]],PARAMETER[\"Easting at "
    "projection "
    "centre\",590476.87,AUTHORITY[\"EPSG\",\"8816\"]],PARAMETER[\"Northing at "
    "projection "
    "centre\",442857.65,AUTHORITY[\"EPSG\",\"8817\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"29873\"]]";
// 9817
dd::String_type epsg22700 =
    "PROJCS[\"Deir ez Zor / Levant Zone\",GEOGCS[\"Deir ez Zor\",DATUM[\"Deir "
    "ez Zor\",SPHEROID[\"Clarke 1880 "
    "(IGN)\",6378249.2,293.4660212936269,AUTHORITY[\"EPSG\",\"7011\"]],TOWGS84["
    "-83.58,-397.54,458.78,-17.595,-2.847,4.256,3.225],AUTHORITY[\"EPSG\","
    "\"6227\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT["
    "\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4227\"]],"
    "PROJECTION[\"Lambert Conic "
    "Near-Conformal\",AUTHORITY[\"EPSG\",\"9817\"]],PARAMETER[\"Latitude of "
    "natural "
    "origin\",34.65,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",37.35,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"Scale "
    "factor at natural "
    "origin\",0.9996256,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",300000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",300000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],"
    "AUTHORITY[\"EPSG\",\"22700\"]]";
// 9818
dd::String_type epsg5880 =
    "PROJCS[\"SIRGAS 2000 / Brazil Polyconic\",GEOGCS[\"SIRGAS "
    "2000\",DATUM[\"Sistema de Referencia Geocentrico para las AmericaS "
    "2000\",SPHEROID[\"GRS "
    "1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,"
    "0,0,0,0],AUTHORITY[\"EPSG\",\"6674\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4674\"]],PROJECTION[\"American "
    "Polyconic\",AUTHORITY[\"EPSG\",\"9818\"]],PARAMETER[\"Latitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of natural "
    "origin\",-54,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",5000000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",10000000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],"
    "AUTHORITY[\"EPSG\",\"5880\"]]";
// 9819
dd::String_type epsg5513 =
    "PROJCS[\"S-JTSK / Krovak\",GEOGCS[\"S-JTSK\",DATUM[\"System of the "
    "Unified Trigonometrical Cadastral Network\",SPHEROID[\"Bessel "
    "1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[589,"
    "76,480,0,0,0,0],AUTHORITY[\"EPSG\",\"6156\"]],PRIMEM[\"Greenwich\",0,"
    "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4156\"]],PROJECTION[\"Krovak\",AUTHORITY[\"EPSG\","
    "\"9819\"]],PARAMETER[\"Latitude of projection "
    "centre\",49.5111111111111,AUTHORITY[\"EPSG\",\"8811\"]],PARAMETER["
    "\"Longitude of "
    "origin\",24.8333333333333,AUTHORITY[\"EPSG\",\"8833\"]],PARAMETER[\"Co-"
    "latitude of cone "
    "axis\",30.2881397222222,AUTHORITY[\"EPSG\",\"1036\"]],PARAMETER["
    "\"Latitude of pseudo standard "
    "parallel\",78.5111111111111,AUTHORITY[\"EPSG\",\"8818\"]],PARAMETER["
    "\"Scale factor on pseudo standard "
    "parallel\",0.9999,AUTHORITY[\"EPSG\",\"8819\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",SOUTH],AXIS[\"Y\",WEST],AUTHORITY[\"EPSG\","
    "\"5513\"]]";
// 9820
dd::String_type epsg3035 =
    "PROJCS[\"ETRS89 / LAEA Europe\",GEOGCS[\"ETRS89\",DATUM[\"European "
    "Terrestrial Reference System 1989\",SPHEROID[\"GRS "
    "1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,"
    "0,0,0,0],AUTHORITY[\"EPSG\",\"6258\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4258\"]],PROJECTION[\"Lambert Azimuthal Equal "
    "Area\",AUTHORITY[\"EPSG\",\"9820\"]],PARAMETER[\"Latitude of natural "
    "origin\",52,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude of "
    "natural origin\",10,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",4321000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",3210000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"Y\",NORTH],AXIS[\"X\",EAST],"
    "AUTHORITY[\"EPSG\",\"3035\"]]";
// 9822
dd::String_type epsg3174 =
    "PROJCS[\"NAD83 / Great Lakes Albers\",GEOGCS[\"NAD83\",DATUM[\"North "
    "American Datum 1983\",SPHEROID[\"GRS "
    "1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[1,1,-1,"
    "0,0,0,0],AUTHORITY[\"EPSG\",\"6269\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4269\"]],PROJECTION[\"Albers Equal "
    "Area\",AUTHORITY[\"EPSG\",\"9822\"]],PARAMETER[\"Latitude of false "
    "origin\",45.568977,AUTHORITY[\"EPSG\",\"8821\"]],PARAMETER[\"Longitude of "
    "false "
    "origin\",-84.455955,AUTHORITY[\"EPSG\",\"8822\"]],PARAMETER[\"Latitude of "
    "1st standard "
    "parallel\",42.122774,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER[\"Latitude "
    "of 2nd standard "
    "parallel\",49.01518,AUTHORITY[\"EPSG\",\"8824\"]],PARAMETER[\"Easting at "
    "false origin\",1000000,AUTHORITY[\"EPSG\",\"8826\"]],PARAMETER[\"Northing "
    "at false "
    "origin\",1000000,AUTHORITY[\"EPSG\",\"8827\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"3174\"]]";
// 9824
dd::String_type epsg32600 =
    "PROJCS[\"WGS 84 / UTM grid system (northern hemisphere)\",GEOGCS[\"WGS "
    "84\",DATUM[\"World Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Transverse Mercator Zoned Grid "
    "System\",AUTHORITY[\"EPSG\",\"9824\"]],PARAMETER[\"Latitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Initial "
    "longitude\",-180,AUTHORITY[\"EPSG\",\"8830\"]],PARAMETER[\"Zone "
    "width\",6,AUTHORITY[\"EPSG\",\"8831\"]],PARAMETER[\"Scale factor at "
    "natural origin\",0.9996,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",500000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],AUTHORITY[\"EPSG\","
    "\"32600\"]]";
dd::String_type epsg32700 =
    "PROJCS[\"WGS 84 / UTM grid system (southern hemisphere)\",GEOGCS[\"WGS "
    "84\",DATUM[\"World Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Transverse Mercator Zoned Grid "
    "System\",AUTHORITY[\"EPSG\",\"9824\"]],PARAMETER[\"Latitude of natural "
    "origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Initial "
    "longitude\",-180,AUTHORITY[\"EPSG\",\"8830\"]],PARAMETER[\"Zone "
    "width\",6,AUTHORITY[\"EPSG\",\"8831\"]],PARAMETER[\"Scale factor at "
    "natural origin\",0.9996,AUTHORITY[\"EPSG\",\"8805\"]],PARAMETER[\"False "
    "easting\",500000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",10000000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",EAST],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"32700\"]]";
// 9828
dd::String_type epsg5017 =
    "PROJCS[\"Lisbon 1890 / Portugal Bonne New\",GEOGCS[\"Lisbon "
    "1890\",DATUM[\"Lisbon 1890\",SPHEROID[\"Bessel "
    "1841\",6377397.155,299.1528128,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[631."
    "392,-66.551,481.442,1.09,-4.445,-4.487,-4.43],AUTHORITY[\"EPSG\",\"6666\"]"
    "],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4666\"]],PROJECTION[\"Bonne (South "
    "Orientated)\",AUTHORITY[\"EPSG\",\"9828\"]],PARAMETER[\"Latitude of "
    "natural "
    "origin\",39.6777777777778,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",-8.13190611111111,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER["
    "\"False easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"P\",SOUTH],AXIS[\"M\",WEST],AUTHORITY[\"EPSG\","
    "\"5017\"]]";
// 9829
dd::String_type epsg3032 =
    "PROJCS[\"WGS 84 / Australian Antarctic Polar Stereographic\",GEOGCS[\"WGS "
    "84\",DATUM[\"World Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Polar Stereographic (variant "
    "B)\",AUTHORITY[\"EPSG\",\"9829\"]],PARAMETER[\"Latitude of standard "
    "parallel\",-71,AUTHORITY[\"EPSG\",\"8832\"]],PARAMETER[\"Longitude of "
    "origin\",70,AUTHORITY[\"EPSG\",\"8833\"]],PARAMETER[\"False "
    "easting\",6000000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",6000000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,"
    "AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"E\",NORTH],AXIS[\"N\",NORTH],"
    "AUTHORITY[\"EPSG\",\"3032\"]]";
// 9830
dd::String_type epsg2985 =
    "PROJCS[\"Petrels 1972 / Terre Adelie Polar "
    "Stereographic\",GEOGCS[\"Petrels 1972\",DATUM[\"Petrels "
    "1972\",SPHEROID[\"International "
    "1924\",6378388,297,AUTHORITY[\"EPSG\",\"7022\"]],TOWGS84[365,194,166,0,0,"
    "0,0],AUTHORITY[\"EPSG\",\"6636\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY["
    "\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY["
    "\"EPSG\",\"4636\"]],PROJECTION[\"Polar Stereographic (variant "
    "C)\",AUTHORITY[\"EPSG\",\"9830\"]],PARAMETER[\"Latitude of standard "
    "parallel\",-67,AUTHORITY[\"EPSG\",\"8832\"]],PARAMETER[\"Longitude of "
    "origin\",140,AUTHORITY[\"EPSG\",\"8833\"]],PARAMETER[\"Easting at false "
    "origin\",300000,AUTHORITY[\"EPSG\",\"8826\"]],PARAMETER[\"Northing at "
    "false "
    "origin\",200000,AUTHORITY[\"EPSG\",\"8827\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",NORTH],AXIS[\"Y\",NORTH],AUTHORITY["
    "\"EPSG\",\"2985\"]]";
// 9831
dd::String_type epsg3993 =
    "PROJCS[\"Guam 1963 / Guam SPCS\",GEOGCS[\"Guam 1963\",DATUM[\"Guam "
    "1963\",SPHEROID[\"Clarke "
    "1866\",6378206.4,294.9786982138982,AUTHORITY[\"EPSG\",\"7008\"]],TOWGS84[-"
    "100,-248,259,0,0,0,0],AUTHORITY[\"EPSG\",\"6675\"]],PRIMEM[\"Greenwich\","
    "0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4675\"]],PROJECTION[\"Guam "
    "Projection\",AUTHORITY[\"EPSG\",\"9831\"]],PARAMETER[\"Latitude of "
    "natural "
    "origin\",13.4724663527778,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",144.748750705556,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",50000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",50000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"3993\"]]";
// 9832
dd::String_type epsg3295 =
    "PROJCS[\"Guam 1963 / Yap Islands\",GEOGCS[\"Guam 1963\",DATUM[\"Guam "
    "1963\",SPHEROID[\"Clarke "
    "1866\",6378206.4,294.9786982138982,AUTHORITY[\"EPSG\",\"7008\"]],TOWGS84[-"
    "100,-248,259,0,0,0,0],AUTHORITY[\"EPSG\",\"6675\"]],PRIMEM[\"Greenwich\","
    "0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],"
    "AUTHORITY[\"EPSG\",\"4675\"]],PROJECTION[\"Modified Azimuthal "
    "Equidistant\",AUTHORITY[\"EPSG\",\"9832\"]],PARAMETER[\"Latitude of "
    "natural "
    "origin\",9.54670833333333,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",138.168744444444,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",40000,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",60000,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"3295\"]]";
// 9833
dd::String_type epsg3139 =
    "PROJCS[\"Vanua Levu 1915 / Vanua Levu Grid\",GEOGCS[\"Vanua Levu "
    "1915\",DATUM[\"Vanua Levu 1915\",SPHEROID[\"Clarke 1880 (international "
    "foot)\",6378306.3696,293.46630765562986,AUTHORITY[\"EPSG\",\"7055\"]],"
    "TOWGS84[51,391,-36,0,0,0,0],AUTHORITY[\"EPSG\",\"6748\"]],PRIMEM["
    "\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0."
    "017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lat\",NORTH],AXIS["
    "\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4748\"]],PROJECTION[\"Hyperbolic "
    "Cassini-Soldner\",AUTHORITY[\"EPSG\",\"9833\"]],PARAMETER[\"Latitude of "
    "natural "
    "origin\",-16.2611111111111,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER["
    "\"Longitude of natural "
    "origin\",179.344444444444,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",1251331.8,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",1662888.5,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"link\",0.201168,"
    "AUTHORITY[\"EPSG\",\"9098\"]],AXIS[\"X\",NORTH],AXIS[\"Y\",EAST],"
    "AUTHORITY[\"EPSG\",\"3139\"]]";
// 9834
dd::String_type epsg3410 =
    "PROJCS[\"NSIDC EASE-Grid Global\",GEOGCS[\"Unspecified datum based upon "
    "the International 1924 Authalic Sphere\",DATUM[\"Not specified (based on "
    "International 1924 Authalic Sphere)\",SPHEROID[\"International 1924 "
    "Authalic "
    "Sphere\",6371228,0,AUTHORITY[\"EPSG\",\"7057\"]],TOWGS84[0,0,0,0,0,0,0],"
    "AUTHORITY[\"EPSG\",\"6053\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\","
    "\"8901\"]],UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\","
    "\"9122\"]],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\","
    "\"4053\"]],PROJECTION[\"Lambert Cylindrical Equal Area "
    "(Spherical)\",AUTHORITY[\"EPSG\",\"9834\"]],PARAMETER[\"Latitude of 1st "
    "standard "
    "parallel\",30,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER[\"Longitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"3410\"]]";
// 9835
dd::String_type epsg6933 =
    "PROJCS[\"WGS 84 / NSIDC EASE-Grid 2.0 Global\",GEOGCS[\"WGS "
    "84\",DATUM[\"World Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY["
    "\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
    "UNIT[\"degree\",0.017453292519943278,AUTHORITY[\"EPSG\",\"9122\"]],AXIS["
    "\"Lat\",NORTH],AXIS[\"Lon\",EAST],AUTHORITY[\"EPSG\",\"4326\"]],"
    "PROJECTION[\"Lambert Cylindrical Equal "
    "Area\",AUTHORITY[\"EPSG\",\"9835\"]],PARAMETER[\"Latitude of 1st standard "
    "parallel\",30,AUTHORITY[\"EPSG\",\"8823\"]],PARAMETER[\"Longitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1,AUTHORITY["
    "\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\","
    "\"6933\"]]";

template <typename Geometry>
struct print {
  static void apply() {}
};

template <>
struct print<gis::Cartesian_point> {
  static void apply(gis::Cartesian_point p) {
    std::cout << std::setprecision(20) << p.x() << " , " << p.y() << std::endl;
  }
};
template <>
struct print<gis::Geographic_point> {
  static void apply(gis::Geographic_point p) {
    const double to_rad = boost::geometry::math::pi<double>() / 180.0;
    std::cout << std::setprecision(20) << p.x() / to_rad << " , "
              << p.y() / to_rad << std::endl;
  }
};

template <>
struct print<gis::Cartesian_linestring> {
  static void apply(gis::Cartesian_linestring g2) {
    for (std::size_t p = 0; p < g2.size(); p++) {
      std::cout << std::setprecision(20) << g2[p].x() << "," << g2[p].y()
                << " ";
    }
    std::cout << std::endl;
  }
};

template <>
struct print<gis::Geographic_linestring> {
  static void apply(gis::Geographic_linestring g2) {
    for (std::size_t p = 0; p < g2.size(); p++) {
      std::cout << std::setprecision(20) << g2[p].x() << "," << g2[p].y()
                << " ";
    }
    std::cout << std::endl;
  }
};

template <typename Geometry1>
auto coverage_transform(const dd::Spatial_reference_system_impl &srs1,
                        const dd::Spatial_reference_system_impl &srs2,
                        const Geometry1 &g1) {
  if (srs1.is_projected())
    assert(g1.coordinate_system() == gis::Coordinate_system::kCartesian);
  else
    assert(g1.coordinate_system() == gis::Coordinate_system::kGeographic);

  std::unique_ptr<gis::Geometry> result_g;
  gis::transform(&srs1, g1, &srs2, "unittest", &result_g);

  return result_g;
}

template <typename Geometry1>
void coverage_transform(const dd::String_type &srs1_str,
                        const dd::String_type &srs2_str, const Geometry1 &g1) {
  auto srs1 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs1->set_id(1000001);
  srs1->set_name("Test1");
  srs1->set_created(0UL);
  srs1->set_last_altered(0UL);
  srs1->set_definition(srs1_str);
  srs1->parse_definition();

  auto srs2 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs2->set_id(1000000);
  srs2->set_name("Test2");
  srs2->set_created(0UL);
  srs2->set_last_altered(0UL);
  srs2->set_definition(srs2_str);
  srs2->parse_definition();

  coverage_transform(*srs1, *srs2, g1);
}

template <typename Geometry1, typename Geometry2>
void check_transform(const dd::Spatial_reference_system_impl &srs1,
                     const dd::Spatial_reference_system_impl &srs2,
                     const Geometry1 &g1, const Geometry2 &g2,
                     bool check_inverse) {
  if (srs2.is_projected())
    assert(g2.coordinate_system() == gis::Coordinate_system::kCartesian);
  else
    assert(g2.coordinate_system() == gis::Coordinate_system::kGeographic);

  // print<Geometry1>::apply(g1);
  // print<Geometry2>::apply(g2);

  std::unique_ptr<gis::Geometry> result_g;
  result_g = coverage_transform(srs1, srs2, g1);

  auto g = dynamic_cast<Geometry2 *>(result_g.get());

  // Verify result is correct.
  EXPECT_NEAR(g->x(), g2.x(), 0.000001f);
  EXPECT_NEAR(g->y(), g2.y(), 0.000001f);

  // for debugging
  // print<Geometry2>::apply(*g);
  // print<Geometry2>::apply(g2);

  // test inverse tranformation
  if (check_inverse) {
    std::unique_ptr<gis::Geometry> result_g_inv;
    result_g_inv = coverage_transform(srs2, srs1, g2);

    auto g_inv = dynamic_cast<Geometry1 *>(result_g_inv.get());

    // Verify result is correct.
    EXPECT_NEAR(g_inv->x(), g1.x(), 0.1f);
    EXPECT_NEAR(g_inv->y(), g1.y(), 0.1f);
  }
}

template <typename Geometry1, typename Geometry2>
void check_transform(const dd::String_type &srs1_str,
                     const dd::String_type &srs2_str, const Geometry1 &g1,
                     const Geometry2 &g2, bool check_inverse = true) {
  auto srs1 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs1->set_id(1000001);
  srs1->set_name("Test1");
  srs1->set_created(0UL);
  srs1->set_last_altered(0UL);
  srs1->set_definition(srs1_str);
  srs1->parse_definition();

  auto srs2 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs2->set_id(1000000);
  srs2->set_name("Test2");
  srs2->set_created(0UL);
  srs2->set_last_altered(0UL);
  srs2->set_definition(srs2_str);
  srs2->parse_definition();

  check_transform(*srs1, *srs2, g1, g2, check_inverse);
}

TEST(TransformTest, GeogcsProjcsCombinations) {
  gis::Geographic_point gp{0.001, 0.0002};

  // Point to Point transformations
  // geogcs - geogcs
  check_transform(wgs84, modairy, gp,
                  gis::Geographic_point{0.0010000000000000000208,
                                        0.00019999503232222135473});
  check_transform(
      modairy, wgs84, gis::Geographic_point{0.1, 0.2},
      gis::Geographic_point{0.099999999999999991673, 0.20000483632518123445});

  // geogcs - projcs
  check_transform(
      wgs84, webmerc3857, gp,
      gis::Cartesian_point{6378.1369999999997162, 1275.6274085031250252});
  check_transform(
      webmerc3857, wgs84,
      gis::Cartesian_point{6378.1369999999997162, 1275.6274085031250252},
      gis::Geographic_point{0.00099999999999999980398,
                            0.00019999999999975592857});
  check_transform(
      modairy, webmerc3857, gp,
      gis::Cartesian_point{6378.1369999999997162, 1275.6590978060371526});
  check_transform(
      wgs84, webmerc_modairy, gp,
      gis::Cartesian_point{6377.3401890000004641, 1275.4363657304534172});
  check_transform(
      modairy, webmerc_modairy, gp,
      gis::Cartesian_point{6377.3401890000004641, 1275.468046302062703});
  check_transform(wgs84, wgs84tmerc, gis::Geographic_point{0., 0.},
                  gis::Cartesian_point{-3801310.4438896430656, 10000000});

  // projcs - projcs
  check_transform(webmerc3857, webmerc_modairy,
                  gis::Cartesian_point{6378.137, 1275.627},
                  gis::Cartesian_point{6377.3401889999986, 1275.4359572862386});
  check_transform(
      webmerc_modairy, webmerc3857,
      gis::Cartesian_point{6377.3401889999986, 1275.4359572862386},
      gis::Cartesian_point{6378.1369999999969878, 1275.6270039824446485});
  check_transform(webmerc3857, wgs84tmerc, gis::Cartesian_point{0., 0.},
                  gis::Cartesian_point{-3801310.4438896430656, 10000000});

  // for the rest of geometry types perform unit tests for coverage
  coverage_transform(webmerc3857, wgs84, base_py<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_ls<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_mpt<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_mls<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_mpy<gis_typeset::Cartesian>());
  typename gis_typeset::Cartesian::Geometrycollection gc_cartesian;
  gc_cartesian.push_back(simple_ls<gis_typeset::Cartesian>());
  gc_cartesian.push_back(base_py<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, gc_cartesian);

  coverage_transform(wgs84, webmerc3857, base_py<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, simple_mpt<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, simple_mls<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, simple_mpy<gis_typeset::Geographic>());
  typename gis_typeset::Geographic::Geometrycollection gc_geo;
  gc_geo.push_back(simple_ls<gis_typeset::Geographic>());
  gc_geo.push_back(base_py<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, gc_geo);

  // Test all supported projected SRSs
  const double to_rad = boost::geometry::math::pi<double>() / 180.0;

  // EPSG 1027
  check_transform(wgs84, epsg2163,
                  gis::Geographic_point{10 * to_rad, 52 * to_rad},
                  gis::Cartesian_point{4413901.784906911, 5358732.968947821});

  // EPSG 1028
  check_transform(wgs84, epsg4087,
                  gis::Geographic_point{10 * to_rad, 52 * to_rad},
                  gis::Cartesian_point{1113194.9079327346, 5788613.52125022});

  // EPSG 1029
  check_transform(wgs84, epsg4088,
                  gis::Geographic_point{10 * to_rad, 52 * to_rad},
                  gis::Cartesian_point{1111950.48817606, 5761364.71140026});

  // EPSG 1041
  check_transform(wgs84, epsg5514,
                  gis::Geographic_point{16.84977 * to_rad, 50.20901 * to_rad},
                  gis::Cartesian_point{-568885.6301656856, -1050469.445646209});

  // EPSG 1051	Lambert Conic Conformal (2SP Michigan)
  check_transform(
      wgs84, epsg6201,
      gis::Geographic_point{-83.166666653 * to_rad, 43.750000014 * to_rad},
      gis::Cartesian_point{2308321.22103756, 156019.767239717});

  // EPSG 1052
  check_transform(
      wgs84, epsg6247,
      gis::Geographic_point{-74.250000023 * to_rad, 4.7999999945 * to_rad},
      gis::Cartesian_point{80859.03040774254, 122543.173684438});

  // EPSG 9801
  check_transform(
      wgs84, epsg24200,
      gis::Geographic_point{-76.943683174 * to_rad, 17.932166647 * to_rad},
      gis::Cartesian_point{255854.11037737486, 142204.1008478572});

  // EPSG 9802
  check_transform(
      wgs84, epsg32040,
      gis::Geographic_point{-95.99999989 * to_rad, 28.500000182 * to_rad},
      gis::Cartesian_point{2963639.1656968565658, 254577.3224964972178});

  // EPSG 9803
  check_transform(
      wgs84, epsg31300,
      gis::Geographic_point{5.80737015 * to_rad, 50.679572292 * to_rad},
      gis::Cartesian_point{252415.172330661, 153108.899168551});

  // EPSG 9804
  check_transform(
      wgs84, epsg3002,
      gis::Geographic_point{119.99999986 * to_rad, -3.0000001398 * to_rad},
      gis::Cartesian_point{5009477.80168315, 568973.523495937});

  // EPSG 9805
  check_transform(
      wgs84, epsg3388,
      gis::Geographic_point{52.999999796 * to_rad, 52.999999796 * to_rad},
      gis::Cartesian_point{165825.171978172, 5171814.99912084});

  // EPSG 9806 Cassini-Soldner
  check_transform(
      wgs84, epsg30200,
      gis::Geographic_point{-62.000000216 * to_rad, 10.000000275 * to_rad},
      gis::Cartesian_point{66247.6252721806, 80477.5039249589});

  // EPSG 9807	Transverse Mercator
  check_transform(
      wgs84, epsg27700,
      gis::Geographic_point{0.50000021429 * to_rad, 50.499999871 * to_rad},
      gis::Cartesian_point{577393.388440983, 69673.6088671646});

  // EPSG 9808	Transverse Mercator (South Orientated)
  check_transform(
      wgs84, epsg2053,
      gis::Geographic_point{28.282632944 * to_rad, -25.732028354 * to_rad},
      gis::Cartesian_point{71984.4909153351, 2847342.73756047});

  // EPSG 9809	Oblique Stereographic
  check_transform(
      wgs84, epsg28992,
      gis::Geographic_point{5.9999999931 * to_rad, 53.000000025 * to_rad},
      gis::Cartesian_point{196139.436718705, 557179.096590178});

  // EPSG 9810	Polar Stereographic (variant A)
  check_transform(
      wgs84, epsg5041,
      gis::Geographic_point{44.000000007 * to_rad, 73.000000003 * to_rad},
      gis::Cartesian_point{3320416.74729058, 632668.431678171});

  // EPSG 9811	New Zealand Map Grid
  check_transform(
      wgs84, epsg27200,
      gis::Geographic_point{174.763336 * to_rad, -36.848461 * to_rad},
      gis::Cartesian_point{2667648.97907212, 6482184.9814991});

  // EPSG 9812	Hotine Oblique Mercator (variant A)
  check_transform(
      wgs84, epsg3079, gis::Geographic_point{117 * to_rad, 12 * to_rad},
      gis::Cartesian_point{-4893794.4284746721, 12634528.929278262});

  // EPSG 9813	Laborde Oblique Mercator
  check_transform(
      wgs84, epsg8441,
      gis::Geographic_point{44.45757 * to_rad, -16.189799986 * to_rad},
      gis::Cartesian_point{188364.97174500348, 1100212.7585672687});

  // EPSG 9815	Hotine Oblique Mercator (variant B)
  check_transform(
      wgs84, epsg29873,
      gis::Geographic_point{115.80550545 * to_rad, 5.3872536023 * to_rad},
      gis::Cartesian_point{678925.284380531, 596659.268844775});

  // EPSG 9817	Lambert Conic Near-Conformal
  check_transform(
      wgs84, epsg22700,
      gis::Geographic_point{34.136469742 * to_rad, 37.521562493 * to_rad},
      gis::Cartesian_point{15583.7954048792, 623198.935092147});

  // EPSG 9818	American Polyconic
  check_transform(wgs84, epsg5880,
                  gis::Geographic_point{-45 * to_rad, -6 * to_rad},
                  gis::Cartesian_point{5996378.70981776, 9328349.94407545});

  // EPSG 9819	Krovak
  check_transform(wgs84, epsg5513,
                  gis::Geographic_point{16.84977 * to_rad, 50.20901 * to_rad},
                  gis::Cartesian_point{1050469.44564621, 568885.630165686});

  // EPSG 9820	Lambert Azimuthal Equal Area
  check_transform(wgs84, epsg3035,
                  gis::Geographic_point{5 * to_rad, 50 * to_rad},
                  gis::Cartesian_point{3962799.45095507, 2999718.85315956});

  // EPSG 9822	Albers Equal Area
  check_transform(wgs84, epsg3174,
                  gis::Geographic_point{-78.75 * to_rad, 42.749999987 * to_rad},
                  gis::Cartesian_point{1466492.30576324, 702903.122081279});

  // EPSG 9824	Transverse Mercator Zoned Grid System
  check_transform(wgs84, epsg32600,
                  gis::Geographic_point{12 * to_rad, 56 * to_rad},
                  gis::Cartesian_point{
                      1798179.0365446017,
                      13588963.310720725,
                  });

  // EPSG 9824
  check_transform(
      wgs84, epsg32700, gis::Geographic_point{174 * to_rad, -44 * to_rad},
      gis::Cartesian_point{-2617060.1631802432, 4328084.4894244494});

  // EPSG 9828	Bonne (South Orientated)
  check_transform(wgs84, epsg5017,
                  gis::Geographic_point{-9.142685 * to_rad, 38.736946 * to_rad},
                  gis::Cartesian_point{87766.669137895, -3183066.76596979});

  // EPSG 9829	Polar Stereographic (variant B)
  check_transform(wgs84, epsg3032,
                  gis::Geographic_point{120 * to_rad, -75 * to_rad},
                  gis::Cartesian_point{7255380.79325839, 7053389.56061016});

  // EPSG 9830	Polar Stereographic (variant C)
  check_transform(
      wgs84, epsg2985,
      gis::Geographic_point{140.07140001 * to_rad, -66.605227791 * to_rad},
      gis::Cartesian_point{303553.11039781151, 244065.20291350142});
  check_transform(
      geogcs2985, epsg2985,
      gis::Geographic_point{140.07140001 * to_rad, -66.605227791 * to_rad},
      gis::Cartesian_point{303169.52229904052, 244055.71902347734});

  // EPSG 9831	Guam Projection
  check_transform(
      wgs84, epsg3993,
      gis::Geographic_point{144.63533131 * to_rad, 13.33903845 * to_rad},
      gis::Cartesian_point{37452.289675775798969, 35082.299014313684893});

  // EPSG 9832	Modified Azimuthal Equidistant
  check_transform(
      wgs84, epsg3295,
      gis::Geographic_point{138.19303 * to_rad, 9.5965258594 * to_rad},
      gis::Cartesian_point{42414.396661825645, 65317.2630414931});

  // EPSG 9833	Hyperbolic Cassini-Soldner
  check_transform(
      wgs84, epsg3139,
      gis::Geographic_point{179.99433651 * to_rad, -16.841456514 * to_rad},
      gis::Cartesian_point{1597583.62617055, 1342373.86918921});

  // EPSG 9834	Lambert Cylindrical Equal Area (Spherical)
  check_transform(wgs84, epsg3410,
                  gis::Geographic_point{10 * to_rad, 52 * to_rad},
                  gis::Cartesian_point{963010.77464927, 5782482.73916603});

  // EPSG 9835	Lambert Cylindrical Equal Area
  check_transform(wgs84, epsg6933,
                  gis::Geographic_point{10 * to_rad, 52 * to_rad},
                  gis::Cartesian_point{964862.802508964, 5775916.83074435});

  // test some tranformations between projection SRSs

  // proj9 returns -6354577.27, -7784081.82
  check_transform(
      epsg3032, epsg8441, gis::Cartesian_point{1000000., 2400000.},
      gis::Cartesian_point{-7783930.2673280816525, -6352094.8328518876806},
      false);

  // proj9 returns -18969118.77, -12275282.69
  check_transform(
      epsg3032, epsg5041, gis::Cartesian_point{1000000, 2400000},
      gis::Cartesian_point{-18969118.766775749624, -12275282.685159243643});
}

}  // namespace gis_transform_unittest
