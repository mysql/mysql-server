SET GLOBAL autocommit = 0;
Verify ["SELECT @@autocommit as value" returns in "value" column, following value 0]
Verify ["SELECT autocommit FROM performance_schema.events_transactions_current JOIN performance_schema.threads USING(thread_id) WHERE processlist_id=connection_id()" returns in "autocommit" column, following value NO]
Verify ["SELECT state FROM performance_schema.events_transactions_current JOIN performance_schema.threads USING(thread_id) WHERE processlist_id=connection_id()" returns in "state" column, following value COMMITTED]
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL autocommit = DEFAULT;
