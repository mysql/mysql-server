--echo ===============
--echo create table & Index
--echo ===============

eval CREATE TABLE tab5(col_1 TINYBLOB, col_2 TINYTEXT,col_3 BLOB,
col_4 TEXT,col_5 MEDIUMBLOB,col_6 MEDIUMTEXT,
col_7 LONGBLOB,col_8 LONGTEXT,col_9 VARCHAR(255)) 
ENGINE=INNODB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=$block_size;


let $i = 9;
while ($i) {

eval CREATE INDEX  idx$i ON tab5(col_$i(10));
dec $i;
}


