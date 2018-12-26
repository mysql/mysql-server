# inc/partition_10.inc
#
#     Do some basic checks on a table.
#
# FIXME: Do not write the statements and results, if SQL return code = 0
#        and result set like expected. Write a message, that all is like
#        expected instead.
#
# All SELECTs are so written, that we get my_value = 1, when everything
# is like expected.
#

--source suite/parts/inc/partition_layout.inc

####### Variations with multiple records
#      Select on empty table
SELECT COUNT(*) = 0 AS my_value FROM t1;
#      (mass) Insert of $max_row records
eval INSERT INTO t1 SELECT * FROM t0_template WHERE f1 BETWEEN 1 AND $max_row;
#      Select
eval SELECT (COUNT(*) = $max_row) AND (MIN(f1) = 1) AND (MAX(f1) = $max_row)
            AS my_value FROM t1;
# DEBUG SELECT COUNT(*),MIN(f1),MAX(f1) FROM t1;
#      (mass) Update $max_row_div4 * 2 + 1 records
eval UPDATE t1 SET f1 = f1 + $max_row
WHERE f1 BETWEEN $max_row_div2 - $max_row_div4 AND $max_row_div2 + $max_row_div4;
#      Select
eval SELECT (COUNT(*) = $max_row) AND (MIN(f1) = 1) AND (MAX(f1) = $max_row_div2 + $max_row_div4 + $max_row )
            AS my_value FROM t1;
# DEBUG SELECT COUNT(*),MIN(f1),MAX(f1) FROM t1;
#      (mass) Delete $max_row_div4 * 2 + 1 records
eval DELETE FROM t1
WHERE f1 BETWEEN $max_row_div2 - $max_row_div4 + $max_row AND $max_row_div2 + $max_row_div4 + $max_row;
#      Select
eval SELECT (COUNT(*) = $max_row - $max_row_div4 - $max_row_div4 - 1) AND (MIN(f1) = 1) AND (MAX(f1) = $max_row)
            AS my_value FROM t1;
# DEBUG SELECT COUNT(*),MIN(f1),MAX(f1) FROM t1;

####### Variations with single records
#      Insert one record at beginning
INSERT INTO t1 SET f1 = 0 , f2 = '#######';
#      Select this record
SELECT COUNT(*) = 1 AS my_value FROM t1 WHERE f1 = 0 AND f2 = '#######';
#      Insert one record at end
eval INSERT INTO t1 SET f1 = $max_row + 1, f2 = '#######';
#      Select this record
eval SELECT COUNT(*) = 1 AS my_value FROM t1 WHERE f1 = $max_row + 1 AND f2 = '#######';
#      Update one record
eval UPDATE t1 SET f1 = $max_row + 2, f2 = 'ZZZZZZZ'
     WHERE  f1 = 0 AND f2 = '#######';
#      Select
eval SELECT COUNT(*) = 1 AS my_value FROM t1 WHERE f1 = $max_row + 2 AND f2 = 'ZZZZZZZ';
# Bug #15968: Partitions: crash when INSERT with f1 = -1 into PARTITION BY HASH(f1)
eval UPDATE t1 SET f1 = 0 - 1, f2 = 'ZZZZZZZ'
     WHERE  f1 = $max_row + 1 AND f2 = '#######';
#      Select
SELECT COUNT(*) AS my_value FROM t1 WHERE f1 = 0 - 1 AND f2 = 'ZZZZZZZ';
#      Delete
eval DELETE FROM t1 WHERE f1 = $max_row + 2 AND f2 = 'ZZZZZZZ';
DELETE FROM t1 WHERE f1 = 0 - 1 AND f2 = 'ZZZZZZZ';
#      Select
SELECT COUNT(*) = 0 AS my_value FROM t1 WHERE f2 = 'ZZZZZZZ';

#      Truncate
TRUNCATE t1;
#      Select on empty table
SELECT COUNT(*) = 0 AS my_value FROM t1;
