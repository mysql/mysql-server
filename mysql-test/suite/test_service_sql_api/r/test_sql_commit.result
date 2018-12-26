# Creating table t1
CREATE TABLE t1(c1 INT NOT NULL, c2 INT, PRIMARY KEY(c1));
INSERT INTO t1 VALUES (1,1), (2,2), (3,1), (4,2), (5,1), (6,2), (7,3);
##########################################
# Run plugin
##########################################
INSTALL PLUGIN test_sql_commit SONAME 'TEST_SQL_COMMIT';
##########################################
# Stop plugin
##########################################
UNINSTALL PLUGIN test_sql_commit;
##########################################
# Plugin log
##########################################
========================================================================
Test in a server thread
[srv_session_open]
------------------------------------------------------------------
EXECUTING:[35][SELECT 'first complex command' as a]
[CS_TEXT_REPRESENTATION]
[protocol init]
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
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
[CS_BINARY_REPRESENTATION]
[protocol init]
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
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[18][SET AUTOCOMMIT = 0]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[27][INSERT INTO t1 VALUES (8,4)]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 1
		[end] server status: IN_TRANS 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][]
------------------------------------------------------------------
EXECUTING:[6][COMMIT]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[18][SET AUTOCOMMIT = 1]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[23][SELECT COUNT(*) FROM t1]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: COUNT(*)
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.COUNT(*)][  1][8]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
[CS_BINARY_REPRESENTATION]
[protocol init]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: COUNT(*)
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.COUNT(*)][  1][8]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
------------------------------------------------------------------
[srv_session_close]
Follows threaded run
========================================================================
init thread
[srv_session_open]
------------------------------------------------------------------
EXECUTING:[35][SELECT 'first complex command' as a]
[CS_TEXT_REPRESENTATION]
[protocol init]
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
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
[CS_BINARY_REPRESENTATION]
[protocol init]
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
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.a][ 21][first complex command]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[18][SET AUTOCOMMIT = 0]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[27][INSERT INTO t1 VALUES (8,4)]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][]
------------------------------------------------------------------
EXECUTING:[6][COMMIT]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 0
		[end] server status: 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[18][SET AUTOCOMMIT = 1]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 0
		[meta] cols: 0
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

		[meta] no columns

		[meta] no charset

		[data] no rows

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
------------------------------------------------------------------
EXECUTING:[COM_INIT_DB][test]
------------------------------------------------------------------
EXECUTING:[23][SELECT COUNT(*) FROM t1]
[CS_TEXT_REPRESENTATION]
[protocol init]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: COUNT(*)
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.COUNT(*)][  1][8]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
[CS_BINARY_REPRESENTATION]
[protocol init]
		[meta] rows: 1
		[meta] cols: 1
		[meta] server status: 2
		[meta] server status: AUTOCOMMIT 
		[meta] warning count: 0

			[meta][field] db name: 
			[meta][field] table name: 
			[meta][field] org table name: 
			[meta][field] col name: COUNT(*)
			[meta][field] org col name: 
			[meta][field] length: 21
			[meta][field] charsetnr: 8
			[meta][field] flags: 129 (NOT_NULL BINARY )
			[meta][field] decimals: 0
			[meta][field] type: LONGLONG (8)


		[meta][charset result] number: 33
		[meta][charset result] name: utf8
		[meta][charset result] collation: utf8_general_ci
		[meta][charset result] sort order: 

		[data][.COUNT(*)][  1][8]

		[end] server status: 2
		[end] server status: AUTOCOMMIT 
		[end] warning count: 0
------------------------------------------------------------------
[srv_session_close]
deinit thread
SET AUTOCOMMIT = 1;
# Dropping the created table
DROP TABLE t1;
