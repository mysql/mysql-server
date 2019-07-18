#
# Bug #29557747: DEBUG-ASSERT WHILE RESTARTING SERVER POST INSTALL
#  COMPONENT
#
INSTALL COMPONENT "file://component_pfs_example_component_population";
include/assert.inc [Nothing was written to the binlog by adding a PFS table]
# Must not crash
# restart
SELECT "it's alive";
it's alive
it's alive
UNINSTALL COMPONENT "file://component_pfs_example_component_population";
include/assert.inc [Nothing was written to the binlog by removing a PFS table]
# End of 8.0 tests
