--echo # Create table & Index

# Separate CREATE INDEX loop leads to delete + insert of SDI records.
# So if SDI record is not purged before next CREATE index, extra page
# could be occupied. Since the aim is to test physical ibd size,
# we try to remove dependency on purge by creating all indexes along
# with CREATE TABLE

eval CREATE TABLE tab5(col_1 TINYBLOB, col_2 TINYTEXT,col_3 BLOB,
col_4 TEXT,col_5 MEDIUMBLOB,col_6 MEDIUMTEXT,
col_7 LONGBLOB,col_8 LONGTEXT,col_9 VARCHAR(255),
KEY idx1(col_1(10)),
KEY idx2(col_2(10)),
KEY idx3(col_3(10)),
KEY idx4(col_4(10)),
KEY idx5(col_5(10)),
KEY idx6(col_6(10)),
KEY idx7(col_7(10)),
KEY idx8(col_8(10)),
KEY idx9(col_9(10))
)ENGINE=INNODB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=$block_size;
