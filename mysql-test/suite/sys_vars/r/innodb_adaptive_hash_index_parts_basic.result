SELECT COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts);
COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts)
1
1 Expected
SET @@GLOBAL.innodb_adaptive_hash_index_parts=1;
ERROR HY000: Variable 'innodb_adaptive_hash_index_parts' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts);
COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts)
1
1 Expected
SELECT @@GLOBAL.innodb_adaptive_hash_index_parts = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_adaptive_hash_index_parts';
@@GLOBAL.innodb_adaptive_hash_index_parts = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts);
COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_adaptive_hash_index_parts';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_adaptive_hash_index_parts = @@GLOBAL.innodb_adaptive_hash_index_parts;
@@innodb_adaptive_hash_index_parts = @@GLOBAL.innodb_adaptive_hash_index_parts
1
1 Expected
SELECT COUNT(@@innodb_adaptive_hash_index_parts);
COUNT(@@innodb_adaptive_hash_index_parts)
1
1 Expected
SELECT COUNT(@@local.innodb_adaptive_hash_index_parts);
ERROR HY000: Variable 'innodb_adaptive_hash_index_parts' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_adaptive_hash_index_parts);
ERROR HY000: Variable 'innodb_adaptive_hash_index_parts' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts);
COUNT(@@GLOBAL.innodb_adaptive_hash_index_parts)
1
1 Expected
SELECT innodb_adaptive_hash_index_parts = @@SESSION.innodb_adaptive_hash_index_parts;
ERROR 42S22: Unknown column 'innodb_adaptive_hash_index_parts' in 'field list'
Expected error 'Readonly variable'
