# ==== Purpose ====
#
# Clean up what include/gtid_utils.inc created.
#
# ==== Usage ====
#
# See include/gtid_utils.inc

--let $include_filename= gtid_utils_end.inc
--source include/begin_include_file.inc

--disable_query_log

DROP FUNCTION GTID_IS_EQUAL;
DROP FUNCTION GTID_IS_DISJOINT;
DROP FUNCTION GTID_IS_DISJOINT_UNION;
DROP FUNCTION GTID_UNION;
DROP FUNCTION GTID_INTERSECT;
DROP FUNCTION GTID_SYMMETRIC_DIFFERENCE;
DROP FUNCTION GTID_SUBTRACT_UUID;
DROP FUNCTION GTID_INTERSECT_UUID;
DROP FUNCTION GTID_COMPARE;
DROP FUNCTION NUMBER_TO_UUID;
DROP FUNCTION UUID_TO_NUMBER;

--let $include_filename= gtid_utils.inc
--source include/end_include_file.inc
