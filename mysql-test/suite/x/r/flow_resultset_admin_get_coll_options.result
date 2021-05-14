CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;


## I. Check command flow when using a get_collection_options admin
##    command.
#
# 1. Check flow of get_collection_options command when used with
#    an invalid option.
# 2. Check flow of get_collection_options command when used with
#    a validation option.

#
# I.1
[ASSERT Mysqlx.Error]

#
# I.2

command ok
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
