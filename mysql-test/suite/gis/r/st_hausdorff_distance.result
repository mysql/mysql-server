SELECT ST_HausdorffDistance(NULL,NULL);
ST_HausdorffDistance(NULL,NULL)
NULL
SELECT ST_HausdorffDistance(NULL,'');
ST_HausdorffDistance(NULL,'')
NULL
DO ST_HausdorffDistance('LINESTRING(0 0, 0 1)');
ERROR 42000: Incorrect parameter count in the call to native function 'ST_HausdorffDistance'
DO ST_HausdorffDistance('','');
ERROR 22023: Invalid GIS data provided to function st_hausdorffdistance.
SELECT st_hausdorffdistance(0xffffffff01020000000500000000000000000000000000000000000000000000000000F03F000000000000F03F000000000000F03F00000000000000400000000000000040000000000000F03F00000000000000400000000000000040,0xffffffff01020000000500000000000000000000000000000000000000000000000000F03F000000000000F03F000000000000F03F00000000000000400000000000000040000000000000F03F00000000000000400000000000000040);
ERROR SR001: There's no spatial reference system with SRID 4294967295.
DO ST_HausdorffDistance
(ST_GEOMFROMTEXT('LINESTRING(0 0,1 1,2 1,1 2,2 2)', 4326),
ST_GEOMFROMTEXT('LINESTRING(0 1,1 0,1 1,1 2,1 3)', 0));
ERROR HY000: Binary geometry function st_hausdorffdistance given two geometries of different srids: 4326 and 0, which should have been identical.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('POINT(0 0)'), ST_GEOMFROMTEXT('POINT(0 1)'));
ERROR 22S00: st_hausdorffdistance(POINT, POINT) has not been implemented for Cartesian spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)'), ST_GEOMFROMTEXT('MULTIPOINT(0 1)'));
ERROR 22S00: st_hausdorffdistance(LINESTRING, MULTIPOINT) has not been implemented for Cartesian spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)'), ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'));
ERROR 22S00: st_hausdorffdistance(LINESTRING, POLYGON) has not been implemented for Cartesian spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)'), ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 1 0, 1 1, 0 0)))'));
ERROR 22S00: st_hausdorffdistance(LINESTRING, MULTIPOLYGON) has not been implemented for Cartesian spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 1 0, 1 1, 0 0)))', 4326), ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)', 4326));
ERROR 22S00: st_hausdorffdistance(MULTIPOLYGON, LINESTRING) has not been implemented for geographic spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)'), ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(0 0, 0 1))'));
ERROR 22S00: st_hausdorffdistance(LINESTRING, GEOMCOLLECTION) has not been implemented for Cartesian spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(0 0, 0 1))'), ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)'));
ERROR 22S00: st_hausdorffdistance(GEOMCOLLECTION, LINESTRING) has not been implemented for Cartesian spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(0 0, 0 1))', 4326), ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(LINESTRING(0 0, 0 1))', 4326));
ERROR 22S00: st_hausdorffdistance(GEOMCOLLECTION, GEOMCOLLECTION) has not been implemented for geographic spatial reference systems.
DO ST_HausdorffDistance(ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1e308, 1e308 1e308)'), ST_GEOMFROMTEXT('LINESTRING(0 0, 0 1)'));
ERROR 22003: Hausdorff_distance value is out of range in 'st_hausdorffdistance'
SELECT
ROUND(
ST_HausdorffDistance
(ST_GEOMFROMTEXT('LINESTRING(0 0, 100 0)', 0),
ST_GEOMFROMTEXT('LINESTRING(0 0, 50 50, 100 0)', 0)),
2
) AS dfd;
dfd
0
SELECT
ROUND(
ST_HausdorffDistance
(ST_GEOMFROMTEXT('LINESTRING(0 0,1 1,2 1,1 2,2 2)', 4326),
ST_GEOMFROMTEXT('LINESTRING(0 1,1 0,1 1,1 2,1 3)', 4326),
'foot'),
2
) AS dfd;
dfd
362775
