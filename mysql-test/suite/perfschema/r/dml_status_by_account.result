select * from performance_schema.status_by_account
where variable_name like 'bytes_%' limit 1;
select * from performance_schema.status_by_account
where variable_name='FOO';
insert into performance_schema.status_by_account
set user='USER', host='HOST', variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'status_by_account'
update performance_schema.status_by_account
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_account'
update performance_schema.status_by_account
set variable_name='FOO' where user <> 'USER';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_account'
delete from performance_schema.status_by_account
where user <> 'USER';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_account'
delete from performance_schema.status_by_account;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_account'
LOCK TABLES performance_schema.status_by_account READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_account'
UNLOCK TABLES;
LOCK TABLES performance_schema.status_by_account WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_account'
UNLOCK TABLES;
