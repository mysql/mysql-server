CREATE TABLE t (a INT, b INT, c INT, d INT, e INT, PRIMARY KEY(a, b, c, d), KEY(b, d)) ENGINE=myisam;
# Since ANALYZE TABLE only reads a subset of the data, the statistics for
# table t depends on the row order. And since the INSERT INTO ... SELECT
# may be executed using different execution plans, we've added ORDER BY
# to ensure that we rows has the same order every time. If not, the
# estimated number of rows in EXPLAIN may change on different platforms.
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, d FROM t WHERE d < 2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`d` AS `d` from `test`.`t` where (`test`.`t`.`d` < 2)
FLUSH STATUS;
SELECT b, d FROM t WHERE d < 2;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	10
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, d FROM t WHERE d < 2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	index	NULL	b	8	NULL	2500	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`d` AS `d` from `test`.`t` where (`test`.`t`.`d` < 2)
FLUSH STATUS;
SELECT b, d FROM t WHERE d < 2;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	0
Handler_read_last	0
Handler_read_next	2500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, d FROM t WHERE d < 2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`d` AS `d` from `test`.`t` where (`test`.`t`.`d` < 2)
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, d FROM t WHERE d < 2;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	10
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, d FROM t WHERE d > 4;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`d` AS `d` from `test`.`t` where (`test`.`t`.`d` > 4)
FLUSH STATUS;
SELECT b, d FROM t WHERE d > 4;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	10
Handler_read_last	0
Handler_read_next	1500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, d FROM t WHERE d > 4;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	index	NULL	b	8	NULL	2500	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`d` AS `d` from `test`.`t` where (`test`.`t`.`d` > 4)
FLUSH STATUS;
SELECT b, d FROM t WHERE d > 4;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	0
Handler_read_last	0
Handler_read_next	2500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, d FROM t WHERE d > 4;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`d` AS `d` from `test`.`t` where (`test`.`t`.`d` > 4)
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, d FROM t WHERE d > 4;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	10
Handler_read_last	0
Handler_read_next	1500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = 5 AND d <= 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` <= 3))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = 5 AND d <= 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = 5 AND d <= 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` <= 3))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = 5 AND d <= 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND d <= 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	64	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` <= 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND d <= 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	51
Handler_read_last	0
Handler_read_next	75
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = 5 AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = 5 AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = 5 AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = 5 AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	64	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	51
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	4	NULL	534	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,5)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	2
Handler_read_last	0
Handler_read_next	500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	4	NULL	534	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,5)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	2
Handler_read_last	0
Handler_read_next	500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	177	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,5)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	102
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	4	NULL	873	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,3,5)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	3
Handler_read_last	0
Handler_read_next	750
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	4	NULL	873	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,3,5)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	3
Handler_read_last	0
Handler_read_next	750
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	290	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,3,5)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	153
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	2	20.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,5)) and (`test`.`t`.`b` in (1,2)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	2
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	2	20.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,5)) and (`test`.`t`.`b` in (1,2)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	2
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	PRIMARY	16	NULL	71	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,5)) and (`test`.`t`.`b` in (1,2)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	44
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	3	50.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,2,3,4,5)) and (`test`.`t`.`b` in (1,2,3)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	3
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	3	50.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,2,3,4,5)) and (`test`.`t`.`b` in (1,2,3)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	3
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	PRIMARY	16	NULL	310	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` in (1,2,3,4,5)) and (`test`.`t`.`b` in (1,2,3)) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	165
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	1	10.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`b` = 2) and (`test`.`t`.`a` = 5) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	1	10.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`b` = 2) and (`test`.`t`.`a` = 5) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	PRIMARY	16	NULL	12	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`b` = 2) and (`test`.`t`.`a` = 5) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a+1, b, c, d FROM t WHERE a = 5 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select (`test`.`t`.`a` + 1) AS `a+1`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT a+1, b, c, d FROM t WHERE a = 5 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a+1, b, c, d FROM t WHERE a = 5 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select (`test`.`t`.`a` + 1) AS `a+1`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT a+1, b, c, d FROM t WHERE a = 5 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a+1, b, c, d FROM t WHERE a = 5 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	64	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ (`test`.`t`.`a` + 1) AS `a+1`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a+1, b, c, d FROM t WHERE a = 5 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	51
Handler_read_last	0
Handler_read_next	50
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	64	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	51
Handler_read_last	0
Handler_read_next	50
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = b AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	index	PRIMARY	PRIMARY	16	NULL	2500	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`b` = `test`.`t`.`a`) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = b AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	0
Handler_read_last	0
Handler_read_next	2500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT a, b, c, d FROM t WHERE a = b AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	index	NULL	PRIMARY	16	NULL	2500	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`b` = `test`.`t`.`a`) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT a, b, c, d FROM t WHERE a = b AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	0
Handler_read_last	0
Handler_read_next	2500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = b AND d >= 98;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	16	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`a` AS `a`,`test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`b` = `test`.`t`.`a`) and (`test`.`t`.`d` >= 98))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = b AND d >= 98;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	500
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan; Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,`test`.`t`.`c`
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using where; Using index; Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,`test`.`t`.`c`
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan; Using filesort
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,`test`.`t`.`c`
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan; Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 1) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,(`test`.`t`.`c` * -(1))
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	200
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using where; Using index; Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 1) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,(`test`.`t`.`c` * -(1))
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan; Using filesort
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 1) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,(`test`.`t`.`c` * -(1))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	200
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT e FROM t WHERE a = 5 AND d <= 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using index condition
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`e` AS `e` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` <= 3))
FLUSH STATUS;
SELECT e FROM t WHERE a = 5 AND d <= 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	75
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT e FROM t WHERE a = 5 AND d <= 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using index condition
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`e` AS `e` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` <= 3))
FLUSH STATUS;
SELECT e FROM t WHERE a = 5 AND d <= 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	75
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ e FROM t WHERE a = 5 AND d <= 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	33.33	Using index condition
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`e` AS `e` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`d` <= 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ e FROM t WHERE a = 5 AND d <= 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	75
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using index condition; Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,`test`.`t`.`c`,`test`.`t`.`e`
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using index condition; Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,`test`.`t`.`c`,`test`.`t`.`e`
FLUSH STATUS;
SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using index condition; Using filesort
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ `test`.`t`.`b` AS `b`,`test`.`t`.`c` AS `c`,`test`.`t`.`d` AS `d` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3)) order by `test`.`t`.`d`,`test`.`t`.`c`,`test`.`t`.`e`
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT count(a), count(b), count(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select count(`test`.`t`.`a`) AS `count(a)`,count(`test`.`t`.`b`) AS `count(b)`,count(`test`.`t`.`c`) AS `count(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT count(a), count(b), count(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT count(a), count(b), count(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select count(`test`.`t`.`a`) AS `count(a)`,count(`test`.`t`.`b`) AS `count(b)`,count(`test`.`t`.`c`) AS `count(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT count(a), count(b), count(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ count(a), count(b), count(c) FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ count(`test`.`t`.`a`) AS `count(a)`,count(`test`.`t`.`b`) AS `count(b)`,count(`test`.`t`.`c`) AS `count(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ count(a), count(b), count(c) FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT min(a), max(b), min(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select min(`test`.`t`.`a`) AS `min(a)`,max(`test`.`t`.`b`) AS `max(b)`,min(`test`.`t`.`c`) AS `min(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT min(a), max(b), min(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT min(a), max(b), min(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select min(`test`.`t`.`a`) AS `min(a)`,max(`test`.`t`.`b`) AS `max(b)`,min(`test`.`t`.`c`) AS `min(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT min(a), max(b), min(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ min(a), max(b), min(c) FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ min(`test`.`t`.`a`) AS `min(a)`,max(`test`.`t`.`b`) AS `max(b)`,min(`test`.`t`.`c`) AS `min(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ min(a), max(b), min(c) FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT sum(a), sum(b), sum(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select sum(`test`.`t`.`a`) AS `sum(a)`,sum(`test`.`t`.`b`) AS `sum(b)`,sum(`test`.`t`.`c`) AS `sum(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT sum(a), sum(b), sum(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT sum(a), sum(b), sum(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	ref	PRIMARY	PRIMARY	4	const	194	11.11	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select sum(`test`.`t`.`a`) AS `sum(a)`,sum(`test`.`t`.`b`) AS `sum(b)`,sum(`test`.`t`.`c`) AS `sum(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT sum(a), sum(b), sum(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ sum(a), avg(b), sum(c) FROM t WHERE a = 5 AND c > 10 AND d < 3;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY	PRIMARY	12	NULL	64	33.33	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ sum(`test`.`t`.`a`) AS `sum(a)`,avg(`test`.`t`.`b`) AS `avg(b)`,sum(`test`.`t`.`c`) AS `sum(c)` from `test`.`t` where ((`test`.`t`.`a` = 5) and (`test`.`t`.`c` > 10) and (`test`.`t`.`d` < 3))
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ sum(a), avg(b), sum(c) FROM t WHERE a = 5 AND c > 10 AND d < 3;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	0
Handler_read_key	11
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = 'skip_scan=on';
EXPLAIN SELECT count(*) FROM t WHERE d < 2;;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select count(0) AS `count(*)` from `test`.`t` where (`test`.`t`.`d` < 2)
FLUSH STATUS;
SELECT count(*) FROM t WHERE d < 2;;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	10
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
set optimizer_switch = 'skip_scan=off';
EXPLAIN SELECT count(*) FROM t WHERE d < 2;;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	index	NULL	b	8	NULL	2500	33.33	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select count(0) AS `count(*)` from `test`.`t` where (`test`.`t`.`d` < 2)
FLUSH STATUS;
SELECT count(*) FROM t WHERE d < 2;;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	0
Handler_read_last	0
Handler_read_next	2500
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.skip_scan, test.no_skip_scan]
EXPLAIN SELECT /*+ SKIP_SCAN(t) */ count(*) FROM t WHERE d < 2;;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t	NULL	range	PRIMARY,b	b	8	NULL	833	100.00	Using where; Using index for skip scan
Warnings:
Note	1003	/* select#1 */ select /*+ SKIP_SCAN(`t`@`select#1`) */ count(0) AS `count(*)` from `test`.`t` where (`test`.`t`.`d` < 2)
FLUSH STATUS;
SELECT /*+ SKIP_SCAN(t) */ count(*) FROM t WHERE d < 2;;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	10
Handler_read_last	0
Handler_read_next	250
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
include/diff_tables.inc [test.hint_skip_scan, test.no_skip_scan]
set optimizer_switch = default;
DROP TABLE t;
