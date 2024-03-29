CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
SET DEBUG='+d,simulate_magic_header_io_failure';
SHOW BINLOG EVENTS;
ERROR HY000: Error when executing command SHOW BINLOG EVENTS: I/O error reading the header from the binary log
SET DEBUG='-d,simulate_magic_header_io_failure';
DROP TABLE t1;
