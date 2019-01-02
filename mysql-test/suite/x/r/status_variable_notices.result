CREATE SCHEMA xtest;
CREATE TABLE xtest.tab (c INT);

command ok
1/0
null
0 rows affected
Warnings generated:
WARNING | 1365 | Division by 0

1 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'c' at row 1
Verify [Status variable "Mysqlx_notice_warning_sent" needs to have a value of 2]
Verify [Status variable "Mysqlx_notice_other_sent" needs to have a value of 4]
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP SCHEMA IF EXISTS xtest;
