#
# Bug #21950389 SMALL TABLESPACES WITH BLOBS TAKE UP TO 80 TIMES MORE
# SPACE IN 5.7 THAN IN 5.6
#
SET GLOBAL innodb_file_per_table=ON;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
Empty table:
The size of file 'test/t1.ibd' is 114688 bytes.
Rows inserted: 5000
The size of file 'test/t1.ibd' is 229376 bytes.
Rows inserted: 10000
The size of file 'test/t1.ibd' is 360448 bytes.
Rows inserted: 15000
The size of file 'test/t1.ibd' is 475136 bytes.
Rows inserted: 20000
The size of file 'test/t1.ibd' is 606208 bytes.
Rows inserted: 25000
The size of file 'test/t1.ibd' is 9437184 bytes.
DROP TABLE t1;
CREATE TABLE t2 (a INT PRIMARY KEY, b BLOB) ENGINE=InnoDB;
Empty table:
The size of file 'test/t2.ibd' is 114688 bytes.
Rows inserted: 4
The size of file 'test/t2.ibd' is 212992 bytes.
Rows inserted: 8
The size of file 'test/t2.ibd' is 344064 bytes.
Rows inserted: 12
The size of file 'test/t2.ibd' is 475136 bytes.
Rows inserted: 16
The size of file 'test/t2.ibd' is 606208 bytes.
Rows inserted: 20
The size of file 'test/t2.ibd' is 4194304 bytes.
DROP TABLE t2;
CREATE TABLE t3 (a INT PRIMARY KEY, b BLOB)
ENGINE=InnoDB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=1;
Empty table:
The size of file 'test/t3.ibd' is 7168 bytes.
Rows inserted: 8
The size of file 'test/t3.ibd' is 14336 bytes.
Rows inserted: 16
The size of file 'test/t3.ibd' is 22528 bytes.
Rows inserted: 24
The size of file 'test/t3.ibd' is 30720 bytes.
Rows inserted: 32
The size of file 'test/t3.ibd' is 5242880 bytes.
Rows inserted: 40
The size of file 'test/t3.ibd' is 5242880 bytes.
DROP TABLE t3;
SET GLOBAL innodb_file_per_table=default;
