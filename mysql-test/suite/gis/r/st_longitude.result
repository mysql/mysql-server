# NULL as one of the parameters must result in NULL return value.
SELECT ST_LONGITUDE(NULL);
ST_LONGITUDE(NULL)
NULL
SELECT ST_LONGITUDE(NULL, NULL);
ST_LONGITUDE(NULL, NULL)
NULL
SELECT ST_LONGITUDE(NULL, 1);
ST_LONGITUDE(NULL, 1)
NULL
SELECT ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL);
ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL)
NULL
# Invalid data is not allowed.
DO ST_LONGITUDE(x'000000000123456789abcdef');
ERROR 22023: Invalid GIS data provided to function st_longitude.
DO ST_LONGITUDE(x'000000000123456789abcdef', 1);
ERROR 22023: Invalid GIS data provided to function st_longitude.
# New value must be within the allowed range.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), -180.000000001);
ERROR 22S02: Longitude -180.000000 is out of range in function st_longitude. It must be within (-180.000000, 180.000000].
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 180.000000001);
ERROR 22S02: Longitude 180.000000 is out of range in function st_longitude. It must be within (-180.000000, 180.000000].
# First parameter must be a point.
# Cartesian SRID 0
DO ST_LONGITUDE(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 0));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_longitude.
# Projected
DO ST_LONGITUDE(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_longitude.
# Geographic
DO ST_LONGITUDE(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_longitude.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_longitude.
# Point must be geographic.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 0));
ERROR 22S00: Function st_longitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 0, which is not geographic.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 3857));
ERROR 22S00: Function st_longitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 3857, which is not geographic.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326));
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 0), 1);
ERROR 22S00: Function st_longitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 0, which is not geographic.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 3857), 1);
ERROR 22S00: Function st_longitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 3857, which is not geographic.
DO ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1);
# Get or set the longitude of a point in a lat-long SRS.
SELECT ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4326));
ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4326))
1
SELECT HEX(ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4326), 1));
HEX(ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4326), 1))
E61000000101000000000000000000F03F0000000000000000
# Get or set the longitude of a point in a long-lat SRS.
SELECT ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 7035));
ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 7035))
1
SELECT HEX(ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1));
HEX(ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1))
7B1B00000101000000000000000000F03F0000000000000000
# Get or set the longitude of a point in a lat-long SRS with a meridian
# that doesn't go through Greenwich.
SELECT ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4801));
ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4801))
1.0000000000000009
SELECT HEX(ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4801), 1));
HEX(ST_LONGITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 4801), 1))
C11200000101000000040000000000F03F0000000000000000
