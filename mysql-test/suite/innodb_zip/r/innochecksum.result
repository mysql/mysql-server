# Set the environmental variables
call mtr.add_suppression("Unable to read tablespace .* page no .* into the buffer pool after 100 attempts");
call mtr.add_suppression("Warning: database page corruption or a failed");
SET GLOBAL innodb_file_per_table=on;
CREATE TABLE tab1(c1 INT PRIMARY KEY,c2 VARCHAR(20)) ENGINE=InnoDB;
CREATE INDEX idx1 ON tab1(c2(10));
INSERT INTO tab1 VALUES(1, 'Innochecksum InnoDB1');
CREATE TABLE t1(id INT AUTO_INCREMENT PRIMARY KEY, msg VARCHAR(255)) ENGINE=INNODB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=4;
insert into t1 values(1,"i");
insert into t1 values(2,"am");
insert into t1 values(3,"compressed table");
# Shutdown the Server
# Server Default checksum = innodb
[1a]: check the innochecksum when file doesn't exists
Pattern "Error: MYSQLD_DATADIR/test/aa.ibd cannot be found" found
[1b]: check the innochecksum without --strict-check
[2]: check the innochecksum with full form --strict-check=crc32
[3]: check the innochecksum with short form -C crc32
[4]: check the innochecksum with --no-check ignores algorithm check, warning is expected
Pattern "Error: --no-check must be associated with --write option." found
[5]: check the innochecksum with short form --no-check ignores algorithm check, warning is expected
Pattern "Error: --no-check must be associated with --write option." found
[6]: check the innochecksum with full form strict-check & no-check , an error is expected
Pattern "Error: --strict-check option cannot be used together with --no-check option." found
[7]: check the innochecksum with short form strict-check & no-check , an error is expected
Pattern "Error: --strict-check option cannot be used together with --no-check option." found
[8]: check the innochecksum with short & full form combination
# strict-check & no-check, an error is expected
Pattern "Error: --strict-check option cannot be used together with --no-check option." found
[9]: check the innochecksum with full form --strict-check=innodb
[10]: check the innochecksum with full form --strict-check=none
# when server Default checksum=crc32
[11]: check the innochecksum with short form -C innodb
# when server Default checksum=crc32
[12]: check the innochecksum with short form -C none
# when server Default checksum=crc32
[13]: check strict-check with invalid values
Pattern "Error while setting value \'strict_innodb\' to \'strict-check\'" found
Pattern "Error while setting value \'strict_innodb\' to \'strict-check\'" found
Pattern "Error while setting value \'strict_crc32\' to \'strict-check\'" found
Pattern "Error while setting value \'strict_crc32\' to \'strict-check\'" found
Pattern "Error while setting value \'strict_none\' to \'strict-check\'" found
Pattern "Error while setting value \'strict_none\' to \'strict-check\'" found
Pattern "Error while setting value \'InnoBD\' to \'strict-check\'" found
Pattern "Error while setting value \'InnoBD\' to \'strict-check\'" found
Pattern "Error while setting value \'crc\' to \'strict-check\'" found
Pattern "Error while setting value \'no\' to \'strict-check\'" found
[14a]: when server default checksum=crc32 rewrite new checksum=crc32 with innochecksum
# Also check the long form of write option.
[14b]: when server default checksum=crc32 rewrite new checksum=innodb with innochecksum
# Also check the long form of write option.
# start the server with innodb_checksum_algorithm=InnoDB
# restart: --innodb_checksum_algorithm=innodb
INSERT INTO tab1 VALUES(2, 'Innochecksum CRC32');
SELECT c1,c2 FROM tab1 order by c1,c2;
c1	c2
1	Innochecksum InnoDB1
2	Innochecksum CRC32
# Stop the server
[15]: when server default checksum=crc32 rewrite new checksum=none with innochecksum
# Also check the short form of write option.
# Start the server with checksum algorithm=none
# restart: --innodb_checksum_algorithm=none
INSERT INTO tab1 VALUES(3, 'Innochecksum None');
SELECT c1,c2 FROM tab1 order by c1,c2;
c1	c2
1	Innochecksum InnoDB1
2	Innochecksum CRC32
3	Innochecksum None
DROP TABLE t1;
# Stop the server
[16]: rewrite into new checksum=crc32 with innochecksum
# Restart the DB server with  innodb_checksum_algorithm=crc32
# restart: --innodb_checksum_algorithm=crc32 --innodb_file_per_table=on
SELECT * FROM tab1;
c1	c2
1	Innochecksum InnoDB1
2	Innochecksum CRC32
3	Innochecksum None
DELETE FROM tab1 where c1=3;
SELECT c1,c2 FROM tab1 order by c1,c2;
c1	c2
1	Innochecksum InnoDB1
2	Innochecksum CRC32
# Stop server
[17]: rewrite into new checksum=InnoDB
# Restart the DB server with innodb_checksum_algorithm=InnoDB
# restart: --innodb_checksum_algorithm=innodb --innodb_file_per_table=on
DELETE FROM tab1 where c1=2;
SELECT * FROM tab1;
c1	c2
1	Innochecksum InnoDB1
# Stop server
[18]:check Innochecksum with invalid write options
Pattern "Error while setting value \'strict_crc32\' to \'write\'" found
Pattern "Error while setting value \'strict_innodb\' to \'write\'" found
Pattern "Error while setting value \'crc23\' to \'write\'" found
# Restart the server
# restart
DROP TABLE tab1;
SET GLOBAL innodb_file_per_table=default;
