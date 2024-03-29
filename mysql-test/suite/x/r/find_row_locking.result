create schema xtest default charset 'utf8mb4';
######### preparation #########

command ok

0 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

0 rows affected
connecting...
active session is now 'session1'
connecting...
active session is now 'session2'
######### Shared lock after shared lock #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 2, "_id": "2"}
command ok
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Shared lock after exclusive lock #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 2, "_id": "2"}
command ok
Got expected error: Lock wait timeout exceeded; try restarting transaction (code 1205)
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Exclusive lock after shared lock #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 2, "_id": "2"}
command ok
doc
{"a": 30, "_id": "3"}
command ok
Got expected error: Lock wait timeout exceeded; try restarting transaction (code 1205)
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Exclusive lock after exclusive lock #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 2, "_id": "2"}
command ok
Got expected error: Lock wait timeout exceeded; try restarting transaction (code 1205)
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Locking with aggregate #########
######### Shared lock with grouping #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 1}
{"a": 2}
{"a": 30}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Exclusive lock with grouping #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 1}
{"a": 2}
{"a": 30}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Locking options #########
######### Shared lock with NOWAIT #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
Got expected error: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set. (code 3572)
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Shared lock with SKIP LOCKED #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 2, "_id": "2"}
{"a": 30, "_id": "3"}
{"a": 30, "_id": "4"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Exclusive lock with NOWAIT #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
Got expected error: Statement aborted because lock(s) could not be acquired immediately and NOWAIT is set. (code 3572)
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### Exclusive lock with SKIP LOCKED #########
switched to session session1
Verify [Expecting performance_schema.data_locks to be empty]

0 rows affected
doc
{"a": 1, "_id": "1"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session2

0 rows affected
doc
{"a": 2, "_id": "2"}
{"a": 30, "_id": "3"}
{"a": 30, "_id": "4"}
command ok
Verify [Expecting performance_schema.data_locks to be not empty]
switched to session session1

0 rows affected
switched to session session2

0 rows affected
Verify [Expecting performance_schema.data_locks to be empty]
######### NOWAIT set but no locking #########
Mysqlx.Error {
  severity: ERROR
  code: 5169
  msg: "Invalid \"find\" message, \"locking\" field is required when \"locking_options\" is set."
  sql_state: "HY000"
}

######### SKIP LOCKED set but no locking #########
Mysqlx.Error {
  severity: ERROR
  code: 5169
  msg: "Invalid \"find\" message, \"locking\" field is required when \"locking_options\" is set."
  sql_state: "HY000"
}

closing session session2
Mysqlx.Ok {
  msg: "bye!"
}
ok
