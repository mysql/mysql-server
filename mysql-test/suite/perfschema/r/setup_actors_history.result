select * from performance_schema.setup_actors;
HOST	USER	ROLE	ENABLED	HISTORY
%	%	%	YES	YES
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
values ('localhost', 'user1', '%', 'YES', 'YES');
insert into performance_schema.setup_actors
values ('localhost', 'user2', '%', 'YES', 'NO');
insert into performance_schema.setup_actors
values ('localhost', 'user3', '%', 'NO', 'YES');
insert into performance_schema.setup_actors
values ('localhost', 'user4', '%', 'NO', 'NO');
create user user1@localhost;
create user user2@localhost;
create user user3@localhost;
create user user4@localhost;
grant ALL on *.* to user1@localhost;
grant ALL on *.* to user2@localhost;
grant ALL on *.* to user3@localhost;
grant ALL on *.* to user4@localhost;
flush privileges;
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	YES	user1	localhost
# Switch to (con2, localhost, user2, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	NO	user2	localhost
# Switch to (con3, localhost, user3, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	YES	user3	localhost
# Switch to (con4, localhost, user4, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	NO	user4	localhost
# Switch to connection default
update performance_schema.setup_actors
set HISTORY='NO' where USER in ('user1', 'user3');
update performance_schema.setup_actors
set HISTORY='YES' where USER in ('user2', 'user4');
# Switch to connection con1
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	YES	user1	localhost
# Switch to connection con2
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	NO	user2	localhost
# Switch to connection con3
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	YES	user3	localhost
# Switch to connection con4
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	NO	user4	localhost
# Disconnect all con
# Switch to (con1, localhost, user1, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	NO	user1	localhost
# Switch to (con2 localhost, user2, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	YES	YES	user2	localhost
# Switch to (con3, localhost, user3, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	NO	user3	localhost
# Switch to (con4 localhost, user4, , )
select NAME, TYPE, INSTRUMENTED, HISTORY, PROCESSLIST_USER, PROCESSLIST_HOST
from performance_schema.threads
where PROCESSLIST_ID = connection_id();
NAME	TYPE	INSTRUMENTED	HISTORY	PROCESSLIST_USER	PROCESSLIST_HOST
thread/sql/one_connection	FOREGROUND	NO	YES	user4	localhost
# Disconnect all con
# Switch to connection default
revoke all privileges, grant option from user1@localhost;
revoke all privileges, grant option from user2@localhost;
revoke all privileges, grant option from user3@localhost;
revoke all privileges, grant option from user4@localhost;
drop user user1@localhost;
drop user user2@localhost;
drop user user3@localhost;
drop user user4@localhost;
flush privileges;
truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
values ('%', '%', '%', 'YES', 'YES');
select * from performance_schema.setup_actors;
HOST	USER	ROLE	ENABLED	HISTORY
%	%	%	YES	YES
