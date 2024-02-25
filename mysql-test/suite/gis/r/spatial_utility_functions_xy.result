# Creating the spatial objects
CREATE TABLE gis_point (fid INTEGER NOT NULL PRIMARY KEY, g POINT);
# Inserting POINT Geometry Values
INSERT INTO gis_point VALUES
(101, ST_POINTFROMTEXT('POINT(0 0)')),
(102, ST_POINTFROMTEXT('POINT(1 0)')),
(103, ST_POINTFROMTEXT('POINT(0 1)')),
(104, ST_POINTFROMTEXT('POINT(1 1)')),
(105, ST_POINTFROMTEXT('POINT(-1 1)'));
INSERT INTO gis_point VALUES
(106, ST_POINTFROMWKB(ST_ASWKB(POINT(0,0)))),
(107, ST_POINTFROMWKB(ST_ASWKB(POINT(10,0)))),
(108, ST_POINTFROMWKB(ST_ASWKB(POINT(0,10)))),
(109, ST_POINTFROMWKB(ST_ASWKB(POINT(-10,0)))),
(110, ST_POINTFROMWKB(ST_ASWKB(POINT(0,-10))));
INSERT INTO gis_point VALUES
(111, ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(1 1)')))),
(112, ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(1e308 1e308)')))),
(113, ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(1e308 -1e308)')))),
(114, ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-1e308 1e308)')))),
(115, ST_POINTFROMWKB(ST_ASWKB(ST_POINTFROMTEXT('POINT(-1e308 -1e308)'))));
# Displaying the inserted POINT Geometry Data
SELECT fid, ST_ASTEXT(g) FROM gis_point;
fid	ST_ASTEXT(g)
101	POINT(0 0)
102	POINT(1 0)
103	POINT(0 1)
104	POINT(1 1)
105	POINT(-1 1)
106	POINT(0 0)
107	POINT(10 0)
108	POINT(0 10)
109	POINT(-10 0)
110	POINT(0 -10)
111	POINT(1 1)
112	POINT(1e308 1e308)
113	POINT(1e308 -1e308)
114	POINT(-1e308 1e308)
115	POINT(-1e308 -1e308)
#=======================================================================
# ST_X(point)
#=======================================================================
SELECT ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(0 0)'),5));
ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(0 0)'),5))
POINT(5 0)
SELECT ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(10 10)'), 5));
ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(10 10)'), 5))
POINT(5 10)
SELECT ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(1e308 1e308)'), 1e205));
ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(1e308 1e308)'), 1e205))
POINT(1e205 1e308)
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(1.1111,-1.1111))), 5.5555));
ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(1.1111,-1.1111))), 5.5555))
POINT(5.5555 -1.1111)
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(10.0000,10.0000))), 50.0000));
ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(10.0000,10.0000))), 50.0000))
POINT(50 10)
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(-1e308,-1e308))), -1e205));
ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(-1e308,-1e308))), -1e205))
POINT(-1e205 -1e308)
#=======================================================================
# ST_Y(point)
#=======================================================================
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(0 0)'),5));
ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(0 0)'),5))
POINT(0 5)
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(10 10)'), 5));
ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(10 10)'), 5))
POINT(10 5)
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(1e308 1e308)'), 1e205));
ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(1e308 1e308)'), 1e205))
POINT(1e308 1e205)
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(1.1111,-1.1111))), 5.5555));
ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(1.1111,-1.1111))), 5.5555))
POINT(1.1111 5.5555)
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(10.0000,10.0000))), 50.0000));
ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(10.0000,10.0000))), 50.0000))
POINT(10 50)
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(-1e308,-1e308))), -1e205));
ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(-1e308,-1e308))), -1e205))
POINT(-1e308 -1e205)
#=======================================================================
# ST_X(point)
#=======================================================================
SELECT ST_ASTEXT(ST_X(NULL, 5));
ST_ASTEXT(ST_X(NULL, 5))
NULL
SELECT ST_ASTEXT(ST_X(POINT(4, 5), NULL));
ST_ASTEXT(ST_X(POINT(4, 5), NULL))
NULL
SELECT ST_ASTEXT(ST_X(NULL, NULL));
ST_ASTEXT(ST_X(NULL, NULL))
NULL
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(NULL), NULL));
ST_ASTEXT(ST_X(ST_POINTFROMWKB(NULL), NULL))
NULL
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(1)), 5)));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')), 5)))' at line 1
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(1)), )));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')), )))' at line 1
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(1, 2)), a)));
ERROR 42S22: Unknown column 'a' in 'field list'
SELECT ST_ASTEXT(ST_X(ST_POINTFROMWKB(ST_ASWKB(POINT(a,b)), c)));
ERROR 42S22: Unknown column 'a' in 'field list'
SELECT ST_ASTEXT(ST_X(ST_POINTFROMTEXT('NULL'), NULL));
ERROR 22023: Invalid GIS data provided to function st_pointfromtext.
SELECT ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT()'), 5));
ERROR 22023: Invalid GIS data provided to function st_pointfromtext.
SELECT ST_ASTEXT(ST_X(ST_POINTFROMTEXT('POINT(1)'), 5));
ERROR 22023: Invalid GIS data provided to function st_pointfromtext.
#=======================================================================
# ST_Y(point)
#=======================================================================
SELECT ST_ASTEXT(ST_Y(NULL, 5));
ST_ASTEXT(ST_Y(NULL, 5))
NULL
SELECT ST_ASTEXT(ST_Y(POINT(4, 5), NULL));
ST_ASTEXT(ST_Y(POINT(4, 5), NULL))
NULL
SELECT ST_ASTEXT(ST_Y(NULL, NULL));
ST_ASTEXT(ST_Y(NULL, NULL))
NULL
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(NULL), NULL));
ST_ASTEXT(ST_Y(ST_POINTFROMWKB(NULL), NULL))
NULL
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(1)), 5)));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')), 5)))' at line 1
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(1)), )));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')), )))' at line 1
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(1, 2)), a)));
ERROR 42S22: Unknown column 'a' in 'field list'
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMWKB(ST_ASWKB(POINT(a,b)), c)));
ERROR 42S22: Unknown column 'a' in 'field list'
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('NULL'), NULL));
ERROR 22023: Invalid GIS data provided to function st_pointfromtext.
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT()'), 5));
ERROR 22023: Invalid GIS data provided to function st_pointfromtext.
SELECT ST_ASTEXT(ST_Y(ST_POINTFROMTEXT('POINT(1)'), 5));
ERROR 22023: Invalid GIS data provided to function st_pointfromtext.
# Try to use ST_X and ST_Y with out of range point values.
SELECT ST_X(POINT(pow(2, 2048), 4), 5);
ERROR 22003: DOUBLE value is out of range in 'pow(2,2048)'
SELECT ST_Y(POINT(pow(2, 2048), 4), 5);
ERROR 22003: DOUBLE value is out of range in 'pow(2,2048)'
SELECT ST_Y(POINT(4, pow(2, 2048)), 5);
ERROR 22003: DOUBLE value is out of range in 'pow(2,2048)'
SELECT ST_X(POINT(4, pow(2, 2048)), 5);
ERROR 22003: DOUBLE value is out of range in 'pow(2,2048)'
SELECT ST_X(POINT(4, 5), pow(2, 2048));
ERROR 22003: DOUBLE value is out of range in 'pow(2,2048)'
SELECT ST_Y(POINT(4, 5), pow(2, 2048));
ERROR 22003: DOUBLE value is out of range in 'pow(2,2048)'
SELECT ST_X(ST_GeomFromText('LINESTRING(0 0, 1 1)'), 5);
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_x.
SELECT ST_Y(ST_GeomFromText('LINESTRING(0 0, 1 1)'), 5);
ERROR 22S01: POINT value is a geometry of unexpected type LINESTRING in st_y.
SELECT ST_X(ST_GEOMFROMTEXT('POLYGON((0 0,2 2,0 2,0 0))'), -1024);
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_x.
SELECT ST_Y(ST_GEOMFROMTEXT('POLYGON((0 0,2 2,0 2,0 0))'), -1024);
ERROR 22S01: POINT value is a geometry of unexpected type POLYGON in st_y.
SELECT ST_X(ST_GEOMFROMTEXT('MULTIPOINT(10 40, 40 30, 20 20, 30 10)'), 1024);
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_x.
SELECT ST_Y(ST_GEOMFROMTEXT('MULTIPOINT(10 40, 40 30, 20 20, 30 10)'), 1024);
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOINT in st_y.
SELECT ST_X(ST_GEOMFROMTEXT("MULTILINESTRING((10 10, 20 20, 10 40)," 
"(40 40, 30 30, 40 20, 30 10))"), 1024);
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_x.
SELECT ST_Y(ST_GEOMFROMTEXT("MULTILINESTRING((10 10, 20 20, 10 40)," 
"(40 40, 30 30, 40 20, 30 10))"), 1024);
ERROR 22S01: POINT value is a geometry of unexpected type MULTILINESTRING in st_y.
SELECT ST_X(ST_GEOMFROMTEXT("MULTIPOLYGON(((40 40, 20 45, 45 30, 40 40))," 
"((20 35, 10 30, 10 10, 30 5, 45 20, 20 35)))"), -1024);
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_x.
SELECT ST_Y(ST_GEOMFROMTEXT("MULTIPOLYGON(((40 40, 20 45, 45 30, 40 40))," 
"((20 35, 10 30, 10 10, 30 5, 45 20, 20 35)))"), -1024);
ERROR 22S01: POINT value is a geometry of unexpected type MULTIPOLYGON in st_y.
SELECT ST_X(ST_GEOMFROMTEXT("GEOMETRYCOLLECTION("
                               "POINT(4 6),"
                               "LINESTRING(4 6,7 10),"
                               "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)),"
                               "MULTIPOINT (10 40, 40 30, 20 20, 30 10),"
                               "MULTILINESTRING ((10 10, 20 20, 10 40),"
                                 "(40 40, 30 30, 40 20, 30 10)),"
                               "MULTIPOLYGON (((30 20, 45 40, 10 40, 30 20)),"
                                 "((15 5, 40 10, 10 20, 5 10, 15 5)))"
                               ")"
                             ), 1024);
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_x.
SELECT ST_Y(ST_GEOMFROMTEXT("GEOMETRYCOLLECTION("
                               "POINT(4 6),"
                               "LINESTRING(4 6,7 10),"
                               "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)),"
                               "MULTIPOINT (10 40, 40 30, 20 20, 30 10),"
                               "MULTILINESTRING ((10 10, 20 20, 10 40),"
                                 "(40 40, 30 30, 40 20, 30 10)),"
                               "MULTIPOLYGON (((30 20, 45 40, 10 40, 30 20)),"
                                 "((15 5, 40 10, 10 20, 5 10, 15 5)))"
                               ")"
                             ), 1024);
ERROR 22S01: POINT value is a geometry of unexpected type GEOMCOLLECTION in st_y.
# Final cleanup
DROP TABLE gis_point;
