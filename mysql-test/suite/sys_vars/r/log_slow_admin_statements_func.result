SET @old_log_output=                @@global.log_output;
SET @old_slow_query_log=            @@global.slow_query_log;
SET @old_long_query_time=           @@session.long_query_time;
SET @old_log_slow_admin_statements= @@global.log_slow_admin_statements;
USE test;
CREATE TABLE log_slow_admin_statements (
i INT PRIMARY KEY AUTO_INCREMENT,
j VARCHAR(255)
) ENGINE=InnoDB;
SET GLOBAL log_output = 'file,table';
SET GLOBAL slow_query_log = on;
SET SESSION long_query_time = 0;
SET GLOBAL log_slow_admin_statements = off;
ALTER TABLE log_slow_admin_statements ADD COLUMN k INT DEFAULT 17;
CREATE PROCEDURE add_rows()
BEGIN
DECLARE count INT;
SET count = 1;
INSERT INTO log_slow_admin_statements(j) values (REPEAT('A', 255));
WHILE count <= 15 DO
INSERT INTO log_slow_admin_statements(j) SELECT j FROM log_slow_admin_statements;
SET count = count + 1;
END WHILE;
END
$
CALL add_rows();
ANALYZE TABLE log_slow_admin_statements;
Table	Op	Msg_type	Msg_text
test.log_slow_admin_statements	analyze	status	OK
OPTIMIZE TABLE log_slow_admin_statements;
Table	Op	Msg_type	Msg_text
test.log_slow_admin_statements	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.log_slow_admin_statements	optimize	status	OK
CHECK TABLE log_slow_admin_statements EXTENDED;
Table	Op	Msg_type	Msg_text
test.log_slow_admin_statements	check	status	OK
Administrative sql statements must not be logged
SELECT count(sql_text)= 0 FROM mysql.slow_log
WHERE sql_text LIKE '%TABLE log_slow_admin_statements%';
count(sql_text)= 0
1
SET GLOBAL log_slow_admin_statements = on;
DROP TABLE log_slow_admin_statements;
CREATE TABLE log_slow_admin_statements (
i INT PRIMARY KEY AUTO_INCREMENT,
j VARCHAR(255)
) ENGINE=InnoDB;
CALL add_rows();
ANALYZE TABLE log_slow_admin_statements;
Table	Op	Msg_type	Msg_text
test.log_slow_admin_statements	analyze	status	OK
OPTIMIZE TABLE log_slow_admin_statements;
Table	Op	Msg_type	Msg_text
test.log_slow_admin_statements	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.log_slow_admin_statements	optimize	status	OK
CHECK TABLE log_slow_admin_statements EXTENDED;
Table	Op	Msg_type	Msg_text
test.log_slow_admin_statements	check	status	OK
At least one administrativ sql statement must be logged in slow query log
SELECT count(sql_text)> 0 FROM mysql.slow_log
WHERE sql_text LIKE '%TABLE log_slow_admin_statements%';
count(sql_text)> 0
1
SET @@global.log_output=                @old_log_output;
SET @@global.slow_query_log=            @old_slow_query_log;
SET @@session.long_query_time=          @old_long_query_time;
SET @@global.log_slow_admin_statements= @old_log_slow_admin_statements;
DROP PROCEDURE add_rows;
DROP TABLE log_slow_admin_statements;
TRUNCATE TABLE mysql.slow_log;
