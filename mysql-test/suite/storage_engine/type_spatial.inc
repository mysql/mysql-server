#
# Spatial objects
#

#
# A calling test can initialize $index variable, 
# which can be SPATIAL, or INDEX, or UNIQUE INDEX;
# Logic for creating tables with spatial and non-spatial indexes is different
#

--disable_warnings
DROP TABLE IF EXISTS t1, gis_point, gis_line, gis_polygon, gis_multi_point, gis_multi_line, gis_multi_polygon, gis_geometrycollection, gis_geometry;
--enable_warnings

--disable_warnings
DROP DATABASE IF EXISTS gis_ogs;
--enable_warnings

CREATE DATABASE gis_ogs;


if ($index=='')
{
	--let $table_name = gis_point
	--let $create_definition = fid $int_col, g POINT
	--source create_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $create_statement
    --let $functionality = Geometry types
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
	  --let $table_name = gis_line
	  --let $create_definition = fid $int_col, g LINESTRING
	  --source create_table.inc
	  --let $table_name = gis_polygon
	  --let $create_definition = fid $int_col, g POLYGON
	  --source create_table.inc
	  --let $table_name = gis_multi_point
	  --let $create_definition = fid $int_col, g MULTIPOINT
	  --source create_table.inc
	  --let $table_name = gis_multi_line
	  --let $create_definition = fid $int_col, g MULTILINESTRING
	  --source create_table.inc
	  --let $table_name = gis_multi_polygon
	  --let $create_definition = fid $int_col, g MULTIPOLYGON
	  --source create_table.inc
	  --let $table_name = gis_geometrycollection
	  --let $create_definition = fid $int_col, g GEOMETRYCOLLECTION
	  --source create_table.inc
	  --let $table_name = gis_geometry
	  --let $create_definition = fid $int_col, g GEOMETRY
	  --source create_table.inc

	  USE gis_ogs;

	  --let $table_name = lakes
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    shore POLYGON;
	  --source create_table.inc

	  --let $table_name = road_segments
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    aliases CHAR(64) $col_opts, 
	    num_lanes INT $col_opts, 
	    centerline LINESTRING;
	  --source create_table.inc

	  --let $table_name = divided_routes
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    num_lanes INT $col_opts, 
	    centerlines MULTILINESTRING;
	  --source create_table.inc

	  --let $table_name = forests
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    boundary MULTIPOLYGON; 
	  --source create_table.inc

	  --let $table_name = bridges
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    position POINT; 
	  --source create_table.inc

	  --let $table_name = streams
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    centerline LINESTRING; 
	  --source create_table.inc

	  --let $table_name = buildings
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    position POINT,
	    footprint POLYGON; 
	  --source create_table.inc

	  --let $table_name = ponds
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    type CHAR(64) $col_opts, 
	    shores MULTIPOLYGON; 
	  --source create_table.inc

	  --let $table_name = named_places
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    boundary POLYGON; 
	  --source create_table.inc

	  --let $table_name = map_neatlines
	  let $create_definition =
	    fid INT $col_opts,
	    neatline POLYGON; 
	  --source create_table.inc
  }
}

if ($index == 'SPATIAL')
{
	--let $table_name = gis_point
	--let $create_definition = fid $int_col, g POINT NOT NULL, SPATIAL INDEX(g)
	--source create_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $create_statement
    --let $functionality = Geometry types or spatial indexes
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
	  --let $table_name = gis_line
	  --let $create_definition = fid $int_col, g LINESTRING NOT NULL, SPATIAL INDEX(g)
	  --source create_table.inc
	  --let $table_name = gis_polygon
	  --let $create_definition = fid $int_col, g POLYGON NOT NULL, SPATIAL INDEX(g)
	  --source create_table.inc
	  --let $table_name = gis_multi_point
	  --let $create_definition = fid $int_col, g MULTIPOINT NOT NULL, SPATIAL INDEX(g)
	  --source create_table.inc
	  --let $table_name = gis_multi_line
	  --let $create_definition = fid $int_col, g MULTILINESTRING NOT NULL, SPATIAL INDEX(g)
	  --source create_table.inc
	  --let $table_name = gis_multi_polygon
	  --let $create_definition = fid $int_col, g MULTIPOLYGON NOT NULL, SPATIAL INDEX(g)
	  --source create_table.inc
	  --let $table_name = gis_geometrycollection
	  --let $create_definition = fid $int_col, g GEOMETRYCOLLECTION NOT NULL, SPATIAL INDEX(g)
	  --source create_table.inc
	  --let $table_name = gis_geometry
	  --let $create_definition = fid $int_col, g GEOMETRY NOT NULL
	  --source create_table.inc

     USE gis_ogs;

	  --let $table_name = lakes
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    shore POLYGON NOT NULL, SPATIAL INDEX s(shore); 
	  --source create_table.inc

	  --let $table_name = road_segments
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    aliases CHAR(64) $col_opts, 
	    num_lanes INT $col_opts, 
	    centerline LINESTRING NOT NULL, SPATIAL INDEX c(centerline);
	  --source create_table.inc

	  --let $table_name = divided_routes
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    num_lanes INT $col_opts, 
	    centerlines MULTILINESTRING NOT NULL, SPATIAL INDEX c(centerlines); 
	  --source create_table.inc

	  --let $table_name = forests
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    boundary MULTIPOLYGON NOT NULL, SPATIAL INDEX b(boundary); 
	  --source create_table.inc

	  --let $table_name = bridges
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    position POINT NOT NULL, SPATIAL INDEX p(position); 
	  --source create_table.inc

	  --let $table_name = streams
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    centerline LINESTRING NOT NULL, SPATIAL INDEX c(centerline); 
	  --source create_table.inc

	  --let $table_name = buildings
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    position POINT NOT NULL,
	    footprint POLYGON NOT NULL, SPATIAL INDEX p(position), SPATIAL INDEX f(footprint); 
	  --source create_table.inc

	  --let $table_name = ponds
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    type CHAR(64) $col_opts, 
	    shores MULTIPOLYGON NOT NULL, SPATIAL INDEX s(shores); 
	  --source create_table.inc

	  --let $table_name = named_places
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    boundary POLYGON NOT NULL, SPATIAL INDEX b(boundary); 
	  --source create_table.inc

	  --let $table_name = map_neatlines
	  let $create_definition =
	    fid INT $col_opts,
	    neatline POLYGON NOT NULL, SPATIAL INDEX n(neatline); 
	  --source create_table.inc
  }
 	let $index =;
}

if ($index)
{
	--let $table_name = gis_point
	--let $create_definition = fid $int_col, g POINT, $index g(g(128))
	--source create_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $create_statement
    --let $functionality = Geometry types or indexes on them
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
	  --let $table_name = gis_line
	  --let $create_definition = fid $int_col, g LINESTRING, $index g(g(256))
	  --source create_table.inc
	  --let $table_name = gis_polygon
	  --let $create_definition = fid $int_col, g POLYGON, $index g(g(512))
	  --source create_table.inc
	  --let $table_name = gis_multi_point
	  --let $create_definition = fid $int_col, g MULTIPOINT, $index g(g(128))
	  --source create_table.inc
	  --let $table_name = gis_multi_line
	  --let $create_definition = fid $int_col, g MULTILINESTRING, $index g(g(256))
	  --source create_table.inc
	  --let $table_name = gis_multi_polygon
	  --let $create_definition = fid $int_col, g MULTIPOLYGON
	  --source create_table.inc
	  --let $table_name = gis_geometrycollection
	  --let $create_definition = fid $int_col, g GEOMETRYCOLLECTION
	  --source create_table.inc
	  --let $table_name = gis_geometry
	  --let $create_definition = fid $int_col, g GEOMETRY
	  --source create_table.inc

     USE gis_ogs;

	  --let $table_name = lakes
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    shore POLYGON $spatial_col_opts, $index s(shore(64)); 
	  --source create_table.inc

	  --let $table_name = road_segments
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    aliases CHAR(64) $col_opts, 
	    num_lanes INT $col_opts, 
	    centerline LINESTRING $spatial_col_opts, $index c(centerline(128));
	  --source create_table.inc

	  --let $table_name = divided_routes
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    num_lanes INT $col_opts, 
	    centerlines MULTILINESTRING $spatial_col_opts, $index c(centerlines(512)); 
	  --source create_table.inc

	  --let $table_name = forests
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    boundary MULTIPOLYGON $spatial_col_opts, $index b(boundary(128)); 
	  --source create_table.inc

	  --let $table_name = bridges
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    position POINT $spatial_col_opts, $index p(`position`(64)); 
	  --source create_table.inc

	  --let $table_name = streams
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    centerline LINESTRING $spatial_col_opts, $index c(centerline(256)); 
	  --source create_table.inc

	  --let $table_name = buildings
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    position POINT $spatial_col_opts,
	    footprint POLYGON $spatial_col_opts, $index p(`position`(64)), $index f(footprint(128)); 
	  --source create_table.inc

	  --let $table_name = ponds
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    type CHAR(64) $col_opts, 
	    shores MULTIPOLYGON $spatial_col_opts, $index s(shores(256)); 
	  --source create_table.inc

	  --let $table_name = named_places
	  let $create_definition =
	    fid INT $col_opts,
	    name CHAR(64) $col_opts,
	    boundary POLYGON $spatial_col_opts, $index b(boundary(512)); 
	  --source create_table.inc

	  --let $table_name = map_neatlines
	  let $create_definition =
	    fid INT $col_opts,
	    neatline POLYGON $spatial_col_opts, $index n(neatline(700)); 
	  --source create_table.inc
  }
}

if (!$mysql_errname)
{
  USE test;

  SHOW FIELDS FROM gis_point;
  SHOW FIELDS FROM gis_line;
  SHOW FIELDS FROM gis_polygon;
  SHOW FIELDS FROM gis_multi_point;
  SHOW FIELDS FROM gis_multi_line;
  SHOW FIELDS FROM gis_multi_polygon;
  SHOW FIELDS FROM gis_geometrycollection;
  SHOW FIELDS FROM gis_geometry;

  INSERT INTO gis_point (fid,g) VALUES 
  (101, PointFromText('POINT(10 10)')),
  (102, PointFromText('POINT(20 10)')),
  (103, PointFromText('POINT(20 20)')),
  (104, PointFromWKB(AsWKB(PointFromText('POINT(10 20)'))));

  INSERT INTO gis_line (fid,g) VALUES
  (105, LineFromText('LINESTRING(0 0,0 10,10 0)')),
  (106, LineStringFromText('LINESTRING(10 10,20 10,20 20,10 20,10 10)')),
  (107, LineStringFromWKB(AsWKB(LineString(Point(10, 10), Point(40, 10)))));

  INSERT INTO gis_polygon (fid,g) VALUES
  (108, PolygonFromText('POLYGON((10 10,20 10,20 20,10 20,10 10))')),
  (109, PolyFromText('POLYGON((0 0,50 0,50 50,0 50,0 0), (10 10,20 10,20 20,10 20,10 10))')),
  (110, PolyFromWKB(AsWKB(Polygon(LineString(Point(0, 0), Point(30, 0), Point(30, 30), Point(0, 0))))));

  INSERT INTO gis_multi_point (fid,g) VALUES
  (111, MultiPointFromText('MULTIPOINT(0 0,10 10,10 20,20 20)')),
  (112, MPointFromText('MULTIPOINT(1 1,11 11,11 21,21 21)')),
  (113, MPointFromWKB(AsWKB(MultiPoint(Point(3, 6), Point(4, 10)))));

  INSERT INTO gis_multi_line (fid,g) VALUES
  (114, MultiLineStringFromText('MULTILINESTRING((10 48,10 21,10 0),(16 0,16 23,16 48))')),
  (115, MLineFromText('MULTILINESTRING((10 48,10 21,10 0))')),
  (116, MLineFromWKB(AsWKB(MultiLineString(LineString(Point(1, 2), Point(3, 5)), LineString(Point(2, 5), Point(5, 8), Point(21, 7))))));

  INSERT INTO gis_multi_polygon (fid,g) VALUES
  (117, MultiPolygonFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))')),
  (118, MPolyFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))')),
  (119, MPolyFromWKB(AsWKB(MultiPolygon(Polygon(LineString(Point(0, 3), Point(3, 3), Point(3, 0), Point(0, 3)))))));

  INSERT INTO gis_geometrycollection (fid,g) VALUES
  (120, GeomCollFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))')),
  (121, GeometryFromWKB(AsWKB(GeometryCollection(Point(44, 6), LineString(Point(3, 6), Point(7, 9)))))),
  (122, GeomFromText('GeometryCollection()')),
  (123, GeomFromText('GeometryCollection EMPTY'));

  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_point;
  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_line;
  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_polygon;
  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_multi_point;
  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_multi_line;
  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_multi_polygon;
  INSERT into gis_geometry (fid,g) SELECT fid,g FROM gis_geometrycollection;

  SELECT fid, AsText(g) FROM gis_point;
  SELECT fid, AsText(g) FROM gis_line;
  SELECT fid, AsText(g) FROM gis_polygon;
  SELECT fid, AsText(g) FROM gis_multi_point;
  SELECT fid, AsText(g) FROM gis_multi_line;
  SELECT fid, AsText(g) FROM gis_multi_polygon;
  SELECT fid, AsText(g) FROM gis_geometrycollection;
  SELECT fid, AsText(g) FROM gis_geometry;

  SELECT fid, Dimension(g) FROM gis_geometry;
  SELECT fid, GeometryType(g) FROM gis_geometry;
  SELECT fid, IsEmpty(g) FROM gis_geometry;
  SELECT fid, AsText(Envelope(g)) FROM gis_geometry;

  SELECT fid, X(g) FROM gis_point;
  SELECT fid, Y(g) FROM gis_point;

  SELECT fid, AsText(StartPoint(g)) FROM gis_line;
  SELECT fid, AsText(EndPoint(g)) FROM gis_line;
  SELECT fid, GLength(g) FROM gis_line;
  SELECT fid, NumPoints(g) FROM gis_line;
  SELECT fid, AsText(PointN(g, 2)) FROM gis_line;
  SELECT fid, IsClosed(g) FROM gis_line;

  SELECT fid, AsText(Centroid(g)) FROM gis_polygon;
  SELECT fid, Area(g) FROM gis_polygon;
  SELECT fid, AsText(ExteriorRing(g)) FROM gis_polygon;
  SELECT fid, NumInteriorRings(g) FROM gis_polygon;
  SELECT fid, AsText(InteriorRingN(g, 1)) FROM gis_polygon;

  SELECT fid, IsClosed(g) FROM gis_multi_line;

  SELECT fid, AsText(Centroid(g)) FROM gis_multi_polygon;
  SELECT fid, Area(g) FROM gis_multi_polygon;

  SELECT fid, NumGeometries(g) from gis_multi_point;
  SELECT fid, NumGeometries(g) from gis_multi_line;
  SELECT fid, NumGeometries(g) from gis_multi_polygon;
  SELECT fid, NumGeometries(g) from gis_geometrycollection;

  SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_point;
  SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_line;
  SELECT fid, AsText(GeometryN(g, 2)) from gis_multi_polygon;
  SELECT fid, AsText(GeometryN(g, 2)) from gis_geometrycollection;
  SELECT fid, AsText(GeometryN(g, 1)) from gis_geometrycollection;

  SELECT g1.fid as first, g2.fid as second,
  Within(g1.g, g2.g) as w, Contains(g1.g, g2.g) as c, Overlaps(g1.g, g2.g) as o,
  Equals(g1.g, g2.g) as e, Disjoint(g1.g, g2.g) as d, Touches(g1.g, g2.g) as t,
  Intersects(g1.g, g2.g) as i, Crosses(g1.g, g2.g) as r
  FROM gis_geometrycollection g1, gis_geometrycollection g2 ORDER BY first, second;

  DROP TABLE gis_point, gis_line, gis_polygon, gis_multi_point, gis_multi_line, gis_multi_polygon, gis_geometrycollection, gis_geometry;


  USE gis_ogs;

  --echo # Lakes
  INSERT INTO lakes (fid,name,shore) VALUES ( 
  101, 'BLUE LAKE', 
  PolyFromText( 
  'POLYGON( 
  (52 18,66 23,73 9,48 6,52 18), 
  (59 18,67 18,67 13,59 13,59 18) 
  )', 
  101)); 

  --echo # Road Segments

  INSERT INTO road_segments (fid,name,aliases,num_lanes,centerline) VALUES(102, 'Route 5', NULL, 2, 
  LineFromText( 
  'LINESTRING( 0 18, 10 21, 16 23, 28 26, 44 31 )' ,101)); 

  INSERT INTO road_segments (fid,name,aliases,num_lanes,centerline) VALUES(103, 'Route 5', 'Main Street', 4, 
  LineFromText( 
  'LINESTRING( 44 31, 56 34, 70 38 )' ,101)); 

  INSERT INTO road_segments (fid,name,aliases,num_lanes,centerline) VALUES(104, 'Route 5', NULL, 2, 
  LineFromText( 
  'LINESTRING( 70 38, 72 48 )' ,101)); 

  INSERT INTO road_segments (fid,name,aliases,num_lanes,centerline) VALUES(105, 'Main Street', NULL, 4, 
  LineFromText( 
  'LINESTRING( 70 38, 84 42 )' ,101)); 

  INSERT INTO road_segments (fid,name,aliases,num_lanes,centerline) VALUES(106, 'Dirt Road by Green Forest', NULL, 
  1, 
  LineFromText( 
  'LINESTRING( 28 26, 28 0 )',101)); 

  --echo # DividedRoutes 

  INSERT INTO divided_routes (fid,name,num_lanes,centerlines) VALUES(119, 'Route 75', 4, 
  MLineFromText( 
  'MULTILINESTRING((10 48,10 21,10 0), 
  (16 0,16 23,16 48))', 101)); 

  --echo # Forests 

  INSERT INTO forests (fid,name,boundary) VALUES(109, 'Green Forest', 
  MPolyFromText( 
  'MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26), 
  (52 18,66 23,73 9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))', 
  101)); 

  --echo # Bridges 

  INSERT INTO bridges (fid,name,position) VALUES(110, 'Cam Bridge', PointFromText( 
  'POINT( 44 31 )', 101)); 

  --echo # Streams 

  INSERT INTO streams (fid,name,centerline) VALUES(111, 'Cam Stream', 
  LineFromText( 
  'LINESTRING( 38 48, 44 41, 41 36, 44 31, 52 18 )', 101)); 

  INSERT INTO streams (fid,name,centerline) VALUES(112, NULL, 
  LineFromText( 
  'LINESTRING( 76 0, 78 4, 73 9 )', 101)); 

  --echo # Buildings 

  INSERT INTO buildings (fid,name,position,footprint) VALUES(113, '123 Main Street', 
  PointFromText( 
  'POINT( 52 30 )', 101), 
  PolyFromText( 
  'POLYGON( ( 50 31, 54 31, 54 29, 50 29, 50 31) )', 101)); 

  INSERT INTO buildings (fid,name,position,footprint) VALUES(114, '215 Main Street', 
  PointFromText( 
  'POINT( 64 33 )', 101), 
  PolyFromText( 
  'POLYGON( ( 66 34, 62 34, 62 32, 66 32, 66 34) )', 101)); 


  --echo # Ponds 

  INSERT INTO ponds (fid,name,type,shores) VALUES(120, NULL, 'Stock Pond', 
  MPolyFromText( 
  'MULTIPOLYGON( ( ( 24 44, 22 42, 24 40, 24 44) ), 
  ( ( 26 44, 26 40, 28 42, 26 44) ) )', 101)); 

  --echo # Named Places 

  INSERT INTO named_places (fid,name,boundary) VALUES(117, 'Ashton', 
  PolyFromText( 
  'POLYGON( ( 62 48, 84 48, 84 30, 56 30, 56 34, 62 48) )', 101)); 

  INSERT INTO named_places (fid,name,boundary) VALUES(118, 'Goose Island', 
  PolyFromText( 
  'POLYGON( ( 67 13, 67 18, 59 18, 59 13, 67 13) )', 101)); 

  --echo # Map Neatlines 

  INSERT INTO map_neatlines (fid,neatline) VALUES(115, 
  PolyFromText( 
  'POLYGON( ( 0 0, 0 48, 84 48, 84 0, 0 0 ) )', 101)); 

  SELECT Dimension(shore) 
  FROM lakes 
  WHERE name = 'Blue Lake'; 

  SELECT GeometryType(centerlines) 
  FROM divided_routes
  WHERE name = 'Route 75'; 

  SELECT AsText(boundary) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT AsText(PolyFromWKB(AsBinary(boundary),101)) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT SRID(boundary) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT IsEmpty(centerline) 
  FROM road_segments 
  WHERE name = 'Route 5' 
  AND aliases = 'Main Street'; 

  SELECT AsText(Envelope(boundary)) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT X(position) 
  FROM bridges 
  WHERE name = 'Cam Bridge'; 

  SELECT Y(position) 
  FROM bridges 
  WHERE name = 'Cam Bridge'; 

  SELECT AsText(StartPoint(centerline)) 
  FROM road_segments 
  WHERE fid = 102; 

  SELECT AsText(EndPoint(centerline)) 
  FROM road_segments 
  WHERE fid = 102; 

  SELECT GLength(centerline) 
  FROM road_segments 
  WHERE fid = 106; 

  SELECT NumPoints(centerline) 
  FROM road_segments 
  WHERE fid = 102; 

  SELECT AsText(PointN(centerline, 1)) 
  FROM road_segments 
  WHERE fid = 102; 

  SELECT AsText(Centroid(boundary)) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT Area(boundary) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT AsText(ExteriorRing(shore)) 
  FROM lakes 
  WHERE name = 'Blue Lake'; 

  SELECT NumInteriorRings(shore) 
  FROM lakes 
  WHERE name = 'Blue Lake'; 

  SELECT AsText(InteriorRingN(shore, 1)) 
  FROM lakes 
  WHERE name = 'Blue Lake'; 

  SELECT NumGeometries(centerlines) 
  FROM divided_routes 
  WHERE name = 'Route 75'; 

  SELECT AsText(GeometryN(centerlines, 2)) 
  FROM divided_routes 
  WHERE name = 'Route 75'; 

  SELECT IsClosed(centerlines) 
  FROM divided_routes 
  WHERE name = 'Route 75'; 

  SELECT GLength(centerlines) 
  FROM divided_routes 
  WHERE name = 'Route 75'; 

  SELECT AsText(Centroid(shores)) 
  FROM ponds 
  WHERE fid = 120; 

  SELECT Area(shores) 
  FROM ponds 
  WHERE fid = 120; 

  SELECT ST_Equals(boundary, 
  PolyFromText('POLYGON( ( 67 13, 67 18, 59 18, 59 13, 67 13) )',1)) 
  FROM named_places 
  WHERE name = 'Goose Island'; 

  SELECT ST_Disjoint(centerlines, boundary) 
  FROM divided_routes, named_places 
  WHERE divided_routes.name = 'Route 75' 
  AND named_places.name = 'Ashton'; 

  SELECT ST_Touches(centerline, shore) 
  FROM streams, lakes 
  WHERE streams.name = 'Cam Stream' 
  AND lakes.name = 'Blue Lake'; 

  SELECT Crosses(road_segments.centerline, divided_routes.centerlines)
  FROM road_segments, divided_routes 
  WHERE road_segments.fid = 102 
  AND divided_routes.name = 'Route 75'; 

  SELECT ST_Intersects(road_segments.centerline, divided_routes.centerlines) 
  FROM road_segments, divided_routes 
  WHERE road_segments.fid = 102 
  AND divided_routes.name = 'Route 75'; 

  SELECT ST_Contains(forests.boundary, named_places.boundary) 
  FROM forests, named_places 
  WHERE forests.name = 'Green Forest' 
  AND named_places.name = 'Ashton'; 

  SELECT ST_Distance(position, boundary) 
  FROM bridges, named_places 
  WHERE bridges.name = 'Cam Bridge' 
  AND named_places.name = 'Ashton'; 

  SELECT AsText(ST_Difference(named_places.boundary, forests.boundary)) 
  FROM named_places, forests 
  WHERE named_places.name = 'Ashton' 
  AND forests.name = 'Green Forest'; 

  SELECT AsText(ST_Union(shore, boundary)) 
  FROM lakes, named_places 
  WHERE lakes.name = 'Blue Lake' 
  AND named_places.name = 'Goose Island'; 

  SELECT AsText(ST_SymDifference(shore, boundary)) 
  FROM lakes, named_places 
  WHERE lakes.name = 'Blue Lake' 
  AND named_places.name = 'Ashton'; 

  SELECT count(*) 
  FROM buildings, bridges 
  WHERE ST_Contains(ST_Buffer(bridges.position, 15.0), buildings.footprint) = 1; 

}

DROP DATABASE gis_ogs;
USE test;


