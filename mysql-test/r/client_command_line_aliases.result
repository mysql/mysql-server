include/save_binlog_position.inc
mysqlbinlog --read-from-remote-source: expect no warning
mysqlbinlog --read-from-remote-master: expect warning
WARNING: --read-from-remote-master is deprecated and will be removed in a future version. Use --read-from-remote-source instead.
mysqldump --apply-replica-statements: expect no warning
mysqldump --apply-slave-statements: expect warning
WARNING: --apply-slave-statements is deprecated and will be removed in a future version. Use --apply-replica-statements instead.
mysqldump --delete-source-logs: expect no warning
mysqldump --delete-master-logs: expect warning
WARNING: --delete-master-logs is deprecated and will be removed in a future version. Use --delete-source-logs instead.
mysqldump --dump-replica: expect no warning
mysqldump --dump-slave: expect warning
WARNING: --dump-slave is deprecated and will be removed in a future version. Use --dump-replica instead.
mysqldump --include-source-host-port: expect no warning
mysqldump --include-master-host-port: expect warning
WARNING: --include-master-host-port is deprecated and will be removed in a future version. Use --include-source-host-port instead.
mysqldump --source-data: expect no warning
mysqldump --master-data: expect warning
WARNING: --master-data is deprecated and will be removed in a future version. Use --source-data instead.
mysqladmin start-replica: expect no warning
mysqladmin start-slave: expect warning
WARNING: start-slave is deprecated and will be removed in a future version. Use start-replica instead.
mysqladmin stop-replica: expect no warning
mysqladmin stop-slave: expect warning
WARNING: stop-slave is deprecated and will be removed in a future version. Use stop-replica instead.
