SET @sql_mode_global = @@global.sql_mode;
SET @sql_mode_session = @@session.sql_mode;
CREATE TABLE t1(a varchar(20) not null, b varchar(20));
CREATE TABLE t2(a varchar(20) null, b varchar(20));
INSERT INTO t1 VALUES('t1a1','t1b1');
INSERT INTO t1 VALUES('t1a2','t1b2');
INSERT INTO t1 VALUES('t1a3','t1b3');
INSERT INTO t1 VALUES('t1a4','t1b4');
INSERT INTO t2 VALUES('t2a1','t2b1');
INSERT INTO t2 VALUES('t2a2','t2b2');
INSERT INTO t2 VALUES('t2a3','t2b3');
INSERT INTO t2 VALUES('t2a4','t2b4');
SET @@global.sql_mode = NO_UNSIGNED_SUBTRACTION;
SELECT @@global.sql_mode;
@@global.sql_mode
NO_UNSIGNED_SUBTRACTION
SELECT @@session.sql_mode;
@@session.sql_mode
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
##########################################
# Run plugin
##########################################
INSTALL PLUGIN test_sql_sqlmode SONAME 'TEST_SQL_SQLMODE';
##########################################
# Stop plugin
##########################################
UNINSTALL PLUGIN test_sql_sqlmode;
##########################################
# Plugin log
##########################################
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[42][SET @@session.sql_mode = @@global.sql_mode]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[30][SELECT CAST(0 AS UNSIGNED) - 1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: CAST(0 AS UNSIGNED) - 1
			[meta][field] org col name: 
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.CAST(0 AS UNSIGNED) - 1][  2][-1]

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
			[meta][field] col name: CAST(0 AS UNSIGNED) - 1
			[meta][field] org col name: 
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.CAST(0 AS UNSIGNED) - 1][  2][-1]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[36][SET @@global.sql_mode = NO_ZERO_DATE]
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
EXECUTING:[37][SET @@session.sql_mode = NO_ZERO_DATE]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 12][NO_ZERO_DATE]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 12][NO_ZERO_DATE]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 12][NO_ZERO_DATE]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 12][NO_ZERO_DATE]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[31][SET @@global.sql_mode = DEFAULT]
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
EXECUTING:[32][SET @@session.sql_mode = DEFAULT]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[57][SELECT * FROM t1 WHERE t1.a IN (SELECT MAX(t1.b) FROM t2)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[29][SET @@session.sql_mode = ANSI]
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
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 78][REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 78][REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[57][SELECT * FROM t1 WHERE t1.a IN (SELECT MAX(t1.b) FROM t2)]
[CS_TEXT_REPRESENTATION]
[1111][HY000][Invalid use of group function]
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
[CS_BINARY_REPRESENTATION]
[1111][HY000][Invalid use of group function]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[44][SET @@session.sql_mode = STRICT_TRANS_TABLES]
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
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 19][STRICT_TRANS_TABLES]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 19][STRICT_TRANS_TABLES]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[34][INSERT INTO t2 VALUES(NULL,'t2b1')]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[31][INSERT INTO t1 SELECT * FROM t2]
[CS_TEXT_REPRESENTATION]
[1048][23000][Column 'a' cannot be null]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[33][SELECT * FROM t1 WHERE a = 't2a2']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[36][SET @@session.sql_mode = TRADITIONAL]
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
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][128][STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][128][STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[34][INSERT INTO t2 VALUES(NULL,'t2b1')]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[31][INSERT INTO t1 SELECT * FROM t2]
[CS_TEXT_REPRESENTATION]
[1048][23000][Column 'a' cannot be null]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[33][SELECT * FROM t1 WHERE a = 't2a2']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[47][SET @@global.sql_mode = NO_UNSIGNED_SUBTRACTION]
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
EXECUTING:[48][SET @@session.sql_mode = NO_UNSIGNED_SUBTRACTION]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[42][SET @@session.sql_mode = @@global.sql_mode]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 23][NO_UNSIGNED_SUBTRACTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[30][SELECT CAST(0 AS UNSIGNED) - 1]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: CAST(0 AS UNSIGNED) - 1
			[meta][field] org col name: 
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.CAST(0 AS UNSIGNED) - 1][  2][-1]

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
			[meta][field] col name: CAST(0 AS UNSIGNED) - 1
			[meta][field] org col name: 
			[meta][field] length: 22
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.CAST(0 AS UNSIGNED) - 1][  2][-1]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[36][SET @@global.sql_mode = NO_ZERO_DATE]
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
EXECUTING:[37][SET @@session.sql_mode = NO_ZERO_DATE]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 12][NO_ZERO_DATE]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][ 12][NO_ZERO_DATE]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 12][NO_ZERO_DATE]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 12][NO_ZERO_DATE]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[31][SET @@global.sql_mode = DEFAULT]
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
EXECUTING:[32][SET @@session.sql_mode = DEFAULT]
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
EXECUTING:[24][SELECT @@global.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

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
			[meta][field] col name: @@global.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@global.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][117][ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[57][SELECT * FROM t1 WHERE t1.a IN (SELECT MAX(t1.b) FROM t2)]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[29][SET @@session.sql_mode = ANSI]
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
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 78][REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 78][REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[57][SELECT * FROM t1 WHERE t1.a IN (SELECT MAX(t1.b) FROM t2)]
[CS_TEXT_REPRESENTATION]
[1111][HY000][Invalid use of group function]
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
[CS_BINARY_REPRESENTATION]
[1111][HY000][Invalid use of group function]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[44][SET @@session.sql_mode = STRICT_TRANS_TABLES]
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
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 19][STRICT_TRANS_TABLES]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][ 19][STRICT_TRANS_TABLES]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[34][INSERT INTO t2 VALUES(NULL,'t2b1')]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[31][INSERT INTO t1 SELECT * FROM t2]
[CS_TEXT_REPRESENTATION]
[1048][23000][Column 'a' cannot be null]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[33][SELECT * FROM t1 WHERE a = 't2a2']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[36][SET @@session.sql_mode = TRADITIONAL]
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
EXECUTING:[25][SELECT @@session.sql_mode]
[CS_TEXT_REPRESENTATION]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][128][STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION]

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
			[meta][field] col name: @@session.sql_mode
			[meta][field] org col name: 
			[meta][field] length: 65535
			[meta][field] charsetnr: 33
			[meta][field] flags: 0
			[meta][field] decimals: 31
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data][.@@session.sql_mode][128][STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[34][INSERT INTO t2 VALUES(NULL,'t2b1')]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[31][INSERT INTO t1 SELECT * FROM t2]
[CS_TEXT_REPRESENTATION]
[1048][23000][Column 'a' cannot be null]
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
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[33][SELECT * FROM t1 WHERE a = 't2a2']
[CS_TEXT_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
[CS_BINARY_REPRESENTATION]
		[meta] rows: 0
		[meta] cols: 2
		[meta] server status: 34
		[meta] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[meta] warning count: 0

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: a
			[meta][field] org col name: a
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 4097 (NOT_NULL NO_DEFAULT_VALUE )
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)

			[meta][field] db name: test
			[meta][field] table name: t1
			[meta][field] org table name: t1
			[meta][field] col name: b
			[meta][field] org col name: b
			[meta][field] length: 80
			[meta][field] charsetnr: 255
			[meta][field] flags: 0
			[meta][field] decimals: 0
			[meta][field] type: VARCHAR (15)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8mb3
		[meta][charset result] collation: utf8mb3_general_ci
		[meta][charset result] sort order: 

		[data] no rows

		[end] server status: 34
		[end] server status: AUTOCOMMIT QUERY_NO_INDEX_USED 
		[end] warning count:  0
		[end] affected rows:  0
		[end] last insert id: 0
		[end] message: 
------------------------------------------------------------------
EXECUTING:[47][SET @@global.sql_mode = NO_UNSIGNED_SUBTRACTION]
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
EXECUTING:[48][SET @@session.sql_mode = NO_UNSIGNED_SUBTRACTION]
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
[srv_session_close]
deinit thread
SELECT @@global.sql_mode;
@@global.sql_mode
NO_UNSIGNED_SUBTRACTION
SELECT @@session.sql_mode;
@@session.sql_mode
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
SET @@session.sql_mode = @@global.sql_mode;
SELECT @@session.sql_mode;
@@session.sql_mode
NO_UNSIGNED_SUBTRACTION
# SQL_MODE : NO_UNSIGNED_SUBTRACTION(Checking whether subtraction result is signed)
SELECT CAST(0 AS UNSIGNED) - 1;
CAST(0 AS UNSIGNED) - 1
-1
##########################################
# Cleanup
##########################################
# Dropping the created tables
DROP TABLE t1;
DROP TABLE t2;
SET @@global.sql_mode = @sql_mode_global;
SET @@session.sql_mode = @sql_mode_session;
