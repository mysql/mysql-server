SET @start_global_value = @@global.innodb_deadlock_detect;
SELECT @start_global_value;
@start_global_value
1
Valid values are 'ON' and 'OFF'
select @@global.innodb_deadlock_detect in (0, 1);
@@global.innodb_deadlock_detect in (0, 1)
1
select @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
1
select @@session.innodb_deadlock_detect in (0, 1);
ERROR HY000: Variable 'innodb_deadlock_detect' is a GLOBAL variable
select @@session.innodb_deadlock_detect;
ERROR HY000: Variable 'innodb_deadlock_detect' is a GLOBAL variable
show global variables like 'innodb_deadlock_detect';
Variable_name	Value
innodb_deadlock_detect	ON
show session variables like 'innodb_deadlock_detect';
Variable_name	Value
innodb_deadlock_detect	ON
set global innodb_deadlock_detect='OFF';
set session innodb_deadlock_detect='OFF';
ERROR HY000: Variable 'innodb_deadlock_detect' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
0
set @@global.innodb_deadlock_detect=1;
select @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
1
set global innodb_deadlock_detect=0;
select @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
0
set @@global.innodb_deadlock_detect='ON';
select @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
1
set global innodb_deadlock_detect=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_deadlock_detect'
set global innodb_deadlock_detect=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_deadlock_detect'
set global innodb_deadlock_detect=2;
ERROR 42000: Variable 'innodb_deadlock_detect' can't be set to the value of '2'
set global innodb_deadlock_detect='AUTO';
ERROR 42000: Variable 'innodb_deadlock_detect' can't be set to the value of 'AUTO'
set global innodb_deadlock_detect=-3;
ERROR 42000: Variable 'innodb_deadlock_detect' can't be set to the value of '-3'
select @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
1
SET @@global.innodb_deadlock_detect = @start_global_value;
SELECT @@global.innodb_deadlock_detect;
@@global.innodb_deadlock_detect
1
