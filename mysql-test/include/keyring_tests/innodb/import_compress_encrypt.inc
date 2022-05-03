# Test "ALTER TABLE ... IMPORT TABLESPACE" in InnoDB

--echo #
--echo # Bug#31313533 : IMPORT FAILS FOR ENCRYPT AND COMPRESSION ENABLED TDE TABLES
--echo #

--echo # SETUP
let $MYSQLD_DATADIR= `SELECT @@datadir`;

--echo #
--echo # Try to create a table with ZLib compression with strict mode OFF.
--echo # If there is an error about Punch Hole not supported, make InnoDB
--echo # think that Punch Hole is working but actually ignore the calls.
--echo #
LET MYSQLD_DATADIR = `SELECT @@datadir`;
--disable_warnings
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION="ZLIB";
let COMPR_ZIP_WARN= `SHOW WARNINGS`;
--enable_warnings

perl;
  use strict;
  my $no_holes = ($ENV{COMPR_ZIP_WARN} =~ /Punch hole not supported/)? 1 : 0;
### we do not expect any other warning
  printf("Unexpected warning: %s\n",$ENV{COMPR_ZIP_WARN})
    if (not $no_holes and $ENV{COMPR_ZIP_WARN} ne '');
  open(DHF,">$ENV{'MYSQLD_DATADIR'}/compr.inc");
  printf DHF "let \$no_holes= %s;\n",$no_holes;
  close(DHF);
EOF

--source $MYSQLD_DATADIR/compr.inc
--remove_file $MYSQLD_DATADIR/compr.inc
LET IGNORE_PUNCH_HOLE=1;
if ($no_holes)
{
  --source include/have_debug.inc
  --disable_query_log
  LET IGNORE_PUNCH_HOLE=1;
  SET DEBUG='+d,ignore_punch_hole';
  --enable_query_log
}
DROP TABLE t1;

--echo
--echo # Test 1 : Check that EXPORT and IMPORT is working fine on same FS
--echo

CREATE TABLE t1(c1 int NOT NULL AUTO_INCREMENT,
                c2 varchar(65000) DEFAULT NULL,
                c3 varchar(255) GENERATED ALWAYS AS (substr(c2,2,100)) STORED,
                c4 varchar(255) GENERATED ALWAYS AS (substr(c2,10,200)) VIRTUAL,
                b bit(64) DEFAULT NULL,
                p_c1 bigint DEFAULT NULL,
                PRIMARY KEY (c1)) ENGINE=Innodb AUTO_INCREMENT=50001 DEFAULT CHARSET=latin1 COMPRESSION='zlib' ENCRYPTION='Y';

--disable_query_log
--let $i = 1
while ($i < 6000) {
  INSERT INTO t1(c2,b,p_c1) VALUES(AES_ENCRYPT("5dd0786a3c7adf503202b8e234a95cbe643bc752028e1792ca8e6ef86c69582eb1cc478670c8e5d6c0133d1b4e5c98017aeb893f9db5f1bff397bc9e0f5fde48","abc", NULL, "pbkdf2_hmac"),10,100);
  --inc $i
}
--enable_query_log

SELECT c1, HEX(SUBSTRING(c2, 10, 10)), HEX(SUBSTRING(c3, 10, 10)),
       HEX(SUBSTRING(c4, 10, 10)), HEX(b)
       FROM t1 ORDER BY c1 limit 10;

--echo # Flush tables for export
FLUSH TABLES t1 FOR EXPORT;

--echo # Copy .cfp .cfg .ibd file to temp
--copy_file $MYSQLD_DATADIR/test/t1.cfp $MYSQLTEST_VARDIR/tmp/t1.cfp
--copy_file $MYSQLD_DATADIR/test/t1.cfg $MYSQLTEST_VARDIR/tmp/t1.cfg
--copy_file $MYSQLD_DATADIR/test/t1.ibd $MYSQLTEST_VARDIR/tmp/t1.ibd

UNLOCK TABLES;

DROP TABLE t1;

CREATE TABLE t1(c1 int NOT NULL AUTO_INCREMENT,
                c2 varchar(65000) DEFAULT NULL,
                c3 varchar(255) GENERATED ALWAYS AS (substr(c2,2,100)) STORED,
                c4 varchar(255) GENERATED ALWAYS AS (substr(c2,10,200)) VIRTUAL,
                b bit(64) DEFAULT NULL,
                p_c1 bigint DEFAULT NULL,
                PRIMARY KEY (c1)) ENGINE=Innodb AUTO_INCREMENT=50001 DEFAULT CHARSET=latin1 COMPRESSION='zlib' ENCRYPTION='Y';

ALTER TABLE t1 DISCARD TABLESPACE;

--echo # Copy .cfp/.cfg and .ibd files from temp to datadir
--copy_file $MYSQLTEST_VARDIR/tmp/t1.cfp $MYSQLD_DATADIR/test/t1.cfp
--copy_file $MYSQLTEST_VARDIR/tmp/t1.cfg $MYSQLD_DATADIR/test/t1.cfg
--copy_file $MYSQLTEST_VARDIR/tmp/t1.ibd $MYSQLD_DATADIR/test/t1.ibd

--echo # Start import
ALTER TABLE t1 IMPORT TABLESPACE;

SELECT c1, HEX(SUBSTRING(c2, 10, 10)), HEX(SUBSTRING(c3, 10, 10)),
       HEX(SUBSTRING(c4, 10, 10)), HEX(b) FROM t1 ORDER BY c1 limit 10;

--echo # Cleanup
DROP TABLE t1;
--remove_file $MYSQLTEST_VARDIR/tmp/t1.cfp
--remove_file $MYSQLTEST_VARDIR/tmp/t1.cfg
--remove_file $MYSQLTEST_VARDIR/tmp/t1.ibd

--echo
--echo # Test 2 : Check that EXPORT and IMPORT is working fine on different FS
--echo

let $MYSQLD_DATADIR= `SELECT @@datadir`;

--echo # Copy and unzip the dir having cfg/cfg/ibd file from a different FS Block Size
--copy_file $MYSQLTEST_VARDIR/std_data/import_8021.zip $MYSQL_TMP_DIR/import_8021.zip
--file_exists $MYSQL_TMP_DIR/import_8021.zip
--exec unzip -qo $MYSQL_TMP_DIR/import_8021.zip -d $MYSQL_TMP_DIR
--file_exists $MYSQL_TMP_DIR/import_8021/t1.cfp
--file_exists $MYSQL_TMP_DIR/import_8021/t1.cfg
--file_exists $MYSQL_TMP_DIR/import_8021/t1.ibd

CREATE TABLE t1(c1 int NOT NULL AUTO_INCREMENT,
                c2 varchar(65000) DEFAULT NULL,
                c3 varchar(255) GENERATED ALWAYS AS (substr(c2,2,100)) STORED,
                c4 varchar(255) GENERATED ALWAYS AS (substr(c2,10,200)) VIRTUAL,
                b bit(64) DEFAULT NULL,
                p_c1 bigint DEFAULT NULL,
                PRIMARY KEY (c1)) ENGINE=Innodb AUTO_INCREMENT=50001 DEFAULT CHARSET=latin1 COMPRESSION='zlib' ENCRYPTION='Y';

ALTER TABLE t1 DISCARD TABLESPACE;

--echo # Copy .cfp/.cfg and .ibd files from temp to datadir
--copy_file $MYSQL_TMP_DIR/import_8021/t1.cfp $MYSQLD_DATADIR/test/t1.cfp
--copy_file $MYSQL_TMP_DIR/import_8021/t1.cfg $MYSQLD_DATADIR/test/t1.cfg
--copy_file $MYSQL_TMP_DIR/import_8021/t1.ibd $MYSQLD_DATADIR/test/t1.ibd

--echo # Start import
ALTER TABLE t1 IMPORT TABLESPACE;

SELECT c1, HEX(SUBSTRING(c2, 10, 10)), HEX(SUBSTRING(c3, 10, 10)),
       HEX(SUBSTRING(c4, 10, 10)), HEX(b) FROM t1 ORDER BY c1 limit 10;

--echo # Cleanup
DROP TABLE t1;

--echo # Remove copied files
--file_exists $MYSQL_TMP_DIR/import_8021.zip
--force-rmdir $MYSQL_TMP_DIR/import_8021
--remove_file $MYSQL_TMP_DIR/import_8021.zip
