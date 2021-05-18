CREATE TABLE t1(c1 INT);
# restart: --innodb-buffer-pool-load-at-startup=off
# restart: --innodb-buffer-pool-load-at-startup=off
SET DEBUG='+d,ib_create_table_fail_disk_full';
# Write file to make mysql-test-run.pl expect the "crash", but don't
# start it until it's told to
SELECT * FROM t1;
ERROR HY000: Lost connection to MySQL server during query
# restart:
# Cleanup
DROP TABLE t1;
