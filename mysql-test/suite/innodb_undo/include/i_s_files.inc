SET default_storage_engine=InnoDB;
LET MYSQLD_DATADIR = `select @@datadir`;

CREATE TABLE t1 (a serial, b text, c text);

--disable_query_log
--vertical_results
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", FILE_ID, TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS,
       ENGINE, EXTENT_SIZE, MAXIMUM_SIZE, AUTOEXTEND_SIZE
FROM information_schema.files WHERE file_id = 0
ORDER BY FILE_ID;

--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", FILE_ID, TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS,
       ENGINE, EXTENT_SIZE, MAXIMUM_SIZE
FROM information_schema.files WHERE tablespace_name RLIKE 'innodb_undo_00[1-2]'
ORDER BY FILE_ID;

--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS,
       ENGINE, EXTENT_SIZE, INITIAL_SIZE, MAXIMUM_SIZE, AUTOEXTEND_SIZE
FROM information_schema.files WHERE file_name LIKE '%ibtmp1%';

--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS, ENGINE,
       DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS, EXTENT_SIZE,
       INITIAL_SIZE, MAXIMUM_SIZE, AUTOEXTEND_SIZE
FROM information_schema.files WHERE file_name LIKE '%t1%';
--enable_query_log

--echo #
--echo # Populate t1 with about 3MB of data
--echo #
delimiter |;
CREATE PROCEDURE populate_t1()
BEGIN
        DECLARE i INT DEFAULT 1;
        while (i <= 6000) DO
                INSERT INTO t1 (b,c) VALUES (repeat('b', 250), repeat('c', 250));
                SET i = i + 1;
        END WHILE;
END |
delimiter ;|

BEGIN;
CALL populate_t1();
FLUSH TABLE t1;

--disable_query_log
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS, ENGINE, DATA_FREE,
       FREE_EXTENTS, TOTAL_EXTENTS, EXTENT_SIZE,
       INITIAL_SIZE, MAXIMUM_SIZE, AUTOEXTEND_SIZE
FROM information_schema.files WHERE file_name LIKE '%t1%';
--enable_query_log

--echo #
--echo # Populate t1 with another 3MB of data
--echo #
CALL populate_t1();
FLUSH TABLE t1;
--disable_query_log
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS, ENGINE, DATA_FREE,
       FREE_EXTENTS, TOTAL_EXTENTS, EXTENT_SIZE,
       INITIAL_SIZE, MAXIMUM_SIZE, AUTOEXTEND_SIZE
FROM information_schema.files WHERE file_name LIKE '%t1%';
--enable_query_log

--echo #
--echo # Delete the half of the records.
--echo #
DELETE FROM t1 WHERE a % 2 = 1;
FLUSH TABLE t1;

--disable_query_log
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT "==I_S.FILES==", TABLESPACE_NAME, FILE_NAME, FILE_TYPE, STATUS, ENGINE, DATA_FREE,
       FREE_EXTENTS, TOTAL_EXTENTS, EXTENT_SIZE,
       INITIAL_SIZE, MAXIMUM_SIZE, AUTOEXTEND_SIZE
FROM information_schema.files WHERE file_name LIKE '%t1%';
--enable_query_log

--echo #
--echo # Cleanup
--echo #
DROP TABLE t1;
DROP PROCEDURE populate_t1;
