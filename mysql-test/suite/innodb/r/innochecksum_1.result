# Set the environmental variables
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Unable to read page \\[page id: space=.*, page number=.*\\] into the buffer pool after 100 attempts");
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Database page corruption on disk or a failed");
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Assertion failure: ut0ut\.cc:");
SET GLOBAL innodb_file_per_table=on;
[1]: Test is to corrupt the ibd file, & do repair for (innodb|crc32|none) checksum through innochecksum tool
# Create and populate the table to be corrupted
CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY, b TEXT) COMPRESSION="none",
ROW_FORMAT=COMPACT ENGINE=InnoDB;
INSERT INTO t1 (b) VALUES ('corrupt me');
INSERT INTO t1 (b) VALUES ('corrupt me');
# Shutdown the server
# Corrupt the t1 table
Munged a string.
Munged a string.
# Backup the corrupted t1.ibd for reuse for further testing.
# Write file to make mysql-test-run.pl start up the server again
# restart
# Write file to make mysql-test-run.pl expect the "crash", but don't
# start it until it's told to
# The below SELECT query will crash the server because some pages
# on the disk are corrupted
SELECT * FROM t1;
ERROR HY000: Lost connection to MySQL server during query
[1(a)]: Repair the ibd file with innochecksum with --write=innodb
# Start the server
# restart
select count(*) from t1;
# Shutdown the server
# Move the corrupted ibd file to t1.ibd
[1(b)]: Repair the ibd file with innochecksum with --write=crc32
# Start the server
# restart
select count(*) from t1;
# Shutdown the server
# Move the corrupted ib file to t1.ibd
[1(c)]: Repair the ibd file with innochecksum with --write=none
# Start the server
# restart
select * from t1;
DROP TABLE t1;
[19]: Test Completed
CREATE TABLE tab1(c1 INT PRIMARY KEY,c2 VARCHAR(20)) COMPRESSION="none", ENGINE=InnoDB;
CREATE INDEX idx1 ON tab1(c2(10));
INSERT INTO tab1 VALUES(1, 'Innochecksum InnoDB1');
# shutdown the server
[2]: Test for verbose short option, output from innochecksum
# Print the verbose output

Variables (--variable-name=value)
and boolean options {FALSE|TRUE}  Value (after reading options)
--------------------------------- ----------------------------------------
verbose                           TRUE
count                             FALSE
start-page                        0
end-page                          0
page                              0
strict-check                      crc32
no-check                          FALSE
allow-mismatches                  0
write                             crc32
page-type-summary                 FALSE
page-type-dump                    (No default value)
log                               (No default value)
[3]: test for --verbose option with --strict-check=innodb for innochecksum
: With verbose long option.
# Print the verbose output

Variables (--variable-name=value)
and boolean options {FALSE|TRUE}  Value (after reading options)
--------------------------------- ----------------------------------------
verbose                           TRUE
count                             FALSE
start-page                        0
end-page                          0
page                              0
strict-check                      crc32
no-check                          FALSE
allow-mismatches                  0
write                             crc32
page-type-summary                 FALSE
page-type-dump                    (No default value)
log                               (No default value)
[4]: Test for --allow-mismatches =99
# Expect the fails for checksum mismatches. Print the error message.
[WARNING] innochecksum: innodb_checksum_algorithm is set to "strict_none" but the page [page id: space=X, page number=Y] contains a valid checksum "crc32".  Accepting the page as valid. Change innodb_checksum_algorithm to "none" to silently accept such pages or rewrite all pages so that they contain "none" checksum.
[WARNING] innochecksum: innodb_checksum_algorithm is set to "strict_none" but the page [page id: space=X, page number=Y] contains a valid checksum "crc32".  Accepting the page as valid. Change innodb_checksum_algorithm to "none" to silently accept such pages or rewrite all pages so that they contain "none" checksum.
[WARNING] innochecksum: innodb_checksum_algorithm is set to "strict_none" but the page [page id: space=X, page number=Y] contains a valid checksum "crc32".  Accepting the page as valid. Change innodb_checksum_algorithm to "none" to silently accept such pages or rewrite all pages so that they contain "none" checksum.
[WARNING] innochecksum: innodb_checksum_algorithm is set to "strict_none" but the page [page id: space=X, page number=Y] contains a valid checksum "crc32".  Accepting the page as valid. Change innodb_checksum_algorithm to "none" to silently accept such pages or rewrite all pages so that they contain "none" checksum.
[WARNING] innochecksum: innodb_checksum_algorithm is set to "strict_none" but the page [page id: space=X, page number=Y] contains a valid checksum "crc32".  Accepting the page as valid. Change innodb_checksum_algorithm to "none" to silently accept such pages or rewrite all pages so that they contain "none" checksum.
[WARNING] innochecksum: innodb_checksum_algorithm is set to "strict_none" but the page [page id: space=X, page number=Y] contains a valid checksum "crc32".  Accepting the page as valid. Change innodb_checksum_algorithm to "none" to silently accept such pages or rewrite all pages so that they contain "none" checksum.
[5]: Test checksum check for page: 2 to page:5
[6]: Test for checksum check for only pageno.= 2
[7]: Further Test are for rewrite checksum (innodb|crc32|none) for all ibd file & start the server.
# Restart the server
# restart
DROP TABLE tab1;
SET GLOBAL innodb_file_per_table=default;
