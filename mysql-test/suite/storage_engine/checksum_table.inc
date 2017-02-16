# 
# CHECKSUM TABLE statements for standard CHECKSUM properties.
# Live checksums are covered in checksum_table_live.test
# 

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

# For most engines CHECKSUM=0 option will be ignored,
# but we are setting it here for those which have it 1 by default
# (there will be another test for live checksum)

--let $table_options = CHECKSUM=0
--let $create_definition = a $int_col, b $char_col
--source create_table.inc
INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');

--let $table_name = t2
--let $table_options = CHECKSUM=0
--let $create_definition = a $int_col, b $char_col
--source create_table.inc

CHECKSUM TABLE t1; 
CHECKSUM TABLE t2, t1;
CHECKSUM TABLE t1, t2 QUICK;
CHECKSUM TABLE t1, t2 EXTENDED;

DROP TABLE t1, t2;


