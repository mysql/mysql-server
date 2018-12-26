CREATE TABLE worklog5743 (col_1_CHAR CHAR(255) , col_2_CHAR CHAR(255) ,
PRIMARY KEY (col_1_CHAR)) engine = innodb STATS_PERSISTENT=0 charset latin1;
INSERT INTO worklog5743 VALUES(repeat("a", 200) , repeat("o", 200));
SELECT col_1_CHAR = repeat("a", 200) , col_2_CHAR  = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("a", 200)	col_2_CHAR  = repeat("o", 200)
1	1
"In connection 1"
SELECT col_1_CHAR = repeat("a", 200) , col_2_CHAR = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("a", 200)	col_2_CHAR = repeat("o", 200)
1	1
SELECT COUNT(*) FROM worklog5743;
COUNT(*)
1
"In connection 2"
START TRANSACTION;
INSERT INTO worklog5743 VALUES(repeat("b", 200) , repeat("o", 200));
UPDATE worklog5743 SET col_1_CHAR = repeat("d", 200) WHERE col_1_CHAR =
repeat("a", 200) AND col_2_CHAR = repeat("o", 200);
SELECT col_1_CHAR = repeat("a", 200) , col_2_CHAR  = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("a", 200)	col_2_CHAR  = repeat("o", 200)
0	1
0	1
"In connection 1"
SELECT col_1_CHAR = repeat("b", 200) , col_2_CHAR = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("b", 200)	col_2_CHAR = repeat("o", 200)
0	1
START TRANSACTION;
SELECT col_1_CHAR = repeat("a", 200) , col_2_CHAR = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("a", 200)	col_2_CHAR = repeat("o", 200)
1	1
SELECT COUNT(*) FROM worklog5743;
COUNT(*)
1
"In connection 2"
COMMIT;
"In connection 1"
SELECT col_1_CHAR = repeat("b", 200) , col_2_CHAR = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("b", 200)	col_2_CHAR = repeat("o", 200)
0	1
SELECT col_1_CHAR = repeat("a", 200) , col_2_CHAR = repeat("o", 200) FROM
worklog5743;
col_1_CHAR = repeat("a", 200)	col_2_CHAR = repeat("o", 200)
1	1
SELECT COUNT(*) FROM worklog5743;
COUNT(*)
1
COMMIT;
DROP TABLE worklog5743;
