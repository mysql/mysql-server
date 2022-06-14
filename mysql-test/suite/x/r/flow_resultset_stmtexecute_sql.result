CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
CREATE PROCEDURE recreate_tables()
BEGIN
CREATE TABLE IF NOT EXISTS xtable (phrase VARCHAR(30), prio INTEGER);
TRUNCATE TABLE xtable;
INSERT INTO xtable (phrase, prio) VALUES ('Alice has a cat', 1);
INSERT INTO xtable (phrase, prio) VALUES ('Bob has a dog 1', 2);
INSERT INTO xtable (phrase, prio) VALUES ('Peppa has a teddy', 1);
INSERT INTO xtable (phrase, prio) VALUES ('Bob has a dog 2', 3);
INSERT INTO xtable (phrase, prio) VALUES ('Row to delete', 4);
CREATE TABLE IF NOT EXISTS xtable_with_pk (p_id INTEGER AUTO_INCREMENT PRIMARY KEY, description varchar(40));
TRUNCATE TABLE xtable_with_pk;
INSERT INTO xtable_with_pk (description) VALUES ('MyRow');
END //
CREATE PROCEDURE two_resultsets()
BEGIN
SELECT phrase FROM xtable limit 3;
SELECT prio FROM xtable limit 2;
END //
CREATE PROCEDURE two_resultsets_with_errors()
BEGIN
SELECT 1/prio as col1 FROM xtable;
SELECT 1/prio as col1 FROM xtable;
END //
CREATE PROCEDURE three_resultsets()
BEGIN
SELECT phrase FROM xtable limit 3;
SELECT prio FROM xtable limit 2;
SELECT prio FROM xtable ORDER BY prio limit 3;
END //
CALL recreate_tables();


## I. Validate simple resultsets
#
# 1. Assert resultset for delete queries
# 2. Assert resultset for update queries
# 3. Assert resultset for insert queries (no auto_inc column)
# 4. Assert resultset for insert queries (has auto_inc column)
# 5. Assert resultset for select queries

## II. Validate simple multi-resultsets
#
# 1. Assert two resultsets for select queries
# 2. Assert three resultsets for select queries

## III. Validate simple multi/resultsets with warnings
#
# 1. Assert one resultsets for select queries
# 2. Assert two resultsets for select queries

#
# I.1
RUN DELETE FROM xtest.xtable WHERE prio=4 or prio=3
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
RUN UPDATE xtest.xtable SET prio=prio-1
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
RUN INSERT INTO xtest.xtable(phrase, prio) VALUE('Bob has a dog 2', 3)
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
RUN INSERT INTO xtest.xtable_with_pk(description) VALUE('MyCustomRow')
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
RUN SELECT phrase FROM xtest.xtable
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
RUN CALL two_resultsets
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.2
RUN CALL three_resultsets
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.1
RUN SELECT 1/prio as col1 FROM xtest.xtable
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.2
RUN CALL two_resultsets_with_errors
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDoneMoreResultsets]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
