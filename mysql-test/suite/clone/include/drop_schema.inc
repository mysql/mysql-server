## Drop test schema
DROP TABLE t1;
DROP TABLE t2;

DROP PROCEDURE execute_dml;

if ($clone_ddl) {
  DROP PROCEDURE execute_ddl;
}
