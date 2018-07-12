#
# Test system variables related to secondary engine.
#
# Test use_secondary_engine system variable.
SET @my_use_secondary_engine = @@use_secondary_engine;
# Test with an invalid argument.
SET @@use_secondary_engine = INVALID;
ERROR 42000: Variable 'use_secondary_engine' can't be set to the value of 'INVALID'
SELECT @@use_secondary_engine;
@@use_secondary_engine
ON
# Test with integer arguments.
SET @@use_secondary_engine = 0;
SELECT @@use_secondary_engine;
@@use_secondary_engine
OFF
SET @@use_secondary_engine = 1;
SELECT @@use_secondary_engine;
@@use_secondary_engine
ON
SET @@use_secondary_engine = 2;
SELECT @@use_secondary_engine;
@@use_secondary_engine
FORCED
SET @@use_secondary_engine = 3;
ERROR 42000: Variable 'use_secondary_engine' can't be set to the value of '3'
SELECT @@use_secondary_engine;
@@use_secondary_engine
FORCED
SET @@use_secondary_engine = "FORCED";
SELECT @@use_secondary_engine;
@@use_secondary_engine
FORCED
SET @@use_secondary_engine = FORCED;
SELECT @@use_secondary_engine;
@@use_secondary_engine
FORCED
SET @@use_secondary_engine = "ON";
SELECT @@use_secondary_engine;
@@use_secondary_engine
ON
SET @@use_secondary_engine = ON;
SELECT @@use_secondary_engine;
@@use_secondary_engine
ON
SET @@use_secondary_engine = "OFF";
SELECT @@use_secondary_engine;
@@use_secondary_engine
OFF
SET @@use_secondary_engine = OFF;
SELECT @@use_secondary_engine;
@@use_secondary_engine
OFF
SET @@use_secondary_engine = @my_use_secondary_engine;
#
# Bug#28319475: MAKE USE_SECONDARY_ENGINE SESSION-ONLY AND NONPERSISTENT
#
SET GLOBAL use_secondary_engine = ON;
ERROR HY000: Variable 'use_secondary_engine' is a SESSION variable and can't be used with SET GLOBAL
SET PERSIST use_secondary_engine = ON;
ERROR HY000: Variable 'use_secondary_engine' is a SESSION variable and can't be used with SET GLOBAL
SET PERSIST_ONLY use_secondary_engine = ON;
ERROR HY000: Variable 'use_secondary_engine' is a SESSION variable and can't be used with SET GLOBAL
