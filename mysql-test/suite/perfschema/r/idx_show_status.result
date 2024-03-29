truncate performance_schema.status_by_account;
truncate performance_schema.status_by_host;
truncate performance_schema.status_by_user;
truncate performance_schema.status_by_thread;
SELECT thread_id INTO @my_thread_id
FROM performance_schema.threads
WHERE processlist_id = connection_id();
CREATE USER user1@localhost;
GRANT ALL ON *.* to 'user1'@localhost;
SELECT * FROM performance_schema.status_by_account WHERE user = 'user1' AND variable_name = 'Opened_tables';
USER	HOST	VARIABLE_NAME	VARIABLE_VALUE
user1	localhost	#	0
SELECT * FROM performance_schema.status_by_host WHERE host = 'localhost' AND variable_name = 'Opened_tables';
HOST	VARIABLE_NAME	VARIABLE_VALUE
localhost	Opened_tables	#
SELECT * FROM performance_schema.status_by_user WHERE user = 'user1' AND variable_name = 'Opened_tables';
USER	VARIABLE_NAME	VARIABLE_VALUE
user1	Opened_tables	#
SELECT * FROM performance_schema.status_by_thread WHERE thread_id = @my_thread_id AND variable_name = 'Opened_tables';
THREAD_ID	VARIABLE_NAME	VARIABLE_VALUE
tid	Opened_tables	#
DROP USER user1@localhost;
