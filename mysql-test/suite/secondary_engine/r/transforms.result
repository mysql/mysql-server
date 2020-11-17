CREATE TABLE t1(a DATETIME NOT NULL);
INSERT INTO t1 VALUES (TIMESTAMP'2006-06-06 15:55:55');
ALTER TABLE t1 SECONDARY_ENGINE MOCK;
ALTER TABLE t1 SECONDARY_LOAD;
EXPLAIN SELECT max(a) from t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select max(`test`.`t1`.`a`) AS `max(a)` from `test`.`t1`
# Check how the two transforms {scalar,table}-subquery-to-derived
# behave in PREPARE-<change conditions>-EXECUTE--<change conditions>-EXECUTE

# Transform is not active for InnoDB while we the do initial PREPARE
PREPARE s FROM "EXPLAIN SELECT a FROM t1 WHERE a=(SELECT MAX(a) FROM t1) AND (a=TIMESTAMP'2006-06-06 15:55:55')";
PREPARE s2 FROM "EXPLAIN SELECT a FROM t1 WHERE a NOT IN (SELECT a FROM t1 AS t2)";
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET secondary_engine_cost_threshold = 10000;
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` = (/* select#2 */ select max(`test`.`t1`.`a`) from `test`.`t1`))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Not exists; Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` anti join (`test`.`t1` `t2`) on((`test`.`t2`.`a` = `test`.`t1`.`a`)) where true
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` = (/* select#2 */ select max(`test`.`t1`.`a`) from `test`.`t1`))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Not exists; Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` anti join (`test`.`t1` `t2`) on((`test`.`t2`.`a` = `test`.`t1`.`a`)) where true
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` = (/* select#2 */ select max(`test`.`t1`.`a`) from `test`.`t1`))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Not exists; Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` anti join (`test`.`t1` `t2`) on((`test`.`t2`.`a` = `test`.`t1`.`a`)) where true
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=on";
# Let transform be active for InnoDB while we the initial PREPARE
PREPARE s FROM "EXPLAIN SELECT a FROM t1 WHERE a=(SELECT MAX(a) FROM t1) AND (a=TIMESTAMP'2006-06-06 15:55:55')";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET secondary_engine_cost_threshold = 10000;
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` = (/* select#2 */ select max(`test`.`t1`.`a`) from `test`.`t1`))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Not exists; Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` anti join (`test`.`t1` `t2`) on((`test`.`t2`.`a` = `test`.`t1`.`a`)) where true
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` = (/* select#2 */ select max(`test`.`t1`.`a`) from `test`.`t1`))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Not exists; Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` anti join (`test`.`t1` `t2`) on((`test`.`t2`.`a` = `test`.`t1`.`a`)) where true
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` = (/* select#2 */ select max(`test`.`t1`.`a`) from `test`.`t1`))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	NULL
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Not exists; Using join buffer (hash join)
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` anti join (`test`.`t1` `t2`) on((`test`.`t2`.`a` = `test`.`t1`.`a`)) where true
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using join buffer (hash join)
2	DERIVED	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` join (/* select#2 */ select max(`test`.`t1`.`a`) AS `MAX(a)` from `test`.`t1`) `derived_1_2` where ((`test`.`t1`.`a` = TIMESTAMP'2006-06-06 15:55:55') and (`derived_1_2`.`MAX(a)` = TIMESTAMP'2006-06-06 15:55:55'))

EXECUTE s2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
1	PRIMARY	<derived2>	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Using where; Using join buffer (hash join)
2	DERIVED	t2	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using temporary; Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` left join (/* select#2 */ select distinct `test`.`t2`.`a` AS `Name_exp_1` from `test`.`t1` `t2`) `derived_1_2` on((`derived_1_2`.`Name_exp_1` = `test`.`t1`.`a`)) where (`derived_1_2`.`Name_exp_1` is null)
DROP PREPARE s;
DROP PREPARE s2;
DROP TABLE t1;
SET secondary_engine_cost_threshold = default;
SET optimizer_switch="subquery_to_derived=default";
