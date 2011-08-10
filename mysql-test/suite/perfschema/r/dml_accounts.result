select * from performance_schema.accounts
limit 1;
select * from performance_schema.accounts
where user='FOO' or host='BAR';
insert into performance_schema.accounts
set user='FOO', host='BAR',
current_connections=1, total_connections=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'accounts'
update performance_schema.accounts
set current_connections=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'accounts'
update performance_schema.accounts
set current_connections=12 where host like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'accounts'
delete from performance_schema.accounts
where total_connections=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'accounts'
delete from performance_schema.accounts;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'accounts'
LOCK TABLES performance_schema.accounts READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'accounts'
UNLOCK TABLES;
LOCK TABLES performance_schema.accounts WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'accounts'
UNLOCK TABLES;
