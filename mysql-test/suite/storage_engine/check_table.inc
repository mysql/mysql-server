# 
# CHECK TABLE statements
#
# Note: the output is likely to be different for the engine under test,
# in which case rdiff will be needed. Or, the output might say that
# the storage engine does not support CHECK.
#

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

--let $create_definition = a $int_col, b $char_col
--source create_table.inc
INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');

--let $table_name = t2
--let $create_definition = a $int_col, b $char_col
--source create_table.inc

CHECK TABLE t1;
INSERT INTO t1 (a,b) VALUES (3,'c');
INSERT INTO t2 (a,b) VALUES (4,'d');
CHECK TABLE t1, t2 FOR UPGRADE;
INSERT INTO t2 (a,b) VALUES (5,'e');
CHECK TABLE t2 QUICK;
INSERT INTO t1 (a,b) VALUES (6,'f');
CHECK TABLE t1 FAST;
INSERT INTO t1 (a,b) VALUES (7,'g');
INSERT INTO t2 (a,b) VALUES (8,'h');
CHECK TABLE t2, t1 MEDIUM;
INSERT INTO t1 (a,b) VALUES (9,'i');
INSERT INTO t2 (a,b) VALUES (10,'j');
CHECK TABLE t1, t2 EXTENDED;
INSERT INTO t1 (a,b) VALUES (11,'k');
CHECK TABLE t1 CHANGED;

DROP TABLE t1, t2;

--let $continue = 1
--source have_default_index.inc
if ($have_default_index)
{
  --let $create_definition = a $int_indexed_col, $default_index(a)
  --source create_table.inc
  INSERT INTO t1 (a) VALUES (1),(2),(5);
  CHECK TABLE t1;
  INSERT INTO t1 (a) VALUES (6),(8),(12);
  CHECK TABLE t1 FOR UPGRADE;
  INSERT INTO t1 (a) VALUES (13),(15),(16);
  CHECK TABLE t1 QUICK;
  INSERT INTO t1 (a) VALUES (17),(120),(132);
  CHECK TABLE t1 FAST;
  INSERT INTO t1 (a) VALUES (801),(900),(7714);
  CHECK TABLE t1 MEDIUM;
  INSERT INTO t1 (a) VALUES (8760),(10023),(12000);
  CHECK TABLE t1 EXTENDED;
  INSERT INTO t1 (a) VALUES (13345),(24456),(78302),(143028);
  CHECK TABLE t1 CHANGED;
  DROP TABLE t1;
}

