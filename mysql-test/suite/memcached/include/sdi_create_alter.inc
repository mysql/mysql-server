--echo # Case 1. Test with CREATE TABLES
CREATE TABLE t1(c1 VARCHAR(32),
	c2 VARCHAR(1024),
	c3 TINYBLOB,
	c4 BLOB,
	c5 MEDIUMBLOB,
	c6 LONGBLOB,
	c7 TEXT,
	c8 INT,
	c9 FLOAT,
	c10 DOUBLE,
	primary key(c1),
	KEY idx1(c4(20)),
	UNIQUE KEY idx2(c6(50)))
CHARSET latin1 ENGINE = INNODB;

perl;
use strict;
use warnings;
my $dest_dir = $ENV{'MYSQLTEST_VARDIR'};
open(DATA, ">$dest_dir/tmp/create_table.sql") or die "Couldn't open file
$dest_dir/tmp/create_table.sql, $!";
print DATA "CREATE TABLE t2(";
my $cols = 999;
for (my $i=1; $i <= $cols; $i++) {
	if ($i == 1) {
		print DATA "c1 INT PRIMARY KEY";
	} else {
		print DATA "c$i INT";
	}
	if ($i != $cols) {
		print DATA ", ";
	}
}
print DATA ");";
close DATA;
EOF

--echo # Create table t2 with large schema (huge number of columns)
--exec $MYSQL -uroot test <$MYSQLTEST_VARDIR/tmp/create_table.sql

--echo # Create table with small schema
CREATE TABLE t3(c1 VARCHAR(32),
	c2 VARCHAR(1024),
	PRIMARY KEY(c1))
ENGINE = INNODB;

--echo # Create tables with constraints
CREATE TABLE t4 (c1 INT PRIMARY KEY,
	c2 BLOB)
ENGINE = INNODB;

CREATE TABLE t5 (c1 INT PRIMARY KEY,
	c2 INT,
	FOREIGN KEY (c2) REFERENCES t4(c1) ON UPDATE CASCADE)
ENGINE = INNODB;

--echo # Create table with spatial index
CREATE TABLE t7(c1 INT PRIMARY KEY,
	c2 INT,
	c3 CHAR(255),
	c4 GEOMETRY NOT NULL SRID 0,
	SPATIAL INDEX(c4))
ENGINE = INNODB;

# Tables must exist before plugin can be started!
--source ../include/load_daemon_memcached_expecting_success.inc

let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc

let TABLE_NAME = t2;
let MEMCACHED_TABLE_MAPPING = desc_t2;
--source ../include/sdi_insert_get_row.inc

let TABLE_NAME = t3;
let MEMCACHED_TABLE_MAPPING = desc_t3;
--source ../include/sdi_insert_get_row.inc

let TABLE_NAME = t4;
let MEMCACHED_TABLE_MAPPING = desc_t4;
--source ../include/sdi_insert_get_row.inc

let TABLE_NAME = t5;
let MEMCACHED_TABLE_MAPPING = desc_t5;
--source ../include/sdi_insert_get_row.inc

let TABLE_NAME = t7;
let MEMCACHED_TABLE_MAPPING = desc_t7;
--source ../include/sdi_insert_get_row.inc

--echo # 2. Test with alter table

# Alter columns
ALTER TABLE t1 ADD COLUMN c11 INT;
let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc

ALTER TABLE t3 ADD COLUMN c3 INT;
let TABLE_NAME = t3;
let MEMCACHED_TABLE_MAPPING = desc_t3;
--source ../include/sdi_insert_get_row.inc

ALTER TABLE t2 DROP COLUMN c100;
ALTER TABLE t2 DROP COLUMN c101;
ALTER TABLE t2 DROP COLUMN c102;
ALTER TABLE t2 DROP COLUMN c103;
ALTER TABLE t2 DROP COLUMN c104;
let TABLE_NAME = t2;
let MEMCACHED_TABLE_MAPPING = desc_t2;
--source ../include/sdi_insert_get_row.inc

ALTER TABLE t7 MODIFY c2 BIGINT NOT NULL;
let TABLE_NAME = t7;
let MEMCACHED_TABLE_MAPPING = desc_t7;
--source ../include/sdi_insert_get_row.inc

# Alter constraints

ALTER TABLE t3 ADD FOREIGN KEY(c3) REFERENCES t2(c1);
SHOW CREATE TABLE t3;
let TABLE_NAME = t3;
let MEMCACHED_TABLE_MAPPING = desc_t3;
--source ../include/sdi_insert_get_row.inc

ALTER TABLE t5 DROP FOREIGN KEY t5_ibfk_1;
let TABLE_NAME = t5;
let MEMCACHED_TABLE_MAPPING = desc_t5;
--source ../include/sdi_insert_get_row.inc

# Alter indexes
ALTER TABLE t7 ADD INDEX idx3(c2,c3(50));
let TABLE_NAME = t7;
let MEMCACHED_TABLE_MAPPING = desc_t7;
--source ../include/sdi_insert_get_row.inc

ALTER TABLE t1 DROP PRIMARY KEY, DROP COLUMN c1, ADD c1 BIGINT PRIMARY
KEY;
let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc

# Alter Fulltext Indexes
ALTER TABLE t1 ADD FULLTEXT INDEX(c2), ALGORITHM=INPLACE;
let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc

ALTER TABLE t1 DROP INDEX c2;
let TABLE_NAME = t1;
let MEMCACHED_TABLE_MAPPING = desc_t1;
--source ../include/sdi_insert_get_row.inc

# Test with SDI after a TRUNCATE TABLE operation
TRUNCATE TABLE t7;
let TABLE_NAME = t7;
let MEMCACHED_TABLE_MAPPING = desc_t7;
--source ../include/sdi_insert_get_row.inc

DROP TABLE t1, t3, t2, t4, t5, t7;
# Stop plugin before innodb_memcached configuration
UNINSTALL PLUGIN daemon_memcached;
--remove_file $MYSQLTEST_VARDIR/tmp/create_table.sql

