test_sequence
------ grant/revoke/drop affects a parallel session test ------
show grants for second_user@localhost ;
ERROR 42000: There is no such grant defined for user 'second_user' on host 'localhost'
create database mysqltest;
use mysqltest;
use test;
create user second_user@localhost
identified by 'looser' ;
grant select on mysqltest.t9 to second_user@localhost;
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
select current_user();
current_user()
second_user@localhost
show grants for current_user();
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
prepare s_t9 from 'select c1 as my_col 
                                 from t9 where c1= 1' ;
execute s_t9 ;
my_col
1
select a as my_col from t1;
ERROR 42000: SELECT command denied to user 'second_user'@'localhost' for table 't1'
grant select on mysqltest.t1 to second_user@localhost;
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t1` TO `second_user`@`localhost`
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
drop table mysqltest.t9 ;
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t1` TO `second_user`@`localhost`
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t1` TO `second_user`@`localhost`
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
prepare s_t1 from 'select a as my_col from t1' ;
execute s_t1 ;
my_col
1
2
3
4
execute s_t9 ;
ERROR 42S02: Table 'mysqltest.t9' doesn't exist
deallocate prepare s_t9;
revoke all privileges on mysqltest.t1 from second_user@localhost;
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT SELECT ON `mysqltest`.`t9` TO `second_user`@`localhost`
GRANT USAGE ON *.* TO `second_user`@`localhost`
execute s_t1 ;
ERROR 42000: SELECT command denied to user 'second_user'@'localhost' for table 't1'
revoke all privileges, grant option from second_user@localhost ;
show grants for second_user@localhost ;
Grants for second_user@localhost
GRANT USAGE ON *.* TO `second_user`@`localhost`
drop user second_user@localhost ;
commit ;
show grants for second_user@localhost ;
ERROR 42000: There is no such grant defined for user 'second_user' on host 'localhost'
drop database mysqltest;
create user drop_user@localhost identified by 'looser';
prepare stmt3 from ' grant all on test.t1 to drop_user@localhost ';
grant all on test.t1 to drop_user@localhost;
prepare stmt3 from ' revoke all privileges on test.t1 from 
drop_user@localhost ';
revoke all privileges on test.t1 from drop_user@localhost ;
prepare stmt3 from ' drop user drop_user@localhost ';
drop user drop_user@localhost;
