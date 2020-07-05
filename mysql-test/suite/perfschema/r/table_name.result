
#
# TEST 1: Temporary tables, no special prefix.
#

CREATE TEMPORARY TABLE sql_temp2_innodb (a int, b text) ENGINE=INNODB;
INSERT INTO sql_temp2_innodb VALUES(1,'one');

# Confirm that the temporary tables are ignored.

SELECT object_type, object_schema, object_name
FROM performance_schema.objects_summary_global_by_type
WHERE object_schema="test"
ORDER BY object_name;
object_type	object_schema	object_name

# Drop the tables, verify that the table objects are not created.

DROP TABLE sql_temp2_innodb;

SELECT object_type, object_schema, object_name
FROM performance_schema.objects_summary_global_by_type
WHERE object_schema="test"
ORDER BY object_name;
object_type	object_schema	object_name

#
# TEST 2: Temporary tables with the "#sql" prefix.
#

CREATE TEMPORARY TABLE `#sql_temp3_innodb` (a int, b text) ENGINE=INNODB;
CHECK TABLE `#sql_temp3_innodb`;
Table	Op	Msg_type	Msg_text
test.#sql_temp3_innodb	check	status	OK
INSERT INTO `#sql_temp3_innodb` VALUES(1,'one');

# Confirm that the temporary tables are ignored.

SELECT object_type, object_schema, object_name
FROM performance_schema.objects_summary_global_by_type
WHERE object_schema="test"
ORDER BY object_name;
object_type	object_schema	object_name

# Drop the temporary tables.

DROP TABLE `#sql_temp3_innodb`;

# Confirm that the temporary tables are still ignored.

SELECT object_type, object_schema, object_name
FROM performance_schema.objects_summary_global_by_type
WHERE object_schema="test"
ORDER BY object_name;
object_type	object_schema	object_name

# Confirm that no tables remain;

SELECT object_type, object_schema, object_name
FROM performance_schema.objects_summary_global_by_type
WHERE object_schema="test"
ORDER BY object_name;
object_type	object_schema	object_name
