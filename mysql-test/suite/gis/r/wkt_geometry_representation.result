##########################################################
# Inserting testing SRS values in spatial reference table
##########################################################
# Inserting geographical srs with long-lat ordering
CREATE SPATIAL REFERENCE SYSTEM 30400000 NAME 'TEST30400000' DEFINITION 'GEOGCS["SIRGAS 1995",DATUM["Sistema de Referencia Geocentrico para America del Sur 1995",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6170"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["LONG",EAST],AXIS["LAT",NORTH],AUTHORITY["EPSG","30400000"]]';
# Inserting geographical srs with lat-long ordering
CREATE SPATIAL REFERENCE SYSTEM 30500000 NAME 'TEST30500000' DEFINITION 'GEOGCS["SIRGAS 1995",DATUM["Sistema de Referencia Geocentrico para America del Sur 1995",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6170"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["LAT",NORTH],AXIS["LONG",EAST],AUTHORITY["EPSG","30500000"]]';
# Inserting projected srs with northing-easting ordering
CREATE SPATIAL REFERENCE SYSTEM 30600000 NAME 'TEST30600000' DEFINITION 'PROJCS["NAD83(NSRS2007) / Virginia Lambert",GEOGCS["NAD83(NSRS2007)",DATUM["NAD83 (National Spatial Reference System 2007)",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6759"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4759"]],PROJECTION["Lambert Conic Conformal (2SP)",AUTHORITY["EPSG","9802"]],PARAMETER["Latitude of false origin",36,AUTHORITY["EPSG","8821"]],PARAMETER["Longitude of false origin",-79.5,AUTHORITY["EPSG","8822"]],PARAMETER["Latitude of 1st standard parallel",37,AUTHORITY["EPSG","8823"]],PARAMETER["Latitude of 2nd standard parallel",39.5,AUTHORITY["EPSG","8824"]],PARAMETER["Easting at false origin",0,AUTHORITY["EPSG","8826"]],PARAMETER["Northing at false origin",0,AUTHORITY["EPSG","8827"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",NORTH],AXIS["Y",EAST],AUTHORITY["EPSG","30600000"]]';
# Inserting projected srs with easting-northing ordering
CREATE SPATIAL REFERENCE SYSTEM 30700000 NAME 'TEST30700000' DEFINITION 'PROJCS["NAD83(NSRS2007) / Virginia Lambert",GEOGCS["NAD83(NSRS2007)",DATUM["NAD83 (National Spatial Reference System 2007)",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6759"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4759"]],PROJECTION["Lambert Conic Conformal (2SP)",AUTHORITY["EPSG","9802"]],PARAMETER["Latitude of false origin",36,AUTHORITY["EPSG","8821"]],PARAMETER["Longitude of false origin",-79.5,AUTHORITY["EPSG","8822"]],PARAMETER["Latitude of 1st standard parallel",37,AUTHORITY["EPSG","8823"]],PARAMETER["Latitude of 2nd standard parallel",39.5,AUTHORITY["EPSG","8824"]],PARAMETER["Easting at false origin",0,AUTHORITY["EPSG","8826"]],PARAMETER["Northing at false origin",0,AUTHORITY["EPSG","8827"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",EAST],AXIS["Y",NORTH],AUTHORITY["EPSG","30700000"]]';
# Creating the spatial objects
CREATE TABLE gis_point (fid INTEGER NOT NULL PRIMARY KEY, g POINT);
CREATE TABLE gis_linestring (fid INTEGER NOT NULL PRIMARY KEY, g LINESTRING);
CREATE TABLE gis_polygon (fid INTEGER NOT NULL PRIMARY KEY, g POLYGON);
CREATE TABLE gis_multi_point (fid INTEGER NOT NULL PRIMARY KEY, g MULTIPOINT);
CREATE TABLE gis_multi_linestring (fid INTEGER NOT NULL PRIMARY KEY, g MULTILINESTRING);
CREATE TABLE gis_multi_polygon (fid INTEGER NOT NULL PRIMARY KEY, g MULTIPOLYGON);
CREATE TABLE gis_geometrycollection (fid INTEGER NOT NULL PRIMARY KEY, g GEOMETRYCOLLECTION);
# Inserting POINT Geometry Values with different SRID types
INSERT INTO gis_point VALUES
(101, ST_SRID(ST_POINTFROMTEXT('POINT(5 10)',0), 30400000)),
(102, ST_SRID(ST_POINTFROMTEXT('POINT(5 10)',0), 30500000)),
(103, ST_SRID(ST_POINTFROMTEXT('POINT(5 10)',0), 30600000)),
(104, ST_SRID(ST_POINTFROMTEXT('POINT(5 10)',0), 30700000)),
(105, ST_SRID(ST_POINTFROMTEXT('POINT(5 10)',0), 0));
# Inserting LINESTRING Geometry Values with different SRID types
INSERT INTO gis_linestring VALUES
(201, ST_SRID(ST_LINEFROMTEXT('LINESTRING(0 5,5 10,10 15)',0), 30400000)),
(202, ST_SRID(ST_LINEFROMTEXT('LINESTRING(0 5,5 10,10 15)',0), 30500000)),
(203, ST_SRID(ST_LINEFROMTEXT('LINESTRING(0 5,5 10,10 15)',0), 30600000)),
(204, ST_SRID(ST_LINEFROMTEXT('LINESTRING(0 5,5 10,10 15)',0), 30700000)),
(205, ST_SRID(ST_LINEFROMTEXT('LINESTRING(0 5,5 10,10 15)',0), 0));
# Inserting POLYGON Geometry Values with different SRID types
INSERT INTO gis_polygon VALUES
(301, ST_SRID(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,0 5,0 0))',0), 30400000)),
(302, ST_SRID(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,0 5,0 0))',0), 30500000)),
(303, ST_SRID(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,0 5,0 0))',0), 30600000)),
(304, ST_SRID(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,0 5,0 0))',0), 30700000)),
(305, ST_SRID(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,0 5,0 0))',0), 0));
# Inserting MULTIPOINT Geometry Values with different SRID types
INSERT INTO gis_multi_point VALUES
(401, ST_SRID(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (8 2), (4 9))',0), 30400000)),
(402, ST_SRID(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (8 2), (4 9))',0), 30500000)),
(403, ST_SRID(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (8 2), (4 9))',0), 30600000)),
(404, ST_SRID(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (8 2), (4 9))',0), 30700000)),
(405, ST_SRID(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (8 2), (4 9))',0), 0));
# Inserting MULTILINESTRING Geometry Values with different SRID types
INSERT INTO gis_multi_linestring VALUES
(501, ST_SRID(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))',0), 30400000)),
(502, ST_SRID(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))',0), 30500000)),
(503, ST_SRID(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))',0), 30600000)),
(504, ST_SRID(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))',0), 30700000)),
(505, ST_SRID(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))',0), 0));
# Inserting MULTIPOLYGON Geometry Values with different SRID types
INSERT INTO gis_multi_polygon VALUES
(601, ST_SRID(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0), 30400000)),
(602, ST_SRID(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0), 30500000)),
(603, ST_SRID(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0), 30600000)),
(604, ST_SRID(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0), 30700000)),
(605, ST_SRID(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0), 0));
# Inserting GEOMETRYCOLLECTION Geometry Values with different SRID types
INSERT INTO gis_geometrycollection VALUES
(701, ST_SRID(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(3 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0), 30400000)),
(702, ST_SRID(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(3 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0), 30500000)),
(703, ST_SRID(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(3 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0), 30600000)),
(704, ST_SRID(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(3 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0), 30700000)),
(705, ST_SRID(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(3 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0), 0));
#####################################################################
# Retrieving geometries with ST_ASTEXT with no parameter
# (Should thus be retrieved with the SRID default axis ordering)
#####################################################################
# Retrieve all values from gis_point
SELECT ST_ASTEXT(g) FROM gis_point
ORDER BY fid;
ST_ASTEXT(g)
POINT(5 10)
POINT(10 5)
POINT(5 10)
POINT(5 10)
POINT(5 10)
# Retrieve all values from gis_linestring
SELECT ST_ASTEXT(g) FROM gis_linestring
ORDER BY fid;
ST_ASTEXT(g)
LINESTRING(0 5,5 10,10 15)
LINESTRING(5 0,10 5,15 10)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
# Retrieve all values from gis_polygon
SELECT ST_ASTEXT(g) FROM gis_polygon
ORDER BY fid;
ST_ASTEXT(g)
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,0 5,5 5,5 0,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
# Retrieve all values from gis_multi_point
SELECT ST_ASTEXT(g) FROM gis_multi_point
ORDER BY fid;
ST_ASTEXT(g)
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((2 0),(7 5),(2 8),(9 4))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
# Retrieve all values from gis_multi_linestring
SELECT ST_ASTEXT(g) FROM gis_multi_linestring
ORDER BY fid;
ST_ASTEXT(g)
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((2 0,4 2,6 4),(11 8,14 11,17 14))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
# Retrieve all values from gis_multi_polygon
SELECT ST_ASTEXT(g) FROM gis_multi_polygon
ORDER BY fid;
ST_ASTEXT(g)
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
# Retrieve all values from gis_geometrycollection
SELECT ST_ASTEXT(g) FROM gis_geometrycollection
ORDER BY fid;
ST_ASTEXT(g)
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
#####################################################################
# Retrieving geometries with ST_ASTEXT with lat-long axis ordering
# (Default SRID axis order should thus be overridden)
#####################################################################
# Retrieve all values from gis_point
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_point
ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
POINT(10 5)
POINT(10 5)
POINT(5 10)
POINT(5 10)
POINT(5 10)
# Retrieve all values from gis_linestring
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_linestring ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
LINESTRING(5 0,10 5,15 10)
LINESTRING(5 0,10 5,15 10)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
# Retrieve all values from gis_polygon
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_polygon
ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
POLYGON((0 0,0 5,5 5,5 0,0 0))
POLYGON((0 0,0 5,5 5,5 0,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
# Retrieve all values from gis_multi_point
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_multi_point ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
MULTIPOINT((2 0),(7 5),(2 8),(9 4))
MULTIPOINT((2 0),(7 5),(2 8),(9 4))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
# Retrieve all values from gis_multi_linestring
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_multi_linestring ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
MULTILINESTRING((2 0,4 2,6 4),(11 8,14 11,17 14))
MULTILINESTRING((2 0,4 2,6 4),(11 8,14 11,17 14))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
# Retrieve all values from gis_multi_polygon
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_multi_polygon ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0)))
MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
# Retrieve all values from gis_geometrycollection
SELECT ST_ASTEXT(g, 'axis-order=lat-long') FROM gis_geometrycollection ORDER BY fid;
ST_ASTEXT(g, 'axis-order=lat-long')
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
#####################################################################
# Retrieving geometries with ST_ASTEXT with long-lat axis ordering
# (Default SRID axis order should thus be overridden)
#####################################################################
# Retrieve all values from gis_point
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_point
ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
POINT(5 10)
POINT(5 10)
POINT(5 10)
POINT(5 10)
POINT(5 10)
# Retrieve all values from gis_linestring
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_linestring ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
# Retrieve all values from gis_polygon
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_polygon
ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
# Retrieve all values from gis_multi_point
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_multi_point ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
# Retrieve all values from gis_multi_linestring
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_multi_linestring ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
# Retrieve all values from gis_multi_polygon
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_multi_polygon ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
# Retrieve all values from gis_geometrycollection
SELECT ST_ASTEXT(g, 'axis-order=long-lat') FROM gis_geometrycollection ORDER BY fid;
ST_ASTEXT(g, 'axis-order=long-lat')
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
#####################################################################
# Retrieving geometries with ST_ASTEXT with srid-default axis ordering
# (Default SRID coordination should thus not be overridden)
#####################################################################
# Retrieve all values from gis_point
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_point
ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
POINT(5 10)
POINT(10 5)
POINT(5 10)
POINT(5 10)
POINT(5 10)
# Retrieve all values from gis_linestring
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_linestring ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
LINESTRING(0 5,5 10,10 15)
LINESTRING(5 0,10 5,15 10)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
# Retrieve all values from gis_polygon
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_polygon ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,0 5,5 5,5 0,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
# Retrieve all values from gis_multi_point
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_multi_point ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((2 0),(7 5),(2 8),(9 4))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
# Retrieve all values from gis_multi_linestring
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_multi_linestring ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((2 0,4 2,6 4),(11 8,14 11,17 14))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
# Retrieve all values from gis_multi_polygon
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_multi_polygon ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
# Retrieve all values from gis_geometrycollection
SELECT ST_ASTEXT(g, 'axis-order=srid-defined') FROM gis_geometrycollection ORDER BY fid;
ST_ASTEXT(g, 'axis-order=srid-defined')
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
##########################################################################
# Retrieving geometries with ST_ASTEXT with empty axis ordering parameter
# (Default SRID coordination should thus be used)
##########################################################################
# Retrieve all values from gis_point
SELECT ST_ASTEXT(g, '') FROM gis_point
ORDER BY fid;
ST_ASTEXT(g, '')
POINT(5 10)
POINT(10 5)
POINT(5 10)
POINT(5 10)
POINT(5 10)
# Retrieve all values from gis_linestring
SELECT ST_ASTEXT(g, '') FROM gis_linestring
ORDER BY fid;
ST_ASTEXT(g, '')
LINESTRING(0 5,5 10,10 15)
LINESTRING(5 0,10 5,15 10)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
LINESTRING(0 5,5 10,10 15)
# Retrieve all values from gis_polygon
SELECT ST_ASTEXT(g, '') FROM gis_polygon
ORDER BY fid;
ST_ASTEXT(g, '')
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,0 5,5 5,5 0,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
POLYGON((0 0,5 0,5 5,0 5,0 0))
# Retrieve all values from gis_multi_point
SELECT ST_ASTEXT(g, '') FROM gis_multi_point
ORDER BY fid;
ST_ASTEXT(g, '')
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((2 0),(7 5),(2 8),(9 4))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
MULTIPOINT((0 2),(5 7),(8 2),(4 9))
# Retrieve all values from gis_multi_linestring
SELECT ST_ASTEXT(g, '') FROM gis_multi_linestring
ORDER BY fid;
ST_ASTEXT(g, '')
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((2 0,4 2,6 4),(11 8,14 11,17 14))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
MULTILINESTRING((0 2,2 4,4 6),(8 11,11 14,14 17))
# Retrieve all values from gis_multi_polygon
SELECT ST_ASTEXT(g, '') FROM gis_multi_polygon
ORDER BY fid;
ST_ASTEXT(g, '')
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
# Retrieve all values from gis_geometrycollection
SELECT ST_ASTEXT(g, '') FROM gis_geometrycollection
ORDER BY fid;
ST_ASTEXT(g, '')
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
################################
# Specific cases testing
# ##############################
# Test with combined lowercase and uppercase characters in optional parameter
SELECT ST_ASTEXT(g, 'aXIs-oRDer=sriD-dEFineD') FROM gis_geometrycollection ORDER BY fid;
ST_ASTEXT(g, 'aXIs-oRDer=sriD-dEFineD')
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
# Test with combined lowercase, uppercase and whitespaces in optional parameter
SELECT ST_ASTEXT(g, '   aXIs-oRDer=sriD-dEFineD    ') FROM gis_geometrycollection ORDER BY fid;
ST_ASTEXT(g, '   aXIs-oRDer=sriD-dEFineD    ')
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
# Test with only whitespaces in optional parameter (Should return same as empty string)
SELECT ST_ASTEXT(g, '       ') FROM gis_geometrycollection
ORDER BY fid;
ST_ASTEXT(g, '       ')
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(5 3),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,6 4,6 6,4 6,4 4)),((0 0,-2 -2,-2 0,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
GEOMETRYCOLLECTION(POINT(3 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
##########################
# Error testing
# ########################
# Test with badly formed options string
SELECT ST_ASTEXT(g, 'axis-order srid-defined') FROM gis_geometrycollection;
ERROR 22023: The string 'axis-order srid-defined' is not a valid key = value pair in function st_astext.
SELECT ST_ASTEXT(g, '=axis-order srid-defined') FROM gis_geometrycollection;
ERROR 22023: The options argument in function st_astext starts with the invalid character '='.
SELECT ST_ASWKT(g, 'axis-order=srid-defined, axis-order=lat-long') FROM gis_geometrycollection;
ERROR 22023: Duplicate option key 'axis-order' in funtion 'st_astext'.
# Test with too many options
SELECT ST_ASTEXT(g, 'axis-order=srid-defined, axis-order2=lat-long') FROM gis_geometrycollection;
ERROR 22023: Invalid option key 'axis-order2' in function st_astext.
# Test with invalid options value
SELECT ST_ASTEXT(g, 'axis-order=invalid-option') FROM gis_geometrycollection;
ERROR 22023: Invalid value 'invalid-option' for option 'axis-order' in function 'st_astext'.
# Test with too many function parameters
SELECT ST_ASTEXT(g, 'axis-order=long-lat', POINT(2, 4)) FROM gis_geometrycollection;
ERROR 42000: Incorrect parameter count in the call to native function 'ST_ASTEXT'
# Test with too invalid start and end characters in substring
SELECT ST_ASTEXT(POINT(4, 2), 'axis-order=test,=long-lathello,=test');
ERROR 22023: The options argument in function st_astext starts with the invalid character '='.
SELECT ST_ASTEXT(POINT(4, 2), 'axis-order=long-lat=, axis-order=lat-long');
ERROR 22023: The options argument in function st_astext ends with the invalid character '='.
# Test with both invalid option and invalid value
SELECT ST_ASTEXT(g, 'invalid_option=invalid-value') FROM gis_geometrycollection;
ERROR 22023: Invalid option key 'invalid_option' in function st_astext.
SELECT ST_ASWKT(g, 'axis-order=invalid-option') FROM gis_geometrycollection;
ERROR 22023: Invalid value 'invalid-option' for option 'axis-order' in function 'st_astext'.
# Test with SRID value 0
SELECT ST_ASWKT(ST_POINTFROMTEXT('POINT(4 2)', 0));
ST_ASWKT(ST_POINTFROMTEXT('POINT(4 2)', 0))
POINT(4 2)
# NULL value testing: Should return NULL if either or both parameters of
# ST_ASTEXT/ST_WKT is NULL
SELECT ST_ASTEXT(NULL, 'axis-order=srid-defined');
ST_ASTEXT(NULL, 'axis-order=srid-defined')
NULL
SELECT ST_ASTEXT(POINT(0, 0), NULL);
ST_ASTEXT(POINT(0, 0), NULL)
NULL
SELECT ST_ASTEXT(NULL, NULL);
ST_ASTEXT(NULL, NULL)
NULL
DROP TABLE gis_point;
DROP TABLE gis_linestring;
DROP TABLE gis_polygon;
DROP TABLE gis_multi_point;
DROP TABLE gis_multi_linestring;
DROP TABLE gis_multi_polygon;
DROP TABLE gis_geometrycollection;
DROP SPATIAL REFERENCE SYSTEM 30400000;
DROP SPATIAL REFERENCE SYSTEM 30500000;
DROP SPATIAL REFERENCE SYSTEM 30600000;
DROP SPATIAL REFERENCE SYSTEM 30700000;
