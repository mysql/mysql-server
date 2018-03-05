# NULL as one of the parameters must result in NULL return value.
SELECT ST_X(NULL);
ST_X(NULL)
NULL
SELECT ST_X(NULL, NULL);
ST_X(NULL, NULL)
NULL
SELECT ST_X(NULL, 1);
ST_X(NULL, 1)
NULL
SELECT ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL);
ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL)
NULL
# Invalid data is not allowed.
DO ST_X(x'000000000123456789abcdef');
ERROR 22023: Invalid GIS data provided to function st_x.
# New value must be within the allowed range.
DO ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 4326), -90.000000001);
ERROR 22S03: Latitude -90.000000 is out of range in function st_x. It must be within [-90.000000, 90.000000].
DO ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 90.000000001);
ERROR 22S03: Latitude 90.000000 is out of range in function st_x. It must be within [-90.000000, 90.000000].
DO ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 7035), -180.000000001);
ERROR 22S02: Longitude -180.000000 is out of range in function st_x. It must be within (-180.000000, 180.000000].
DO ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 180.000000001);
ERROR 22S02: Longitude 180.000000 is out of range in function st_x. It must be within (-180.000000, 180.000000].
# First parameter must be a point.
# Cartesian SRID 0
DO ST_X(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 0));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_x.
DO ST_X(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_x.
DO ST_X(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_x.
# Projected
DO ST_X(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_x.
DO ST_X(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_x.
DO ST_X(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_x.
# Geographic
DO ST_X(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_x.
DO ST_X(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_x.
DO ST_X(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_x.
DO ST_X(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_x.
# Get or set the X coordinate of SRID 0.
SELECT ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 0));
ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 0))
1
SELECT HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 0), 1));
HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 0), 1))
000000000101000000000000000000F03F0000000000000000
# Get or set the X coordinate of a projected SRS.
SELECT ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 3857));
ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 3857))
1
SELECT HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 3857), 1));
HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 3857), 1))
110F00000101000000000000000000F03F0000000000000000
# Get or set the X coordinate (latitude) of a point in a lat-long SRS.
SELECT ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 4326));
ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 4326))
1
SELECT HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1));
HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1))
E610000001010000000000000000000000000000000000F03F
# Get or set the X coordinate (longitude) of a point in a long-lat SRS.
SELECT ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 7035));
ST_X(ST_GEOMFROMTEXT('POINT(1 0)', 7035))
1
SELECT HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1));
HEX(ST_X(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1))
7B1B00000101000000000000000000F03F0000000000000000
