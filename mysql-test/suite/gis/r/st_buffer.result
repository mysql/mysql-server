# Creating geometry objects and parameters.
USE test;
CREATE TABLE gis_geometries (fid INTEGER NOT NULL PRIMARY KEY, g GEOMETRY);
# INSERT into table
INSERT INTO gis_geometries VALUES
(119,ST_GEOMFROMTEXT('POINT(0 179)', 4326)),
(120,ST_GEOMFROMTEXT('POINT(0 -179)', 4326));
# Checking the integrity of above create/insert statements
# 2 rows.
SELECT count(ST_ASTEXT(g) != 'NULL') FROM gis_geometries;
count(ST_ASTEXT(g) != 'NULL')
2
SET @pt_lat_minus_91 = x'E6100000010100000000000000000000000000000000C056C0';
SET @pt_lat_plus_91 = x'E6100000010100000000000000000000000000000000C05640';
SET @pt_long_minus_181 = x'E610000001010000000000000000A066C00000000000000000';
SET @pt_long_plus_181 = x'E610000001010000000000000000A066400000000000000000';
SET @p_square = ST_BUFFER_STRATEGY('point_square');
SET @p_circle = ST_BUFFER_STRATEGY('point_circle', 42);
SET @e_flat = ST_BUFFER_STRATEGY('end_flat');
SET @e_round = ST_BUFFER_STRATEGY('end_round', 42);
SET @j_miter = ST_BUFFER_STRATEGY('join_miter', 3.3);
SET @j_round = ST_BUFFER_STRATEGY('join_round', 42);
SET @geo_pt = ST_GEOMFROMTEXT('POINT(0 0)', 4326);
SET @cart_pt = ST_GEOMFROMTEXT('POINT(0 0)');
SET @cart_ls = ST_GEOMFROMTEXT('LINESTRING(0 0, 1 1, 2 0)');
SET @cart_py = ST_GEOMFROMTEXT('POLYGON((0 0, 0 1, 1 1, 1 0, 0 0))');
SET @cart_mpt = ST_GEOMFROMTEXT('MULTIPOINT((0 0), (2 2))');
SET @cart_mls = ST_GEOMFROMTEXT('MULTILINESTRING((0 0, 1 1, 2 0), (3 0, 4 1, 5 0))');
SET @cart_mpy = ST_GEOMFROMTEXT('MULTIPOLYGON(((0 0, 0 1, 1 1, 1 0, 0 0)), ((3 0, 3 1, 4 1, 4 0, 3 0)))');
SET @cart_gc = ST_GEOMFROMTEXT('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1, 2 0), POLYGON((0 0, 0 1, 1 1, 1 0, 0 0)))');
#####################################################################################
# ST_BUFFER(geometry, distance, [strategy1, strategy2, strategy3])
#####################################################################################
SELECT ST_ASTEXT(ST_BUFFER(@cart_pt, 2, ST_BUFFER_STRATEGY('point_square')));
ST_ASTEXT(ST_BUFFER(@cart_pt, 2, ST_BUFFER_STRATEGY('point_square')))
POLYGON((-2 -2,2 -2,2 2,-2 2,-2 -2))
SELECT ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT('POINT(0 0)'), 2, @p_square));
ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT('POINT(0 0)'), 2, @p_square))
POLYGON((-2 -2,2 -2,2 2,-2 2,-2 -2))
SELECT ST_ASTEXT(ST_BUFFER(g, 200000)) FROM gis_geometries WHERE fid=119;
ST_ASTEXT(ST_BUFFER(g, 200000))
POLYGON((1.808732012132 179.001797223319,1.774319790048 178.651146270628,1.671703263552 178.313926070286,1.504835706186 178.003112171801,1.280142356871 177.730651561997,1.006269248335 177.507002685328,0.693747172682 177.340737607074,0.354584968899 177.238220226374,0.001808440257 177.203370329483,-0.351037520274 177.237519652159,-0.690405373542 177.339363233902,-1.003261471999 177.505007084359,-1.277584215986 177.728111162577,-1.502825601244 178.000124412105,-1.67031857023 178.310605716248,-1.773613842393 178.647621023172,-1.808732012132 178.998202776681,-1.774319790048 179.348853729372,-1.671703263552 179.686073929714,-1.504835706186 179.996887828199,-1.280142356871 -179.730651561998,-1.006269248335 -179.507002685328,-0.693747172682 -179.340737607075,-0.354584968899 -179.238220226374,-0.001808440257 -179.203370329484,0.351037520274 -179.237519652159,0.690405373542 -179.339363233902,1.003261471999 -179.505007084359,1.277584215986 -179.728111162577,1.502825601244 179.999875587896,1.67031857023 179.689394283752,1.773613842393 179.352378976829,1.808732012132 179.001797223319))
SELECT ST_ASTEXT(ST_BUFFER(g, 200000)) FROM gis_geometries WHERE fid=120;
ST_ASTEXT(ST_BUFFER(g, 200000))
POLYGON((1.808732012132 -178.998202776681,1.774319790048 -179.348853729372,1.671703263552 -179.686073929714,1.504835706186 -179.996887828199,1.280142356871 179.730651561998,1.006269248335 179.507002685328,0.693747172682 179.340737607075,0.354584968899 179.238220226374,0.001808440257 179.203370329484,-0.351037520274 179.237519652159,-0.690405373542 179.339363233902,-1.003261471999 179.505007084359,-1.277584215986 179.728111162577,-1.502825601244 -179.999875587896,-1.67031857023 -179.689394283752,-1.773613842393 -179.352378976829,-1.808732012132 -179.001797223319,-1.774319790048 -178.651146270628,-1.671703263552 -178.313926070286,-1.504835706186 -178.003112171801,-1.280142356871 -177.730651561997,-1.006269248335 -177.507002685328,-0.693747172682 -177.340737607074,-0.354584968899 -177.238220226374,-0.001808440257 -177.203370329483,0.351037520274 -177.237519652159,0.690405373542 -177.339363233902,1.003261471999 -177.505007084359,1.277584215986 -177.728111162577,1.502825601244 -178.000124412105,1.67031857023 -178.310605716248,1.773613842393 -178.647621023172,1.808732012132 -178.998202776681))
DROP TABLE gis_geometries;
#####################################################################################
# Invalid function calls
#####################################################################################
SELECT ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION()'), 20));
ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT('GEOMETRYCOLLECTION()'), 20))
GEOMETRYCOLLECTION EMPTY
SELECT ST_ASTEXT(ST_BUFFER(NULL, 1));
ST_ASTEXT(ST_BUFFER(NULL, 1))
NULL
DO ST_ASTEXT(ST_BUFFER(@cart_pt, 1, @e_flat));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_pt, 1, @j_round));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_pt, -1));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_ls, 2, @p_square));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_ls, -2));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_py, 3, @p_square));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_py, 3, @e_flat));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mpt, 4, @e_flat));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mpt, 4, @j_round));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mpt, -4));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mls, 5, @p_square));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mls, -5));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mpy, 6, @p_square));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_mpy, 6, @e_flat));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_gc, -7));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@geo_pt, 2000, @p_square));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@geo_pt, 2000, @e_flat));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@geo_pt, 2000, @j_round));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@geo_pt, -2000));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_pt, 1, @p_square, @p_circle));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_pt, 1, @p_circle, @p_square));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_ls, 2, @e_flat, @e_round));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_ls, 2, @e_round, @e_flat));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_py, 2, @j_miter, @j_round));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_py, 2, @j_round, @j_miter));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(@cart_pt, 2, 2));
ERROR HY000: Incorrect arguments to st_buffer
DO ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT('POINT(12,34 0)'), 1));
ERROR 22023: Invalid GIS data provided to function st_geomfromtext.
DO ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT('LINESTRING(0 0, 2 0, 2 2, 2 0, 1 1)', 4326), 100));
ERROR 22S00: st_buffer(LINESTRING) has not been implemented for geographic spatial reference systems.
DO ST_BUFFER(@pt_lat_minus_91, 20000);
ERROR 22S03: A parameter of function st_buffer contains a geometry with latitude -91.000000, which is out of range. It must be within [-90.000000, 90.000000].
DO ST_BUFFER(@pt_lat_plus_91, 20000);
ERROR 22S03: A parameter of function st_buffer contains a geometry with latitude 91.000000, which is out of range. It must be within [-90.000000, 90.000000].
DO ST_BUFFER(@pt_long_minus_181, 20000);
ERROR 22S02: A parameter of function st_buffer contains a geometry with longitude -181.000000, which is out of range. It must be within (-180.000000, 180.000000].
DO ST_BUFFER(@pt_long_plus_181, 20000);
ERROR 22S02: A parameter of function st_buffer contains a geometry with longitude 181.000000, which is out of range. It must be within (-180.000000, 180.000000].
# Assume SRID 10 is not defined.
DO ST_BUFFER(x'0A000000010100000000000000000000000000000000000000', 10);
ERROR SR001: There's no spatial reference system with SRID 10.
