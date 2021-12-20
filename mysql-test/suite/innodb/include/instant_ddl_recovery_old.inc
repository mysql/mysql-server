##########################################################################
# Test script to test RECOVERY of a crashed server in older release with
# table having INSTANT ADD columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo #########
--echo # SETUP #
--echo #########

--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc

eval SELECT * FROM t1_$row_format;

--echo ###########
--echo # CLEANUP #
--echo ###########
eval DROP TABLE t1_$row_format;
