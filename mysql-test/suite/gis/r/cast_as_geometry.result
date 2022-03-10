#####################################
# Testing of general requirements   #
#####################################
#
####################################################################
# Requirement F-1a:                                                #
# Casting using CONVERT(..., <geometry type>) must be allowed      #
# and give the same result as CAST(... AS <geometry type>).        #
####################################################################
#
# Casting using CONVERT(..., POINT) must give the same result
# as CAST(... AS POINT).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS POINT), CONVERT(ST_GEOMFROMTEXT('MULTIPOINT((1 1))'), POINT));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS POINT), CONVERT(ST_GEOMFROMTEXT('MULTIPOINT((1 1))'), POINT))
1
#
# Casting using CONVERT(..., LINESTRING) must give the same result
# as CAST(... AS LINESTRING).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))') AS LINESTRING), CONVERT(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))'), LINESTRING));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))') AS LINESTRING), CONVERT(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))'), LINESTRING))
1
#
# Casting using CONVERT(..., POLYGON) must give the same result
# as CAST(... AS POLYGON).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)))') AS POLYGON), CONVERT(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)))'), POLYGON));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)))') AS POLYGON), CONVERT(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)))'), POLYGON))
1
#
# Casting using CONVERT(..., MULTIPOINT) must give the same result
# as CAST(... AS MULTIPOINT).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT), CONVERT(ST_GEOMFROMTEXT('POINT(1 1)'), MULTIPOINT));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT), CONVERT(ST_GEOMFROMTEXT('POINT(1 1)'), MULTIPOINT))
1
#
# Casting using CONVERT(..., MULTILINESTRING) must give the same result
# as CAST(... AS MULTILINESTRING).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS MULTILINESTRING), CONVERT(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), MULTILINESTRING));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS MULTILINESTRING), CONVERT(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), MULTILINESTRING))
1
#
# Casting using CONVERT(..., MULTIPOLYGON) must give the same result
# as CAST(... AS MULTIPOLYGON).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS MULTIPOLYGON), CONVERT(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))'), MULTIPOLYGON));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS MULTIPOLYGON), CONVERT(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))'), MULTIPOLYGON))
1
#
# Casting using CONVERT(..., GEOMETRYCOLLECTION) must give the same result
# as CAST(... AS GEOMETRYCOLLECTION).
# Must return 1.
SELECT ST_EQUALS(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION), CONVERT(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))'), GEOMETRYCOLLECTION));
ST_EQUALS(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION), CONVERT(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))'), GEOMETRYCOLLECTION))
1
#
####################################################################
# Requirement F-1b:                                                #
# Casting to geometry types using                                  #
# JSON_VALUE(... RETURNING <geometry type>) must not be allowed.   #
####################################################################
#
# Casting to POINT using JSON_VALUE(... RETURNING POINT) must raise error.
DO JSON_VALUE('{"geometry": {"type" : "Point", "coordinates": [1.0, 1.0]}}', '$.geometry' RETURNING POINT);
ERROR 22S01: Invalid cast from JSON to POINT.
#
# Casting to LINESTRING using JSON_VALUE(... RETURNING LINESTRING) must raise error.
DO JSON_VALUE('{"geometry": {"type": "LineString", "coordinates": [[1.0, 1.0], [2.0, 2.0]]}}', '$.geometry' RETURNING LINESTRING);
ERROR 22S01: Invalid cast from JSON to LINESTRING.
#
# Casting to POLYGON using JSON_VALUE(... RETURNING POLYGON) must raise error.
DO JSON_VALUE('{"geometry": {"type": "Polygon", "coordinates": [[[1.0, 1.0], [4.0, 1.0], [4.0, 4.0], [1.0, 4.0], [1.0, 1.0]]]}}', '$.geometry' RETURNING POLYGON);
ERROR 22S01: Invalid cast from JSON to POLYGON.
#
# Casting to MULTIPOINT using JSON_VALUE(... RETURNING MULTIPOINT) must raise error.
DO JSON_VALUE('{"geometry": {"type": "MultiPoint", "coordinates": [[1.0, 1.0], [2.0, 2.0]]}}', '$.geometry' RETURNING MULTIPOINT);
ERROR 22S01: Invalid cast from JSON to MULTIPOINT.
#
# Casting to MULTILINESTRING using JSON_VALUE(... RETURNING MULTILINESTRING) must raise error.
DO JSON_VALUE('{"geometry": {"type": "MultiLineString", "coordinates": [[[1.0, 1.0], [2.0, 2.0]], [[3.0, 3.0], [4.0, 4.0]]]}}', '$.geometry' RETURNING MULTILINESTRING);
ERROR 22S01: Invalid cast from JSON to MULTILINESTRING.
#
# Casting to MULTIPOLYGON using JSON_VALUE(... RETURNING MULTIPOLYGON) must raise error.
DO JSON_VALUE('{"geometry": {"type": "{"type": "MultiPolygon", "coordinates": [[[[1.0, 1.0], [4.0, 1.0], [4.0, 4.0], [1.0, 4.0], [1.0, 1.0]]], [[[2.0, 2.0], [2.0, 3.0], [3.0, 3.0], [3.0, 2.0], [2.0, 2.0]]]]}}', '$.geometry' RETURNING MULTIPOLYGON);
ERROR 22S01: Invalid cast from JSON to MULTIPOLYGON.
#
# Casting to GEOMETRYCOLLECTION using JSON_VALUE(... RETURNING GEOMETRYCOLLECTION) must raise error.
DO JSON_VALUE('{"geometry": {"type": "GeometryCollection", "geometries": [{"type": "Point", "coordinates": [1.0, 1.0]}, {"type": "LineString", "coordinates": [[2.0, 2.0], [3.0, 3.0]]}]}}', '$.geometry' RETURNING GEOMETRYCOLLECTION);
ERROR 22S01: Invalid cast from JSON to GEOMETRYCOLLECTION.
#
####################################################################
# Requirement F-1c:                                                #
# Casting to ARRAY of geometry types must not be allowed.          #
####################################################################
#
# Casting using CAST(... AS POINT ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of POINT'
#
# Casting using CAST(... AS LINESTRING ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of LINESTRING'
#
# Casting using CAST(... POLYGON ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS POLYGON ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of POLYGON'
#
# Casting using CAST(... MULTIPOINT ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of MULTIPOINT'
#
# Casting using CAST(... MULTILINESTRING ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of MULTILINESTRING>'
#
# Casting using CAST(... MULTIPOLYGON ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of MULTIPOLYGON'
#
# Casting using CAST(... GEOMETRYCOLLECTION ARRAY) must raise error.
DO CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION ARRAY);
ERROR 42000: This version of MySQL doesn't yet support 'CAST-ing data to array of GEOMETRYCOLLECTION'
#
####################################################################
# Requirement F-1d:                                                #
# Non-instantiable target types must not be allowed.               #
####################################################################
#
# Casting to GEOMETRY must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS GEOMETRY));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'GEOMETRY))' at line 1
#
# Casting to CURVE must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS CURVE));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'CURVE))' at line 1
#
# Casting to SURFACE must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS SURFACE));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'SURFACE))' at line 1
#
# Casting to MULTICURVE must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTICURVE));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'MULTICURVE))' at line 1
#
# Casting to MULTISURFACE must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTISURFACE));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'MULTISURFACE))' at line 1
#
####################################################################
# Requirement F-1e:                                                #
# Casting to non-geometry types must give same result as before.   #
####################################################################
#
# Casting geometry to SIGNED must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS SIGNED);
ERROR HY000: Incorrect arguments to cast_as_signed
#
# Casting geometry to UNSIGNED must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS UNSIGNED);
ERROR HY000: Incorrect arguments to cast_as_unsigned
#
# Casting geometry to YEAR must raise error.
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS YEAR);
ERROR HY000: Incorrect arguments to cast_as_year
#
# Casting geometry to DATE must return NULL.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DATE);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DATE)
NULL
#
# Casting geometry to TIME must return NULL.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS TIME);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS TIME)
NULL
#
# Casting geometry to DATETIME must return NULL.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DATETIME);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DATETIME)
NULL
#
# Casting geometry to BINARY must return SWKB.
SELECT HEX(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS BINARY));
HEX(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS BINARY))
000000000101000000000000000000F03F000000000000F03F
#
# Casting geometry to DECIMAL must return 0.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DECIMAL);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DECIMAL)
0
#
# Casting geometry to FLOAT must return 0.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS FLOAT);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS FLOAT)
0
#
# Casting geometry to DOUBLE must return 0.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DOUBLE);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS DOUBLE)
0
#
# Casting geometry to JSON must return JSON-string.
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS JSON);
CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS JSON)
{"type": "Point", "coordinates": [1.0, 1.0]}
#
####################################################################
# Requirement F-2:                                                 #
# Casting NULL to geometry types must return NULL.               #
####################################################################
#
SELECT CAST(NULL AS POINT);
CAST(NULL AS POINT)
NULL
#
####################################################################
# Requirement F-3:                                                 #
# Casting geometries that are not syntactically well-formed        #
# must not be allowed.                                             #
####################################################################
#
# Casting 'POINT(1)' to POINT must raise error.
DO CAST(0x000000000101000000000000000000F03F AS POINT);
ERROR 22023: Invalid GIS data provided to function cast_as_point.
#
####################################################################
# Requirement F-4:                                                 #
# Casting geometry with undefined SRS must raise error.          #
####################################################################
#
DO CAST(ST_GEOMFROMTEXT('POINT(1 1)', 256) AS POINT);
ERROR SR001: There's no spatial reference system with SRID 256.
#
####################################################################
# Requirement F-5:                                                 #
# Casting must not change coordinate values or order.              #
####################################################################
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('POINT(0 1)'), CAST(ST_GEOMFROMTEXT('POINT(0 1)') AS POINT));
ST_EQUALS(ST_GEOMFROMTEXT('POINT(0 1)'), CAST(ST_GEOMFROMTEXT('POINT(0 1)') AS POINT))
1
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 2, 3 4)'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 2), (3 4))') AS LINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 2, 3 4)'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 2), (3 4))') AS LINESTRING))
1
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 4 1, 4 4, 1 4, 1 1)') AS POLYGON));
ST_EQUALS(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 4 1, 4 4, 1 4, 1 1)') AS POLYGON))
1
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 2), (3 4))'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 2, 3 4)') AS MULTIPOINT));
ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 2), (3 4))'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 2, 3 4)') AS MULTIPOINT))
1
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 2 3, 3 3, 3 2, 2 2))'), CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 2 3, 3 3, 3 2, 2 2))') AS MULTILINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 2 3, 3 3, 3 2, 2 2))'), CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 2 3, 3 3, 3 2, 2 2))') AS MULTILINESTRING))
1
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 3 2, 3 3, 2 3, 2 2)))'), CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 3 2, 3 3, 2 3, 2 2))') AS MULTIPOLYGON));
ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 3 2, 3 3, 2 3, 2 2)))'), CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 3 2, 3 3, 2 3, 2 2))') AS MULTIPOLYGON))
1
#
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 2), POINT(3 4))'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 2), (3 4))') AS GEOMETRYCOLLECTION));
ST_EQUALS(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 2), POINT(3 4))'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 2), (3 4))') AS GEOMETRYCOLLECTION))
1
#
####################################################################
# Requirement F-6a:                                                #
# Casting must not change the SRS of the geometry.                 #
####################################################################
#
# Must return 0.
SELECT ST_SRID(CAST(ST_GEOMFROMTEXT('POINT(1 1)', 0) AS POINT));
ST_SRID(CAST(ST_GEOMFROMTEXT('POINT(1 1)', 0) AS POINT))
0
#
# Must return 4326.
SELECT ST_SRID(CAST(ST_GEOMFROMTEXT('POINT(45 90)', 4326) AS POINT));
ST_SRID(CAST(ST_GEOMFROMTEXT('POINT(45 90)', 4326) AS POINT))
4326
#
####################################################################
# Requirement F-6b:                                                #
# Casting geographic geometry with longitude outside of            #
# (-180, 180] must not be allowed.                                 #
####################################################################
#
# Casting POINT(0 180.000000001) with SRID=4326 to POINT must raise error.
DO CAST(0xE6100000010100000070890000008066400000000000000000 AS POINT);
ERROR 22S02: A parameter of function cast_as_point contains a geometry with longitude 180.000000, which is out of range. It must be within (-180.000000, 180.000000].
#
# Casting POINT(0 -180.000000001) with SRID=4326 to POINT must raise error.
DO CAST(0xE6100000010100000070890000008066C00000000000000000 AS POINT);
ERROR 22S02: A parameter of function cast_as_point contains a geometry with longitude -180.000000, which is out of range. It must be within (-180.000000, 180.000000].
#
####################################################################
# Requirement F-6c:                                                #
# Casting geographic geometry with latitude outside of             #
# [-90, 90] must not be allowed.                                   #
####################################################################
#
# Casting POINT(90.000000001 0) with SRID=4326 to POINT must raise error.
DO CAST(0xE610000001010000000000000000000000E112010000805640 AS POINT);
ERROR 22S03: A parameter of function cast_as_point contains a geometry with latitude 90.000000, which is out of range. It must be within [-90.000000, 90.000000].
#
# Casting POINT(-90.000000001 0) with SRID=4326 to POINT must raise error.
DO CAST(0xE610000001010000000000000000000000E1120100008056C0 AS POINT);
ERROR 22S03: A parameter of function cast_as_point contains a geometry with latitude -90.000000, which is out of range. It must be within [-90.000000, 90.000000].
#
####################################################################
# Queries with EXPLAIN should return appropiate function name      #
####################################################################
#
# Should return /* select#1 */ select cast(st_geomfromtext('point(1 1)') as point) AS `CAST(ST_GEOMFROMTEXT('POINT((1 1))') AS POINT)'
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('POINT(1 1)') as point) AS `CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT)`
#
# Should return /* select#1 */ select cast(st_geomfromtext('LINESTRING(1 1, 2 2)') as linestring) AS `CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING)`
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('LINESTRING(1 1, 2 2)') as linestring) AS `CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING)`
#
# Should return /* select#1 */ select cast(st_geomfromtext('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') as polygon) AS `CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS POLYGON)`
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS POLYGON);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') as polygon) AS `CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS POLYGON)`
#
# Should return /* select#1 */ select cast(st_geomfromtext('MULTIPOINT((1 1), (2 2))') as multipoint) AS `CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT)`
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('MULTIPOINT((1 1), (2 2))') as multipoint) AS `CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT)`
#
# Should return /* select#1 */ select cast(st_geomfromtext('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') as multilinestring) AS `CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING)`
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') as multilinestring) AS `CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING)`
#
# Should return /* select#1 */ select cast(st_geomfromtext('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') as multipolygon) AS `CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON)`
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') as multipolygon) AS `CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON)`
#
# Should return /* select#1 */ select cast(st_geomfromtext('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') as geometrycollection) AS `CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION)`
EXPLAIN SELECT CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select cast(st_geomfromtext('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') as geometrycollection) AS `CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION)`
####################################################################
# SELECT ... UNION SELECT ... with                                 #
# CAST(... AS <GEOMETRY_TYPE>) columns should work as expected     #
####################################################################
#
# Should return Point.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT)
UNION
SELECT CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
POINT(1 1)
#
# Should return Linestring.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING)
UNION
SELECT CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
LINESTRING(1 1,2 2)
#
# Should return Polygon.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS POLYGON)
UNION
SELECT CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))') AS POLYGON))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
POLYGON((1 1,4 1,4 4,1 4,1 1))
#
# Should return Multipoint.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT)
UNION
SELECT CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
MULTIPOINT((1 1),(2 2))
#
# Should return Multilinestring.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING) UNION SELECT CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING)
UNION
SELECT CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING) UNION SELECT CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTILINESTRING))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
MULTILINESTRING((1 1,2 2),(3 3,4 4))
#
# Should return Multipolygon.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON) UNION SELECT CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON)
UNION
SELECT CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON) UNION SELECT CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((2 2, 2 3, 3 3, 3 2, 2 2)))') AS MULTIPOLYGON))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
MULTIPOLYGON(((1 1,4 1,4 4,1 4,1 1)),((2 2,3 2,3 3,2 3,2 2)))
#
# Should return Geometrycollection.
WITH g(g1) AS (
SELECT CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION) UNION SELECT CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION)
UNION
SELECT CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION) UNION SELECT CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(2 2, 3 3))') AS GEOMETRYCOLLECTION))
SELECT ST_ASTEXT(g1) FROM g;
ST_ASTEXT(g1)
GEOMETRYCOLLECTION(POINT(1 1),LINESTRING(2 2,3 3))
