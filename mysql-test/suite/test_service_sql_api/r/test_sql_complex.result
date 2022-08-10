------ Run plugin ------------------------------------------------
INSTALL PLUGIN test_sql_complex SONAME 'TEST_SQL_COMPLEX';
------ Stop plugin -----------------------------------------------
UNINSTALL PLUGIN test_sql_complex;
------ plugin log ------------------------------------------------
========================================================================
Test in a server thread
[srv_session_open]
------------------------------------------------------------------
EXECUTING:[35][SELECT 'first complex command' as a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: a
			[meta][field] org col name: 
			[meta][field] length: 63
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: a
			[meta][field] org col name: 
			[meta][field] length: 63
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[21][CREATE DATABASE test1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[8][USE test]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[343][CREATE TABLE test_inserts( a INT UNSIGNED, b VARCHAR(100), c DOUBLE, d INT, e FLOAT,f DATETIME, g DATE, h TIME, i TINYINT, k TINYINT UNSIGNED,l SMALLINT, m SMALLINT UNSIGNED, n MEDIUMINT, o MEDIUMINT UNSIGNED,p INTEGER, q INTEGER UNSIGNED, r BIGINT, s BIGINT UNSIGNED,t YEAR, u DECIMAL(5,2) UNSIGNED, v DECIMAL(5,2), PRIMARY KEY(a), INDEX(d));]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  1
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[244][INSERT INTO test_inserts VALUES (1, 'one', 1.23, -1, 11.2323, '2014-07-06 07:06:05', '1980-02-19', '-830:12:23', 127, 255, 32767, 65535, 8388607, 16777215, 2147483647, 4294967295, 9223372036854775807, 18446744073709551615,1901, 999.99, -999.99)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[236][INSERT INTO test_inserts VALUES (2, 'two', 2.34, -2, 22.3434, '2015-07-06 21:22:23', '2014-06-05', '356:22:33', -128, 0, -32768, 32768, -8388608, 8388607, -2147483648, 0, -9223372036854775808, 18446744073709551615,2039, 123.45, -543.21)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[179][INSERT INTO test_inserts VALUES (3, 'three',3.45,-3, 33.4545, '2016-09-12 11:12:13', '2013-05-04', '821:33:44', -1, 128, -1, 65534, -1, 16777214, 1, 2, 3, 4,2155, 222.22, -567.89)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[43][SELECT * FROM test1.test_inserts ORDER BY a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 3
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][2]
		[data][test_inserts.b][  3][two]
		[data][test_inserts.c][  4][2.34]
		[data][test_inserts.d][  2][-2]
		[data][test_inserts.e][  7][22.3434]
		[data][test_inserts.f][ 19][2015-07-06 21:22:23]
		[data][test_inserts.g][ 10][2014-06-05]
		[data][test_inserts.h][  9][356:22:33]
		[data][test_inserts.i][  4][-128]
		[data][test_inserts.k][  1][0]
		[data][test_inserts.l][  6][-32768]
		[data][test_inserts.m][  5][32768]
		[data][test_inserts.n][  8][-8388608]
		[data][test_inserts.o][  7][8388607]
		[data][test_inserts.p][ 11][-2147483648]
		[data][test_inserts.q][  1][0]
		[data][test_inserts.r][ 20][-9223372036854775808]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][2039]
		[data][test_inserts.u][  6][123.45]
		[data][test_inserts.v][  7][-543.21]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 3
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][2]
		[data][test_inserts.b][  3][two]
		[data][test_inserts.c][  4][2.34]
		[data][test_inserts.d][  2][-2]
		[data][test_inserts.e][  7][22.3434]
		[data][test_inserts.f][ 19][2015-07-06 21:22:23]
		[data][test_inserts.g][ 10][2014-06-05]
		[data][test_inserts.h][  9][356:22:33]
		[data][test_inserts.i][  4][-128]
		[data][test_inserts.k][  1][0]
		[data][test_inserts.l][  6][-32768]
		[data][test_inserts.m][  5][32768]
		[data][test_inserts.n][  8][-8388608]
		[data][test_inserts.o][  7][8388607]
		[data][test_inserts.p][ 11][-2147483648]
		[data][test_inserts.q][  1][0]
		[data][test_inserts.r][ 20][-9223372036854775808]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][2039]
		[data][test_inserts.u][  6][123.45]
		[data][test_inserts.v][  7][-543.21]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[34][DELETE FROM test_inserts WHERE a=2]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[37][SELECT * FROM test_inserts ORDER BY a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 2
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 2
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[21][TRUNCATE test_inserts]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[37][SELECT * FROM test_inserts ORDER BY a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[27][PREPARE ps1 FROM 'select 1']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: Statement prepared
------------------------------------------------------------------
EXECUTING:[11][EXECUTE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: 1
			[meta][field] org col name: 
			[meta][field] length: 2
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.1][  1][1]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[22][DEALLOCATE PREPARE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[24][CREATE TABLE tbl (a INT)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[55][PREPARE ps1 FROM 'INSERT INTO tbl VALUES (1), (2), (3)']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: Statement prepared
------------------------------------------------------------------
EXECUTING:[11][EXECUTE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Records: 3  Duplicates: 0  Warnings: 0
------------------------------------------------------------------
EXECUTING:[22][DEALLOCATE PREPARE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[46][SELECT IF(SUM(a)=6, 'OK:)', 'FAIL:(') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(a)=6, 'OK:)', 'FAIL:(')
			[meta][field] org col name: 
			[meta][field] length: 18
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(a)=6, 'OK:)', 'FAIL:(')][  4][OK:)]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[22][DEALLOCATE PREPARE ps1]
[CS_TEXT_REPRESENTATION]
[1243][HY000][Unknown prepared statement handler (ps1) given to DEALLOCATE PREPARE]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1243][HY000][Unknown prepared statement handler (ps1) given to DEALLOCATE PREPARE]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[7][garbage]
[CS_TEXT_REPRESENTATION]
[1064][42000][You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'garbage' at line 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1064][42000][You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'garbage' at line 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[17][SELECT b FROM tbl]
[CS_TEXT_REPRESENTATION]
[1054][42S22][Unknown column 'b' in 'field list']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1054][42S22][Unknown column 'b' in 'field list']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[43][ALTER USER bogus@remotehost PASSWORD EXPIRE]
[CS_TEXT_REPRESENTATION]
[1396][HY000][Operation ALTER USER failed for 'bogus'@'remotehost']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1396][HY000][Operation ALTER USER failed for 'bogus'@'remotehost']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[26][CREATE TABLE tbld (d TIME)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[36][INSERT INTO tbld VALUES ('43141231')]
[CS_TEXT_REPRESENTATION]
[1292][22007][Incorrect time value: '43141231' for column 'd' at row 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1292][22007][Incorrect time value: '43141231' for column 'd' at row 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[10][SELECT 1/0]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: 1/0
			[meta][field] org col name: 
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 4
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.1/0][  6][[NULL]]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  1
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=5]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[17][START TRANSACTION]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 3
		[end] server status: IN_TRANS AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=4]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 35
		[end] server status: IN_TRANS AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[8][ROLLBACK]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(a) = 15, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(a) = 15, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(a) = 15, 'OK', 'FAIL')][  2][OK]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[17][START TRANSACTION]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 3
		[end] server status: IN_TRANS AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=4]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 35
		[end] server status: IN_TRANS AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[6][COMMIT]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(a) = 12, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(a) = 12, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(a) = 12, 'OK', 'FAIL')][  2][OK]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[27][START TRANSACTION READ ONLY]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 8195
		[end] server status: IN_TRANS AUTOCOMMIT IN_TRANS_READONLY 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=2]
[CS_TEXT_REPRESENTATION]
[1792][25006][Cannot execute statement in a READ ONLY transaction.]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[6][COMMIT]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(4) = 12, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(4) = 12, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(4) = 12, 'OK', 'FAIL')][  2][OK]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[16][SET autocommit=0]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=2]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[8][ROLLBACK]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(4) = 12, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 33
		[meta] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(4) = 12, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(4) = 12, 'OK', 'FAIL')][  2][OK]

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[35][set @a=((2) in (select a from tbl))]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[9][SELECT @a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @a
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@a][  1][0]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @a
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@a][  1][0]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[9][set @b=42]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[9][SELECT @b]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @b
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@b][  2][42]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @b
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@b][  2][42]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[20][SELECT @non_existing]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @non_existing
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@non_existing][  6][[NULL]]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @non_existing
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@non_existing][  6][[NULL]]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[0][]
[CS_TEXT_REPRESENTATION]
[1065][42000][Query was empty]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1065][42000][Query was empty]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[14][DROP TABLE tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[15][DROP TABLE tbld]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[19][DROP DATABASE test1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 256
		[end] server status: DB_DROPPED 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
[srv_session_close]
Follows threaded run
========================================================================
init thread
[srv_session_open]
------------------------------------------------------------------
EXECUTING:[35][SELECT 'first complex command' as a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: a
			[meta][field] org col name: 
			[meta][field] length: 63
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: a
			[meta][field] org col name: 
			[meta][field] length: 63
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[21][CREATE DATABASE test1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[8][USE test]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[343][CREATE TABLE test_inserts( a INT UNSIGNED, b VARCHAR(100), c DOUBLE, d INT, e FLOAT,f DATETIME, g DATE, h TIME, i TINYINT, k TINYINT UNSIGNED,l SMALLINT, m SMALLINT UNSIGNED, n MEDIUMINT, o MEDIUMINT UNSIGNED,p INTEGER, q INTEGER UNSIGNED, r BIGINT, s BIGINT UNSIGNED,t YEAR, u DECIMAL(5,2) UNSIGNED, v DECIMAL(5,2), PRIMARY KEY(a), INDEX(d));]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  1
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[244][INSERT INTO test_inserts VALUES (1, 'one', 1.23, -1, 11.2323, '2014-07-06 07:06:05', '1980-02-19', '-830:12:23', 127, 255, 32767, 65535, 8388607, 16777215, 2147483647, 4294967295, 9223372036854775807, 18446744073709551615,1901, 999.99, -999.99)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[236][INSERT INTO test_inserts VALUES (2, 'two', 2.34, -2, 22.3434, '2015-07-06 21:22:23', '2014-06-05', '356:22:33', -128, 0, -32768, 32768, -8388608, 8388607, -2147483648, 0, -9223372036854775808, 18446744073709551615,2039, 123.45, -543.21)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[179][INSERT INTO test_inserts VALUES (3, 'three',3.45,-3, 33.4545, '2016-09-12 11:12:13', '2013-05-04', '821:33:44', -1, 128, -1, 65534, -1, 16777214, 1, 2, 3, 4,2155, 222.22, -567.89)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[43][SELECT * FROM test1.test_inserts ORDER BY a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 3
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][2]
		[data][test_inserts.b][  3][two]
		[data][test_inserts.c][  4][2.34]
		[data][test_inserts.d][  2][-2]
		[data][test_inserts.e][  7][22.3434]
		[data][test_inserts.f][ 19][2015-07-06 21:22:23]
		[data][test_inserts.g][ 10][2014-06-05]
		[data][test_inserts.h][  9][356:22:33]
		[data][test_inserts.i][  4][-128]
		[data][test_inserts.k][  1][0]
		[data][test_inserts.l][  6][-32768]
		[data][test_inserts.m][  5][32768]
		[data][test_inserts.n][  8][-8388608]
		[data][test_inserts.o][  7][8388607]
		[data][test_inserts.p][ 11][-2147483648]
		[data][test_inserts.q][  1][0]
		[data][test_inserts.r][ 20][-9223372036854775808]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][2039]
		[data][test_inserts.u][  6][123.45]
		[data][test_inserts.v][  7][-543.21]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 3
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][2]
		[data][test_inserts.b][  3][two]
		[data][test_inserts.c][  4][2.34]
		[data][test_inserts.d][  2][-2]
		[data][test_inserts.e][  7][22.3434]
		[data][test_inserts.f][ 19][2015-07-06 21:22:23]
		[data][test_inserts.g][ 10][2014-06-05]
		[data][test_inserts.h][  9][356:22:33]
		[data][test_inserts.i][  4][-128]
		[data][test_inserts.k][  1][0]
		[data][test_inserts.l][  6][-32768]
		[data][test_inserts.m][  5][32768]
		[data][test_inserts.n][  8][-8388608]
		[data][test_inserts.o][  7][8388607]
		[data][test_inserts.p][ 11][-2147483648]
		[data][test_inserts.q][  1][0]
		[data][test_inserts.r][ 20][-9223372036854775808]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][2039]
		[data][test_inserts.u][  6][123.45]
		[data][test_inserts.v][  7][-543.21]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[34][DELETE FROM test_inserts WHERE a=2]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[37][SELECT * FROM test_inserts ORDER BY a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 2
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 2
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][test_inserts.a][  1][1]
		[data][test_inserts.b][  3][one]
		[data][test_inserts.c][  4][1.23]
		[data][test_inserts.d][  2][-1]
		[data][test_inserts.e][  7][11.2323]
		[data][test_inserts.f][ 19][2014-07-06 07:06:05]
		[data][test_inserts.g][ 10][1980-02-19]
		[data][test_inserts.h][ 10][-830:12:23]
		[data][test_inserts.i][  3][127]
		[data][test_inserts.k][  3][255]
		[data][test_inserts.l][  5][32767]
		[data][test_inserts.m][  5][65535]
		[data][test_inserts.n][  7][8388607]
		[data][test_inserts.o][  8][16777215]
		[data][test_inserts.p][ 10][2147483647]
		[data][test_inserts.q][ 10][4294967295]
		[data][test_inserts.r][ 19][9223372036854775807]
		[data][test_inserts.s][ 20][18446744073709551615]
		[data][test_inserts.t][  4][1901]
		[data][test_inserts.u][  6][999.99]
		[data][test_inserts.v][  7][-999.99]

		[data][test_inserts.a][  1][3]
		[data][test_inserts.b][  5][three]
		[data][test_inserts.c][  4][3.45]
		[data][test_inserts.d][  2][-3]
		[data][test_inserts.e][  7][33.4545]
		[data][test_inserts.f][ 19][2016-09-12 11:12:13]
		[data][test_inserts.g][ 10][2013-05-04]
		[data][test_inserts.h][  9][821:33:44]
		[data][test_inserts.i][  2][-1]
		[data][test_inserts.k][  3][128]
		[data][test_inserts.l][  2][-1]
		[data][test_inserts.m][  5][65534]
		[data][test_inserts.n][  2][-1]
		[data][test_inserts.o][  8][16777214]
		[data][test_inserts.p][  1][1]
		[data][test_inserts.q][  1][2]
		[data][test_inserts.r][  1][3]
		[data][test_inserts.s][  1][4]
		[data][test_inserts.t][  4][2155]
		[data][test_inserts.u][  6][222.22]
		[data][test_inserts.v][  7][-567.89]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[21][TRUNCATE test_inserts]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[37][SELECT * FROM test_inserts ORDER BY a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 21
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 20515 (NOT_NULL PRI_KEY UNSIGNED NO_DEFAULT_VALUE PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 400
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: c
			[meta][field] org col name: c
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: DOUBLE (5)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: d
			[meta][field] org col name: d
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 16392 (MULTIPLE_KEY PART_KEY )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: e
			[meta][field] org col name: e
			[meta][field] length: 12
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: FLOAT (4)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: f
			[meta][field] org col name: f
			[meta][field] length: 19
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATETIME (12)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: g
			[meta][field] org col name: g
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: DATE (10)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: h
			[meta][field] org col name: h
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: TIME (11)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: i
			[meta][field] org col name: i
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: k
			[meta][field] org col name: k
			[meta][field] length: 3
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: TINY (1)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: l
			[meta][field] org col name: l
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: m
			[meta][field] org col name: m
			[meta][field] length: 5
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: SHORT (2)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: n
			[meta][field] org col name: n
			[meta][field] length: 9
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: o
			[meta][field] org col name: o
			[meta][field] length: 8
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: INT24 (9)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: p
			[meta][field] org col name: p
			[meta][field] length: 11
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: q
			[meta][field] org col name: q
			[meta][field] length: 10
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONG (3)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: r
			[meta][field] org col name: r
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: s
			[meta][field] org col name: s
			[meta][field] length: 20
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: t
			[meta][field] org col name: t
			[meta][field] length: 4
			[meta][field] charsetnr: 8
			[meta][field] flags: 96 (UNSIGNED ZEROFILL )
			[meta][field] decimals: 0
			[meta][field] type: YEAR (13)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: u
			[meta][field] org col name: u
			[meta][field] length: 6
			[meta][field] charsetnr: 8
			[meta][field] flags: 32 (UNSIGNED )
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)

			[meta][field] db name: test1
			[meta][field] table name: test_inserts
			[meta][field] org table name: test_inserts
			[meta][field] col name: v
			[meta][field] org col name: v
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 0
			[meta][field] decimals: 2
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[27][PREPARE ps1 FROM 'select 1']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: Statement prepared
------------------------------------------------------------------
EXECUTING:[11][EXECUTE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: 1
			[meta][field] org col name: 
			[meta][field] length: 2
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.1][  1][1]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[22][DEALLOCATE PREPARE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[24][CREATE TABLE tbl (a INT)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[55][PREPARE ps1 FROM 'INSERT INTO tbl VALUES (1), (2), (3)']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: Statement prepared
------------------------------------------------------------------
EXECUTING:[11][EXECUTE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Records: 3  Duplicates: 0  Warnings: 0
------------------------------------------------------------------
EXECUTING:[22][DEALLOCATE PREPARE ps1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[46][SELECT IF(SUM(a)=6, 'OK:)', 'FAIL:(') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(a)=6, 'OK:)', 'FAIL:(')
			[meta][field] org col name: 
			[meta][field] length: 18
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(a)=6, 'OK:)', 'FAIL:(')][  4][OK:)]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[22][DEALLOCATE PREPARE ps1]
[CS_TEXT_REPRESENTATION]
[1243][HY000][Unknown prepared statement handler (ps1) given to DEALLOCATE PREPARE]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1243][HY000][Unknown prepared statement handler (ps1) given to DEALLOCATE PREPARE]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[7][garbage]
[CS_TEXT_REPRESENTATION]
[1064][42000][You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'garbage' at line 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1064][42000][You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'garbage' at line 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[17][SELECT b FROM tbl]
[CS_TEXT_REPRESENTATION]
[1054][42S22][Unknown column 'b' in 'field list']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1054][42S22][Unknown column 'b' in 'field list']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[43][ALTER USER bogus@remotehost PASSWORD EXPIRE]
[CS_TEXT_REPRESENTATION]
[1396][HY000][Operation ALTER USER failed for 'bogus'@'remotehost']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1396][HY000][Operation ALTER USER failed for 'bogus'@'remotehost']		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[26][CREATE TABLE tbld (d TIME)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[36][INSERT INTO tbld VALUES ('43141231')]
[CS_TEXT_REPRESENTATION]
[1292][22007][Incorrect time value: '43141231' for column 'd' at row 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1292][22007][Incorrect time value: '43141231' for column 'd' at row 1]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[10][SELECT 1/0]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: 1/0
			[meta][field] org col name: 
			[meta][field] length: 7
			[meta][field] charsetnr: 8
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 4
			[meta][field] type: NEWDECIMAL (246)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.1/0][  6][[NULL]]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  1
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=5]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[17][START TRANSACTION]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 3
		[end] server status: IN_TRANS AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=4]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 35
		[end] server status: IN_TRANS AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[8][ROLLBACK]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(a) = 15, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(a) = 15, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(a) = 15, 'OK', 'FAIL')][  2][OK]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[17][START TRANSACTION]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 3
		[end] server status: IN_TRANS AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=4]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 35
		[end] server status: IN_TRANS AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[6][COMMIT]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(a) = 12, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(a) = 12, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(a) = 12, 'OK', 'FAIL')][  2][OK]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[27][START TRANSACTION READ ONLY]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 8195
		[end] server status: IN_TRANS AUTOCOMMIT IN_TRANS_READONLY 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=2]
[CS_TEXT_REPRESENTATION]
[1792][25006][Cannot execute statement in a READ ONLY transaction.]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[6][COMMIT]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(4) = 12, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(4) = 12, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(4) = 12, 'OK', 'FAIL')][  2][OK]

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[16][SET autocommit=0]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[18][UPDATE tbl SET a=2]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  3
		[end] last insert id: 0
		[end] message: Rows matched: 3  Changed: 3  Warnings: 0
------------------------------------------------------------------
EXECUTING:[8][ROLLBACK]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[45][SELECT IF(SUM(4) = 12, 'OK', 'FAIL') FROM tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 33
		[meta] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: IF(SUM(4) = 12, 'OK', 'FAIL')
			[meta][field] org col name: 
			[meta][field] length: 12
			[meta][field] charsetnr: 33
			[meta][field] flags: 1 (NOT_NULL )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.IF(SUM(4) = 12, 'OK', 'FAIL')][  2][OK]

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[35][set @a=((2) in (select a from tbl))]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 33
		[end] server status: IN_TRANS QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[9][SELECT @a]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @a
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@a][  1][0]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @a
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@a][  1][0]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[9][set @b=42]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[9][SELECT @b]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @b
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@b][  2][42]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @b
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@b][  2][42]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[20][SELECT @non_existing]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @non_existing
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@non_existing][  6][[NULL]]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 1
		[meta] server status: IN_TRANS 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @non_existing
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 128 (BINARY )
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@non_existing][  6][[NULL]]

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[0][]
[CS_TEXT_REPRESENTATION]
[1065][42000][Query was empty]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
[1065][42000][Query was empty]		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[14][DROP TABLE tbl]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[15][DROP TABLE tbld]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[19][DROP DATABASE test1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 0
		[meta] server status: 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 256
		[end] server status: DB_DROPPED 
		[end] warning count:  0
		[end] affected rows:  1
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
[srv_session_close]
deinit thread
------ cleanup ---------------------------------------------------
