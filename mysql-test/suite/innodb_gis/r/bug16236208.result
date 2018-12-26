DROP TABLE IF EXISTS linestring;
CREATE TABLE linestring (pk INTEGER NOT NULL PRIMARY KEY, linestring_key
GEOMETRY NOT NULL, linestring_nokey GEOMETRY NOT NULL) ENGINE=InnoDB ;
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (1,
ST_GeomFromText('POINT(10 10) '), ST_GeomFromText('POINT(10 10) '));
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (2,
ST_GeomFromText('LINESTRING(10 10,20 20,30 30)'), ST_GeomFromText('LINESTRING(10
10,20 20,30 30)'));
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (3,
ST_GeomFromText('POLYGON((0 0,5 5,10 10,15 15,0 0),(10 10,20 20,30 30,40 40,10
10))'), ST_GeomFromText('POLYGON((0 0,5 5,10 10,15 15,0 0),(10 10,20 20,30 30,40
40,10 10))'));
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (4,
ST_GeomFromText('MULTIPOINT(0 0,5 5,10 10,20 20) '), ST_GeomFromText('MULTIPOINT(0
0,5 5,10 10,20 20) '));
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (5,
ST_GeomFromText('MULTILINESTRING((1 1,2 2,3 3),(10 10,20 20,30 30))'),
ST_GeomFromText('MULTILINESTRING((1 1,2 2,3 3),(10 10,20 20,30 30))'));
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (6,
ST_GeomFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73
9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))'),
ST_GeomFromText('MULTIPOLYGON(((28 26,28 0,84 0,84 42,28 26),(52 18,66 23,73
9,48 6,52 18)),((59 18,67 18,67 13,59 13,59 18)))'));
INSERT INTO linestring (pk, linestring_key, linestring_nokey) VALUES (7,
ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))'),
ST_GeomFromText('GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0,10 10))'));
CREATE INDEX linestring_index ON linestring(linestring_nokey(5));
ERROR HY000: Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys
ALTER TABLE linestring ADD  KEY (linestring_key(5));
ERROR HY000: Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys
# Invalid polygon.
SELECT ST_AsText(linestring_nokey) FROM linestring FORCE KEY (
linestring_key ) WHERE ST_CONTAINS( ST_GeomFromText('POLYGON( ( 3923 2815 , 4246
2122 , 4028 2971 , 4017 3019 , 3923 2815 ) )') , linestring_key ) AND
ST_WITHIN( ST_GeomFromText('POLYGON( ( 4135 3009 , 4914 3087 , 4236 3194 , 4091
3036 , 4022 3004 , 4087 3004 , 3853 2825 , 3490 3118 , 3784 3141 , 4052 3072
, 4205 2787 , 4024 2951 , 4076 3036 , 4041 3053 , 3789 3401 , 4080 3100 ,
4169 2867 , 4056 3029 , 4715 3943 , 4052 3098 , 4035 3080 , 4013 3077 , 4218
2915 , 3789 2980 , 4058 3046 , 4042 3043 , 4135 3009 ) )') , linestring_nokey
) AND ST_CONTAINS( ST_GeomFromText('POLYGON( ( 4082 2842 , 3678 3647 , 3420 3068
, 4018 3030 , 4082 2842 ) )') , linestring_key ) OR ST_INTERSECTS(
ST_GeomFromText('MULTILINESTRING( ( 4147 2871 , 4072 3042 , 4081 3099 , 3796
3021 , 4007 3463 ) , ( 4042 2808 , 4109 2816 , 3730 3673 ) )') ,
linestring_key ) AND ST_Length( linestring_nokey ) > 1;
ERROR 42000: Key 'linestring_key' doesn't exist in table 'linestring'
DROP TABLE linestring;
