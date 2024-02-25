if ($ddl_drop_schema_tables) {
  DROP TABLE t;

  if ($ddl_recreate_schema_tables) {
    --let $ddl_op = CREATE TABLE
    --let $ddl_stmt = (col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
    --eval $ddl_op t1 $ddl_stmt
    --eval $ddl_op t2 $ddl_stmt
  }

  --source ../include/drop_schema.inc
  --let $dropped_schema_tables = 1
}

if($ddl_drop_tables) {
  DROP TABLE t0;
  DROP TABLE t01;
  if (!$dropped_schema_tables) {
    DROP TABLE t1;
    DROP TABLE t2;
  }
  DROP TABLE t3;
  DROP TABLE t11;
  DROP TABLE t21;
}

if ($ddl_drop_renamed_tables) {
  --eval DROP TABLE t0$ddl_table_extn_show
  --eval DROP TABLE t01$ddl_table_extn_show
  --eval DROP TABLE t1$ddl_table_extn_show
  --eval DROP TABLE t2$ddl_table_extn_show
  --eval DROP TABLE t3$ddl_table_extn_show
  --eval DROP TABLE t11$ddl_table_extn_show
  --eval DROP TABLE t21$ddl_table_extn_show
}
