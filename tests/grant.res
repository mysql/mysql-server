delete from user where user='grant_user' or user='grant_user2'
delete from db where user='grant_user'
delete from tables_priv
delete from columns_priv
lock tables mysql.user write
flush privileges
unlock tables
drop database grant_test
Error in execute: Can't drop database 'grant_test'. Database doesn't exist
create database grant_test
Connecting grant_user
Access denied for user: '@localhost' to database 'grant_test'
grant select on *.* to grant_user@localhost
set password FOR grant_user2@localhost = password('test')
Error in execute: Can't find any matching row in the user table
set password FOR grant_user=password('test')
Connecting grant_user
Access denied for user: 'grant_user@localhost' (Using password: NO)
set password FOR grant_user=''
Connecting grant_user
select * from mysql.user where user = 'grant_user'
localhost	grant_user		Y	N	N	N	N	N	N	N	N	N	N	N	N	N

select * from mysql.db where user = 'grant_user'
grant select on *.* to grant_user@localhost,grant_user@localhost
insert into mysql.user (host,user) values ('error','grant_user')
Error in execute: Access denied for user: 'grant_user@localhost' to database 'mysql'
update mysql.user set host='error' WHERE user='grant_user'
Error in execute: Access denied for user: 'grant_user@localhost' to database 'mysql'
create table grant_test.test (a int,b int)
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
grant select on *.* to grant_user2@localhost
Error in execute: Access denied for user: 'grant_user@localhost' (Using password: NO)
revoke select on grant_test.test from grant_user@opt_host
Error in execute: There is no such grant defined for user 'grant_user' on host 'opt_host'
revoke select on grant_test.* from grant_user@opt_host
Error in execute: There is no such grant defined for user 'grant_user' on host 'opt_host'
revoke select on *.* from grant_user
Error in execute: There is no such grant defined for user 'grant_user' on host '%'
grant select on grant_test.not_exists to grant_user
Error in execute: Table 'grant_test.not_exists' doesn't exist
grant FILE on grant_test.test to grant_user
Error in execute: Illegal GRANT/REVOKE command. Please consult the manual which privileges can be used.
grant select on *.* to wrong___________user_name
Error in execute: The host or user argument to GRANT is too long
grant select on grant_test.* to wrong___________user_name
Error in execute: The host or user argument to GRANT is too long
grant select on grant_test.test to grant_user with grant option
Error in execute: grant command denied to user: 'grant_user@localhost' for table 'test'
set password FOR ''@''=''
Error in execute: You are using MySQL as an anonymous users and anonymous users are not allowed to change passwords
set password FOR root@localhost = password('test')
Error in execute: Access denied for user: 'grant_user@localhost' to database 'mysql'
revoke select on *.* from grant_user@localhost
grant create on *.* to grant_user@localhost
Connecting grant_user
create table grant_test.test (a int,b int)
grant select(c) on grant_test.test to grant_user@localhost
Error in execute: Unknown column 'c' in 'test'
revoke select(c) on grant_test.test from grant_user@localhost
Error in execute: There is no such grant defined for user 'grant_user' on host 'localhost' on table 'test'
grant select on grant_test.test to wrong___________user_name
Error in execute: The host or user argument to GRANT is too long
INSERT INTO grant_test.test values (2,0)
Error in execute: insert command denied to user: 'grant_user@localhost' for table 'test'
grant ALL PRIVILEGES on *.* to grant_user@localhost
REVOKE INSERT on *.* from grant_user@localhost
Connecting grant_user
INSERT INTO grant_test.test values (1,0)
Error in execute: insert command denied to user: 'grant_user@localhost' for table 'test'
grant INSERT on *.* to grant_user@localhost
Connecting grant_user
INSERT INTO grant_test.test values (2,0)
select count(*) from grant_test.test
1

revoke SELECT on *.* from grant_user@localhost
Connecting grant_user
select count(*) from grant_test.test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
INSERT INTO grant_test.test values (3,0)
grant SELECT on *.* to grant_user@localhost
Connecting grant_user
select count(*) from grant_test.test
2

revoke ALL PRIVILEGES on *.* from grant_user@localhost
Connecting grant_user
Access denied for user: 'grant_user@localhost' to database 'grant_test'
delete from user where user='grant_user'
flush privileges
delete from user where user='grant_user'
flush privileges
grant select on grant_test.* to grant_user@localhost
select * from mysql.user where user = 'grant_user'
localhost	grant_user		N	N	N	N	N	N	N	N	N	N	N	N	N	N

select * from mysql.db where user = 'grant_user'
localhost	grant_test	grant_user	Y	N	N	N	N	N	N	N	N	N

Connecting grant_user
select count(*) from grant_test.test
2

select * from mysql.user where user = 'grant_user'
Error in execute: Access denied for user: 'grant_user@localhost' to database 'mysql'
insert into grant_test.test values (4,0)
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
update grant_test.test set a=1
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
delete from grant_test.test
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
create table grant_test.test2 (a int)
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
ALTER TABLE grant_test.test add c int
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
CREATE INDEX dummy ON grant_test.test (a)
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
drop table grant_test.test
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
grant ALL PRIVILEGES on grant_test.* to grant_user2@localhost
Error in execute: Access denied for user: 'grant_user@localhost' (Using password: NO)
grant ALL PRIVILEGES on grant_test.* to grant_user@localhost WITH GRANT OPTION
Connecting grant_user
insert into grant_test.test values (5,0)
REVOKE ALL PRIVILEGES on * from grant_user@localhost
Error in execute: There is no such grant defined for user 'grant_user' on host 'localhost'
REVOKE ALL PRIVILEGES on *.* from grant_user@localhost
REVOKE ALL PRIVILEGES on grant_test.* from grant_user@localhost
REVOKE ALL PRIVILEGES on grant_test.* from grant_user@localhost
Connecting grant_user
insert into grant_test.test values (6,0)
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
REVOKE GRANT OPTION on grant_test.* from grant_user@localhost
Connecting grant_user
Access denied for user: 'grant_user@localhost' to database 'grant_test'
grant ALL PRIVILEGES on grant_test.* to grant_user@localhost
Connecting grant_user
select * from mysql.user where user = 'grant_user'
Error in execute: Access denied for user: 'grant_user@localhost' to database 'mysql'
insert into grant_test.test values (7,0)
update grant_test.test set a=3 where a=2
delete from grant_test.test where a=3
create table grant_test.test2 (a int not null)
alter table grant_test.test2 add b int
create index dummy on grant_test.test2 (a)
drop table grant_test.test2
show tables
insert into mysql.user (host,user) values ('error','grant_user',0)
Error in execute: Access denied for user: 'grant_user@localhost' to database 'mysql'
revoke ALL PRIVILEGES on grant_test.* from grant_user@localhost
select * from mysql.user where user = 'grant_user'
localhost	grant_user		N	N	N	N	N	N	N	N	N	N	N	N	N	N

select * from mysql.db where user = 'grant_user'
Connecting grant_user
Access denied for user: 'grant_user@localhost' to database 'grant_test'
grant create on grant_test.test2 to grant_user@localhost
Connecting grant_user
create table grant_test.test2 (a int not null)
show tables
test2

show columns from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
show keys from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
show columns from test2
a	int(11)			0		

show keys from test2
select * from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
grant insert on grant_test.test to grant_user@localhost
show tables
test
test2

insert into grant_test.test values (8,0)
update grant_test.test set b=1
Error in execute: update command denied to user: 'grant_user@localhost' for table 'test'
grant update on grant_test.test to grant_user@localhost
update grant_test.test set b=2
delete from grant_test.test
Error in execute: delete command denied to user: 'grant_user@localhost' for table 'test'
grant delete on grant_test.test to grant_user@localhost
delete from grant_test.test where a=1
Error in execute: select command denied to user: 'grant_user@localhost' for column 'a' in table 'test'
update grant_test.test set b=3 where b=1
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
update grant_test.test set b=b+1
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
select * from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
grant select on grant_test.test to grant_user@localhost
delete from grant_test.test where a=1
update grant_test.test set b=2 where b=1
update grant_test.test set b=b+1
select count(*) from test
3

create table grant_test.test3 (a int)
Error in execute: create command denied to user: 'grant_user@localhost' for table 'test3'
alter table grant_test.test2 add c int
Error in execute: alter command denied to user: 'grant_user@localhost' for table 'test2'
grant alter on grant_test.test2 to grant_user@localhost
alter table grant_test.test2 add c int
create index dummy ON grant_test.test (a)
Error in execute: index command denied to user: 'grant_user@localhost' for table 'test'
grant index on grant_test.test2 to grant_user@localhost
create index dummy ON grant_test.test2 (a)
insert into test2 SELECT a,a from test
Error in execute: insert command denied to user: 'grant_user@localhost' for table 'test2'
grant insert on test2 to grant_user@localhost
Error in execute: Table 'mysql.test2' doesn't exist
grant insert(a) on grant_test.test2 to grant_user@localhost
insert into test2 SELECT a,a from test
Error in execute: insert command denied to user: 'grant_user@localhost' for column 'c' in table 'test2'
grant insert(c) on grant_test.test2 to grant_user@localhost
insert into test2 SELECT a,a from test
select count(*) from test2,test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test2'
select count(*) from test,test2
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test2'
replace into test2 SELECT a from test
Error in execute: update command denied to user: 'grant_user@localhost' for table 'test2'
grant update on grant_test.test2 to grant_user@localhost
replace into test2 SELECT a,a from test
Error in execute: delete command denied to user: 'grant_user@localhost' for table 'test2'
grant DELETE on grant_test.test2 to grant_user@localhost
replace into test2 SELECT a,a from test
insert into test (a) SELECT a from test2
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test2'
drop table grant_test.test2
Error in execute: drop command denied to user: 'grant_user@localhost' for table 'test2'
grant select on grant_test.test2 to grant_user@localhost with grant option
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test2'
grant drop on grant_test.test2 to grant_user@localhost with grant option
grant drop on grant_test.test2 to grant_user@localhost with grant option
grant select on grant_test.test2 to grant_user@localhost with grant option
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test2'
drop table grant_test.test2
create database grant_test
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
drop database grant_test
Error in execute: Access denied for user: 'grant_user@localhost' to database 'grant_test'
flush tables
Error in execute: Access denied for user: 'grant_user@localhost' (Using password: NO)
flush privileges
select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	grant_user@localhost	Update,Delete,Create,Drop,Grant,Index,Alter	Insert
localhost	grant_test	grant_user	test	root@localhost	Select,Insert,Update,Delete	

revoke ALL PRIVILEGES on grant_test.test from grant_user@localhost
revoke ALL PRIVILEGES on grant_test.test2 from grant_user@localhost
revoke GRANT OPTION on grant_test.test2 from grant_user@localhost
select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	

select count(a) from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
delete from grant_test.test where a=2
Error in execute: delete command denied to user: 'grant_user@localhost' for table 'test'
delete from grant_test.test where A=2
Error in execute: delete command denied to user: 'grant_user@localhost' for table 'test'
update test set b=5 where b>0
Error in execute: update command denied to user: 'grant_user@localhost' for table 'test'
grant update(b),delete on grant_test.test to grant_user@localhost
revoke update(a) on grant_test.test from grant_user@localhost
Error in execute: There is no such grant defined for user 'grant_user' on host 'localhost' on table 'test'
delete from grant_test.test where a=2
Error in execute: select command denied to user: 'grant_user@localhost' for column 'a' in table 'test'
update test set b=5 where b>0
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
grant select(a),select(b) on grant_test.test to grant_user@localhost
delete from grant_test.test where a=2
delete from grant_test.test where A=2
update test set b=5 where b>0
update test set a=11 where b>5
Error in execute: update command denied to user: 'grant_user@localhost' for column 'a' in table 'test'
select a,A from test
8	8
5	5
7	7

select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	
localhost	grant_test	grant_user	test	root@localhost	Delete	Select,Update

revoke ALL PRIVILEGES on grant_test.test from grant_user@localhost
select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	

revoke GRANT OPTION on grant_test.test from grant_user@localhost
Error in execute: There is no such grant defined for user 'grant_user' on host 'localhost' on table 'test'
grant select(a) on grant_test.test to grant_user@localhost
show columns from test
a	int(11)	YES		NULL		select
b	int(11)	YES		NULL		

grant insert (b), update (b) on grant_test.test to grant_user@localhost
select count(a) from test
3

select count(skr.a) from test as skr
3

select count(a) from test where a > 5
2

insert into test (b) values (5)
insert into test (b) values (a)
update test set b=3 where a > 0
select * from test
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
select b from test
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
select a from test where b > 0
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
insert into test (a) values (10)
Error in execute: insert command denied to user: 'grant_user@localhost' for column 'a' in table 'test'
insert into test (b) values (b)
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
insert into test (a,b) values (1,5)
Error in execute: insert command denied to user: 'grant_user@localhost' for column 'a' in table 'test'
insert into test (b) values (1),(b)
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
update test set b=3 where b > 0
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	
localhost	grant_test	grant_user	test	root@localhost		Select,Insert,Update

select Host, Db, User, Table_name, Column_name, Column_priv from mysql.columns_priv
localhost	grant_test	grant_user	test	b	Insert,Update
localhost	grant_test	grant_user	test	a	Select

revoke select(a), update (b) on grant_test.test from grant_user@localhost
select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	
localhost	grant_test	grant_user	test	root@localhost		Insert

select Host, Db, User, Table_name, Column_name, Column_priv from mysql.columns_priv
localhost	grant_test	grant_user	test	b	Insert

select count(a) from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
update test set b=4
Error in execute: update command denied to user: 'grant_user@localhost' for table 'test'
grant select(a,b), update (a,b) on grant_test.test to grant_user@localhost
select count(a),count(b) from test where a+b > 0
3	3

insert into test (b) values (9)
update test set b=6 where b > 0
flush privileges
select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	
localhost	grant_test	grant_user	test	root@localhost		Select,Insert,Update

select Host, Db, User, Table_name, Column_name, Column_priv from mysql.columns_priv
localhost	grant_test	grant_user	test	b	Select,Insert,Update
localhost	grant_test	grant_user	test	a	Select,Update

insert into test (a,b) values (12,12)
Error in execute: insert command denied to user: 'grant_user@localhost' for column 'a' in table 'test'
grant insert on grant_test.* to grant_user@localhost
Connecting grant_user
insert into test (a,b) values (13,13)
revoke select(b) on grant_test.test from grant_user@localhost
select count(a) from test where a+b > 0
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
update test set b=5 where a=2
grant select on grant_test.test to grant_user@localhost
Connecting grant_user
select count(a) from test where a+b > 0
4

revoke select(b) on grant_test.test from grant_user@localhost
select count(a) from test where a+b > 0
4

revoke select on grant_test.test from grant_user@localhost
Connecting grant_user
select count(a) from test where a+b > 0
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
grant select(a) on grant_test.test to grant_user@localhost
select count(a) from test where a+b > 0
Error in execute: select command denied to user: 'grant_user@localhost' for column 'b' in table 'test'
grant select on *.* to grant_user@localhost
Connecting grant_user
select count(a) from test where a+b > 0
4

revoke select on *.* from grant_user@localhost
grant select(b) on grant_test.test to grant_user@localhost
Connecting grant_user
select count(a) from test where a+b > 0
4

select * from mysql.db where user = 'grant_user'
localhost	grant_test	grant_user	N	Y	N	N	N	N	N	N	N	N

select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv where user = 'grant_user'
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	
localhost	grant_test	grant_user	test	root@localhost		Select,Insert,Update

select Host, Db, User, Table_name, Column_name, Column_priv from mysql.columns_priv where user = 'grant_user'
localhost	grant_test	grant_user	test	b	Select,Insert,Update
localhost	grant_test	grant_user	test	a	Select,Update

revoke ALL PRIVILEGES on grant_test.test from grant_user@localhost
select count(a) from test
Error in execute: select command denied to user: 'grant_user@localhost' for table 'test'
select * from mysql.user
Error in execute: select command denied to user: 'grant_user@localhost' for table 'user'
select * from mysql.db where user = 'grant_user'
localhost	grant_test	grant_user	N	Y	N	N	N	N	N	N	N	N

select Host, Db, User, Table_name, Grantor, Table_priv, Column_priv from mysql.tables_priv where user = 'grant_user'
localhost	grant_test	grant_user	test2	root@localhost	Grant,Index,Alter	

select Host, Db, User, Table_name, Column_name, Column_priv from mysql.columns_priv where user = 'grant_user'
delete from user where user='grant_user'
flush privileges
grant ALL PRIVILEGES on grant_test.test to grant_user@localhost identified by 'dummy',  grant_user@127.0.0.1 identified by 'dummy2'
Connecting grant_user
grant SELECT on grant_test.* to grant_user@localhost identified by ''
Connecting grant_user
drop database grant_test
delete from user where user='grant_user'
delete from db where user='grant_user'
delete from tables_priv
delete from columns_priv
flush privileges
end of test
