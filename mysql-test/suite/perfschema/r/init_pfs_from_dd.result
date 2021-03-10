#
# WL7900 -- New DD: Support flexible creation and versioning of
#           virtual P_S tables
#
CALL mtr.add_suppression("Native table .* has the wrong structure");
# Case 1:
# Restart server with the actual P_S version number differed
# from the expected version number.
# - Using a debug flag for testing.
# - Check if abandoned P_S table is removed, in case server version is
#   changed. The test case expects P_S.cond_instances to be missed.
# check that in normal mode the table P_S.cond_instances exists.
SELECT COUNT(*) FROM performance_schema.cond_instances;
COUNT(*)
#
# restart:--debug=+d,test_p_s_metadata_version
SELECT COUNT(*) FROM performance_schema.cond_instances;
ERROR 42S02: Table 'performance_schema.cond_instances' doesn't exist
# Case 2:
# Restart server should fail if P_S version is changed
# and --innodb-read-only mode is ON.
Can't open shared library component_reference_cache
Can't open shared library component_reference_cache
# restart:--debug=-d,test_p_s_metadata_version
