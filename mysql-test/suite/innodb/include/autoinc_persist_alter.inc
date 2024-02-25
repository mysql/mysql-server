
eval CREATE TABLE $table LIKE $template;

eval INSERT INTO $table SELECT * FROM $template;

eval SELECT * FROM $table;

eval SHOW CREATE TABLE $table;

--echo # This will keep the autoinc counter
eval ALTER TABLE $table AUTO_INCREMENT = 250, ALGORITHM = $algorithm;
--echo # We expect the counter to be 250
eval SHOW CREATE TABLE $table;

--echo # This should keep the autoinc counter as well
eval ALTER TABLE $table ADD COLUMN b INT, ALGORITHM = $algorithm;
--echo # We expect the counter to be 250
eval SHOW CREATE TABLE $table;

eval DELETE FROM $table WHERE a > 150;

eval SELECT * FROM $table;

--echo # This should reset the autoinc counter to the one specified
--echo # Since it's smaller than current one but bigger than existing
--echo # biggest counter in the table
eval ALTER TABLE $table AUTO_INCREMENT = 180, ALGORITHM = $algorithm;
--echo # We expect the counter to be 180
eval SHOW CREATE TABLE $table;

--echo # This should reset the autoinc counter to the next value of
--echo # current max counter in the table, since the specified value
--echo # is smaller than the existing biggest value(50 < 123)
eval ALTER TABLE $table DROP COLUMN b, AUTO_INCREMENT = 50, ALGORITHM = $algorithm;
--echo # We expect the counter to be 123
eval SHOW CREATE TABLE $table;

eval INSERT INTO $table VALUES(0), (0);

eval SELECT MAX(a) AS `Expect 124` FROM $table;

eval OPTIMIZE TABLE $table;

eval SHOW CREATE TABLE $table;

--source include/restart_mysqld.inc

--echo # We expect the counter to still be 125
eval SHOW CREATE TABLE $table;

eval DELETE FROM $table WHERE a >= 123;

eval CREATE UNIQUE INDEX idx_aa ON $table(a);

--source include/restart_mysqld.inc

eval INSERT INTO $table VALUES(0), (0);

eval SELECT MAX(a) AS `Expect 126` FROM $table;

eval DROP TABLE $table;
