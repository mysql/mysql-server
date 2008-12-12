** Setup **

SET @original_sql_buffer_result = @@sql_buffer_result;
CREATE TEMPORARY TABLE t1(a varchar(20), b varchar(20));
INSERT INTO t1 VALUES('aa','bb');
INSERT INTO t1 VALUES('aa','bb');
INSERT INTO t1 VALUES('aa','bb');
INSERT INTO t1 VALUES('aa','bb');
INSERT INTO t1 VALUES('aa','bb');
'#--------------------FN_DYNVARS_156_01-------------------------#'
SET SESSION sql_buffer_result = 1;
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	0
Expected value : 0.
SELECT * FROM t1;
a	b
aa	bb
aa	bb
aa	bb
aa	bb
aa	bb
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	1
Expected value : 1.
SELECT * FROM t1;
a	b
aa	bb
aa	bb
aa	bb
aa	bb
aa	bb
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	2
Expected value : 2.
'#--------------------FN_DYNVARS_156_02-------------------------#'
SET SESSION sql_buffer_result = 0;
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	2
Expected value : 2.
SELECT * FROM t1;
a	b
aa	bb
aa	bb
aa	bb
aa	bb
aa	bb
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	2
Expected value : 2.
'#--------------------FN_DYNVARS_156_03-------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_buffer_result;
@@SESSION.sql_buffer_result
0
0 / FALSE Expected
SET SESSION sql_buffer_result = FALSE;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_buffer_result;
@@SESSION.sql_buffer_result
0
0 / FALSE Expected
SET SESSION sql_buffer_result = TRUE;
** Connection con_int1 **
SELECT @@SESSION.sql_buffer_result;
@@SESSION.sql_buffer_result
0
0 / FALSE Expected
** Connection con_int2 **
SELECT @@SESSION.sql_buffer_result;
@@SESSION.sql_buffer_result
1
1 / TRUE Expected
** Connection default **
Disconnecting Connections con_int1, con_int2
** Cleanup **
SET @@sql_buffer_result = @original_sql_buffer_result;
DROP TABLE t1;
