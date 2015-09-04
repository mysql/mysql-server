CREATE TABLE t_int (c1  TINYINT,
c2  SMALLINT,
c3  MEDIUMINT,
c4  INT,
c5  INTEGER,
c6  SMALLINT UNSIGNED,
c7  MEDIUMINT UNSIGNED,
c8  INT UNSIGNED,
c9  INTEGER);
INSERT INTO t_int VALUES (91,92,93,94,95,96,97,98,99);
INSERT INTO t_int VALUES (81,82,83,84,85,86,87,88,89);
INSERT INTO t_int VALUES (71,72,73,74,75,76,77,78,79);
SELECT * FROM t_int order by c1;
c1	c2	c3	c4	c5	c6	c7	c8	c9
71	72	73	74	75	76	77	78	79
81	82	83	84	85	86	87	88	89
91	92	93	94	95	96	97	98	99
##########################################
# Run plugin
##########################################
INSTALL PLUGIN test_session_detach SONAME 'TEST_SESSION_DETACH';
##########################################
# Stop plugin
##########################################
UNINSTALL PLUGIN test_session_detach;
##########################################
# Plugin log
##########################################
=========================================================================
Test in a server thread
Opening Session 1
=========================================================================

Query 01: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 02: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 03: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 04: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 05: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1
=========================================================================
Close Session 1
Follows threaded run
=========================================================================
init thread
Opening Session 1
=========================================================================

Query 01: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 02: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 03: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 04: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1

Query 05: SELECT * FROM test.t_int ORDER BY c1
num_cols : 9
nb rows  : 3
c1 TINY(1)	c2 SHORT(2)	c3 INT24(9)	c4 LONG(3)	c5 LONG(3)	c6 SHORT(2)	c7 INT24(9)	c8 LONG(3)	c9 LONG(3)	
71		72		73		74		75		76		77		78		79		
81		82		83		84		85		86		87		88		89		
91		92		93		94		95		96		97		98		99		

affected rows : 0
server status : 34
warn count    : 0

Detach Session 1
=========================================================================
Close Session 1
deinit thread
##########################################
# Cleanup
##########################################
DROP TABLE IF EXISTS t_int;
