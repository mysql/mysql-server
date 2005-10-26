source include/have_geometry.inc;
--source include/have_ndb.inc

#
# Spatial objects
#

--disable_warnings
DROP TABLE IF EXISTS t1, gis_point, gis_line, gis_polygon, gis_multi_point, gis_multi_line, gis_multi_polygon, gis_geometrycollection, gis_geometry;
--enable_warnings

CREATE TABLE gis_point  (fid INTEGER, g POINT);
CREATE TABLE gis_line  (fid INTEGER, g LINESTRING);
CREATE TABLE gis_polygon   (fid INTEGER, g POLYGON);
CREATE TABLE gis_multi_point (fid INTEGER, g MULTIPOINT);
CREATE TABLE gis_multi_line (fid INTEGER, g MULTILINESTRING);
CREATE TABLE gis_multi_polygon  (fid INTEGER, g MULTIPOLYGON);
CREATE TABLE gis_geometrycollection  (fid INTEGER, g GEOMETRYCOLLECTION);
CREATE TABLE gis_geometry (fid INTEGER, g GEOMETRY);

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
(101, PointFromText('POINT(10 10)')),
(102, PointFromText('POINT(20 10)')),
(103, PointFromText('POINT(20 20)')),
(104, PointFromWKB(AsWKB(PointFromText('POINT(10 20)'))));

INSERT INTO gis_line VALUES
(105, LineFromText('LINESTRING(0 0,0 10,10 0)')),
(106, LineStringFromText('LINESTRING(10 10,20 10,20 20,10 20,10 10)')),
(107, LineStringFromWKB(LineString(Point(10, 10), Point(40, 10))));

INSERT INTO gis_polygon VALUES
(108, PolygonFromText('POLYGON((10 10,20 10,20 20,10 20,10 10))')),
(109, PolyFromText('POLYGON((0 0,50 0,50 50,0 50,0 0), (10 10,20 10,20 20,10 20,10 10))')),
(110, PolyFromWKB(Polygon(LineString(Point(0, 0), Point(30, 0), Point(30, 30), Point(0, 0)))));

INSERT INTO gis_multi_point VALUES
(111, MultiPointFromText('MULTIPOINT(0 0,10 10,10 20,20 20)')),
(112, MPointFromText('MULTIPOINT(1 1,11 11,11 21,21 21)')),
(113, MPointFromWKB(MultiPoint(Point(3, 6), Point(4, 10))));

INSERT INTO gis_multi_line VALUES
(114, MultiLineStringFromText('MULTILINESTRING((10 48,10 21,10 0),(16 0,16 23,16 48))')),
(115, MLineFromText('MULTILINESTRING((10 48,10 21,10 0))')),
(116, MLineFromWKB(MultiLineString(LineString(Point(1, 2), Point(3, 5)), LineString(Point(2, 5), Point(5, 8), Point(21, 7)))));


INSERT INTO gis_multi_polygon VALUES
(117, MultiPolygonFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))')),
(118, MPolyFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))')),
(119, MPolyFromWKB(MultiPolygon(Polygon(LineString(Point(0, 3), Point(3, 3), Point(3, 0), Point(0, 3))))));

INSERT INTO gis_geometrycollection VALUES
(120, GeomCollFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))')),
(121, GeometryFromWKB(GeometryCollection(Point(44, 6), LineString(Point(3, 6), Point(7, 9)))));

INSERT into gis_geometry SELECT * FROM gis_point;
INSERT into gis_geometry SELECT * FROM gis_line;
INSERT into gis_geometry SELECT * FROM gis_polygon;
INSERT into gis_geometry SELECT * FROM gis_multi_point;
INSERT into gis_geometry SELECT * FROM gis_multi_line;
INSERT into gis_geometry SELECT * FROM gis_multi_polygon;
INSERT into gis_geometry SELECT * FROM gis_geometrycollection;

SELECT fid, AsText(g) FROM gis_point ORDER by fid;
SELECT fid, AsText(g) FROM gis_line ORDER by fid;
SELECT fid, AsText(g) FROM gis_polygon ORDER by fid;
SELECT fid, AsText(g) FROM gis_multi_point ORDER by fid;
SELECT fid, AsText(g) FROM gis_multi_line ORDER by fid;
SELECT fid, AsText(g) FROM gis_multi_polygon ORDER by fid;
SELECT fid, AsText(g) FROM gis_geometrycollection ORDER by fid;
SELECT fid, AsText(g) FROM gis_geometry ORDER by fid;

SELECT fid, Dimension(g) FROM gis_geometry ORDER by fid;
SELECT fid, GeometryType(g) FROM gis_geometry ORDER by fid;
SELECT fid, IsEmpty(g) FROM gis_geometry ORDER by fid;
SELECT fid, AsText(Envelope(g)) FROM gis_geometry ORDER by fid;
explain extended select Dimension(g), GeometryType(g), IsEmpty(g), AsText(Envelope(g)) from gis_geometry;

SELECT fid, X(g) FROM gis_point ORDER by fid;
SELECT fid, Y(g) FROM gis_point ORDER by fid;
explain extended select X(g),Y(g) FROM gis_point;

SELECT fid, AsText(StartPoint(g)) FROM gis_line ORDER by fid;
SELECT fid, AsText(EndPoint(g)) FROM gis_line ORDER by fid;
SELECT fid, GLength(g) FROM gis_line ORDER by fid;
SELECT fid, NumPoints(g) FROM gis_line ORDER by fid;
SELECT fid, AsText(PointN(g, 2)) FROM gis_line ORDER by fid;
SELECT fid, IsClosed(g) FROM gis_line ORDER by fid;
explain extended select AsText(StartPoint(g)),AsText(EndPoint(g)),GLength(g),NumPoints(g),AsText(PointN(g, 2)),IsClosed(g) FROM gis_line;

SELECT fid, AsText(Centroid(g)) FROM gis_polygon ORDER by fid;
SELECT fid, Area(g) FROM gis_polygon ORDER by fid;
SELECT fid, AsText(ExteriorRing(g)) FROM gis_polygon ORDER by fid;
SELECT fid, NumInteriorRings(g) FROM gis_polygon ORDER by fid;
SELECT fid, AsText(InteriorRingN(g, 1)) FROM gis_polygon ORDER by fid;
explain extended select AsText(Centroid(g)),Area(g),AsText(ExteriorRing(g)),NumInteriorRings(g),AsText(InteriorRingN(g, 1)) FROM gis_polygon;

SELECT fid, IsClosed(g) FROM gis_multi_line ORDER by fid;

SELECT fid, AsText(Centroid(g)) FROM gis_multi_polygon ORDER by fid;
SELECT fid, Area(g) FROM gis_multi_polygon ORDER by fid;

SELECT fid, NumGeometries(g) from gis_multi_point ORDER by fid;
SELECT fid, NumGeometries(g) from gis_multi_line ORDER by fid;
SELECT fid, NumGeometries(g) from gis_multi_polygon ORDER by fid;
SELECT fid, NumGeometries(g) from gis_geometrycollection ORDER by fid;
explain extended SELECT fid, NumGeometries(g) from gis_multi_point;

SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_point ORDER by fid;
SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_line ORDER by fid;
SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_polygon ORDER by fid;
SELECT fid, AsText(GeometryN(g, 2)) from gis_geometrycollection ORDER by fid;
SELECT fid, AsText(GeometryN(g, 1)) from gis_geometrycollection ORDER by fid;
explain extended SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_point;

SELECT g1.fid as first, g2.fid as second,
Within(g1.g, g2.g) as w, Contains(g1.g, g2.g) as c, Overlaps(g1.g, g2.g) as o,
Equals(g1.g, g2.g) as e, Disjoint(g1.g, g2.g) as d, Touches(g1.g, g2.g) as t,
Intersects(g1.g, g2.g) as i, Crosses(g1.g, g2.g) as r
FROM gis_geometrycollection g1, gis_geometrycollection g2 ORDER BY first, second;
explain extended SELECT g1.fid as first, g2.fid as second,
Within(g1.g, g2.g) as w, Contains(g1.g, g2.g) as c, Overlaps(g1.g, g2.g) as o,
Equals(g1.g, g2.g) as e, Disjoint(g1.g, g2.g) as d, Touches(g1.g, g2.g) as t,
Intersects(g1.g, g2.g) as i, Crosses(g1.g, g2.g) as r
FROM gis_geometrycollection g1, gis_geometrycollection g2 ORDER BY first, second;

DROP TABLE gis_point, gis_line, gis_polygon, gis_multi_point, gis_multi_line, gis_multi_polygon, gis_geometrycollection, gis_geometry;

#
# Check that ALTER TABLE doesn't loose geometry type
#
CREATE TABLE t1 (
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

create table t1 (a geometry not null);
insert into t1 values (GeomFromText('Point(1 2)'));
-- error 1416
insert into t1 values ('Garbage');
-- error 1416
insert IGNORE into t1 values ('Garbage');

drop table t1;

create table t1 (fl geometry);
--error 1416
insert into t1 values (1);
--error 1416
insert into t1 values (1.11);
--error 1416
insert into t1 values ("qwerty");
--error 1416
insert into t1 values (pointfromtext('point(1,1)'));

drop table t1;

# End of 5.0 tests
