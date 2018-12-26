SET GLOBAL innodb_file_per_table=on;
SHOW variables like '%innodb_checksum_algorithm%';
Variable_name	Value
innodb_checksum_algorithm	crc32
CREATE TABLE t1(c1 INT PRIMARY KEY,c2 VARCHAR(20)) COMPRESSION="none", ENGINE=InnoDB;
INSERT INTO t1 VALUES(1, 'Innochecksum InnoDB');
INSERT INTO t1 VALUES(2, 'Innochecksum CRC32');
# Shutdown the Server
# Test[1(a)]: for read from standard input for innochecksum tool without rewrite of checksum with log option.
InnoDB File Checksum Utility.
Filename = -
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Test[1(b)]: for read from standard input for innochecksum tool Test checking for page: 3 to page:5
InnoDB File Checksum Utility.
Filename = -
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Test[1] End
# Test [2] for read from standard input for innochecksum tool with rewrite of crc32 checksum.
# Backup the t1.ibd before any rewrite of checksum, so used for further testing.
# Print the information for log option.
InnoDB File Checksum Utility.
Filename = -
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Recheck the a.ibd created file having crc32 checksum with the --strict-check=crc32 for innochecksum tool.
InnoDB File Checksum Utility.
Filename = a.ibd
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; crc32 calculated = #; recorded checksum field# = # recorded checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; crc32 calculated = #; recorded checksum field# = # recorded checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; crc32 calculated = #; recorded checksum field# = # recorded checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; crc32 calculated = #; recorded checksum field# = # recorded checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; crc32 calculated = #; recorded checksum field# = # recorded checksum field# = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
#Replace the t.ibd with the newly created a.ibd having crc32 checksum.
# Start the server to validate the t1.ibd having crc32 checksum.
# restart
select * from t1;
c1	c2
1	Innochecksum InnoDB
2	Innochecksum CRC32
# Shutdown the server
# Test[2] End
# Test[3] for read from standard input for innochecksum tool with rewrite of "none" checksum.
# Retrieve the original t1.ibd before any rewrite of checksum, so used for further testing.
# Check for innochecksum tool to read from stdin and rewrite the checksum to "none"
# along with check for --verbose & --page-type-summary
# Print the content for --verbose and --page-type-summary

Variables (--variable-name=value)
and boolean options {FALSE|TRUE}  Value (after reading options)
--------------------------------- ----------------------------------------
verbose                           TRUE
count                             FALSE
start-page                        #
end-page                          #
page                              #
strict-check                      crc#
no-check                          FALSE
allow-mismatches                  #
write                             none
page-type-summary                 TRUE
page-type-dump                    (No default value)
log                               mtrchecksum.log

================PAGE TYPE SUMMARY==============
#PAGE_COUNT	PAGE_TYPE
===============================================
       #	Index page
       #	SDI Index page
       #	Undo log page
       #	Inode page
       #	Insert buffer free list page
       #	Freshly allocated page
       #	Insert buffer bitmap
       #	System page
       #	Transaction system page
       #	File Space Header
       #	Extent descriptor page
       #	BLOB page
       #	Compressed BLOB page
       #	Subsequent Compressed BLOB page
       #	SDI BLOB page
       #	Compressed SDI BLOB page
       #	Other type of page
===============================================
Additional information:
Undo page type: # insert, # update, # other
Undo page state: # active, # cached, # to_free, # to_purge, # prepared, # other
# Print the information for log option.
InnoDB File Checksum Utility.
Filename = -
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Recheck the a.ibd created file having crc32 checksum with the --strict-check=none for innochecksum tool.
InnoDB File Checksum Utility.
Filename = a.ibd
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; none checksum: calculated = #; recorded checksum_field# = # recorded checksum_field# = #
page::#; log sequence number:first = #; second = #
page::#; none checksum: calculated = #; recorded checksum_field# = # recorded checksum_field# = #
page::#; log sequence number:first = #; second = #
page::#; none checksum: calculated = #; recorded checksum_field# = # recorded checksum_field# = #
page::#; log sequence number:first = #; second = #
page::#; none checksum: calculated = #; recorded checksum_field# = # recorded checksum_field# = #
page::#; log sequence number:first = #; second = #
page::#; none checksum: calculated = #; recorded checksum_field# = # recorded checksum_field# = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Start the server to validate the t1.ibd having "none" checksum.
# restart
select * from t1;
c1	c2
1	Innochecksum InnoDB
2	Innochecksum CRC32
# Shutdown the server
# Test[3] End
# Test[4] for read from standard input for innochecksum tool with rewrite of "innodb" checksum.
# Command for innochecksum tool to read from stdin and rewrite the checksum to "innodb"
# Print the information for log option.
InnoDB File Checksum Utility.
Filename = -
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded = #
page::#; new style: calculated = #; crc32 = #; recorded = #
page::#; Updated checksum field# = #
page::#; Updated checksum field# = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Recheck the a.ibd created file having crc32 checksum with the --strict-check=innodb for innochecksum tool.
InnoDB File Checksum Utility.
Filename = a.ibd
Innochecksum: checking pages in range # to #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded checksum = #
page::#; new style: calculated = #; recorded checksum = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded checksum = #
page::#; new style: calculated = #; recorded checksum = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded checksum = #
page::#; new style: calculated = #; recorded checksum = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded checksum = #
page::#; new style: calculated = #; recorded checksum = #
page::#; log sequence number:first = #; second = #
page::#; old style: calculated = #; recorded checksum = #
page::#; new style: calculated = #; recorded checksum = #
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
page::#; log sequence number:first = #; second = #
Page::# is empty and uncorrupted
# Replace the t.ibd with the newly created a.ibd having "innodb" checksum.
# Start the server to validate the t1.ibd having "innodb" checksum.
# restart
select * from t1;
c1	c2
1	Innochecksum InnoDB
2	Innochecksum CRC32
# Test[4] End
# Test[5] for lock.
# Test Scenario: As mysqld is running, & then start the innochecksum which must fail.
Pattern "Error: Unable to lock file:: MYSQLD_DATADIR/test/t1.ibd" found
Pattern "fcntl: Resource temporarily unavailable" found
# Test[5] end
DROP TABLE t1;
