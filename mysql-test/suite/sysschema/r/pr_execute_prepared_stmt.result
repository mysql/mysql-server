SET @sql := "CREATE TABLE test.t1 (i INT)";
CALL sys.execute_prepared_stmt(@sql);
SHOW CREATE TABLE test.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET @sql := CONCAT('INSERT INTO test.t1 VALUES (', 1, ')');
CALL sys.execute_prepared_stmt(@sql);
SELECT * FROM test.t1;
i
1
SET @sql := "DROP TABLE test.t1";
CALL sys.execute_prepared_stmt(@sql);
SHOW TABLES;
Tables_in_test
SET @sql = NULL;
CALL sys.execute_prepared_stmt(NULL);
ERROR 45000: The @sys.execute_prepared_stmt.sql must contain a query
CALL sys.execute_prepared_stmt('foo');
ERROR 45000: The @sys.execute_prepared_stmt.sql must contain a query
