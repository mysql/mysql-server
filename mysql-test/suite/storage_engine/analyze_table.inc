# 
# ANALYZE TABLE statements
#
# Note: the output is likely to be different for the engine under test,
# in which case rdiff will be needed. Or, the output might say that
# the storage engine does not support ANALYZE.
#

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

--let $create_definition = a $int_col, b $char_col
--source create_table.inc
INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');
--let $create_definition = a $int_col, b $char_col
--let $table_name = t2
--source create_table.inc

INSERT INTO t1 (a,b) VALUES (3,'c');
ANALYZE TABLE t1;
INSERT INTO t2 (a,b) VALUES (4,'d');
ANALYZE NO_WRITE_TO_BINLOG TABLE t2;
INSERT INTO t1 (a,b) VALUES (5,'e');
INSERT INTO t2 (a,b) VALUES (6,'f');
ANALYZE LOCAL TABLE t1, t2;

DROP TABLE t1, t2;

--let $continue = 1
--source have_default_index.inc

if ($have_default_index)
{
  --let $create_definition = a $int_indexed_col, $default_index(a)
  --source create_table.inc
  INSERT INTO t1 (a) VALUES (1),(2),(4),(7);
  ANALYZE TABLE t1;
  INSERT INTO t1 (a) VALUES (8),(10),(11),(12);
  ANALYZE TABLE t1;
  DROP TABLE t1;
}

