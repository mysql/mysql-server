SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
SET @@GLOBAL.innodb_doublewrite=off;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SET @@GLOBAL.innodb_doublewrite=on;
SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
SELECT @@GLOBAL.innodb_doublewrite = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_doublewrite';
@@GLOBAL.innodb_doublewrite = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_doublewrite';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_doublewrite = @@GLOBAL.innodb_doublewrite;
@@innodb_doublewrite = @@GLOBAL.innodb_doublewrite
1
1 Expected
SELECT COUNT(@@innodb_doublewrite);
COUNT(@@innodb_doublewrite)
1
1 Expected
SELECT COUNT(@@local.innodb_doublewrite);
ERROR HY000: Variable 'innodb_doublewrite' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_doublewrite);
ERROR HY000: Variable 'innodb_doublewrite' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
"##############################################"
# restart: --innodb-doublewrite=ON
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to FALSE if doublewrite is enabled. Please shutdown and change value to FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
"##############################################"
# restart: --innodb-doublewrite=DETECT_AND_RECOVER
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to FALSE if doublewrite is enabled. Please shutdown and change value to FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
"##############################################"
# restart: --innodb-doublewrite=off
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to ON if doublewrite is disabled. Please shutdown and change value to ON
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_ONLY if doublewrite is disabled. Please shutdown and change value to DETECT_ONLY
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_AND_RECOVER if doublewrite is disabled. Please shutdown and change value to DETECT_AND_RECOVER
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to TRUE if doublewrite is disabled. Please shutdown and change value to TRUE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
"##############################################"
# restart: --innodb-doublewrite=FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to ON if doublewrite is disabled. Please shutdown and change value to ON
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_ONLY if doublewrite is disabled. Please shutdown and change value to DETECT_ONLY
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_AND_RECOVER if doublewrite is disabled. Please shutdown and change value to DETECT_AND_RECOVER
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to TRUE if doublewrite is disabled. Please shutdown and change value to TRUE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
"##############################################"
# restart: --innodb-doublewrite=DETECT_ONLY
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=off;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=false;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=on;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
SET @@GLOBAL.innodb_doublewrite=detect_only;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=DETECT_AND_RECOVER;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=true;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
"##############################################"
# restart: --innodb-doublewrite
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
SET @@GLOBAL.innodb_doublewrite=off;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
SET @@GLOBAL.innodb_doublewrite=false;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
SET @@GLOBAL.innodb_doublewrite=on;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
SET @@GLOBAL.innodb_doublewrite=detect_only;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=true;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
SET @@GLOBAL.innodb_doublewrite=detect_and_recover;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
"##############################################"
# restart: --innodb-doublewrite=0
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to ON if doublewrite is disabled. Please shutdown and change value to ON
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_ONLY if doublewrite is disabled. Please shutdown and change value to DETECT_ONLY
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_AND_RECOVER if doublewrite is disabled. Please shutdown and change value to DETECT_AND_RECOVER
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to TRUE if doublewrite is disabled. Please shutdown and change value to TRUE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
OFF
"##############################################"
# restart: --innodb-doublewrite=1
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to FALSE if doublewrite is enabled. Please shutdown and change value to FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
"##############################################"
# restart: --innodb-doublewrite=3
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to FALSE if doublewrite is enabled. Please shutdown and change value to FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
"##############################################"
# restart: --innodb-doublewrite=4
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to ON if doublewrite is disabled. Please shutdown and change value to ON
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_ONLY if doublewrite is disabled. Please shutdown and change value to DETECT_ONLY
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to DETECT_AND_RECOVER if doublewrite is disabled. Please shutdown and change value to DETECT_AND_RECOVER
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to TRUE if doublewrite is disabled. Please shutdown and change value to TRUE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
FALSE
"##############################################"
# restart: --innodb-doublewrite=5
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
TRUE
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
TRUE
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
TRUE
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to FALSE if doublewrite is enabled. Please shutdown and change value to FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
"##############################################"
# restart: --innodb-doublewrite=2
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is OFF
SET @@GLOBAL.innodb_doublewrite=0;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to OFF if doublewrite is enabled. Please shutdown and change value to OFF
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is ON
SET @@GLOBAL.innodb_doublewrite=1;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
ON
# doublewrite mode is DETECT_ONLY
SET @@GLOBAL.innodb_doublewrite=2;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_ONLY
# doublewrite mode is DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=3;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is FALSE
SET @@GLOBAL.innodb_doublewrite=4;
ERROR HY000: Incorrect arguments to InnoDB: cannot change doublewrite mode to FALSE if doublewrite is enabled. Please shutdown and change value to FALSE
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# doublewrite mode is TRUE
SET @@GLOBAL.innodb_doublewrite=5;
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
SET @@GLOBAL.innodb_doublewrite=6;
ERROR 42000: Variable 'innodb_doublewrite' can't be set to the value of '6'
SELECT @@global.innodb_doublewrite;
@@global.innodb_doublewrite
DETECT_AND_RECOVER
# restart
SET PERSIST innodb_doublewrite=ON;
ERROR HY000: Variable 'innodb_doublewrite' is a non persistent variable
SET PERSIST innodb_doublewrite=OFF;
ERROR HY000: Variable 'innodb_doublewrite' is a non persistent variable
SET PERSIST innodb_doublewrite=DETECT_ONLY;
ERROR HY000: Variable 'innodb_doublewrite' is a non persistent variable
SET PERSIST_ONLY innodb_doublewrite=ON;
ERROR HY000: Variable 'innodb_doublewrite' is a non persistent variable
SET PERSIST_ONLY innodb_doublewrite=OFF;
ERROR HY000: Variable 'innodb_doublewrite' is a non persistent variable
SET PERSIST_ONLY innodb_doublewrite=DETECT_ONLY;
ERROR HY000: Variable 'innodb_doublewrite' is a non persistent variable
