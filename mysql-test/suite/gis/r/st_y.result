# NULL as one of the parameters must result in NULL return value.
SELECT ST_Y(NULL);
ST_Y(NULL)
NULL
SELECT ST_Y(NULL, NULL);
ST_Y(NULL, NULL)
NULL
SELECT ST_Y(NULL, 1);
ST_Y(NULL, 1)
NULL
SELECT ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL);
ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL)
NULL
# Invalid data is not allowed.
DO ST_Y(x'000000000123456789abcdef');
ERROR 22023: Invalid GIS data provided to function st_y.
# New value must be within the allowed range.
DO ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 4326), -180.000000001);
ERROR 22S02: Longitude -180.000000 is out of range in function st_y. It must be within (-180.000000, 180.000000].
DO ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 180.000000001);
ERROR 22S02: Longitude 180.000000 is out of range in function st_y. It must be within (-180.000000, 180.000000].
DO ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 7035), -90.000000001);
ERROR 22S03: Latitude -90.000000 is out of range in function st_y. It must be within [-90.000000, 90.000000].
DO ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 90.000000001);
ERROR 22S03: Latitude 90.000000 is out of range in function st_y. It must be within [-90.000000, 90.000000].
# First parameter must be a point.
# Cartesian SRID 0
DO ST_Y(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 0));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_y.
DO ST_Y(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_y.
DO ST_Y(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_y.
# Projected
DO ST_Y(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_y.
DO ST_Y(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_y.
DO ST_Y(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_y.
# Geographic
DO ST_Y(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_y.
DO ST_Y(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_y.
DO ST_Y(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_y.
DO ST_Y(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_y.
# Get or set the Y coordinate of SRID 0.
SELECT ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 0));
ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 0))
1
SELECT HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 0), 1));
HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 0), 1))
0000000001010000000000000000000000000000000000F03F
# Get or set the Y coordinate of a projected SRS.
SELECT ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 3857));
ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 3857))
1
SELECT HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 3857), 1));
HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 3857), 1))
110F000001010000000000000000000000000000000000F03F
# Get or set the Y coordinate (longitude) of a point in a lat-long SRS.
SELECT ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 4326));
ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 4326))
1
SELECT HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1));
HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1))
E61000000101000000000000000000F03F0000000000000000
# Get or set the Y coordinate (latitude) of a point in a long-lat SRS.
SELECT ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 7035));
ST_Y(ST_GEOMFROMTEXT('POINT(0 1)', 7035))
1
SELECT HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1));
HEX(ST_Y(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1))
7B1B000001010000000000000000000000000000000000F03F
