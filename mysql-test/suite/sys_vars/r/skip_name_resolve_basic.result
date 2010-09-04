SHOW VARIABLES LIKE 'skip_name_resolve';
Variable_name	Value
skip_name_resolve	ON
SHOW GLOBAL VARIABLES LIKE 'skip_name_resolve';
Variable_name	Value
skip_name_resolve	ON
SHOW SESSION VARIABLES LIKE 'skip_name_resolve';
Variable_name	Value
skip_name_resolve	ON
SELECT @@skip_name_resolve;
@@skip_name_resolve
1
SELECT @@LOCAL.skip_name_resolve;
ERROR HY000: Variable 'skip_name_resolve' is a GLOBAL variable
SELECT @@GLOBAL.skip_name_resolve;
@@GLOBAL.skip_name_resolve
1
SET @@skip_name_resolve=0;
ERROR HY000: Variable 'skip_name_resolve' is a read only variable
SET @@LOCAL.skip_name_resolve=0;
ERROR HY000: Variable 'skip_name_resolve' is a read only variable
SET @@GLOBAL.skip_name_resolve=0;
ERROR HY000: Variable 'skip_name_resolve' is a read only variable
