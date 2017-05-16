include/install_replication_observers_example.inc
#
# Phase 1: The before commit hook fails.
# Set a GTID in an empty transaction and watch it fail
#
SET SESSION sql_log_bin= 0;
call mtr.add_suppression("Run function 'before_commit' in plugin");
SET SESSION sql_log_bin= 1;
SET @debug_saved= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,force_error_on_before_commit_listener';
SET gtid_next='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
COMMIT;
ERROR HY000: Error on observer while running replication hook 'before_commit'.
SET @@GLOBAL.DEBUG= @debug_saved;
ROLLBACK;
SET gtid_next='automatic';
#
# Phase 2: The before commit hook makes the transaction rollback
# Set a GTID in an empty transaction and watch it fail
#
SET @@GLOBAL.DEBUG= '+d,force_negative_certification_outcome';
SET gtid_next='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
COMMIT;
ERROR 40000: Plugin instructed the server to rollback the current transaction.
SET @@GLOBAL.DEBUG= @debug_saved;
ROLLBACK;
SET gtid_next='automatic';
#
# Clean up
#
include/uninstall_replication_observers_example.inc
