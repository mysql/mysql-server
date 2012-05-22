CREATE TABLE bug12429573 (i INTEGER PRIMARY KEY, j INTEGER, KEY(j))
ENGINE=INNODB STATS_PERSISTENT=1;
ANALYZE TABLE bug12429573;
Table	Op	Msg_type	Msg_text
test.bug12429573	analyze	status	OK
SELECT last_update FROM mysql.innodb_index_stats WHERE
table_name = 'bug12429573' AND
last_update NOT IN
(SELECT last_update FROM mysql.innodb_table_stats
WHERE table_name = 'bug12429573');
last_update
ANALYZE TABLE bug12429573;
Table	Op	Msg_type	Msg_text
test.bug12429573	analyze	status	OK
SELECT last_update FROM mysql.innodb_index_stats WHERE
table_name = 'bug12429573' AND
last_update NOT IN
(SELECT last_update FROM mysql.innodb_table_stats
WHERE table_name = 'bug12429573');
last_update
DROP TABLE bug12429573;
