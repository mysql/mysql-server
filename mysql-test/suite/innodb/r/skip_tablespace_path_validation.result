CREATE TABLE t1 (id INT, PRIMARY KEY (id));
INSERT INTO t1(id) values(1);
# Shut down mysqld and then move the t1.ibd file to a different directory.
# Restart without validation using --skip-innodb-validate-tablespace-paths
# restart:--skip-innodb-validate-tablespace-paths --log-error=SEARCH_FILE
# Check for the skip validation warning in the log
Pattern "\[Note\] \[.*\] Skipping InnoDB tablespace path validation. Manually moved tablespace files will not be detected!" found
# Ensure we didn't receive a warning about the missing t1.ibd file
Pattern "\[Warning\] \[.*\] Tablespace .*, name 'test/t1', file '.*test.*t1.ibd' is missing!" not found
# Restart without validation using --innodb-validate-tablespace-paths=0
# restart:--innodb-validate-tablespace-paths=0 --log-error=SEARCH_FILE
# Check for the skip validation warning in the log
Pattern "\[Note\] \[.*\] Skipping InnoDB tablespace path validation. Manually moved tablespace files will not be detected!" found
# Ensure we didn't receive a warning about the missing t1.ibd file
Pattern "\[Warning\] \[.*\] Tablespace .*, name 'test/t1', file '.*test.*t1.ibd' is missing!" not found
# Restart with validation
# restart:--innodb-validate-tablespace-paths=1 --log-error=SEARCH_FILE
# Ensure the warning is visible now
Pattern "\[Warning\] \[.*\] Tablespace .*, name 'test/t1', file '.*test.*t1.ibd' is missing!" found
drop table t1;
# restart:
