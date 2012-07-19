# 
# OPTIMIZE TABLE statements
#
# Note: the output is likely to be different for the engine under test,
# in which case rdiff will be needed. Or, the output might say that
# the storage engine does not support OPTIMIZE.
#

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

--source create_table.inc
INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');
--let $table_name = t2
--source create_table.inc

INSERT INTO t1 (a,b) VALUES (3,'c'),(4,'d');
OPTIMIZE TABLE t1;
INSERT INTO t2 (a,b) VALUES (4,'d');
OPTIMIZE NO_WRITE_TO_BINLOG TABLE t2;
INSERT INTO t2 (a,b) VALUES (5,'e');
INSERT INTO t1 (a,b) VALUES (6,'f');
OPTIMIZE LOCAL TABLE t1, t2;
OPTIMIZE TABLE t1, t2;

DROP TABLE t1, t2;

# OPTIMIZE on a table with index

--let $continue = 1
--source have_default_index.inc
if ($have_default_index)
{
  --let $create_definition = a $int_indexed_col, b $char_col, $default_index (a)
  --source create_table.inc
  INSERT INTO t1 (a,b) VALUES (1,'a'),(100,'b'),(2,'c'),(3,'d');
  OPTIMIZE TABLE t1;
  DROP TABLE t1;
}


