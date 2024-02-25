SET @wkb_pt = ST_AsWKB(ST_GeomFromText('POINT(0 1)'));
SET @wkb_ln = ST_AsWKB(ST_GeomFromText('LINESTRING(0 1,2 3,4 5,6 7,8 9, 10 11)'));
SET @wkb_py = ST_AsWKB(ST_GeomFromText('POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))'));
SET @wkb_mpt = ST_AsWKB(ST_GeomFromText('MULTIPOINT(0 1,2 3,4 5,6 7)'));
SET @wkb_mln = ST_AsWKB(ST_GeomFromText('MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))'));
SET @wkb_mpy = ST_AsWKB(ST_GeomFromText('MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),'
                                                     '((10 11,10 12,12 13,12 10,10 11)))'));
SET @wkb_gc = ST_AsWKB(ST_GeomFromText('GEOMETRYCOLLECTION('
                                       'POINT(0 1),'
                                       'LINESTRING(0 1,10 11),'
                                       'POLYGON((0 0,0 10,10 10,10 0, 0 0)),'
                                       'MULTIPOINT(0 1,2 3,4 5,6 7,8 9,10 11),'
                                       'MULTILINESTRING((0 1,10 11),(0 10,10 0)),'
                                       'MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))'));
#
# Functions that accept a WKT value of any geometry type.
#
# ST_GeomFromWKB()
SELECT ST_AsText(ST_GeomFromWKB(@wkb_pt));
ST_AsText(ST_GeomFromWKB(@wkb_pt))
POINT(0 1)
SELECT ST_AsText(ST_GeomFromWKB(@wkb_ln));
ST_AsText(ST_GeomFromWKB(@wkb_ln))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
SELECT ST_AsText(ST_GeomFromWKB(@wkb_py));
ST_AsText(ST_GeomFromWKB(@wkb_py))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
SELECT ST_AsText(ST_GeomFromWKB(@wkb_mpt));
ST_AsText(ST_GeomFromWKB(@wkb_mpt))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
SELECT ST_AsText(ST_GeomFromWKB(@wkb_mln));
ST_AsText(ST_GeomFromWKB(@wkb_mln))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
SELECT ST_AsText(ST_GeomFromWKB(@wkb_mpy));
ST_AsText(ST_GeomFromWKB(@wkb_mpy))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
SELECT ST_AsText(ST_GeomFromWKB(@wkb_gc));
ST_AsText(ST_GeomFromWKB(@wkb_gc))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
# ST_GeometryFromWKB()
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_pt));
ST_AsText(ST_GeometryFromWKB(@wkb_pt))
POINT(0 1)
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_ln));
ST_AsText(ST_GeometryFromWKB(@wkb_ln))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_py));
ST_AsText(ST_GeometryFromWKB(@wkb_py))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_mpt));
ST_AsText(ST_GeometryFromWKB(@wkb_mpt))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_mln));
ST_AsText(ST_GeometryFromWKB(@wkb_mln))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_mpy));
ST_AsText(ST_GeometryFromWKB(@wkb_mpy))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
SELECT ST_AsText(ST_GeometryFromWKB(@wkb_gc));
ST_AsText(ST_GeometryFromWKB(@wkb_gc))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
#
# Function that accepts a WKT value of Point geometry
#
# ST_PointFromWKB()
SELECT ST_AsText(ST_PointFromWKB(@wkb_pt));
ST_AsText(ST_PointFromWKB(@wkb_pt))
POINT(0 1)
#
# Functions that accept a WKT value of LineString geometry
#
# ST_LineFromWKB()
SELECT ST_AsText(ST_LineFromWKB(@wkb_ln));
ST_AsText(ST_LineFromWKB(@wkb_ln))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
# ST_LineStringFromWKB()
SELECT ST_AsText(ST_LineStringFromWKB(@wkb_ln));
ST_AsText(ST_LineStringFromWKB(@wkb_ln))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
#
# Functions that accept a WKT value of Polygon geometry
#
# ST_PolyFromWKB()
SELECT ST_AsText(ST_PolyFromWKB(@wkb_py));
ST_AsText(ST_PolyFromWKB(@wkb_py))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
# ST_PolygonFromWKB()
SELECT ST_AsText(ST_PolygonFromWKB(@wkb_py));
ST_AsText(ST_PolygonFromWKB(@wkb_py))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
#
# Functions that accept a WKT value of MultiPoint geometry
#
# ST_MPointFromWKB()
SELECT ST_AsText(ST_MPointFromWKB(@wkb_mpt));
ST_AsText(ST_MPointFromWKB(@wkb_mpt))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
# ST_MultiPointFromWKB()
SELECT ST_AsText(ST_MultiPointFromWKB(@wkb_mpt));
ST_AsText(ST_MultiPointFromWKB(@wkb_mpt))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
#
# Functions that accept a WKT value of MultiLineString geometry
#
# ST_MLineFromWKB()
SELECT ST_AsText(ST_MLineFromWKB(@wkb_mln));
ST_AsText(ST_MLineFromWKB(@wkb_mln))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
# ST_MultiLineStringFromWKB()
SELECT ST_AsText(ST_MultiLineStringFromWKB(@wkb_mln));
ST_AsText(ST_MultiLineStringFromWKB(@wkb_mln))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
#
# Functions that accept a WKT value of MultiPolygon geometry
#
# ST_MPolyFromWKB()
SELECT ST_AsText(ST_MPolyFromWKB(@wkb_mpy));
ST_AsText(ST_MPolyFromWKB(@wkb_mpy))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
# ST_MultiPolygonFromWKB()
SELECT ST_AsText(ST_MultiPolygonFromWKB(@wkb_mpy));
ST_AsText(ST_MultiPolygonFromWKB(@wkb_mpy))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
#
# Functions that accept a WKT value of GeometryCollection geometry
#
# ST_GeomCollFromWKB()
SELECT ST_AsText(ST_GeomCollFromWKB(@wkb_gc));
ST_AsText(ST_GeomCollFromWKB(@wkb_gc))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
# ST_GeometryCollectionFromWKB()
SELECT ST_AsText(ST_GeometryCollectionFromWKB(@wkb_gc));
ST_AsText(ST_GeometryCollectionFromWKB(@wkb_gc))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
#
# Invalid function calls
#
SELECT ST_GeomFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_GeomFromWKB'
SELECT ST_GeomFromWKB(NULL);
ST_GeomFromWKB(NULL)
NULL
SELECT ST_GeomFromWKB(ST_GeomFromText('POINT()'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_GeomFromWKB(ST_GeomFromText('LINESTRING(0 0,! 10)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_GeometryFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_GeometryFromWKB'
SELECT ST_GeometryFromWKB(NULL);
ST_GeometryFromWKB(NULL)
NULL
SELECT ST_GeometryFromWKB(ST_GeomFromText('POINT(! 0)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_GeometryFromWKB(ST_GeomFromText('POLYGON((0 0,0 10,10 10,10 0,0 0)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_PointFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_PointFromWKB'
SELECT ST_PointFromWKB(NULL);
ST_PointFromWKB(NULL)
NULL
SELECT ST_PointFromWKB(ST_GeomFromText('POINT()'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_LineFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_LineFromWKB'
SELECT ST_LineFromWKB(NULL);
ST_LineFromWKB(NULL)
NULL
SELECT ST_LineFromWKB(ST_GeomFromText('LINESTRING(a a,b b)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_LineStringFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_LineStringFromWKB'
SELECT ST_LineStringFromWKB(NULL);
ST_LineStringFromWKB(NULL)
NULL
SELECT ST_LineStringFromWKB(ST_GeomFromText('LINESTRING(1 1,1 b)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_PolyFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_PolyFromWKB'
SELECT ST_PolyFromWKB(NULL);
ST_PolyFromWKB(NULL)
NULL
SELECT ST_PolyFromWKB(ST_GeomFromText('POLYGON(())'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_PolygonFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_PolygonFromWKB'
SELECT ST_PolygonFromWKB(NULL);
ST_PolygonFromWKB(NULL)
NULL
SELECT ST_PolygonFromWKB(ST_GeomFromText('POLYGON((0 0,0 5,5 5,5 0,0 0),())'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_MPointFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_MPointFromWKB'
SELECT ST_MPointFromWKB(NULL);
ST_MPointFromWKB(NULL)
NULL
SELECT ST_MPointFromWKB(ST_GeomFromText('MULTIPOINT(0 0,1)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_MultiPointFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_MultiPointFromWKB'
SELECT ST_MultiPointFromWKB(NULL);
ST_MultiPointFromWKB(NULL)
NULL
SELECT ST_MultiPointFromWKB(ST_GeomFromText('MULTIPOINT(0 0,1 1 1)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_MLineFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_MLineFromWKB'
SELECT ST_MLineFromWKB(NULL);
ST_MLineFromWKB(NULL)
NULL
SELECT ST_MLineFromWKB(ST_GeomFromText('MULTILINESTRING((0 0,1),())'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_MultiLineStringFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_MultiLineStringFromWKB'
SELECT ST_MultiLineStringFromWKB(NULL);
ST_MultiLineStringFromWKB(NULL)
NULL
SELECT ST_MultiLineStringFromWKB(ST_GeomFromText('MULTILINESTRING((0 0,1),(1 1,2 2)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_MPolyFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_MPolyFromWKB'
SELECT ST_MPolyFromWKB(NULL);
ST_MPolyFromWKB(NULL)
NULL
SELECT ST_MPolyFromWKB(ST_GeomFromText('MULTIPOLYGON(((0 0,1)),(()))'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_MultiPolygonFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_MultiPolygonFromWKB'
SELECT ST_MultiPolygonFromWKB(NULL);
ST_MultiPolygonFromWKB(NULL)
NULL
SELECT ST_MultiPolygonFromWKB(ST_GeomFromText('MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0)),(2 2,2 3,3 3,3 2,2 2))'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_GeomCollFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_GeomCollFromWKB'
SELECT ST_GeomCollFromWKB(NULL);
ST_GeomCollFromWKB(NULL)
NULL
SELECT ST_GeomCollFromWKB(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(a a))'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
SELECT ST_GeometryCollectionFromWKB();
ERROR 42000: Incorrect parameter count in the call to native function 'ST_GeometryCollectionFromWKB'
SELECT ST_GeometryCollectionFromWKB(NULL);
ST_GeometryCollectionFromWKB(NULL)
NULL
SELECT ST_GeometryCollectionFromWKB(ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,1 1)'));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
#
# WL#8579 Spatial Reference Systems
#
CREATE PROCEDURE gis_wkb_funcs(IN srid INT)
BEGIN
SET @wkb_pt = ST_AsWKB(ST_GeomFromText('POINT(0 1)'));
SET @wkb_ln = ST_AsWKB(ST_GeomFromText('LINESTRING(0 1,2 3,4 5,6 7,8 9, 10 11)'));
SET @wkb_py = ST_AsWKB(ST_GeomFromText('POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))'));
SET @wkb_mpt = ST_AsWKB(ST_GeomFromText('MULTIPOINT(0 1,2 3,4 5,6 7)'));
SET @wkb_mln = ST_AsWKB(ST_GeomFromText('MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))'));
SET @wkb_mpy = ST_AsWKB(ST_GeomFromText('MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),'
                                                     '((10 11,10 12,12 13,12 10,10 11)))'));
SET @wkb_gc = ST_AsWKB(ST_GeomFromText('GEOMETRYCOLLECTION('
                                       'POINT(0 1),'
                                       'LINESTRING(0 1,10 11),'
                                       'POLYGON((0 0,0 10,10 10,10 0, 0 0)),'
                                       'MULTIPOINT(0 1,2 3,4 5,6 7,8 9,10 11),'
                                       'MULTILINESTRING((0 1,10 11),(0 10,10 0)),'
                                       'MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))'));
# GIS WKB functions
DO ST_GeomFromWKB(@wkb_pt, srid);
DO ST_GeometryFromWKB(@wkb_pt, srid);
DO ST_PointFromWKB(@wkb_pt, srid);
DO ST_LineFromWKB(@wkb_ln, srid);
DO ST_LineStringFromWKB(@wkb_ln, srid);
DO ST_PolyFromWKB(@wkb_py, srid);
DO ST_PolygonFromWKB(@wkb_py, srid);
DO ST_MPointFromWKB(@wkb_mpt, srid);
DO ST_MultiPointFromWKB(@wkb_mpt, srid);
DO ST_MLineFromWKB(@wkb_mln, srid);
DO ST_MultiLineStringFromWKB(@wkb_mln, srid);
DO ST_MPolyFromWKB(@wkb_mpy, srid);
DO ST_MultiPolygonFromWKB(@wkb_mpy, srid);
DO ST_GeomCollFromWKB(@wkb_gc, srid);
DO ST_GeometryCollectionFromWKB(@wkb_gc, srid);
END |
# SRID 0 (should pass)
CALL gis_wkb_funcs(0);
# Projected SRS (should pass)
CALL gis_wkb_funcs(2000);
# Geographic SRS (should pass)
CALL gis_wkb_funcs(4326);
# Undefined SRS (should pass)
CALL gis_wkb_funcs(19000000);
ERROR SR001: There's no spatial reference system with SRID 19000000.
########################################################################
# WL 9435 Axis order in WKB parsing functions
# ST_GeomFromWKB
# ST_GeometryFromWKB
# ST_PointFromWKB
# ST_LineFromWKB
# ST_LineStringFromWKB
# ST_PolyFromWKB
# ST_PolygonFromWKB
# ST_GeomCollFromWKB
# ST_GeometryCollectionFromWKB
# ST_MPointFromWKB
# ST_MultiPointFromWKB
# ST_MLineFromWKB
# ST_MultiLineStringFromWKB
# ST_MPolyFromWKB
# ST_MultiPolygonFromWKB
########################################################################
########################################################################
# Inserting testing SRS values in spatial reference table
########################################################################
# Inserting geographical srs with long-lat ordering
CREATE SPATIAL REFERENCE SYSTEM 31000000 NAME 'TEST31000000' DEFINITION 'GEOGCS["SIRGAS 1995",DATUM["Sistema de Referencia Geocentrico para America del Sur 1995",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6170"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["LONG",EAST],AXIS["LAT",NORTH],AUTHORITY["EPSG","31000000"]]';
# Inserting geographical srs with lat-long ordering
CREATE SPATIAL REFERENCE SYSTEM 31100000 NAME 'TEST31100000' DEFINITION 'GEOGCS["SIRGAS 1995",DATUM["Sistema de Referencia Geocentrico para America del Sur 1995",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6170"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["LAT",NORTH],AXIS["LONG",EAST],AUTHORITY["EPSG","31100000"]]';
# Inserting projected srs with northing-easting ordering
CREATE SPATIAL REFERENCE SYSTEM 31200000 NAME 'TEST31200000' DEFINITION 'PROJCS["NAD83(NSRS2007) / Virginia Lambert",GEOGCS["NAD83(NSRS2007)",DATUM["NAD83 (National Spatial Reference System 2007)",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6759"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4759"]],PROJECTION["Lambert Conic Conformal (2SP)",AUTHORITY["EPSG","9802"]],PARAMETER["Latitude of false origin",36,AUTHORITY["EPSG","8821"]],PARAMETER["Longitude of false origin",-79.5,AUTHORITY["EPSG","8822"]],PARAMETER["Latitude of 1st standard parallel",37,AUTHORITY["EPSG","8823"]],PARAMETER["Latitude of 2nd standard parallel",39.5,AUTHORITY["EPSG","8824"]],PARAMETER["Easting at false origin",0,AUTHORITY["EPSG","8826"]],PARAMETER["Northing at false origin",0,AUTHORITY["EPSG","8827"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",NORTH],AXIS["Y",EAST],AUTHORITY["EPSG","31200000"]]';
# Inserting projected srs with easting-northing ordering
CREATE SPATIAL REFERENCE SYSTEM 31300000 NAME 'TEST31300000' DEFINITION 'PROJCS["NAD83(NSRS2007) / Virginia Lambert",GEOGCS["NAD83(NSRS2007)",DATUM["NAD83 (National Spatial Reference System 2007)",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6759"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4759"]],PROJECTION["Lambert Conic Conformal (2SP)",AUTHORITY["EPSG","9802"]],PARAMETER["Latitude of false origin",36,AUTHORITY["EPSG","8821"]],PARAMETER["Longitude of false origin",-79.5,AUTHORITY["EPSG","8822"]],PARAMETER["Latitude of 1st standard parallel",37,AUTHORITY["EPSG","8823"]],PARAMETER["Latitude of 2nd standard parallel",39.5,AUTHORITY["EPSG","8824"]],PARAMETER["Easting at false origin",0,AUTHORITY["EPSG","8826"]],PARAMETER["Northing at false origin",0,AUTHORITY["EPSG","8827"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",EAST],AXIS["Y",NORTH],AUTHORITY["EPSG","31300000"]]';
# When the geometry is in a projected spatial reference system,
# the coordinates are interpreted as in the order they appear in the geometry.
########################################################################
# Retrieving geometrycollections with the ST_GEOMFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_GEOMFROMWKB(ST_ASWKB(GEOMETRYCOLLECTION(POINT(3, 5))),
31000000, 'axis-order=long-lat'));
ST_ASTEXT(ST_GEOMFROMWKB(ST_ASWKB(GEOMETRYCOLLECTION(POINT(3, 5))),
31000000, 'axis-order=long-lat'))
GEOMETRYCOLLECTION(POINT(3 5))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_GEOMFROMWKB(ST_ASWKB(GEOMETRYCOLLECTION(POINT(3, 5))),
31000000, 'axis-order=lat-long'));
ST_ASTEXT(ST_GEOMFROMWKB(ST_ASWKB(GEOMETRYCOLLECTION(POINT(3, 5))),
31000000, 'axis-order=lat-long'))
GEOMETRYCOLLECTION(POINT(5 3))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_GEOMFROMWKB(ST_ASWKB(GEOMETRYCOLLECTION(POINT(3, 5))),
31000000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_GEOMFROMWKB(ST_ASWKB(GEOMETRYCOLLECTION(POINT(3, 5))),
31000000, 'axis-order=srid-defined'))
GEOMETRYCOLLECTION(POINT(3 5))
########################################################################
# Retrieving points with the ST_GEOMETRYFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_GEOMETRYFROMWKB(@wkb_pt,
31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_GEOMETRYFROMWKB(@wkb_pt,
31000000,'axis-order=long-lat'))
POINT(0 1)
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_GEOMETRYFROMWKB(@wkb_mpt,
31100000,'axis-order=lat-long'));
ST_ASTEXT(ST_GEOMETRYFROMWKB(@wkb_mpt,
31100000,'axis-order=lat-long'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_GEOMETRYFROMWKB(@wkb_mpt,
31200000,'axis-order=srid-defined'));
ST_ASTEXT(ST_GEOMETRYFROMWKB(@wkb_mpt,
31200000,'axis-order=srid-defined'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
########################################################################
# Retrieving points with the ST_POINTFROMWKB function
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)), 31000000, 'axis-order=long-lat'));
ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)), 31000000, 'axis-order=long-lat'))
POINT(1 2)
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)), 31000000, 'axis-order=lat-long'));
ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)), 31000000, 'axis-order=lat-long'))
POINT(2 1)
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)),31000000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)),31000000, 'axis-order=srid-defined'))
POINT(1 2)
SELECT ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)),31300000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_POINTFROMWKB(St_AsWKB(point(1, 2)),31300000, 'axis-order=srid-defined'))
POINT(1 2)
########################################################################
# Retrieving linestrings with the ST_LineFromWKB function
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31000000,
'axis-order=long-lat'));
ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31000000,
'axis-order=long-lat'))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31000000,
'axis-order=lat-long'));
ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31000000,
'axis-order=lat-long'))
LINESTRING(1 0,3 2,5 4,7 6,9 8,11 10)
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31100000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31100000,
'axis-order=srid-defined'))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
SELECT ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31000000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_LINEFROMWKB(@wkb_ln, 31000000,
'axis-order=srid-defined'))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
########################################################################
# Retrieving linestrings with the ST_LineStringFromWKB function
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31000000,
'axis-order=long-lat'));
ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31000000,
'axis-order=long-lat'))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31000000,
'axis-order=lat-long'));
ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31000000,
'axis-order=lat-long'))
LINESTRING(1 0,3 2,5 4,7 6,9 8,11 10)
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31100000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31100000,
'axis-order=srid-defined'))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
SELECT ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31000000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_LINESTRINGFROMWKB(@wkb_ln, 31000000,
'axis-order=srid-defined'))
LINESTRING(0 1,2 3,4 5,6 7,8 9,10 11)
########################################################################
# Retrieving polygons with the ST_POLYFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31000000,
'axis-order=long-lat'));
ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31000000,
'axis-order=long-lat'))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31000000,
'axis-order=lat-long'));
ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31000000,
'axis-order=lat-long'))
POLYGON((0 0,10 0,10 10,0 10,0 0),(4 4,6 4,6 6,4 6,4 4))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31000000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31000000,
'axis-order=srid-defined'))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
SELECT ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31300000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_POLYFROMWKB(@wkb_py, 31300000,
'axis-order=srid-defined'))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
########################################################################
# Retrieving polygons with the ST_POLYGONFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31000000,
'axis-order=long-lat'));
ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31000000,
'axis-order=long-lat'))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31000000,
'axis-order=lat-long'));
ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31000000,
'axis-order=lat-long'))
POLYGON((0 0,10 0,10 10,0 10,0 0),(4 4,6 4,6 6,4 6,4 4))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31000000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31000000,
'axis-order=srid-defined'))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
SELECT ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31300000,
'axis-order=srid-defined'));
ST_ASTEXT(ST_POLYGONFROMWKB(@wkb_py, 31300000,
'axis-order=srid-defined'))
POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))
########################################################################
# Retrieving geometrycollections with the ST_GEOMCOLLFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,'axis-order=long-lat'))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,'axis-order=lat-long'));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,'axis-order=lat-long'))
GEOMETRYCOLLECTION(POINT(1 0),LINESTRING(1 0,11 10),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOINT((1 0),(3 2),(5 4),(7 6),(9 8),(11 10)),MULTILINESTRING((1 0,11 10),(10 0,0 10)),MULTIPOLYGON(((1 0,5 0,6 5,0 5,1 0)),((4 5,10 5,11 10,5 10,4 5))))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,'axis-order=srid-defined'));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,'axis-order=srid-defined'))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31300000,'axis-order=srid-defined'));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31300000,'axis-order=srid-defined'))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
########################################################################
# Retrieving geometrycollections with the ST_GEOMETRYCOLLECTIONFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31000000,'axis-order=long-lat'))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31000000,'axis-order=lat-long'));
ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31000000,'axis-order=lat-long'))
GEOMETRYCOLLECTION(POINT(1 0),LINESTRING(1 0,11 10),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOINT((1 0),(3 2),(5 4),(7 6),(9 8),(11 10)),MULTILINESTRING((1 0,11 10),(10 0,0 10)),MULTIPOLYGON(((1 0,5 0,6 5,0 5,1 0)),((4 5,10 5,11 10,5 10,4 5))))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31000000,'axis-order=srid-defined'));
ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31000000,'axis-order=srid-defined'))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
SELECT ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31300000,'axis-order=srid-defined'));
ST_ASTEXT(ST_GEOMETRYCOLLECTIONFROMWKB(@wkb_gc,
31300000,'axis-order=srid-defined'))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
########################################################################
# Retrieving multipoints with the ST_MPOINTFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=long-lat'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=lat-long'));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=lat-long'))
MULTIPOINT((1 0),(3 2),(5 4),(7 6))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=srid-defined'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31300000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt,
31300000,'axis-order=srid-defined'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
########################################################################
# Retrieving multipoints with the ST_MULTIPOINTFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=long-lat'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=lat-long'));
ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=lat-long'))
MULTIPOINT((1 0),(3 2),(5 4),(7 6))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31000000,'axis-order=srid-defined'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
SELECT ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31300000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTIPOINTFROMWKB(@wkb_mpt,
31300000,'axis-order=srid-defined'))
MULTIPOINT((0 1),(2 3),(4 5),(6 7))
########################################################################
# Retrieving multistrings with the ST_MLINEFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31000000,'axis-order=long-lat'))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31000000,'axis-order=lat-long'));
ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31000000,'axis-order=lat-long'))
MULTILINESTRING((1 0,3 2,5 4),(7 6,9 8,11 10))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31000000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31000000,'axis-order=srid-defined'))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
SELECT ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31300000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MLINEFROMWKB(@wkb_mln, 31300000,'axis-order=srid-defined'))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
########################################################################
# Retrieving multistrings with the ST_MULTILINESTRINGFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31000000,'axis-order=long-lat'));
ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31000000,'axis-order=long-lat'))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31000000,'axis-order=lat-long'));
ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31000000,'axis-order=lat-long'))
MULTILINESTRING((1 0,3 2,5 4),(7 6,9 8,11 10))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31000000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31000000,'axis-order=srid-defined'))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
SELECT ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31300000,'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTILINESTRINGFROMWKB(@wkb_mln,
31300000,'axis-order=srid-defined'))
MULTILINESTRING((0 1,2 3,4 5),(6 7,8 9,10 11))
########################################################################
# Retrieving multipolygons with the ST_MPOLYFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy, 31000000, 'axis-order=long-lat'));
ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy, 31000000, 'axis-order=long-lat'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy, 31000000, 'axis-order=lat-long'));
ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy, 31000000, 'axis-order=lat-long'))
MULTIPOLYGON(((1 0,10 0,11 10,0 10,1 0),(5 4,6 4,7 6,4 6,5 4)),((11 10,12 10,13 12,10 12,11 10)))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy,
31000000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy,
31000000, 'axis-order=srid-defined'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
SELECT ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy,
31300000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy,
31300000, 'axis-order=srid-defined'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
########################################################################
# Retrieving multipolygons with the ST_MULTIPOLYGONFROMWKB function
#
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'axis-order=long-lat'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'axis-order=long-lat'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'axis-order=lat-long'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'axis-order=lat-long'))
MULTIPOLYGON(((1 0,10 0,11 10,0 10,1 0),(5 4,6 4,7 6,4 6,5 4)),((11 10,12 10,13 12,10 12,11 10)))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31000000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31000000, 'axis-order=srid-defined'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31300000, 'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31300000, 'axis-order=srid-defined'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
########################################################################
# Error testing
########################################################################
# Test with too many options.
SELECT ST_ASTEXT(ST_MPOLYFROMWKB(@wkb_mpy, 31000000,
'axis-order=srid-defined, axis-order=lat-long'));
ERROR 22023: Duplicate option key 'axis-order' in funtion 'st_mpolyfromwkb'.
# Test with invalid options key.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axix-order=srid-defined'));
ERROR 22023: Invalid option key 'axix-order' in function st_geomcollfromwkb.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axix-order=lat-long'));
ERROR 22023: Invalid option key 'axix-order' in function st_geomcollfromwkb.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axix-order=lat-long'));
ERROR 22023: Invalid option key 'axix-order' in function st_geomcollfromwkb.
# Test with invalid options value
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31300000,'axis-order=srid-dfined'));
ERROR 22023: Invalid value 'srid-dfined' for option 'axis-order' in function 'st_geomcollfromwkb'.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axis-order=lat-bong'));
ERROR 22023: Invalid value 'lat-bong' for option 'axis-order' in function 'st_geomcollfromwkb'.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axis-order=lat-shwong'));
ERROR 22023: Invalid value 'lat-shwong' for option 'axis-order' in function 'st_geomcollfromwkb'.
# Test with both invalid option and invalid value
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axis-border=lat-shwong'));
ERROR 22023: Invalid option key 'axis-border' in function st_geomcollfromwkb.
# Testing for badly formed options argument.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axis-order srid-defined'));
ERROR 22023: The string 'axis-order srid-defined' is not a valid key = value pair in function st_geomcollfromwkb.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'=axis-order srid-defined'));
ERROR 22023: The options argument in function st_geomcollfromwkb starts with the invalid character '='.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axis-order=srid-defined='));
ERROR 22023: The options argument in function st_geomcollfromwkb ends with the invalid character '='.
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,'axis-order== srid-defined'));
ERROR 22023: The options argument in function st_geomcollfromwkb contains the invalid character sequence '=='.
# ######################################################################
# Empty string and white space testing
# ######################################################################
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,''));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,''))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,' '));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc, 31000000,' '))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,' axis-order = srid-defined '));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(@wkb_gc,
31000000,' axis-order = srid-defined '))
GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,10 11),POLYGON((0 0,0 10,10 10,10 0,0 0)),MULTIPOINT((0 1),(2 3),(4 5),(6 7),(8 9),(10 11)),MULTILINESTRING((0 1,10 11),(0 10,10 0)),MULTIPOLYGON(((0 1,0 5,5 6,5 0,0 1)),((5 4,5 10,10 11,10 5,5 4))))
########################################################################
# Case insensitive testing in otions argument
# (both option keys and values)
########################################################################
# Retrieve values with long lat ordering.
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'aXis-Order=LONG-LAT'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'aXis-Order=LONG-LAT'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
# Retrieve values with lat-long ordering.
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'AXis-OrdEr=Lat-Long'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 31000000, 'AXis-OrdEr=Lat-Long'))
MULTIPOLYGON(((1 0,10 0,11 10,0 10,1 0),(5 4,6 4,7 6,4 6,5 4)),((11 10,12 10,13 12,10 12,11 10)))
# Retrieve values with SRID-defined ordering.
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31000000, 'Axis-Order=sRid-dEfined'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31000000, 'Axis-Order=sRid-dEfined'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31300000, 'aXiS-oRdeR=sriD-dEfinEd'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy,
31300000, 'aXiS-oRdeR=sriD-dEfinEd'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
# ######################################################################
# NULL value and 0 value srid testing
# ######################################################################
# NULL value testing: Should return NULL if either or both parameters is NULL
SELECT ST_ASTEXT(ST_POINTFROMWKB(NULL,31000000, 'axis-order=long-lat'));
ST_ASTEXT(ST_POINTFROMWKB(NULL,31000000, 'axis-order=long-lat'))
NULL
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt, NULL, 'axis-order=lat-long'));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt, NULL, 'axis-order=lat-long'))
NULL
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt, 31000000, NULL));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt, 31000000, NULL))
NULL
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt, NULL, NULL));
ST_ASTEXT(ST_MPOINTFROMWKB(@wkb_mpt, NULL, NULL))
NULL
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(NULL, NULL, NULL));
ST_ASTEXT(ST_MPOINTFROMWKB(NULL, NULL, NULL))
NULL
# Coordinates are interpreted as in the order they appear in the geometry
SELECT ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 0, 'axis-order=srid-defined'));
ST_ASTEXT(ST_MULTIPOLYGONFROMWKB(@wkb_mpy, 0, 'axis-order=srid-defined'))
MULTIPOLYGON(((0 1,0 10,10 11,10 0,0 1),(4 5,4 6,6 7,6 4,4 5)),((10 11,10 12,12 13,12 10,10 11)))
##########################################################
# Inserting testing SRS values in spatial reference table
##########################################################
# Inserting geographical srs with long-lat ordering
CREATE SPATIAL REFERENCE SYSTEM 32000000 NAME 'TEST32000000' DEFINITION 'GEOGCS["SIRGAS 1995",DATUM["Sistema de Referencia Geocentrico para America del Sur 1995",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6170"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["LONG",EAST],AXIS["LAT",NORTH],AUTHORITY["EPSG","32000000"]]';
# Inserting geographical srs with lat-long ordering
CREATE SPATIAL REFERENCE SYSTEM 32100000 NAME 'TEST32100000' DEFINITION 'GEOGCS["SIRGAS 1995",DATUM["Sistema de Referencia Geocentrico para America del Sur 1995",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6170"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["LAT",NORTH],AXIS["LONG",EAST],AUTHORITY["EPSG","32100000"]]';
# Inserting projected srs with northing-easting ordering
CREATE SPATIAL REFERENCE SYSTEM 32200000 NAME 'TEST32200000' DEFINITION 'PROJCS["NAD83(NSRS2007) / Virginia Lambert",GEOGCS["NAD83(NSRS2007)",DATUM["NAD83 (National Spatial Reference System 2007)",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6759"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4759"]],PROJECTION["Lambert Conic Conformal (2SP)",AUTHORITY["EPSG","9802"]],PARAMETER["Latitude of false origin",36,AUTHORITY["EPSG","8821"]],PARAMETER["Longitude of false origin",-79.5,AUTHORITY["EPSG","8822"]],PARAMETER["Latitude of 1st standard parallel",37,AUTHORITY["EPSG","8823"]],PARAMETER["Latitude of 2nd standard parallel",39.5,AUTHORITY["EPSG","8824"]],PARAMETER["Easting at false origin",0,AUTHORITY["EPSG","8826"]],PARAMETER["Northing at false origin",0,AUTHORITY["EPSG","8827"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["X",NORTH],AXIS["Y",EAST],AUTHORITY["EPSG","32200000"]]';
#########################################################################
# Test with geometries with geographic SRS with long-lat ordering outside
# valid range.
#########################################################################
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-200 10)', 0)), 32000000);
ERROR 22S02: Longitude -200.000000 is out of range in function st_pointfromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_LINEFROMWKB(ST_ASWKB(ST_LINEFROMTEXT('LINESTRING(0 5,194 10,10 15)', 0)), 32000000);
ERROR 22S02: Longitude 194.000000 is out of range in function st_linefromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_POLYFROMWKB(ST_ASWKB(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,-300 5,0 0))',0)), 32000000);
ERROR 22S02: Longitude -300.000000 is out of range in function st_polyfromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_MPOINTFROMWKB(ST_ASWKB(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (798 2), (-344 9))',0)),32000000);
ERROR 22S02: Longitude 798.000000 is out of range in function st_mpointfromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_MLINEFROMWKB(ST_ASWKB(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(744 11,11 14,14 17))',0)),32000000);
ERROR 22S02: Longitude 744.000000 is out of range in function st_mlinefromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_MPOLYFROMWKB(ST_ASWKB(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,350 0,350 350,0 350,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0)),32000000);
ERROR 22S02: Longitude 350.000000 is out of range in function st_mpolyfromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_GEOMCOLLFROMWKB(ST_ASWKB(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(-199 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0)), 32000000);
ERROR 22S02: Longitude -199.000000 is out of range in function st_geomcollfromwkb. It must be within (-180.000000, 180.000000].
#########################################################################
# Test with geometries with geographic SRS with lat-long ordering outside
# valid range (same geometries as above, but with different error since
# coordinates are flipped).
#########################################################################
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-200 10)',0)),32100000);
ERROR 22S03: Latitude -200.000000 is out of range in function st_pointfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_LINEFROMWKB(ST_ASWKB(ST_LINEFROMTEXT('LINESTRING(0 5,194 10,10 15)',0)),32100000);
ERROR 22S03: Latitude 194.000000 is out of range in function st_linefromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_POLYFROMWKB(ST_ASWKB(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,-300 5,0 0))',0)),32100000);
ERROR 22S03: Latitude -300.000000 is out of range in function st_polyfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_MPOINTFROMWKB(ST_ASWKB(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (798 2), (4 9))',0)),32100000);
ERROR 22S03: Latitude 798.000000 is out of range in function st_mpointfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_MLINEFROMWKB(ST_ASWKB(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(744 11,11 14,14 17))',0)),32100000);
ERROR 22S03: Latitude 744.000000 is out of range in function st_mlinefromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_MPOLYFROMWKB(ST_ASWKB(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,350 0,350 350,0 350,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0)),32100000);
ERROR 22S03: Latitude 350.000000 is out of range in function st_mpolyfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_GEOMCOLLFROMWKB(ST_ASWKB(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(-199 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0)), 32100000);
ERROR 22S03: Latitude -199.000000 is out of range in function st_geomcollfromwkb. It must be within [-90.000000, 90.000000].
##############################
# Miscellaneous error testing
##############################
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-91 10)',0)),32100000);
ERROR 22S03: Latitude -91.000000 is out of range in function st_pointfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 -182)',0)),32100000);
ERROR 22S02: Longitude -182.000000 is out of range in function st_pointfromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_MLINEFROMWKB(ST_ASWKB(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(744 11,11 91,14 17))',0)),32000000);
ERROR 22S02: Longitude 744.000000 is out of range in function st_mlinefromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_MLINEFROMWKB(ST_ASWKB(ST_MLINEFROMTEXT('MULTILINESTRING((0 95,2 4,4 6),(744 11,11 91,14 17))',0)),32000000);
ERROR 22S03: Latitude 95.000000 is out of range in function st_mlinefromwkb. It must be within [-90.000000, 90.000000].
###################
# Edge case testing
###################
SELECT ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-90.0000000 10)',0)),32100000));
ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-90.0000000 10)',0)),32100000))
POINT(-90 10)
SELECT ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 -180.0000000)',0)),32100000));
ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 -180.0000000)',0)),32100000))
POINT(10 -180)
SELECT ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(90.0000000 10)',0)),32100000));
ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(90.0000000 10)',0)),32100000))
POINT(90 10)
SELECT ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 180.0000000)',0)),32100000));
ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 180.0000000)',0)),32100000))
POINT(10 180)
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-90.0000001 10)',0)),32100000);
ERROR 22S03: Latitude -90.000000 is out of range in function st_pointfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 -180.0000001)',0)),32100000);
ERROR 22S02: Longitude -180.000000 is out of range in function st_pointfromwkb. It must be within (-180.000000, 180.000000].
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(90.0000001 10)',0)),32100000);
ERROR 22S03: Latitude 90.000000 is out of range in function st_pointfromwkb. It must be within [-90.000000, 90.000000].
SELECT ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(10 180.0000001)',0)),32100000);
ERROR 22S02: Longitude 180.000000 is out of range in function st_pointfromwkb. It must be within (-180.000000, 180.000000].
#################################################################
# Test that range restriction does not apply to projected spatial
# reference systems
#################################################################
SELECT ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-200 10)',0)),32200000));
ST_ASTEXT(ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-200 10)',0)),32200000))
POINT(-200 10)
SELECT ST_ASTEXT(ST_LINEFROMWKB(ST_ASWKB(ST_LINEFROMTEXT('LINESTRING(0 5,194 10,10 15)',0)),32200000));
ST_ASTEXT(ST_LINEFROMWKB(ST_ASWKB(ST_LINEFROMTEXT('LINESTRING(0 5,194 10,10 15)',0)),32200000))
LINESTRING(0 5,194 10,10 15)
SELECT ST_ASTEXT(ST_POLYFROMWKB(ST_ASWKB(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,-300 5,0 0))',0)),32200000));
ST_ASTEXT(ST_POLYFROMWKB(ST_ASWKB(ST_POLYFROMTEXT('POLYGON((0 0,5 0,5 5,-300 5,0 0))',0)),32200000))
POLYGON((0 0,5 0,5 5,-300 5,0 0))
SELECT ST_ASTEXT(ST_MPOINTFROMWKB(ST_ASWKB(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (798 2), (4 9))',0)),32200000));
ST_ASTEXT(ST_MPOINTFROMWKB(ST_ASWKB(ST_MPOINTFROMTEXT('MULTIPOINT((0 2), (5 7), (798 2), (4 9))',0)),32200000))
MULTIPOINT((0 2),(5 7),(798 2),(4 9))
SELECT ST_ASTEXT(ST_MLINEFROMWKB(ST_ASWKB(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(744 11,11 14,14 17))',0)),32200000));
ST_ASTEXT(ST_MLINEFROMWKB(ST_ASWKB(ST_MLINEFROMTEXT('MULTILINESTRING((0 2,2 4,4 6),(744 11,11 14,14 17))',0)),32200000))
MULTILINESTRING((0 2,2 4,4 6),(744 11,11 14,14 17))
SELECT ST_ASTEXT(ST_MPOLYFROMWKB(ST_ASWKB(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,350 0,350 350,0 350,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0)),32200000));
ST_ASTEXT(ST_MPOLYFROMWKB(ST_ASWKB(ST_MPOLYFROMTEXT('MULTIPOLYGON(((0 0,350 0,350 350,0 350,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0)))',0)),32200000))
MULTIPOLYGON(((0 0,350 0,350 350,0 350,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0)))
SELECT ST_ASTEXT(ST_GEOMCOLLFROMWKB(ST_ASWKB(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(-199 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0)), 32200000));
ST_ASTEXT(ST_GEOMCOLLFROMWKB(ST_ASWKB(ST_GEOMCOLLFROMTEXT('GEOMETRYCOLLECTION(POINT(-199 5), POLYGON((0 0,10 0,10 10,0 10,0 0)), MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4, 4 4)),((0 0,-2 -2,0 -2, 0 0))))',0)), 32200000))
GEOMETRYCOLLECTION(POINT(-199 5),POLYGON((0 0,10 0,10 10,0 10,0 0)),MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(4 4,4 6,6 6,6 4,4 4)),((0 0,-2 -2,0 -2,0 0))))
# Clean up
DROP PROCEDURE gis_wkb_funcs;
DROP SPATIAL REFERENCE SYSTEM 31000000;
DROP SPATIAL REFERENCE SYSTEM 31100000;
DROP SPATIAL REFERENCE SYSTEM 31200000;
DROP SPATIAL REFERENCE SYSTEM 31300000;
DROP SPATIAL REFERENCE SYSTEM 32000000;
DROP SPATIAL REFERENCE SYSTEM 32100000;
DROP SPATIAL REFERENCE SYSTEM 32200000;
#
# WL#11096 Don't do Cartesian computations on geographic geometries
#
# Assume SRID 10 is not defined. Should raise warning.
DO ST_ASBINARY(x'0A000000010100000000000000000000000000000000000000');
Warnings:
Warning	3565	There's no spatial reference system with SRID 10. The axis order is unknown.
DO ST_ASWKB(x'0A000000010100000000000000000000000000000000000000');
Warnings:
Warning	3565	There's no spatial reference system with SRID 10. The axis order is unknown.
