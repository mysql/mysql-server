--source include/rpl/force_binlog_format_statement.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
--echo #
--echo # Bug#51851: Server with SBR locks mutex twice on LOAD DATA into
--echo #            partitioned MyISAM table
--echo # The test was rewritten to use InnoDB after WL#8971.

--write_file $MYSQLTEST_VARDIR/tmp/init_file.txt
abcd
EOF

CREATE TABLE t1
(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
 name TINYBLOB NOT NULL,
 modified TIMESTAMP DEFAULT '0000-00-00 00:00:00',
 INDEX namelocs (name(255))) ENGINE = InnoDB
PARTITION BY HASH(id) PARTITIONS 2;

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval LOAD DATA LOCAL INFILE '$MYSQLTEST_VARDIR/tmp/init_file.txt'
INTO TABLE t1 (name);

--remove_file $MYSQLTEST_VARDIR/tmp/init_file.txt
DROP TABLE t1;
SET sql_mode = default;

--source include/rpl/restore_default_binlog_format.inc
