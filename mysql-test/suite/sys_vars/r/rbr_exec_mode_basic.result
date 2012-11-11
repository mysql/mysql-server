set @saved_session_rbr_exec_mode = @@session.rbr_exec_mode;
SET global rbr_exec_mode=DEFAULT;
ERROR HY000: Variable 'rbr_exec_mode' is a SESSION variable and can't be used with SET GLOBAL
SELECT @@global.rbr_exec_mode;
@@global.rbr_exec_mode
STRICT
SET global rbr_exec_mode='IDEMPOTENT';
ERROR HY000: Variable 'rbr_exec_mode' is a SESSION variable and can't be used with SET GLOBAL
SELECT @@global.rbr_exec_mode;
@@global.rbr_exec_mode
STRICT
SET global rbr_exec_mode='STRICT';
ERROR HY000: Variable 'rbr_exec_mode' is a SESSION variable and can't be used with SET GLOBAL
SELECT @@global.rbr_exec_mode;
@@global.rbr_exec_mode
STRICT
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
SET session rbr_exec_mode=DEFAULT;
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
SET session rbr_exec_mode='IDEMPOTENT';
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
IDEMPOTENT
SET session rbr_exec_mode='STRICT';
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
SET session rbr_exec_mode='STIRCT';
ERROR 42000: Variable 'rbr_exec_mode' can't be set to the value of 'STIRCT'
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
SET session rbr_exec_mode='';
ERROR 42000: Variable 'rbr_exec_mode' can't be set to the value of ''
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
SET session rbr_exec_mode='1';
ERROR 42000: Variable 'rbr_exec_mode' can't be set to the value of '1'
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
SET session rbr_exec_mode=NULL;
ERROR 42000: Variable 'rbr_exec_mode' can't be set to the value of 'NULL'
SELECT @@session.rbr_exec_mode;
@@session.rbr_exec_mode
STRICT
set session rbr_exec_mode = @saved_session_rbr_exec_mode;
