########################################
# Requirement F-7a:                    #
# CAST(point AS POINT)                 #
########################################
#
# Casting from POINT to POINT is always allowed.
# Must return Point.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT))
POINT(1 1)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('POINT(1 1)'), CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT));
ST_EQUALS(ST_GEOMFROMTEXT('POINT(1 1)'), CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT))
1
#
########################################
# Requirement F-7b:                    #
# CAST(multipoint AS POINT)            #
########################################
#
# Casting from MULTIPOINT with only one point to POINT is allowed.
# Must return Point.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS POINT))
POINT(1 1)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('POINT(1 1)'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS POINT));
ST_EQUALS(ST_GEOMFROMTEXT('POINT(1 1)'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS POINT))
1
#
# Casting from MULTIPOINT with multiple points to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS POINT));
ERROR 22S01: Invalid cast from MULTIPOINT to POINT.
#
########################################
# Requirement F-7c:                    #
# CAST(geometrycollection AS POINT)    #
########################################
#
# Casting from GEOMETRYCOLLECTION with only one point to POINT is allowed.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1))') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1))') AS POINT))
POINT(1 1)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('POINT(1 1)'), CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1))') AS POINT));
ST_EQUALS(ST_GEOMFROMTEXT('POINT(1 1)'), CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1))') AS POINT))
1
#
# Casting from empty GEOMETRYCOLLECTION to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION()') AS POINT));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to POINT.
#
# Casting from GEOMETRYCOLLECTION with multiple points to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), POINT(2 2))') AS POINT));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to POINT.
#
# Casting from GEOMETRYCOLLECTION with not only points to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), GEOMETRYCOLLECTION())') AS POINT));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to POINT.
#
########################################
# Requirement F-7d:                    #
# Disallowed casts to POINT            #
########################################
#
# Casting from LINESTRING to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS POINT));
ERROR 22S01: Invalid cast from LINESTRING to POINT.
#
# Casting from POLYGON to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 2 2, 3 3, 1 1))') AS POINT));
ERROR 22S01: Invalid cast from POLYGON to POINT.
#
# Casting from MULTILINESTRING to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (1 1, 2 2))') AS POINT));
ERROR 22S01: Invalid cast from MULTILINESTRING to POINT.
#
# Casting from MULTIPOLYGON to POINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 2 2, 3 3, 1 1)), ((1 1, 2 2, 3 3, 1 1)))') AS POINT));
ERROR 22S01: Invalid cast from MULTIPOLYGON to POINT.
#
#############################################
# Testing result of WKT input               #
#############################################
#
# Casting from ST_GEOMFROMTEXT(<wkt>) to POINT must return expected point.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT))
POINT(1 1)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((2 2))') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((2 2))') AS POINT))
POINT(2 2)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(3 3))') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(3 3))') AS POINT))
POINT(3 3)
#
#############################################
# Testing result of user variable input     #
#############################################
#
SET @point = ST_GEOMFROMTEXT('POINT(1 1)');
SET @multipoint = ST_GEOMFROMTEXT('MULTIPOINT((2 2))');
SET @geomcollection = ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(3 3))');
#
# Casting from geometry in user variable to POINT must return expected point.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS POINT))
POINT(1 1)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((2 2))') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((2 2))') AS POINT))
POINT(2 2)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(3 3))') AS POINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(3 3))') AS POINT))
POINT(3 3)
#
#############################################
# Testing result of table column input      #
#############################################
#
CREATE TABLE gis_point_castable (fid INTEGER NOT NULL PRIMARY KEY, g GEOMETRY);
INSERT INTO gis_point_castable values
(101, ST_GEOMFROMTEXT('POINT(1 1)')),
(102, ST_GEOMFROMTEXT('MULTIPOINT((2 2))')),
(103, ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(3 3))'));
#
# Casting from geometry column to POINT must return expected point.
SELECT ST_ASTEXT(CAST(g AS POINT)) FROM gis_point_castable;
ST_ASTEXT(CAST(g AS POINT))
POINT(1 1)
POINT(2 2)
POINT(3 3)
#
# Cleanup.
DROP TABLE gis_point_castable;
