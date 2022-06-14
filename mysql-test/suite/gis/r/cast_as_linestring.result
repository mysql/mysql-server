############################################
# Requirement F-8a:                        #
# CAST(linestring AS LINESTRING)           #
############################################
#
# Casting from LINESTRING to LINESTRING is always allowed.
# Must return Linestring.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING))
LINESTRING(1 1,2 2)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING))
1
#
#
############################################
# Requirement F-8b:                        #
# CAST(polygon AS LINESTRING)              #
############################################
#
# Casting from POLYGON with only exterior ring to LINESTRING is allowed.
# Must return Linestring.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 2 2, 3 3, 1 1))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 2 2, 3 3, 1 1))') AS LINESTRING))
LINESTRING(1 1,2 2,3 3,1 1)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2, 3 3, 1 1)'), CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 2 2, 3 3, 1 1))') AS LINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2, 3 3, 1 1)'), CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 2 2, 3 3, 1 1))') AS LINESTRING))
1
#
# Casting from POLYGON with multiple rings to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((1 1, 2 2, 3 3, 1 1), (4 4, 5 5, 6 6, 4 4))') AS LINESTRING));
ERROR 22S01: Invalid cast from POLYGON to LINESTRING.
#
############################################
# Requirement F-8c:                        #
# CAST(multipoint AS LINESTRING)           #
############################################
#
# Casting from MULTIPOINT with two or more points to LINESTRING is allowed.
# Must return Linestring.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS LINESTRING))
LINESTRING(1 1,2 2)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS LINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1), (2 2))') AS LINESTRING))
1
#
# Casting from MULTIPOINT with only one point to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((1 1))') AS LINESTRING));
ERROR 22S01: Invalid cast from MULTIPOINT to LINESTRING.
#
############################################
# Requirement F-8d:                        #
# CAST(multilinestring AS LINESTRING)      #
############################################
#
# Casting from MULTILINESTRING with only one Linestring to LINESTRING is allowed.
# Must return Linestring.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))') AS LINESTRING))
LINESTRING(1 1,2 2)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))') AS LINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2))') AS LINESTRING))
1
#
# Casting from MULTILINESTRING with multiple Linestrings to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((1 1, 2 2), (3 3, 4 4))') AS LINESTRING));
ERROR 22S01: Invalid cast from MULTILINESTRING to LINESTRING.
#
############################################
# Requirement F-8e:                        #
# CAST(geometrycollection AS LINESTRING)   #
############################################
#
# Casting from GEOMETRYCOLLECTION with only one Linestring to LINESTRING is allowed.
# Must return Linestring.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(1 1, 2 2))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(1 1, 2 2))') AS LINESTRING))
LINESTRING(1 1,2 2)
# Must return 1.
SELECT ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(1 1, 2 2))') AS LINESTRING));
ST_EQUALS(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)'), CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(1 1, 2 2))') AS LINESTRING))
1
#
# Casting from empty GEOMETRYCOLLECTION to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION()') AS LINESTRING));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to LINESTRING.
#
# Casting from GEOMETRYCOLLECTION with multiple Linestrings to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(1 1, 2 2), LINESTRING(3 3, 4 4))') AS LINESTRING));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to LINESTRING.
#
# Casting from GEOMETRYCOLLECTION with other geometries than Linestrings to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(1 1, 2 2), GEOMETRYCOLLECTION())') AS LINESTRING));
ERROR 22S01: Invalid cast from GEOMCOLLECTION to LINESTRING.
#
############################################
# Requirement F-8d:                        #
# Disallowed casts to LINESTRING           #
############################################
#
# Casting from POINT to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POINT(1 1)') AS LINESTRING));
ERROR 22S01: Invalid cast from POINT to LINESTRING.
#
# Casting from MULTIPOLYGON to LINESTRING is not allowed.
# Must raise error.
DO ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOLYGON(((1 1, 2 2, 3 3, 1 1)))') AS LINESTRING));
ERROR 22S01: Invalid cast from MULTIPOLYGON to LINESTRING.
#
#############################################
# Testing result of WKT input               #
#############################################
#
# Casting from ST_GEOMFROMTEXT(<wkt>) to LINESTRING must return expected linestring.
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)') AS LINESTRING))
LINESTRING(1 1,2 2)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((3 3, 4 3, 4 4, 3 4, 3 3))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('POLYGON((3 3, 4 3, 4 4, 3 4, 3 3))') AS LINESTRING))
LINESTRING(3 3,4 3,4 4,3 4,3 3)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((5 5), (6 6))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTIPOINT((5 5), (6 6))') AS LINESTRING))
LINESTRING(5 5,6 6)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((7 7, 8 8))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('MULTILINESTRING((7 7, 8 8))') AS LINESTRING))
LINESTRING(7 7,8 8)
SELECT ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(9 9, 10 10))') AS LINESTRING));
ST_ASTEXT(CAST(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(9 9, 10 10))') AS LINESTRING))
LINESTRING(9 9,10 10)
#
#############################################
# Testing result of user variable input     #
#############################################
#
SET @linestring = ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)');
SET @polygon = ST_GEOMFROMTEXT('POLYGON((3 3, 4 3, 4 4, 3 4, 3 3))');
SET @multipoint = ST_GEOMFROMTEXT('MULTIPOINT((5 5), (6 6))');
SET @multilinestring = ST_GEOMFROMTEXT('MULTILINESTRING((7 7, 8 8))');
SET @geomcollection = ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(9 9, 10 10))');
#
# Casting from geometry in user variable to LINESTRING must return expected linestring.
SELECT ST_ASTEXT(CAST(@linestring AS LINESTRING));
ST_ASTEXT(CAST(@linestring AS LINESTRING))
LINESTRING(1 1,2 2)
SELECT ST_ASTEXT(CAST(@multipoint AS LINESTRING));
ST_ASTEXT(CAST(@multipoint AS LINESTRING))
LINESTRING(5 5,6 6)
SELECT ST_ASTEXT(CAST(@multilinestring AS LINESTRING));
ST_ASTEXT(CAST(@multilinestring AS LINESTRING))
LINESTRING(7 7,8 8)
SELECT ST_ASTEXT(CAST(@geomcollection AS LINESTRING));
ST_ASTEXT(CAST(@geomcollection AS LINESTRING))
LINESTRING(9 9,10 10)
#
#############################################
# Testing result of table column input      #
#############################################
#
CREATE TABLE gis_linestring_castable (fid INTEGER NOT NULL PRIMARY KEY, g GEOMETRY);
INSERT INTO gis_linestring_castable VALUES
(101, ST_GEOMFROMTEXT('LINESTRING(1 1, 2 2)')),
(102, ST_GEOMFROMTEXT('POLYGON((3 3, 4 3, 4 4, 3 4, 3 3))')),
(103, ST_GEOMFROMTEXT('MULTIPOINT((5 5), (6 6))')),
(104, ST_GEOMFROMTEXT('MULTILINESTRING((7 7, 8 8))')),
(105, ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(9 9, 10 10))'));
#
# Casting from geometry column to LINESTRING must return expected linestring.
SELECT ST_ASTEXT(CAST(g AS LINESTRING)) FROM gis_linestring_castable;
ST_ASTEXT(CAST(g AS LINESTRING))
LINESTRING(1 1,2 2)
LINESTRING(3 3,4 3,4 4,3 4,3 3)
LINESTRING(5 5,6 6)
LINESTRING(7 7,8 8)
LINESTRING(9 9,10 10)
#
# Cleanup.
DROP TABLE gis_linestring_castable;
