# Bug#25330449 ASSERT SIZE==SPACE->SIZE DURING BUF_READ_AHEAD_RANDOM
CREATE TABLE t1 (a INT NOT NULL, d INT NOT NULL, b BLOB NOT NULL, c
TEXT, INDEX(d), INDEX(a),  KEY (b(10), a, d), INDEX(c(120), b(120)),
INDEX(b(5), c(10), a) ) charset latin1 ENGINE=INNODB  ROW_FORMAT=COMPRESSED
KEY_BLOCK_SIZE=1;
# restart
SET GLOBAL innodb_random_read_ahead = 1;
call mtr.add_suppression("\\[ERROR\\].* '.*t1.ibd' read failed! - attempted to read 4 bytes, read only 0 bytes at offset 16418");
call mtr.add_suppression("\\[Warning\\].* Ignoring '.*t1.ibd' invalid tablespace ID in the header");
DROP TABLE t1;
SET GLOBAL innodb_random_read_ahead = default;
