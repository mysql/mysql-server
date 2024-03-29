SET @@cte_max_recursion_depth := 10000;
DROP TABLE IF EXISTS test;
Warnings:
Note	1051	Unknown table 'test.test'
CREATE TABLE test (id INT, json_value JSON);
INSERT INTO test (id, json_value)
WITH RECURSIVE cte AS (
SELECT 1 id
UNION ALL
SELECT 1 + id FROM cte WHERE id < @@cte_max_recursion_depth
)
SELECT id,
JSON_ARRAY(CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED),
CAST(@seed := (@seed * 73127 + 36671) % 99053 AS UNSIGNED))
FROM cte
WHERE (@seed := 0) = 0
ORDER BY id;
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
SELECT id, CAST(json_value AS CHAR) json_value FROM test ORDER BY id LIMIT 20;
id	json_value
1	[36671, 15019, 31420, 53623, 15628, 90966, 5032, 29840, 8761, 27514]
2	[88413, 26706, 37385, 26766, 66673, 46376, 96862, 83168, 8807, 23554]
3	[37412, 20135, 25971, 74819, 34176, 18780, 90939, 11663, 70542, 79371]
4	[90200, 53748, 43627, 49276, 92689, 7437, 81200, 18880, 73717, 77364]
5	[21804, 41638, 9477, 86462, 91302, 10560, 40603, 98577, 94875, 90570]
6	[69269, 98520, 86862, 22414, 75258, 43757, 46698, 69142, 23320, 61863]
7	[42709, 76624, 89815, 30905, 33358, 28906, 54713, 85446, 84020, 8674]
8	[4857, 10452, 67127, 63279, 80156, 44155, 29662, 67151, 35373, 88000]
9	[36420, 84000, 31929, 31338, 389, 54863, 59613, 33992, 34620, 96837]
10	[37947, 17145, 85265, 22082, 65079, 67319, 38137, 43855, 81328, 68154]
11	[82534, 3093, 80483, 84911, 87010, 48433, 57594, 78602, 18588, 17028]
12	[47964, 33369, 40879, 74817, 86028, 51144, 785, 89679, 89986, 54944]
13	[39720, 10939, 20896, 7832, 42889, 65435, 49592, 22419, 44681, 61900]
14	[73977, 72208, 73763, 73404, 69856, 35067, 97116, 35462, 58805, 82017]
15	[34680, 27072, 57557, 47334, 23004, 33080, 5465, 95924, 34718, 32414]
16	[36959, 76359, 26495, 59856, 73366, 64514, 55665, 68091, 31971, 32029]
17	[14116, 66090, 6125, 21880, 52322, 67334, 45459, 98284, 64112, 77352]
18	[35757, 37716, 62871, 59293, 9860, 62104, 34882, 39829, 57542, 40012]
19	[67628, 50296, 95320, 43648, 95, 50026, 62577, 54456, 12824, 82568]
20	[13086, 25560, 32681, 48427, 15044, 76641, 45285, 52970, 7243, 59141]
SET @random_value := 37947;
SELECT id FROM test WHERE @random_value MEMBER OF (json_value->'$');
id
10
2486
4962
7438
9915
ALTER TABLE test ADD INDEX mvi ((CAST(json_value->'$' AS UNSIGNED ARRAY)));
EXPLAIN SELECT id FROM test WHERE @random_value MEMBER OF (json_value->'$');
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	test	NULL	ref	mvi	mvi	9	const	5	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`test`.`id` AS `id` from `test`.`test` where json'37947' member of (cast(json_extract(`json_value`,_utf8mb4'$') as unsigned array))
SELECT id FROM test WHERE @random_value MEMBER OF (json_value->'$');
id
10
2486
4962
7438
9915
SELECT id, CAST(json_value AS CHAR) json_value
FROM test
WHERE id IN (1311, 1312);
id	json_value
1311	[48176, 84025, 77150, 23000, 37731, 70193, 14669, 91697, 71302, 87158]
1312	[74452, 39930, 14394, 89531, 63967, 72608, 4875, 39049, 73010, 82241]
SELECT id FROM test WHERE 48176 MEMBER OF (json_value->'$');
id
1311
3787
6263
8739
SELECT id FROM test WHERE 74452 MEMBER OF (json_value->'$');
id
1312
3788
6264
8740
DROP TABLE test;
