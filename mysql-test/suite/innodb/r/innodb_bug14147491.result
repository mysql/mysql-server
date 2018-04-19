# Create and populate the table to be corrupted
CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY, b TEXT) COMPRESSION="none",
ROW_FORMAT=COMPACT ENGINE=InnoDB;
INSERT INTO t1 (b) VALUES ('corrupt me');
INSERT INTO t1 (b) VALUES ('corrupt me');
# Backup the t1.ibd before corrupting
# Corrupt the table
Munged a string.
Munged a string.
# restart
SET DEBUG = '+d,innodb_page_corruption_retries';
# Write file to make mysql-test-run.pl expect the "crash", but don't
# start it until it's told to
# The below SELECT query will crash the server because some pages
# on the disk are corrupted
SELECT * FROM t1;
ERROR HY000: Lost connection to MySQL server during query
# Restore the original t1.ibd
# restart
# Cleanup
DROP TABLE t1;
