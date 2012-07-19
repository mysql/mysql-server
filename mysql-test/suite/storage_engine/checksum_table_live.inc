# 
# CHECKSUM TABLE statements for live CHECKSUM.
#
# Note: the feature is likely to be unsupported, in which case
# instead of numeric values some CHECKSUMs will produce NULL
# 

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

# For most engines CHECKSUM=1 option will be ignored,
# and the results will be different

--let $table_options = CHECKSUM=1
--let $create_definition = a $int_col, b $char_col
--source create_table.inc
INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');
--let $table_name = t2
--let $table_options = CHECKSUM=1
--let $create_definition = a $int_col, b $char_col
--source create_table.inc

CHECKSUM TABLE t1; 
CHECKSUM TABLE t2, t1;
CHECKSUM TABLE t1, t2 QUICK;
CHECKSUM TABLE t1, t2 EXTENDED;

DROP TABLE t1, t2;

