--source ../include/create_schema.inc

call execute_dml(0, 0, 100, 100, 10, 0);

CREATE TABLE t SELECT * FROM t1;

if ($ddl_no_table) {
  DROP table t1;
  DROP table t2;
}

if (!$ddl_no_table) {
  --let $ddl_op = CREATE TABLE
  --let $ddl_stmt = (col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)

  --eval $ddl_op t0 $ddl_stmt
  --eval $ddl_op t01 $ddl_stmt
  --eval $ddl_op t3 $ddl_stmt
  --eval $ddl_op t11 $ddl_stmt
  --eval $ddl_op t21 $ddl_stmt

  --let $ddl_op =
  --let $ddl_stmt =

  INSERT INTO t11 SELECT * FROM t;
  INSERT INTO t21 SELECT * FROM t;
}

SHOW VARIABLES LIKE 'clone_ddl_timeout';
SHOW VARIABLES LIKE 'clone_block_ddl';
