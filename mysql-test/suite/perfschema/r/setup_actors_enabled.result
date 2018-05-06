select * from performance_schema.setup_actors;
HOST	USER	ROLE	ENABLED	HISTORY
%	%	%	YES	YES
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
values ('localhost', 'user1', '%', 'YES', 'YES');
insert into performance_schema.setup_actors
values ('localhost', 'user2', '%', 'NO', 'NO');
create user user1@localhost, user2@localhost;
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
flush privileges;
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	user1	localhost
# Switch to (con2, localhost, user2, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	user2	localhost
# Switch to connection default
update performance_schema.setup_actors set ENABLED='NO' where USER='user1';
update performance_schema.setup_actors set ENABLED='YES' where USER='user2';
# Switch to connection con1
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	user1	localhost
# Switch to connection con2
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	user2	localhost
# Disconnect con1 and con2
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	user1	localhost
# Switch to (con2 localhost, user2, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	user2	localhost
# Disconnect con1 and con2
# Switch to connection default
revoke update on *.* from 'user2'@'localhost';
flush privileges;
# Switch to (con2 localhost, user2, , )
update performance_schema.setup_actors 
set ENABLED='NO';
ERROR 42000: UPDATE command denied to user 'user2'@'localhost' for table 'setup_actors'
insert into performance_schema.setup_actors
values ('%', 'user1', '%', 'YES', 'YES');
insert into performance_schema.setup_actors
values ('localhost', '%', '%', 'NO', 'NO');
insert into performance_schema.setup_actors
values ('%', '%', '%', 'YES', 'YES');
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	user1	localhost
delete from performance_schema.setup_actors where
HOST='localhost' and USER='user1';
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	user1	localhost
delete from performance_schema.setup_actors where
HOST='%' and USER='user1';
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	user1	localhost
delete from performance_schema.setup_actors where
HOST='localhost' and USER='%';
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	user1	localhost
revoke all privileges, grant option from user1@localhost;
revoke all privileges, grant option from user2@localhost;
drop user user1@localhost;
drop user user2@localhost;
flush privileges;
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
values ('%', '%', '%', 'YES', 'YES');
select * from performance_schema.setup_actors;
HOST	USER	ROLE	ENABLED	HISTORY
%	%	%	YES	YES
