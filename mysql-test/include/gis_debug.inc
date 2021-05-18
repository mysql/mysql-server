#
# This is a shared file included from t/gis-precise.test and t/gis-debug.test
#
# - gis-precise.test is executed both in debug and production builds
# and makes sure that the checked GIS functions return the expected results.
# 
# - gis-debug.test is executed only in debug builds
# (and is skipped in production builds).
# gis-debug.test activates tracing of the internal GIS routines.
# The trace log is printed to the client side warnings.
# So gis-debug.test makes sure not only that the correct results are returned,
# but also check *how* these results were generated - makes sure that
# the internal GIS routines went through the expected data and code flow paths.
#

--disable_warnings
DROP TABLE IF EXISTS p1;
--enable_warnings

DELIMITER |;
CREATE PROCEDURE p1(dist DOUBLE, geom TEXT)
BEGIN
  DECLARE g GEOMETRY;
  SET g=ST_GeomFromText(geom);
  SELECT geom AS `-----`;
  SELECT dist, ST_GeometryType(@buf:=ST_Buffer(g, dist)) AS `buffer`, ROUND(ST_AREA(@buf),2) AS buf_area;
END|
DELIMITER ;|

--disable_query_log

--echo #
--echo # Testing ST_BUFFER with positive distance
--echo #
--error ER_GIS_INVALID_DATA
CALL p1(1, 'POINT(0 0))');
CALL p1(1, 'POINT(0 0)');
--error ER_GIS_INVALID_DATA
CALL p1(1, 'LineString(0 1, 1 1))');
CALL p1(1, 'LineString(0 1, 1 1)');
CALL p1(1, 'LineString(9 9,8 1,1 5,0 0)');
CALL p1(1, 'Polygon((2 2,2 8,8 8,8 2,2 2))');
CALL p1(1, 'Polygon((0 0,0 8,8 8,8 0,0 0),(2 2,6 2,6 6,2 6,2 2))');
CALL p1(1, 'Polygon((0 0, 0 8, 8 8, 8 10, -10 10, -10 0, 0 0))');
CALL p1(1, 'MultiPoint(9 9,8 1,1 5)');
CALL p1(1, 'MultiLineString((0 0,2 2))');
CALL p1(1, 'MultiLineString((0 0,2 2,0 4))');
CALL p1(1, 'MultiLineString((0 0,2 2),(0 2,2 0))');
CALL p1(1, 'MultiLineString((2 2,2 8,-2 8),(-6 -6, 6 6),(10 10, 14 14))');
CALL p1(1, 'MultiLineString((0 0,0 1),(3 0,3 1))');
--echo # Invalid geometry in input
CALL p1(1, 'MultiPolygon(((2 2,2 8,8 8,8 2,2 2)), ((9 9,8 1,1 5,9 9)))');
--echo # Invalid geometry in input
CALL p1(1, 'MultiPolygon(((2 2,2 8,8 8,8 2,2 2), (4 4,4 6,6 6,6 4,4 4)),((9 9,8 1,1 5,9 9)))');
CALL p1(1, 'GeometryCollection(Point(0 0))');
--error ER_GIS_INVALID_DATA
CALL p1(1, 'GeometryCollection(LineString(0 0, 2 2)))');
CALL p1(1, 'GeometryCollection(LineString(0 0, 2 2))');
--error ER_GIS_INVALID_DATA
CALL p1(1, 'GeometryCollection(Polygon((2 2,2 8,8 8,8 2,2 2))))');
CALL p1(1, 'GeometryCollection(Polygon((2 2,2 8,8 8,8 2,2 2)))');
CALL p1(1, 'GeometryCollection(MultiPoint(9 9,8 1,1 5))');
CALL p1(1, 'GeometryCollection(MultiLineString((0 0,0 1),(3 0,3 1)))');
CALL p1(1, 'GeometryCollection(MultiPolygon(((0 0, 3 0, 3 3, 0 3, 0 0)),((6 6,6 9,9 9,9 6,6 6))))');
CALL p1(1, 'GeometryCollection(Point(9 9),LineString(1 5,0 0),Polygon((2 2,2 8,8 8,8 2,2 2)))');


--echo #
--echo # Testing ST_BUFFER with zero distance
--echo #

# Be careful so as to not apply ST_AREA to something that is not a
# (multi)polygon since that is not valid in SQL.
CALL p1(0, 'Polygon((2 2,2 8,8 8,8 2,2 2))');
CALL p1(0, 'Polygon((0 0,0 8,8 8,8 0,0 0),(2 2,6 2,6 6,2 6,2 2))');
CALL p1(0, 'Polygon((0 0, 0 8, 8 8, 8 10, -10 10, -10 0, 0 0))');
--echo # Invalid geometry in input
CALL p1(0, 'MultiPolygon(((2 2,2 8,8 8,8 2,2 2)), ((9 9,8 1,1 5,9 9)))');
--echo # Invalid geometry in input
CALL p1(0, 'MultiPolygon(((2 2,2 8,8 8,8 2,2 2), (4 4,4 6,6 6,6 4,4 4)),((9 9,8 1,1 5,9 9)))');
--error ER_GIS_INVALID_DATA
CALL p1(0, 'GeometryCollection(Polygon((2 2,2 8,8 8,8 2,2 2))))');
CALL p1(0, 'GeometryCollection(Polygon((2 2,2 8,8 8,8 2,2 2)))');
CALL p1(0, 'GeometryCollection(MultiPolygon(((0 0, 3 0, 3 3, 0 3, 0 0)),((6 6,6 9,9 9,9 6,6 6))))');


--echo #
--echo # Testing ST_BUFFER with negative distance
--echo #

--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'POINT(0 0)');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'LineString(0 1, 1 1)');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'LineString(9 9,8 1,1 5,0 0)');
CALL p1(-1, 'Polygon((2 2,2 8,8 8,8 2,2 2))');
#
# Wrong shape
CALL p1(-1, 'Polygon((0 0,0 8,8 8,8 0,0 0),(2 2,6 2,6 6,2 6,2 2))');
# Wrong shape
CALL p1(-1, 'Polygon((0 0, 0 8, 8 8, 8 10, -10 10, -10 0, 0 0))');
#
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'MultiPoint(9 9,8 1,1 5)');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'MultiLineString((0 0,2 2))');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'MultiLineString((0 0,2 2,0 4))');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'MultiLineString((0 0,2 2),(0 2,2 0))');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'MultiLineString((2 2,2 8,-2 8),(-6 -6, 6 6),(10 10, 14 14))');
#
# Wrong shape
--echo # Invalid geometry in input
CALL p1(-1, 'MultiPolygon(((2 2,2 8,8 8,8 2,2 2)), ((9 9,8 1,1 5,9 9)))');
--echo # Invalid geometry in input
CALL p1(-1, 'MultiPolygon(((2 2,2 8,8 8,8 2,2 2), (4 4,4 6,6 6,6 4,4 4)),((9 9,8 1,1 5,9 9)))');
#
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'GeometryCollection(Point(0 0))');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'GeometryCollection(LineString(0 0, 2 2))');
--error ER_GIS_INVALID_DATA
CALL p1(-1, 'GeometryCollection(LineString(0 0, 2 2)))');
--error ER_GIS_INVALID_DATA
CALL p1(-1, 'GeometryCollection(Polygon((2 2,2 8,8 8,8 2,2 2))))');
CALL p1(-1, 'GeometryCollection(Polygon((2 2,2 8,8 8,8 2,2 2)))');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'GeometryCollection(MultiPoint(9 9,8 1,1 5))');
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'GeometryCollection(MultiLineString((0 0,0 1),(3 0,3 1)))');
#
# Wrong shape
# CALL p1(-1, 'GeometryCollection(MultiPolygon(((0 0, 3 0, 3 3, 0 3, 0 0)),((6 6,6 9,9 9,9 6,6 6))))');
#
--error ER_WRONG_ARGUMENTS
CALL p1(-1, 'GeometryCollection(Point(9 9),LineString(1 5,0 0),Polygon((2 2,2 8,8 8,8 2,2 2)))');


--enable_query_log

SELECT ST_CONTAINS(
  ST_GeomFromText('MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((6 6, 6 11, 11 11, 11 6, 6 6)))'),
  ST_GeomFromText('POINT(5 10)'));
SELECT ST_AsText(ST_UNION(
  ST_GeomFromText('MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((6 6, 6 11, 11 11, 11 6, 6 6)))'),
  ST_GeomFromText('POINT(5 10)')));

DROP PROCEDURE p1;

--echo #
--echo # Bug #13833019 ASSERTION `T1->RESULT_RANGE' FAILED IN GCALC_OPERATION_REDUCER::END_COUPLE
--echo #
SELECT ST_GeometryType(ST_BUFFER(ST_MULTIPOLYGONFROMTEXT('MULTIPOLYGON(((0 0,9 4,3 3,0 0)),((2 2,2 2,8 8,2 3,2 2)))'), 3));

--echo #
--echo # Bug #13832749 	HANDLE_FATAL_SIGNAL IN GCALC_FUNCTION::COUNT_INTERNAL
--echo #
SELECT ST_GeometryType(ST_BUFFER(ST_MULTIPOLYGONFROMTEXT('MULTIPOLYGON(((3 5,2 5,2 4,3 4,3 5)),((2 2,2 8,8 8,8 2,2 2), (4 4,4 6,6 6,6 4,4 4)), ((9 9,8 1,1 5,9 9)))'),1));


--echo #
--echo # Bug#13358363 - ASSERTION: N > 0 && N < SINUSES_CALCULATED*2+1 | GET_N_SINCOS/ADD_EDGE_BUFFER
--echo #

DO ST_BUFFER(ST_LINEFROMTEXT('linestring(1 1,2 2)'),'');

SELECT ST_WITHIN(
        ST_LINESTRINGFROMTEXT(' LINESTRING(3 8,9 2,3 8,3 3,7 6,4 7,4 7,8 1) '),
        ST_BUFFER(ST_MULTIPOLYGONFROMTEXT(' MULTIPOLYGON(((3 5,2 5,2 4,3 4,3 5)),((2 2,2 8,8 8,8 2,2 2),(4 4,4 6,6 6,6 4,4 4)),((0 5,3 5,3 2,1 2,1 1,3 1,3 0,0 0,0 3,2 3,2 4,0 4,0 5))) '),
                  ST_NUMINTERIORRINGS(ST_POLYGONFROMTEXT('POLYGON((3 5,2 4,2 5,3 5)) '))));

SELECT ST_DIMENSION(ST_BUFFER(ST_POLYGONFROMTEXT(' POLYGON((3 5,2 5,2 4,3 4,3 5)) '),
                              ST_NUMINTERIORRINGS(ST_POLYGONFROMTEXT(' POLYGON((0 0,9 3,4 2,0 0))'))));

SELECT ST_NUMINTERIORRINGS(
       ST_ENVELOPE(ST_BUFFER(ST_MULTIPOLYGONFROMTEXT('MULTIPOLYGON(((3 5,2 5,2 4,3 4,3 5))) '),
                             ST_SRID(ST_MULTILINESTRINGFROMTEXT('MULTILINESTRING((2 2,4 2,1 2,2 4,2 2)) ')))));

SELECT ST_ASTEXT(ST_BUFFER(ST_POLYGONFROMTEXT(' POLYGON((9 9,5 2,4 5,9 9))'),
                        ST_SRID(ST_GEOMETRYFROMTEXT(' MULTIPOINT(8 4,5 0,7 8,6 9,3 4,7 3,5 5) '))));

--echo #
--echo # 18701868 CRASH IN MY_DECIMAL::SANITY_CHECK,
--echo #          ORDER BY CAST (INVALID GIS FUNCTION RESULT)
--echo #

CREATE TABLE t1 (a INT) engine=innodb;
INSERT INTO t1 VALUES (1);
SELECT * FROM t1 ORDER BY cast(st_distance(1,1) as DECIMAL);
--error ER_GIS_INVALID_DATA
DELETE FROM t1 ORDER BY cast(st_distance(1,1) as DECIMAL);
--error ER_GIS_INVALID_DATA
UPDATE t1 SET a=1 ORDER BY cast(st_distance(1,1) as DECIMAL);
DROP TABLE t1;
