# NULL as one of the parameters must result in NULL return value.
SELECT ST_LATITUDE(NULL);
ST_LATITUDE(NULL)
NULL
SELECT ST_LATITUDE(NULL, NULL);
ST_LATITUDE(NULL, NULL)
NULL
SELECT ST_LATITUDE(NULL, 1);
ST_LATITUDE(NULL, 1)
NULL
SELECT ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL);
ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), NULL)
NULL
# Invalid data is not allowed.
DO ST_LATITUDE(x'000000000123456789abcdef');
ERROR 22023: Invalid GIS data provided to function st_latitude.
DO ST_LATITUDE(x'000000000123456789abcdef', 1);
ERROR 22023: Invalid GIS data provided to function st_latitude.
# New value must be within the allowed range.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), -90.000000001);
ERROR 22S03: Latitude -90.000000 is out of range in function st_latitude. It must be within [-90.000000, 90.000000].
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 90.000000001);
ERROR 22S03: Latitude 90.000000 is out of range in function st_latitude. It must be within [-90.000000, 90.000000].
# First parameter must be a point.
# Cartesian SRID 0
DO ST_LATITUDE(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 0));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 0));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_latitude.
# Projected
DO ST_LATITUDE(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 3857));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_latitude.
# Geographic
DO ST_LATITUDE(ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1)', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOINT((0 0), (1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 0 0)))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_latitude.
DO ST_LATITUDE(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0))', 4326));
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_latitude.
# Point must be geographic.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 0));
ERROR 22S00: Function st_latitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 0, which is not geographic.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 3857));
ERROR 22S00: Function st_latitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 3857, which is not geographic.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326));
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 0), 1);
ERROR 22S00: Function st_latitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 0, which is not geographic.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 3857), 1);
ERROR 22S00: Function st_latitude is only defined for geographic spatial reference systems, but one of its arguments is in SRID 3857, which is not geographic.
DO ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1);
# Get or set the latitude of a point in a lat-long SRS.
SELECT ST_LATITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 4326));
ST_LATITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 4326))
1
SELECT HEX(ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1));
HEX(ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 4326), 1))
E610000001010000000000000000000000000000000000F03F
# Get or set the latitude of a point in a long-lat SRS.
SELECT ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 7035));
ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 1)', 7035))
1
SELECT HEX(ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1));
HEX(ST_LATITUDE(ST_GEOMFROMTEXT('POINT(0 0)', 7035), 1))
7B1B000001010000000000000000000000000000000000F03F
# Get or set the latitude of a point in a lat-long SRS with a meridian
# that doesn't go through Greenwich.
SELECT ST_LATITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 4801));
ST_LATITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 4801))
1
SELECT HEX(ST_LATITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 4801), 1));
HEX(ST_LATITUDE(ST_GEOMFROMTEXT('POINT(1 0)', 4801), 1))
C112000001010000000000000000000000000000000000F03F
