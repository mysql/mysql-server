#
# Variable name : persisted_globals_load
# Scope         : Global Read only
#
# Global - default
SELECT @@global.persisted_globals_load;
@@global.persisted_globals_load
1
# via performance_schema.global_variables
SELECT * FROM performance_schema.global_variables WHERE VARIABLE_NAME
LIKE 'persisted_globals_load';
VARIABLE_NAME	VARIABLE_VALUE
persisted_globals_load	ON
SELECT @@local.persisted_globals_load;
ERROR HY000: Variable 'persisted_globals_load' is a GLOBAL variable
SELECT @@session.persisted_globals_load;
ERROR HY000: Variable 'persisted_globals_load' is a GLOBAL variable
SELECT persisted_globals_load = @@SESSION.persisted_globals_load;
ERROR 42S22: Unknown column 'persisted_globals_load' in 'field list'
SET GLOBAL persisted_globals_load= 1;
ERROR HY000: Variable 'persisted_globals_load' is a read only variable
SET PERSIST persisted_globals_load= 1;
ERROR HY000: Variable 'persisted_globals_load' is a read only variable
# End of tests.
