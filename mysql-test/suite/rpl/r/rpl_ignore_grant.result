include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
delete from mysql.user where user=_binary'rpl_ignore_grant';
delete from mysql.db where user=_binary'rpl_ignore_grant';
flush privileges;
delete from mysql.user where user=_binary'rpl_ignore_grant';
delete from mysql.db where user=_binary'rpl_ignore_grant';
flush privileges;
create user rpl_ignore_grant@localhost;
grant select on *.* to rpl_ignore_grant@localhost;
grant drop on test.* to rpl_ignore_grant@localhost;
show grants for rpl_ignore_grant@localhost;
Grants for rpl_ignore_grant@localhost
GRANT SELECT ON *.* TO `rpl_ignore_grant`@`localhost`
GRANT DROP ON `test`.* TO `rpl_ignore_grant`@`localhost`
show grants for rpl_ignore_grant@localhost;
ERROR 42000: There is no such grant defined for user 'rpl_ignore_grant' on host 'localhost'
select count(*) from mysql.user where user=_binary'rpl_ignore_grant';
count(*)
0
select count(*) from mysql.db where user=_binary'rpl_ignore_grant';
count(*)
0
create user rpl_ignore_grant@localhost;
grant select on *.* to rpl_ignore_grant@localhost;
set password for rpl_ignore_grant@localhost="does it work?";
select authentication_string<>_binary'' from mysql.user where user=_binary'rpl_ignore_grant';
authentication_string<>_binary''
0
delete from mysql.user where user=_binary'rpl_ignore_grant';
delete from mysql.db where user=_binary'rpl_ignore_grant';
flush privileges;
delete from mysql.user where user=_binary'rpl_ignore_grant';
delete from mysql.db where user=_binary'rpl_ignore_grant';
flush privileges;
include/rpl_end.inc
