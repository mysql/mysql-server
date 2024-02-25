# $column1_type_name must be set.
# $column2_type_name must be set.
# $column2_value must be set.

--echo # t1 column type name: $column1_type_name
--echo # t2 column type name: $column2_type_name
--echo # t2 column value: $column2_value

eval CREATE TABLE t1(a INT, b $column1_type_name);
eval CREATE TABLE t2(a INT, b $column2_type_name NOT NULL);

INSERT INTO t1 VALUES (1, NULL);
eval INSERT INTO t2 VALUES (1, $column2_value);

CREATE TRIGGER t2_bu BEFORE UPDATE ON t2 FOR EACH ROW SET @b = 1;

--error ER_BAD_NULL_ERROR
UPDATE t1, t2 SET t2.b = t1.b;

DROP TABLE t1, t2;
