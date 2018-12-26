--eval SET GLOBAL innodb_default_row_format = $INNODB_DEFAULT_ROW_FORMAT
# Check default row_format without setting new global variable
SELECT @@innodb_default_row_format;

eval CREATE $TEMPORARY TABLE t1(c1 TEXT,c2 BLOB) ENGINE=InnoDB;
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;
--echo

eval CREATE $TEMPORARY TABLE t1(c1 TEXT,c2 BLOB) ENGINE=InnoDB ROW_FORMAT=DYNAMIC;
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;
--echo

eval CREATE $TEMPORARY TABLE t1(c1 TEXT,c2 BLOB) ENGINE=InnoDB ROW_FORMAT=COMPACT;
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;
--echo

eval CREATE $TEMPORARY TABLE t1(c1 TEXT,c2 BLOB) ENGINE=InnoDB ROW_FORMAT=REDUNDANT;
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;
--echo

if ($TEMPORARY != 'TEMPORARY') {
eval CREATE $TEMPORARY TABLE t1(c1 TEXT,c2 BLOB) ENGINE=InnoDB
ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=1;
--source suite/innodb/include/default_row_format_show.inc
DROP TABLE t1;
}
--echo
