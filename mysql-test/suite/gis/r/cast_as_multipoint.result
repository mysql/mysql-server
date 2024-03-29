###########################################
# Requirement F-10a:                      #
# CAST(point AS MULTIPOINT)               #
###########################################
#
# Casting from POINT to MULTIPOINT is always allowed.
# Must return MULTIPOINT.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT))
MULTIPOINT((1 1))
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1))'), CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT));
ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1))'), CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT))
1
#
###########################################
# Requirement F-10b:                      #
# CAST(linestring AS MULTIPOINT)          #
###########################################
#
# Casting from LINESTRING to MULTIPOINT is always allowed.
# Must return MULTIPOINT.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS MULTIPOINT))
MULTIPOINT((1 1),(2 2))
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS MULTIPOINT));
ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS MULTIPOINT))
1
#
###########################################
# Requirement F-10c:                      #
# CAST(multipoint AS MULTIPOINT)          #
###########################################
#
# Casting from MULTIPOINT to MULTIPOINT is always allowed.
# Must return MULTIPOINT.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT))
MULTIPOINT((1 1),(2 2))
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT));
ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS MULTIPOINT))
1
#
###########################################
# Requirement F-10d:                      #
# CAST(geometrycollection AS MULTIPOINT)  #
###########################################
#
# Casting from GEOMETRYCOLLECTION with only points to MULTIPOINT is allowed.
# Must return MULTIPOINT.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), POINT(2 2))') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), POINT(2 2))') AS MULTIPOINT))
MULTIPOINT((1 1),(2 2))
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))'), CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), POINT(2 2))') AS MULTIPOINT));
ST_EQUALS(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))'), CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), POINT(2 2))') AS MULTIPOINT))
1
#
# Casting from empty GEOMETRYCOLLECTION to MULTIPOINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION()') AS MULTIPOINT));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to MULTIPOINT.
#
# Casting from GEOMETRYCOLLECTION with not only points to MULTIPOINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(1 1), POINT(2 2), GEOMETRYCOLLECTION())') AS MULTIPOINT));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to MULTIPOINT.
#
###########################################
# Requirement F-10e:                      #
# Disallowed casts to MULTIPOINT          #
###########################################
#
# Casting from POLYGON to MULTIPOINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 4 1, 4 4, 1 4, 1 1), (2 2, 2 3, 3 3, 3 2, 2 2))') AS MULTIPOINT));
ERROR 22S01: Invalid cast from POLYGON to MULTIPOINT.
#
# Casting from MULTILINESTRING to MULTIPOINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS MULTIPOINT));
ERROR 22S01: Invalid cast from MULTILINESTRING to MULTIPOINT.
#
# Casting from MULTIPOLYGON to MULTIPOINT is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 4 1, 4 4, 1 4, 1 1)), ((5 5, 5 6, 6 6, 6 5, 5 5)))') AS MULTIPOINT));
ERROR 22S01: Invalid cast from MULTIPOLYGON to MULTIPOINT.
#
#############################################
# Testing result of WKT input               #
#############################################
#
# Casting from ST_GEOMFROMTEXT(<WKT>) to MULTIPOINT must return expected multipoint.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS MULTIPOINT))
MULTIPOINT((1 1))
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(2 2, 3 3)') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(2 2, 3 3)') AS MULTIPOINT))
MULTIPOINT((2 2),(3 3))
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((4 4), (5 5))') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((4 4), (5 5))') AS MULTIPOINT))
MULTIPOINT((4 4),(5 5))
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(6 6), POINT(7 7))') AS MULTIPOINT));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(6 6), POINT(7 7))') AS MULTIPOINT))
MULTIPOINT((6 6),(7 7))
#
#############################################
# Testing result of user variable input     #
#############################################
#
SET @point = ST_GEOMFROMTEXT('POINT(1 1)');
SET @linestring = ST_GEOMFROMTEXT('LINESTRING(2 2, 3 3)');
SET @multipoint = ST_GEOMFROMTEXT('MULTIPOINT((4 4), (5 5))');
SET @geomcollection = ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(6 6), POINT(7 7))');
#
# Casting from geometry in user variable to MULTIPOINT must return expected multipoint.
SELECT ST_ASTEXT(CAST(@point AS MULTIPOINT));
ST_ASTEXT(CAST(@point AS MULTIPOINT))
MULTIPOINT((1 1))
SELECT ST_ASTEXT(CAST(@linestring AS MULTIPOINT));
ST_ASTEXT(CAST(@linestring AS MULTIPOINT))
MULTIPOINT((2 2),(3 3))
SELECT ST_ASTEXT(CAST(@multipoint AS MULTIPOINT));
ST_ASTEXT(CAST(@multipoint AS MULTIPOINT))
MULTIPOINT((4 4),(5 5))
SELECT ST_ASTEXT(CAST(@geomcollection AS MULTIPOINT));
ST_ASTEXT(CAST(@geomcollection AS MULTIPOINT))
MULTIPOINT((6 6),(7 7))
#
#############################################
# Testing result of table column input      #
#############################################
#
CREATE TABLE gis_multipoint_castable (fid INTEGER NOT NULL PRIMARY KEY, g GEOMETRY);
INSERT INTO gis_multipoint_castable VALUES
(101, ST_GEOMFROMTEXT('POINT(1 1)')),
(102, ST_GEOMFROMTEXT('LINESTRING(2 2, 3 3)')),
(103, ST_GEOMFROMTEXT('MULTIPOINT((4 4), (5 5))')),
(104, ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(6 6), POINT(7 7))'));
#
# Casting from geometry column to MULTIPOINT must return expected multipoint.
SELECT ST_ASTEXT(CAST(g AS MULTIPOINT)) FROM gis_multipoint_castable;
ST_ASTEXT(CAST(g AS MULTIPOINT))
MULTIPOINT((1 1))
MULTIPOINT((2 2),(3 3))
MULTIPOINT((4 4),(5 5))
MULTIPOINT((6 6),(7 7))
#
# Cleanup.
DROP TABLE gis_multipoint_castable;
