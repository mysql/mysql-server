#
# This test validates that the iterator will return a graceful
# error if the binary log is disabled, instead of asserting.
#

call mtr.add_suppression("\\[Warning\\] \\[[^]]*\\] \\[[^]]*\\] You need to use --log-bin to make --binlog-format work.");

--source include/install_replication_observers_example.inc

--error ER_WRONG_PERFSCHEMA_USAGE
SELECT * FROM performance_schema.binlog_storage_iterator_entries;

--source include/uninstall_replication_observers_example.inc
