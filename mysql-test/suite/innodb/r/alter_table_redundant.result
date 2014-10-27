SET GLOBAL innodb_file_per_table=ON;
DROP TABLE IF EXISTS t;
Warnings:
Note	1051	Unknown table 'test.t'
CREATE TABLE t(C INT) ROW_FORMAT=redundant ENGINE=InnoDB;
#restart the server
# restart
ALTER TABLE t ROW_FORMAT=REDUNDANT KEY_BLOCK_SIZE=0, ALGORITHM=DEFAULT;
DROP TABLE t;
