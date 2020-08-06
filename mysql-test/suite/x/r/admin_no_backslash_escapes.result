CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';

command ok

command ok
Verify ["SHOW INDEX FROM xtest.bar WHERE Key_name='foo'" returns in "Key_name" column, following value foo]

0 rows affected
Verify ["list_objects" returns "bar" as a COLLECTION]

command ok
Verify ["SHOW INDEX FROM xtest.bar WHERE Key_name='foo'" returns in "Key_name" column, following value null]
Mysqlx.Ok {
  msg: "bye!"
}
ok
