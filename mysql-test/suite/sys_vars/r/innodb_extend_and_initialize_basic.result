CALL mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* ignoring option '--innodb-extend-and-initialize'");
SET @start_global_value = @@global.innodb_extend_and_initialize;
SET @start_session_value = @@session.innodb_extend_and_initialize;
ERROR HY000: Variable 'innodb_extend_and_initialize' is a GLOBAL variable
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET @@global.innodb_extend_and_initialize = DEFAULT;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET @@session.innodb_extend_and_initialize = DEFAULT;
ERROR HY000: Variable 'innodb_extend_and_initialize' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.innodb_extend_and_initialize = 10;
ERROR 42000: Variable 'innodb_extend_and_initialize' can't be set to the value of '10'
SET @@global.innodb_extend_and_initialize = 'abc';
ERROR 42000: Variable 'innodb_extend_and_initialize' can't be set to the value of 'abc'
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET @@global.innodb_extend_and_initialize = false;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
SET @@global.innodb_extend_and_initialize = true;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET @@global.innodb_extend_and_initialize = OFF;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
SET @@global.innodb_extend_and_initialize = ON;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET @@global.innodb_extend_and_initialize = 0;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
SET @@global.innodb_extend_and_initialize = 1;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET @@global.innodb_extend_and_initialize = @start_global_value;
# restart: --innodb-extend-and-initialize=10
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
# restart: --innodb-extend-and-initialize='xyz'
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
# restart: --innodb-extend-and-initialize=false
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
# restart: --innodb-extend-and-initialize=true
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
# restart: --innodb-extend-and-initialize=off
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
# restart: --innodb-extend-and-initialize=on
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
# restart: --innodb-extend-and-initialize=0
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
SET @@global.innodb_extend_and_initialize = on;
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
# restart: --innodb-extend-and-initialize=1
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
# restart:
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
1
SET PERSIST innodb_extend_and_initialize = FALSE;
# restart:
SELECT @@global.innodb_extend_and_initialize;
@@global.innodb_extend_and_initialize
0
SET PERSIST innodb_extend_and_initialize = DEFAULT;
# restart:
RESET PERSIST;
