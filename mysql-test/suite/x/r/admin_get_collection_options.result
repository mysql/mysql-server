CREATE SCHEMA xtest;
CREATE USER test_user@localhost;


## I. Check invalid option setting for get_collection_options command.
#
# 1. Check just an invalid option.
# 2. Check both valid and invalid options.

## II. Check get_collection_options when used for not existing schema.

## III. Check get_collection_options that returns a validation.
#
# 1. Get default validation schema.
# 2. Get custom validation schema.
# 3. Get custom validation schema and non-default validation level.
# 4. Check command that returns string with special characters.

## IV. Get option for a collection user has no access to.
#
# 1. Collection does not exist.
# 2. Collection exists.

## V. Check Mysqlx_stmt_get_collection_options variable.

#
# I.1
Got expected error: Requested collection option 'foo' doesn't exist. (code 5181)

#
# I.2
Got expected error: Requested collection option 'foo' doesn't exist. (code 5181)
# I.3
Got expected error: Invalid value for argument 'options' (code 5017)
# I.4
Got expected error: Invalid value for argument 'options[0]' (code 5017)
# I.5
Got expected error: Invalid value for argument 'options[0]' (code 5017)

#
# II
Got expected error: Table 'xtest.test_coll' doesn't exist (code 1146)

#
# III.1

command ok
Result
{ "validation": { "level": "off", "schema": {"type":"object"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# III.2

command ok
Result
{ "validation": { "level": "strict", "schema": {"description":"test"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# III.3

command ok
Result
{ "validation": { "level": "off", "schema": {"description":"test"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# III.4

command ok
Result
{ "validation": { "level": "strict", "schema": {"description":"t`y\\\\p"e o`b`\\\\\\\\c""t"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# IV.1
connecting...
active session is now 'temp'
Got expected error: SELECT command denied to user 'test_user'@'localhost' for table 'test_coll' (code 1142)
closing session temp
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# IV.2

command ok
connecting...
active session is now 'temp'
Got expected error: SELECT command denied to user 'test_user'@'localhost' for table 'test_coll' (code 1142)
closing session temp
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# V
connecting...
active session is now 'temp'

command ok
Verify [Status variable "Mysqlx_stmt_get_collection_options" needs to have a value of 0]
Result
{ "validation": { "level": "off", "schema": {"type":"object"} } }
command ok
Verify [Status variable "Mysqlx_stmt_get_collection_options" needs to have a value of 1]
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5181
  msg: "Requested collection option \'foo\' doesn\'t exist."
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_stmt_get_collection_options" needs to have a value of 2]
Result
{ "validation": { "level": "off", "schema": {"type":"object"} } }
command ok
Verify [Status variable "Mysqlx_stmt_get_collection_options" needs to have a value of 3]

0 rows affected
closing session temp
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP USER test_user@localhost;
