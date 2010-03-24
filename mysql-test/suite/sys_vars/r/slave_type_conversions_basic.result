set @saved_slave_type_conversions = @@global.slave_type_conversions;
SELECT @@global.slave_type_conversions;
@@global.slave_type_conversions

SET GLOBAL SLAVE_TYPE_CONVERSIONS='';
SELECT @@global.slave_type_conversions;
@@global.slave_type_conversions

SET GLOBAL SLAVE_TYPE_CONVERSIONS='ALL_NON_LOSSY';
SELECT @@global.slave_type_conversions;
@@global.slave_type_conversions
ALL_NON_LOSSY
SET GLOBAL SLAVE_TYPE_CONVERSIONS='ALL_LOSSY';
SELECT @@global.slave_type_conversions;
@@global.slave_type_conversions
ALL_LOSSY
SET GLOBAL SLAVE_TYPE_CONVERSIONS='ALL_LOSSY,ALL_NON_LOSSY';
SELECT @@global.slave_type_conversions;
@@global.slave_type_conversions
ALL_LOSSY,ALL_NON_LOSSY
SET GLOBAL SLAVE_TYPE_CONVERSIONS='ALL_LOSSY,ALL_NON_LOSSY,NONEXISTING_BIT';
ERROR 42000: Variable 'slave_type_conversions' can't be set to the value of 'NONEXISTING_BIT'
SELECT @@global.slave_type_conversions;
@@global.slave_type_conversions
ALL_LOSSY,ALL_NON_LOSSY
set global slave_type_conversions = @saved_slave_type_conversions;
