# restart
SELECT @@global.temptable_use_mmap;
@@global.temptable_use_mmap
1
SET @@session.temptable_use_mmap=false;
ERROR HY000: Variable 'temptable_use_mmap' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.temptable_use_mmap=NULL;
ERROR 42000: Variable 'temptable_use_mmap' can't be set to the value of 'NULL'
# Test with temptable_use_mmap set to false
# to stop overflowing temptable to disk
SET @@global.temptable_use_mmap=false;
Warnings:
Warning	1287	'temptable_use_mmap' is deprecated and will be removed in a future release.
SELECT @@global.temptable_use_mmap;
@@global.temptable_use_mmap
0
SELECT count_alloc > 0
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';
count_alloc > 0
0
CREATE TABLE t (c VARCHAR(128));
INSERT INTO t VALUES
(REPEAT('a', 128)),
(REPEAT('b', 128)),
(REPEAT('c', 128)),
(REPEAT('d', 128));
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET GLOBAL temptable_max_ram = 2097152;
SELECT * FROM
t AS t1,
t AS t2,
t AS t3,
t AS t4,
t AS t5,
t AS t6
ORDER BY 1
LIMIT 2;
SET GLOBAL temptable_max_ram = default;
SELECT count_alloc > 0
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';
count_alloc > 0
0
DROP TABLE t;
# Test with temptable_use_mmap set to true
# to enable overflowing temptable to disk
SET @@global.temptable_use_mmap = true;
Warnings:
Warning	1287	'temptable_use_mmap' is deprecated and will be removed in a future release.
SELECT @@global.temptable_use_mmap;
@@global.temptable_use_mmap
1
CREATE TABLE t (c LONGBLOB);
INSERT INTO t VALUES
(REPEAT('a', 128)),
(REPEAT('b', 128)),
(REPEAT('c', 128)),
(REPEAT('d', 128));
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET GLOBAL temptable_max_ram = 2097152;
SELECT * FROM
t AS t1,
t AS t2,
t AS t3,
t AS t4,
t AS t5,
t AS t6
ORDER BY 1
LIMIT 2;
SET GLOBAL temptable_max_ram = default;
SET GLOBAL temptable_use_mmap = default;
Warnings:
Warning	1287	'temptable_use_mmap' is deprecated and will be removed in a future release.
SELECT @@global.temptable_use_mmap;
@@global.temptable_use_mmap
1
SELECT count_alloc > 0
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';
count_alloc > 0
1
DROP TABLE t;
CREATE TABLE t (c LONGBLOB);
INSERT INTO t VALUES
(REPEAT('a', 128)),
(REPEAT('b', 128)),
(REPEAT('c', 128)),
(REPEAT('d', 128));
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET GLOBAL temptable_max_ram = 2*1024*1024;
SET GLOBAL temptable_max_mmap = 4*1024*1024;
SELECT * FROM
t AS t1,
t AS t2,
t AS t3,
t AS t4,
t AS t5,
t AS t6
ORDER BY 1
LIMIT 2;
SELECT sum_number_of_bytes_alloc >= 2*1024*1024
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_ram';
sum_number_of_bytes_alloc >= 2*1024*1024
1
SELECT sum_number_of_bytes_alloc = 4*1024*1024 + 64
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/temptable/physical_disk';
sum_number_of_bytes_alloc = 4*1024*1024 + 64
1
DROP TABLE t;
SET GLOBAL temptable_max_ram = default;
SET GLOBAL temptable_max_mmap = default;
SET GLOBAL temptable_use_mmap = default;
Warnings:
Warning	1287	'temptable_use_mmap' is deprecated and will be removed in a future release.
