--source include/have_geometry.inc

#
# Spatial objects
#

--disable_warnings
DROP TABLE IF EXISTS t1, gis_point, gis_line, gis_polygon, gis_multi_point, gis_multi_line, gis_multi_polygon, gis_geometrycollection, gis_geometry;
--enable_warnings

CREATE TABLE gis_point  (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g POINT);
CREATE TABLE gis_line  (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g LINESTRING);
CREATE TABLE gis_polygon   (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g POLYGON);
CREATE TABLE gis_multi_point (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g MULTIPOINT);
CREATE TABLE gis_multi_line (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g MULTILINESTRING);
CREATE TABLE gis_multi_polygon  (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g MULTIPOLYGON);
CREATE TABLE gis_geometrycollection  (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g GEOMETRYCOLLECTION);
CREATE TABLE gis_geometry (fid INTEGER PRIMARY KEY AUTO_INCREMENT, g GEOMETRY);

SHOW CREATE TABLE gis_point;
SHOW FIELDS FROM gis_point;
SHOW FIELDS FROM gis_line;
SHOW FIELDS FROM gis_polygon;
SHOW FIELDS FROM gis_multi_point;
SHOW FIELDS FROM gis_multi_line;
SHOW FIELDS FROM gis_multi_polygon;
SHOW FIELDS FROM gis_geometrycollection;
SHOW FIELDS FROM gis_geometry;


INSERT INTO gis_point VALUES 
(101, ST_PointFromText('POINT(10 10)')),
(102, ST_PointFromText('POINT(20 10)')),
(103, ST_PointFromText('POINT(20 20)')),
(104, ST_PointFromWKB(ST_AsWKB(ST_PointFromText('POINT(10 20)'))));

INSERT INTO gis_line VALUES
(105, ST_LineFromText('LINESTRING(0 0,0 10,10 0)')),
(106, ST_LineStringFromText('LINESTRING(10 10,20 10,20 20,10 20,10 10)')),
(107, ST_LineStringFromWKB(LineString(Point(10, 10), Point(40, 10))));

INSERT INTO gis_polygon VALUES
(108, ST_PolygonFromText('POLYGON((10 10,20 10,20 20,10 20,10 10))')),
(109, ST_PolyFromText('POLYGON((0 0,50 0,50 50,0 50,0 0), (10 10,20 10,20 20,10 20,10 10))')),
(110, ST_PolyFromWKB(Polygon(LineString(Point(0, 0), Point(30, 0), Point(30, 30), Point(0, 0)))));

INSERT INTO gis_multi_point VALUES
(111, ST_MultiPointFromText('MULTIPOINT(0 0,10 10,10 20,20 20)')),
(112, ST_MPointFromText('MULTIPOINT(1 1,11 11,11 21,21 21)')),
(113, ST_MPointFromWKB(MultiPoint(Point(3, 6), Point(4, 10))));

INSERT INTO gis_multi_line VALUES
(114, ST_MultiLineStringFromText('MULTILINESTRING((10 48,10 21,10 0),(16 0,16 23,16 48))')),
(115, ST_MLineFromText('MULTILINESTRING((10 48,10 21,10 0))')),
(116, ST_MLineFromWKB(MultiLineString(LineString(Point(1, 2), Point(3, 5)), LineString(Point(2, 5), Point(5, 8), Point(21, 7)))));


INSERT INTO gis_multi_polygon VALUES
(117, ST_MultiPolygonFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))')),
(118, ST_MPolyFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))')),
(119, ST_MPolyFromWKB(MultiPolygon(Polygon(LineString(Point(0, 3), Point(3, 3), Point(3, 0), Point(0, 3))))));

INSERT INTO gis_geometrycollection VALUES
(120, ST_GeomCollFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))')),
(121, ST_GeometryFromWKB(GeometryCollection(Point(44, 6), LineString(Point(3, 6), Point(7, 9)))));

INSERT into gis_geometry SELECT * FROM gis_point;
INSERT into gis_geometry SELECT * FROM gis_line;
INSERT into gis_geometry SELECT * FROM gis_polygon;
INSERT into gis_geometry SELECT * FROM gis_multi_point;
INSERT into gis_geometry SELECT * FROM gis_multi_line;
INSERT into gis_geometry SELECT * FROM gis_multi_polygon;
INSERT into gis_geometry SELECT * FROM gis_geometrycollection;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE gis_point;
ANALYZE TABLE gis_line;
ANALYZE TABLE gis_polygon;
ANALYZE TABLE gis_multi_point;
ANALYZE TABLE gis_multi_line;
ANALYZE TABLE gis_multi_polygon;
ANALYZE TABLE gis_geometrycollection;
ANALYZE TABLE gis_geometry;
-- enable_result_log
-- enable_query_log

SELECT fid, ST_AsText(g) FROM gis_point ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_line ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_polygon ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_multi_point ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_multi_line ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_multi_polygon ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_geometrycollection ORDER by fid;
SELECT fid, ST_AsText(g) FROM gis_geometry ORDER by fid;

SELECT fid, ST_Dimension(g) FROM gis_geometry ORDER by fid;
SELECT fid, ST_GeometryType(g) FROM gis_geometry ORDER by fid;
SELECT fid, ST_IsEmpty(g) FROM gis_geometry ORDER by fid;
SELECT fid, ST_AsText(ST_Envelope(g)) FROM gis_geometry ORDER by fid;
explain extended select ST_Dimension(g), ST_GeometryType(g), ST_IsEmpty(g), ST_AsText(ST_Envelope(g)) from gis_geometry;

SELECT fid, ST_X(g) FROM gis_point ORDER by fid;
SELECT fid, ST_Y(g) FROM gis_point ORDER by fid;
explain extended select ST_X(g),ST_Y(g) FROM gis_point;

SELECT fid, ST_AsText(ST_StartPoint(g)) FROM gis_line ORDER by fid;
SELECT fid, ST_AsText(ST_EndPoint(g)) FROM gis_line ORDER by fid;
SELECT fid, ST_Length(g) FROM gis_line ORDER by fid;
SELECT fid, ST_NumPoints(g) FROM gis_line ORDER by fid;
SELECT fid, ST_AsText(ST_PointN(g, 2)) FROM gis_line ORDER by fid;
SELECT fid, ST_IsClosed(g) FROM gis_line ORDER by fid;
explain extended select ST_AsText(ST_StartPoint(g)),ST_AsText(ST_EndPoint(g)),ST_Length(g),ST_NumPoints(g),ST_AsText(ST_PointN(g, 2)),ST_IsClosed(g) FROM gis_line;

SELECT fid, ST_AsText(ST_Centroid(g)) FROM gis_polygon ORDER by fid;
SELECT fid, ST_Area(g) FROM gis_polygon ORDER by fid;
SELECT fid, ST_AsText(ST_ExteriorRing(g)) FROM gis_polygon ORDER by fid;
SELECT fid, ST_NumInteriorRings(g) FROM gis_polygon ORDER by fid;
SELECT fid, ST_AsText(ST_InteriorRingN(g, 1)) FROM gis_polygon ORDER by fid;
explain extended select ST_AsText(ST_Centroid(g)),ST_Area(g),ST_AsText(ST_ExteriorRing(g)),ST_NumInteriorRings(g),ST_AsText(ST_InteriorRingN(g, 1)) FROM gis_polygon;

SELECT fid, ST_IsClosed(g) FROM gis_multi_line ORDER by fid;

SELECT fid, ST_AsText(ST_Centroid(g)) FROM gis_multi_polygon ORDER by fid;
SELECT fid, ST_Area(g) FROM gis_multi_polygon ORDER by fid;

SELECT fid, ST_NumGeometries(g) from gis_multi_point ORDER by fid;
SELECT fid, ST_NumGeometries(g) from gis_multi_line ORDER by fid;
SELECT fid, ST_NumGeometries(g) from gis_multi_polygon ORDER by fid;
SELECT fid, ST_NumGeometries(g) from gis_geometrycollection ORDER by fid;
explain extended SELECT fid, ST_NumGeometries(g) from gis_multi_point;

SELECT fid, ST_AsText(ST_GeometryN(g, 2)) from gis_multi_point ORDER by fid;
SELECT fid, ST_AsText(ST_GeometryN(g, 2)) from gis_multi_line ORDER by fid;
SELECT fid, ST_AsText(ST_GeometryN(g, 2)) from gis_multi_polygon ORDER by fid;
SELECT fid, ST_AsText(ST_GeometryN(g, 2)) from gis_geometrycollection ORDER by fid;
SELECT fid, ST_AsText(ST_GeometryN(g, 1)) from gis_geometrycollection ORDER by fid;
explain extended SELECT fid, ST_AsText(ST_GeometryN(g, 2)) from gis_multi_point;

SELECT g1.fid as first, g2.fid as second,
MBRWithin(g1.g, g2.g) as w, MBRContains(g1.g, g2.g) as c, MBROverlaps(g1.g, g2.g) as o,
MBREquals(g1.g, g2.g) as e, MBRDisjoint(g1.g, g2.g) as d, ST_Touches(g1.g, g2.g) as t,
MBRIntersects(g1.g, g2.g) as i, ST_Crosses(g1.g, g2.g) as r
FROM gis_geometrycollection g1, gis_geometrycollection g2 ORDER BY first, second;
explain extended SELECT g1.fid as first, g2.fid as second,
MBRWithin(g1.g, g2.g) as w, MBRContains(g1.g, g2.g) as c, MBROverlaps(g1.g, g2.g) as o,
MBREquals(g1.g, g2.g) as e, MBRDisjoint(g1.g, g2.g) as d, ST_Touches(g1.g, g2.g) as t,
MBRIntersects(g1.g, g2.g) as i, ST_Crosses(g1.g, g2.g) as r
FROM gis_geometrycollection g1, gis_geometrycollection g2 ORDER BY first, second;

DROP TABLE gis_point, gis_line, gis_polygon, gis_multi_point, gis_multi_line, gis_multi_polygon, gis_geometrycollection, gis_geometry;

#
# Check that ALTER TABLE doesn't loose geometry type
#
CREATE TABLE t1 (
  a INTEGER PRIMARY KEY AUTO_INCREMENT,
  gp  point,
  ln  linestring,
  pg  polygon,
  mp  multipoint,
  mln multilinestring,
  mpg multipolygon,
  gc  geometrycollection,
  gm  geometry
);

SHOW FIELDS FROM t1;
ALTER TABLE t1 ADD fid INT;
SHOW FIELDS FROM t1;
DROP TABLE t1;

create table t1 (pk integer primary key auto_increment, a geometry not null);
insert into t1 (a) values (ST_GeomFromText('Point(1 2)'));
-- error 1416
insert into t1 (a) values ('Garbage');
-- error 1416
insert IGNORE into t1 (a) values ('Garbage');

drop table t1;

create table t1 (pk integer primary key auto_increment, fl geometry not null);
--error 1416
insert into t1 (fl) values (1);
--error 1416
insert into t1 (fl) values (1.11);
--error 1416
insert into t1 (fl) values ("qwerty");
--error ER_GIS_INVALID_DATA
insert into t1 (fl) values (ST_pointfromtext('point(1,1)'));

drop table t1;

--echo End of 4.1 tests


#
# Bug#24563: MBROverlaps does not seem to function propertly
# Bug#54888: MBROverlaps missing in 5.1?
#

# Test all MBR* functions and their non-MBR-prefixed aliases,
# using shifted squares to verify the spatial relations.

CREATE TABLE t1 (name VARCHAR(100), square GEOMETRY);

INSERT INTO t1 VALUES("center", ST_GeomFromText('POLYGON (( 0 0, 0 2, 2 2, 2 0, 0 0))'));

INSERT INTO t1 VALUES("small",  ST_GeomFromText('POLYGON (( 0 0, 0 1, 1 1, 1 0, 0 0))'));
INSERT INTO t1 VALUES("big",    ST_GeomFromText('POLYGON (( 0 0, 0 3, 3 3, 3 0, 0 0))'));

INSERT INTO t1 VALUES("up",     ST_GeomFromText('POLYGON (( 0 1, 0 3, 2 3, 2 1, 0 1))'));
INSERT INTO t1 VALUES("up2",    ST_GeomFromText('POLYGON (( 0 2, 0 4, 2 4, 2 2, 0 2))'));
INSERT INTO t1 VALUES("up3",    ST_GeomFromText('POLYGON (( 0 3, 0 5, 2 5, 2 3, 0 3))'));

INSERT INTO t1 VALUES("down",   ST_GeomFromText('POLYGON (( 0 -1, 0  1, 2  1, 2 -1, 0 -1))'));
INSERT INTO t1 VALUES("down2",  ST_GeomFromText('POLYGON (( 0 -2, 0  0, 2  0, 2 -2, 0 -2))'));
INSERT INTO t1 VALUES("down3",  ST_GeomFromText('POLYGON (( 0 -3, 0 -1, 2 -1, 2 -3, 0 -3))'));

INSERT INTO t1 VALUES("right",  ST_GeomFromText('POLYGON (( 1 0, 1 2, 3 2, 3 0, 1 0))'));
INSERT INTO t1 VALUES("right2", ST_GeomFromText('POLYGON (( 2 0, 2 2, 4 2, 4 0, 2 0))'));
INSERT INTO t1 VALUES("right3", ST_GeomFromText('POLYGON (( 3 0, 3 2, 5 2, 5 0, 3 0))'));

INSERT INTO t1 VALUES("left",   ST_GeomFromText('POLYGON (( -1 0, -1 2,  1 2,  1 0, -1 0))'));
INSERT INTO t1 VALUES("left2",  ST_GeomFromText('POLYGON (( -2 0, -2 2,  0 2,  0 0, -2 0))'));
INSERT INTO t1 VALUES("left3",  ST_GeomFromText('POLYGON (( -3 0, -3 2, -1 2, -1 0, -3 0))'));

SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbrcontains  FROM t1 a1 JOIN t1 a2 ON MBRContains(   a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbrdisjoint  FROM t1 a1 JOIN t1 a2 ON MBRDisjoint(   a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbrequals     FROM t1 a1 JOIN t1 a2 ON MBREquals(      a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbrintersect FROM t1 a1 JOIN t1 a2 ON MBRIntersects( a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbroverlaps  FROM t1 a1 JOIN t1 a2 ON MBROverlaps(   a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbrtouches   FROM t1 a1 JOIN t1 a2 ON MBRTouches(    a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS mbrwithin    FROM t1 a1 JOIN t1 a2 ON MBRWithin(     a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;

SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS MBRcontains     FROM t1 a1 JOIN t1 a2 ON MBRContains(      a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS MBRdisjoint     FROM t1 a1 JOIN t1 a2 ON MBRDisjoint(      a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS MBRequals       FROM t1 a1 JOIN t1 a2 ON MBREquals(        a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS intersect    FROM t1 a1 JOIN t1 a2 ON MBRIntersects(    a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS MBRoverlaps     FROM t1 a1 JOIN t1 a2 ON MBROverlaps(      a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS ST_touches      FROM t1 a1 JOIN t1 a2 ON ST_Touches(       a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;
SELECT GROUP_CONCAT(a2.name ORDER BY a2.name) AS MBRwithin       FROM t1 a1 JOIN t1 a2 ON MBRWithin(        a1.square, a2.square) WHERE a1.name = "center" GROUP BY a1.name;

# MBROverlaps needs a few more tests, with point and line dimensions

--error ER_GIS_INVALID_DATA
SET @vert1   = ST_GeomFromText('POLYGON ((0 -2, 0 2, 0 -2))');
--error ER_GIS_INVALID_DATA
SET @horiz1  = ST_GeomFromText('POLYGON ((-2 0, 2 0, -2 0))');
--error ER_GIS_INVALID_DATA
SET @horiz2 = ST_GeomFromText('POLYGON ((-1 0, 3 0, -1 0))');
--error ER_GIS_INVALID_DATA
SET @horiz3 = ST_GeomFromText('POLYGON ((2 0, 3 0, 2 0))');
--error ER_GIS_INVALID_DATA
SET @point1 = ST_GeomFromText('POLYGON ((0 0))');
--error ER_GIS_INVALID_DATA
SET @point2 = ST_GeomFromText('POLYGON ((-2 0))');

SELECT GROUP_CONCAT(a1.name ORDER BY a1.name) AS MBRoverlaps FROM t1 a1 WHERE MBROverlaps(a1.square, @vert1) GROUP BY a1.name;
SELECT GROUP_CONCAT(a1.name ORDER BY a1.name) AS MBRoverlaps FROM t1 a1 WHERE MBROverlaps(a1.square, @horiz1) GROUP BY a1.name;
SELECT MBROverlaps(@horiz1, @vert1) FROM DUAL;
SELECT MBROverlaps(@horiz1, @horiz2) FROM DUAL;
SELECT MBROverlaps(@horiz1, @horiz3) FROM DUAL;
SELECT MBROverlaps(@horiz1, @point1) FROM DUAL;
SELECT MBROverlaps(@horiz1, @point2) FROM DUAL;

DROP TABLE t1;

--echo End of 5.0 tests
