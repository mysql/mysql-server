SELECT * FROM performance_schema.session_connect_attrs
LIMIT 1;
SELECT * FROM performance_schema.session_connect_attrs
where ATTR_NAME='FOO' OR ATTR_VALUE='BAR';
INSERT INTO performance_schema.session_connect_attrs
SET ATTR_NAME='FOO', ATTR_VALUE='BAR',
ORDINAL_POSITION=100, PROCESS_ID=102;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'session_connect_attrs'
UPDATE performance_schema.session_connect_attrs
SET ATTR_NAME='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'session_connect_attrs'
UPDATE performance_schema.session_connect_attrs
SET ATTR_NAME='FOO' WHERE ATTR_VALUE='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'session_connect_attrs'
DELETE FROM performance_schema.session_connect_attrs
WHERE ATTR_VALUE='BAR';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'session_connect_attrs'
DELETE FROM performance_schema.session_connect_attrs;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'session_connect_attrs'
LOCK TABLES performance_schema.session_connect_attrs READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'session_connect_attrs'
UNLOCK TABLES;
LOCK TABLES performance_schema.session_connect_attrs WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'session_connect_attrs'
UNLOCK TABLES;
