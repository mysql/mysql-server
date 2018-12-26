# Creating the spatial Geometry object
USE test;
# ST_VALIDATE must return null when its parameter is NULL
SELECT ST_ASTEXT(ST_VALIDATE( NULL ));
ST_ASTEXT(ST_VALIDATE( NULL ))
NULL
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT(NULL,0)));
ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT(NULL,0)))
NULL
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT(NULL,4053)));
ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT(NULL,4053)))
NULL
# ST_VALIDATE raises an error if the data is malformed
DO ST_VALIDATE( x'00000000DEADBEEF');
ERROR 22023: Invalid GIS data provided to function st_validate.
# ST_VALIDATE raises an error if the SRS is not known
DO ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POINT(0 0)',10)));
ERROR SR001: There's no spatial reference system with SRID 10.
# ST_VALIDATE raises an error if longitude is out of range
DO ST_VALIDATE(ST_GEOMFROMTEXT('POINT(0 270)', 4326));
ERROR 22S02: Longitude 270.000000 is out of range in function st_geomfromtext. It must be within (-180.000000, 180.000000].
# ST_VALIDATE raises an error if latitude is out of range
DO ST_VALIDATE(ST_GEOMFROMTEXT('POINT(270 0)', 4326));
ERROR 22S03: Latitude 270.000000 is out of range in function st_geomfromtext. It must be within [-90.000000, 90.000000].
# ST_VALIDATE returns the same geometry as it was given when it is valid
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 0.25 0.75, 0.75 0.75, 0.75 0.25, 0.25 0.25))'))) AS
valid_polygon;
valid_polygon
POLYGON((0 0,1 0,1 1,0 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25))
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 0.25 0.75, 0.75 0.75, 0.75 0.25, 0.25 0.25))',4053))) AS
valid_polygon;
valid_polygon
POLYGON((0 0,1 0,1 1,0 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25))
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 0.25 0.75, 0.75 0.75, 0.75 0.25, 0.25 0.25))',2000))) AS
valid_polygon;
valid_polygon
POLYGON((0 0,1 0,1 1,0 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25))
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 0.25 0.75, 0.75 0.75, 0.75 0.25, 0.25 0.25))',4326))) AS
valid_polygon;
valid_polygon
POLYGON((0 0,1 0,1 1,0 1,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25))
# ST_VALIDATE returns NULL if the geometry is invalid
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 1.75 0.25, 0.75 0.75, 0.25 0.75, 0.25 0.25))'))) AS
should_be_null;
should_be_null
NULL
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 1.75 0.25, 0.75 0.75, 0.25 0.75, 0.25 0.25))',4053))) AS
should_be_null;
should_be_null
NULL
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 1.75 0.25, 0.75 0.75, 0.25 0.75, 0.25 0.25))',2000))) AS
should_be_null;
should_be_null
NULL
SELECT ST_ASTEXT(ST_VALIDATE( ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 1, 0
0),( 0.25 0.25, 1.75 0.25, 0.75 0.75, 0.25 0.75, 0.25 0.25))',4326))) AS
should_be_null;
should_be_null
NULL
